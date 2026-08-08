#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <UIAutomation.h>
#include <objbase.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
using namespace std::chrono_literals;

constexpr wchar_t kMainWindowName[] = L"SnowShot";
constexpr wchar_t kScreenshotControlAutomationIdSuffix[] = L".settings-item-quick-screenshot";
constexpr wchar_t kScrollingScreenshotControlAutomationIdSuffix[] =
    L".screenshotScrollingScreenshotButton";
constexpr wchar_t kScrollingThumbnailAutomationIdSuffix[] = L".screenshot-scrolling-thumbnail";

constexpr LONG kSelectionLeft = 0;
constexpr LONG kSelectionTop = 0;
constexpr LONG kSelectionRight = 500;
constexpr LONG kSelectionBottom = 500;

constexpr wchar_t kArtifactsDirectory[] = L"e2e-artifacts";
constexpr char kThumbnailBeforeClicksFile[] = "e2e-artifacts/scrolling-thumbnail-before-clicks.bmp";
constexpr char kThumbnailAfterClicksFile[] = "e2e-artifacts/scrolling-thumbnail-after-clicks.bmp";
constexpr char kThumbnailBeforeDragFile[] = "e2e-artifacts/scrolling-thumbnail-before-drag.bmp";
constexpr char kThumbnailAfterDragFile[] = "e2e-artifacts/scrolling-thumbnail-after-drag.bmp";

template <typename T> class ComPtr final {
  public:
    ComPtr() = default;
    ~ComPtr() {
        reset();
    }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    ComPtr(ComPtr&& other) noexcept : m_pointer(other.detach()) {}

    ComPtr& operator=(ComPtr&& other) noexcept {
        if (this != &other) {
            reset(other.detach());
        }
        return *this;
    }

    [[nodiscard]] T* get() const {
        return m_pointer;
    }

    T** put() {
        reset();
        return &m_pointer;
    }

    T* detach() {
        T* const pointer = m_pointer;
        m_pointer = nullptr;
        return pointer;
    }

    void reset(T* pointer = nullptr) {
        if (m_pointer != nullptr) {
            m_pointer->Release();
        }
        m_pointer = pointer;
    }

  private:
    T* m_pointer = nullptr;
};

class ScopedCom final {
  public:
    ScopedCom() {
        m_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    }

    ~ScopedCom() {
        if (SUCCEEDED(m_result)) {
            CoUninitialize();
        }
    }

    [[nodiscard]] HRESULT result() const {
        return m_result;
    }

  private:
    HRESULT m_result = E_FAIL;
};

class ScopedProcess final {
  public:
    ScopedProcess() = default;
    ~ScopedProcess() {
        terminate();
    }

    ScopedProcess(const ScopedProcess&) = delete;
    ScopedProcess& operator=(const ScopedProcess&) = delete;

    bool start(const std::wstring& executablePath) {
        const std::wstring command =
            L'"' + executablePath +
            L"\" --show-main-window --e2e-allow-overlay-capture --e2e-instance-id=" +
            std::to_wstring(GetCurrentProcessId());
        std::vector<wchar_t> commandLine(command.begin(), command.end());
        commandLine.push_back(L'\0');

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        PROCESS_INFORMATION processInfo{};
        if (!CreateProcessW(executablePath.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0,
                            nullptr, nullptr, &startupInfo, &processInfo)) {
            return false;
        }

        CloseHandle(processInfo.hThread);
        m_process = processInfo.hProcess;
        m_processId = processInfo.dwProcessId;
        return true;
    }

    [[nodiscard]] DWORD processId() const {
        return m_processId;
    }

  private:
    void terminate() {
        if (m_process == nullptr) {
            return;
        }

        if (WaitForSingleObject(m_process, 0) == WAIT_TIMEOUT) {
            TerminateProcess(m_process, 1);
            WaitForSingleObject(m_process, 5000);
        }
        CloseHandle(m_process);
        m_process = nullptr;
    }

    HANDLE m_process = nullptr;
    DWORD m_processId = 0;
};

void require(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

[[nodiscard]] std::wstring executablePathFromArguments(int argc, char* argv[]) {
    require(argc == 2, "expected the snow_shot executable path as the sole argument");

    const int requiredLength =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argv[1], -1, nullptr, 0);
    require(requiredLength > 0, "could not convert the executable path to UTF-16");

    std::wstring path(static_cast<std::size_t>(requiredLength), L'\0');
    require(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argv[1], -1, path.data(),
                                requiredLength) == requiredLength,
            "could not convert the executable path to UTF-16");
    path.pop_back();
    return path;
}

[[nodiscard]] ComPtr<IUIAutomationElement> findProcessWindow(IUIAutomation& automation,
                                                             DWORD processId) {
    ComPtr<IUIAutomationElement> root;
    if (FAILED(automation.GetRootElement(root.put()))) {
        return {};
    }

    VARIANT processIdValue;
    VariantInit(&processIdValue);
    processIdValue.vt = VT_I4;
    processIdValue.lVal = static_cast<LONG>(processId);
    ComPtr<IUIAutomationCondition> processCondition;
    const HRESULT conditionResult = automation.CreatePropertyCondition(
        UIA_ProcessIdPropertyId, processIdValue, processCondition.put());
    VariantClear(&processIdValue);
    if (FAILED(conditionResult)) {
        return {};
    }

    ComPtr<IUIAutomationElementArray> candidates;
    if (FAILED(
            root.get()->FindAll(TreeScope_Descendants, processCondition.get(), candidates.put()))) {
        return {};
    }

    int length = 0;
    if (FAILED(candidates.get()->get_Length(&length))) {
        return {};
    }

    for (int index = 0; index < length; ++index) {
        ComPtr<IUIAutomationElement> candidate;
        if (FAILED(candidates.get()->GetElement(index, candidate.put()))) {
            continue;
        }

        CONTROLTYPEID controlType = 0;
        BSTR name = nullptr;
        const HRESULT typeResult = candidate.get()->get_CurrentControlType(&controlType);
        const HRESULT nameResult = candidate.get()->get_CurrentName(&name);
        const bool isMainWindow = SUCCEEDED(typeResult) && SUCCEEDED(nameResult) &&
                                  controlType == UIA_WindowControlTypeId && name != nullptr &&
                                  wcscmp(name, kMainWindowName) == 0;
        SysFreeString(name);
        if (isMainWindow) {
            return candidate;
        }
    }

    return {};
}

[[nodiscard]] ComPtr<IUIAutomationElement>
findProcessDescendantByAutomationIdSuffix(IUIAutomation& automation, DWORD processId,
                                          const wchar_t* automationIdSuffix) {
    ComPtr<IUIAutomationElement> root;
    if (FAILED(automation.GetRootElement(root.put()))) {
        return {};
    }

    VARIANT processIdValue;
    VariantInit(&processIdValue);
    processIdValue.vt = VT_I4;
    processIdValue.lVal = static_cast<LONG>(processId);
    ComPtr<IUIAutomationCondition> processCondition;
    const HRESULT processConditionResult = automation.CreatePropertyCondition(
        UIA_ProcessIdPropertyId, processIdValue, processCondition.put());
    VariantClear(&processIdValue);
    if (FAILED(processConditionResult)) {
        return {};
    }

    ComPtr<IUIAutomationElementArray> elements;
    if (FAILED(
            root.get()->FindAll(TreeScope_Descendants, processCondition.get(), elements.put()))) {
        return {};
    }

    int length = 0;
    if (FAILED(elements.get()->get_Length(&length))) {
        return {};
    }

    const std::size_t suffixLength = std::wcslen(automationIdSuffix);
    for (int index = 0; index < length; ++index) {
        ComPtr<IUIAutomationElement> element;
        if (FAILED(elements.get()->GetElement(index, element.put()))) {
            continue;
        }

        BSTR automationId = nullptr;
        if (FAILED(element.get()->get_CurrentAutomationId(&automationId))) {
            continue;
        }
        const std::size_t automationIdLength = SysStringLen(automationId);
        const bool matches = automationIdLength >= suffixLength &&
                             std::wmemcmp(automationId + automationIdLength - suffixLength,
                                          automationIdSuffix, suffixLength) == 0;
        SysFreeString(automationId);
        if (matches) {
            return element;
        }
    }
    return {};
}

void reportProcessUiAutomationElements(IUIAutomation& automation, DWORD processId) {
    ComPtr<IUIAutomationElement> root;
    if (FAILED(automation.GetRootElement(root.put()))) {
        return;
    }

    VARIANT processIdValue;
    VariantInit(&processIdValue);
    processIdValue.vt = VT_I4;
    processIdValue.lVal = static_cast<LONG>(processId);
    ComPtr<IUIAutomationCondition> processCondition;
    const HRESULT conditionResult = automation.CreatePropertyCondition(
        UIA_ProcessIdPropertyId, processIdValue, processCondition.put());
    VariantClear(&processIdValue);
    if (FAILED(conditionResult)) {
        return;
    }

    ComPtr<IUIAutomationElementArray> elements;
    if (FAILED(
            root.get()->FindAll(TreeScope_Descendants, processCondition.get(), elements.put()))) {
        return;
    }

    int length = 0;
    if (FAILED(elements.get()->get_Length(&length))) {
        return;
    }

    std::wcerr << L"UIA elements exposed by snow_shot:\n";
    for (int index = 0; index < length; ++index) {
        ComPtr<IUIAutomationElement> element;
        if (FAILED(elements.get()->GetElement(index, element.put()))) {
            continue;
        }
        BSTR name = nullptr;
        BSTR automationId = nullptr;
        if (SUCCEEDED(element.get()->get_CurrentName(&name)) &&
            SUCCEEDED(element.get()->get_CurrentAutomationId(&automationId)) &&
            ((name != nullptr && name[0] != L'\0') ||
             (automationId != nullptr && automationId[0] != L'\0'))) {
            std::wcerr << L"  name='" << (name != nullptr ? name : L"") << L"', automationId='"
                       << (automationId != nullptr ? automationId : L"") << L"'\n";
        }
        SysFreeString(name);
        SysFreeString(automationId);
    }
}

[[nodiscard]] bool invoke(IUIAutomationElement& element) {
    ComPtr<IUIAutomationInvokePattern> invokePattern;
    if (SUCCEEDED(
            element.GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(invokePattern.put())))) {
        return SUCCEEDED(invokePattern.get()->Invoke());
    }

    ComPtr<IUIAutomationLegacyIAccessiblePattern> legacyPattern;
    if (SUCCEEDED(element.GetCurrentPatternAs(UIA_LegacyIAccessiblePatternId,
                                              IID_PPV_ARGS(legacyPattern.put())))) {
        return SUCCEEDED(legacyPattern.get()->DoDefaultAction());
    }

    return false;
}

template <typename Finder>
[[nodiscard]] ComPtr<IUIAutomationElement> waitForElement(Finder&& finder) {
    constexpr auto timeout = 5s;
    constexpr auto pollInterval = 50ms;
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do {
        ComPtr<IUIAutomationElement> element = finder();
        if (element.get() != nullptr) {
            return element;
        }
        std::this_thread::sleep_for(pollInterval);
    } while (std::chrono::steady_clock::now() < deadline);
    return {};
}

void moveCursor(LONG x, LONG y) {
    require(SetCursorPos(x, y) != FALSE, "could not move the cursor");
}

void moveCursorWithInput(LONG x, LONG y) {
    const LONG virtualLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const LONG virtualTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const LONG virtualWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const LONG virtualHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    require(virtualWidth > 1 && virtualHeight > 1,
            "the virtual desktop is too small for absolute mouse input");

    INPUT move{};
    move.type = INPUT_MOUSE;
    move.mi.dx = static_cast<LONG>((static_cast<long long>(x - virtualLeft) * 65535LL) /
                                   static_cast<long long>(virtualWidth - 1));
    move.mi.dy = static_cast<LONG>((static_cast<long long>(y - virtualTop) * 65535LL) /
                                   static_cast<long long>(virtualHeight - 1));
    move.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    require(SendInput(1, &move, sizeof(move)) == 1, "could not send mouse movement");
}

void leftClick() {
    INPUT input[2]{};
    input[0].type = INPUT_MOUSE;
    input[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    input[1].type = INPUT_MOUSE;
    input[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    require(SendInput(static_cast<UINT>(std::size(input)), input, sizeof(INPUT)) ==
                std::size(input),
            "could not send a left click");
}

void dragSelect500By500() {
    moveCursor(kSelectionLeft, kSelectionTop);

    INPUT down{};
    down.type = INPUT_MOUSE;
    down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    require(SendInput(1, &down, sizeof(down)) == 1, "could not press the left mouse button");

    // Let the overlay consume the press before the movement.  Without this
    // boundary Windows can coalesce the messages and leave the app in its
    // intelligent-selection state instead of starting a manual rectangle.
    std::this_thread::sleep_for(25ms);

    moveCursorWithInput(kSelectionRight, kSelectionBottom);
    std::this_thread::sleep_for(25ms);

    INPUT up{};
    up.type = INPUT_MOUSE;
    up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    require(SendInput(1, &up, sizeof(up)) == 1, "could not release the left mouse button");
}

void dragThumbnailHeadHandle(LONG x, LONG startY, LONG endY) {
    moveCursorWithInput(x, startY);
    std::this_thread::sleep_for(100ms);

    INPUT down{};
    down.type = INPUT_MOUSE;
    down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    require(SendInput(1, &down, sizeof(down)) == 1, "could not press the thumbnail crop handle");

    std::this_thread::sleep_for(50ms);
    constexpr int movementSteps = 6;
    for (int step = 1; step <= movementSteps; ++step) {
        const LONG y = startY + ((endY - startY) * static_cast<LONG>(step)) / movementSteps;
        moveCursorWithInput(x, y);
        std::this_thread::sleep_for(25ms);
    }

    INPUT up{};
    up.type = INPUT_MOUSE;
    up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    require(SendInput(1, &up, sizeof(up)) == 1, "could not release the thumbnail crop handle");
}

[[nodiscard]] std::vector<COLORREF> captureScreenRegion(const RECT& region) {
    const LONG width = region.right - region.left;
    const LONG height = region.bottom - region.top;
    require(width > 0 && height > 0, "screen capture region must not be empty");

    HDC screenDc = GetDC(nullptr);
    require(screenDc != nullptr, "could not obtain the screen device context");
    HDC memoryDc = CreateCompatibleDC(screenDc);
    require(memoryDc != nullptr, "could not create the memory device context");

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = width;
    bitmapInfo.bmiHeader.biHeight = -height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(screenDc, &bitmapInfo, DIB_RGB_COLORS, &pixels, nullptr, 0);
    require(bitmap != nullptr && pixels != nullptr, "could not allocate the screenshot bitmap");

    HGDIOBJ previousBitmap = SelectObject(memoryDc, bitmap);
    const BOOL copied = BitBlt(memoryDc, 0, 0, width, height, screenDc, region.left, region.top,
                               SRCCOPY | CAPTUREBLT);

    std::vector<COLORREF> image(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    if (copied != FALSE) {
        const auto* const source = static_cast<const std::uint32_t*>(pixels);
        for (std::size_t index = 0; index < image.size(); ++index) {
            image[index] = static_cast<COLORREF>(source[index]);
        }
    }

    SelectObject(memoryDc, previousBitmap);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);
    require(copied != FALSE, "could not capture the requested screen region");
    return image;
}

[[nodiscard]] bool isScrollingThumbnailTrimControlPixel(COLORREF pixel) {
    // captureScreenRegion stores the native 32-bit DIB pixels directly.  A
    // Windows DIB is BGRA in memory, whereas COLORREF accessors expect a
    // 0x00bbggrr value.  Read red and blue from the opposite COLORREF channels
    // to preserve the original screen colours.
    const int red = GetBValue(pixel);
    const int green = GetGValue(pixel);
    const int blue = GetRValue(pixel);
    return red >= 220 && green >= 130 && green <= 210 && blue <= 60;
}

[[nodiscard]] bool hasScrollingThumbnailTrimControl(const std::vector<COLORREF>& image) {
    // The thumbnail's head/tail crop controls are painted with #faad14.  The
    // threshold leaves room for desktop-composition color conversion while
    // requiring a substantial part of the 2 px horizontal trim line.
    std::size_t cropHandlePixels = 0;
    for (const COLORREF pixel : image) {
        if (isScrollingThumbnailTrimControlPixel(pixel)) {
            ++cropHandlePixels;
        }
    }
    return cropHandlePixels >= 80;
}

[[nodiscard]] std::optional<LONG>
scrollingThumbnailTrimControlRow(const std::vector<COLORREF>& image, LONG width, LONG height) {
    require(image.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
            "the thumbnail analysis dimensions do not match its captured pixels");

    LONG strongestRow = -1;
    std::size_t strongestRowPixels = 0;
    for (LONG y = 0; y < height; ++y) {
        std::size_t cropHandlePixels = 0;
        for (LONG x = 0; x < width; ++x) {
            const COLORREF pixel =
                image[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                      static_cast<std::size_t>(x)];
            if (isScrollingThumbnailTrimControlPixel(pixel)) {
                ++cropHandlePixels;
            }
        }
        if (cropHandlePixels > strongestRowPixels) {
            strongestRow = y;
            strongestRowPixels = cropHandlePixels;
        }
    }

    return strongestRowPixels >= 80 ? std::optional<LONG>(strongestRow) : std::nullopt;
}

void saveCaptureAsBmp(const char* fileName, const std::vector<COLORREF>& image, LONG width,
                      LONG height) {
    require(image.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
            "the captured image dimensions do not match the BMP dimensions");

    BITMAPFILEHEADER fileHeader{};
    fileHeader.bfType = 0x4D42;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fileHeader.bfSize = fileHeader.bfOffBits + static_cast<DWORD>(image.size() * sizeof(COLORREF));

    BITMAPINFOHEADER infoHeader{};
    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = width;
    infoHeader.biHeight = -height;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 32;
    infoHeader.biCompression = BI_RGB;
    infoHeader.biSizeImage = static_cast<DWORD>(image.size() * sizeof(COLORREF));

    std::ofstream output(fileName, std::ios::binary | std::ios::trunc);
    require(output.is_open(), "could not create the screenshot artifact");
    output.write(reinterpret_cast<const char*>(&fileHeader),
                 static_cast<std::streamsize>(sizeof(fileHeader)));
    output.write(reinterpret_cast<const char*>(&infoHeader),
                 static_cast<std::streamsize>(sizeof(infoHeader)));
    output.write(reinterpret_cast<const char*>(image.data()),
                 static_cast<std::streamsize>(image.size() * sizeof(COLORREF)));
    require(static_cast<bool>(output), "could not write the screenshot artifact");
}

[[nodiscard]] bool thumbnailTrimControlVisible(const char* artifactFileName,
                                               const RECT& thumbnailBounds) {
    const RECT thumbnailCrop{
        thumbnailBounds.left,
        thumbnailBounds.top,
        thumbnailBounds.right,
        std::min(thumbnailBounds.bottom, thumbnailBounds.top + 25),
    };
    const std::vector<COLORREF> image = captureScreenRegion(thumbnailCrop);
    saveCaptureAsBmp(artifactFileName, image, thumbnailCrop.right - thumbnailCrop.left,
                     thumbnailCrop.bottom - thumbnailCrop.top);
    return hasScrollingThumbnailTrimControl(image);
}

[[nodiscard]] std::optional<LONG> thumbnailTrimControlRow(const char* artifactFileName,
                                                          const RECT& thumbnailBounds) {
    const std::vector<COLORREF> image = captureScreenRegion(thumbnailBounds);
    const LONG width = thumbnailBounds.right - thumbnailBounds.left;
    const LONG height = thumbnailBounds.bottom - thumbnailBounds.top;
    saveCaptureAsBmp(artifactFileName, image, width, height);
    const std::optional<LONG> localRow = scrollingThumbnailTrimControlRow(image, width, height);
    return localRow.has_value() ? std::optional<LONG>(*localRow + thumbnailBounds.top)
                                : std::nullopt;
}

void requireDesktopCanRunScenario() {
    const int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    require(screenWidth >= kSelectionRight && screenHeight >= kSelectionBottom,
            "the primary display must contain the 0,0-500,500 selection region");
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        // Keep UIA, cursor positions, and BitBlt coordinates in physical pixels.
        SetProcessDPIAware();
        requireDesktopCanRunScenario();
        const BOOL artifactsDirectoryCreated = CreateDirectoryW(kArtifactsDirectory, nullptr);
        require(artifactsDirectoryCreated != FALSE || GetLastError() == ERROR_ALREADY_EXISTS,
                "could not create the screenshot artifact directory");

        const ScopedCom com;
        require(SUCCEEDED(com.result()), "could not initialize COM for UI Automation");

        ComPtr<IUIAutomation> automation;
        require(SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                           IID_PPV_ARGS(automation.put()))),
                "could not create the UI Automation client");

        ScopedProcess application;
        require(application.start(executablePathFromArguments(argc, argv)),
                "could not start snow_shot");

        std::this_thread::sleep_for(2s);

        ComPtr<IUIAutomationElement> mainWindow = waitForElement([&automation, &application]() {
            return findProcessWindow(*automation.get(), application.processId());
        });
        require(mainWindow.get() != nullptr, "could not find the SnowShot main window through UIA");

        ComPtr<IUIAutomationElement> screenshotControl = findProcessDescendantByAutomationIdSuffix(
            *automation.get(), application.processId(), kScreenshotControlAutomationIdSuffix);
        if (screenshotControl.get() == nullptr) {
            reportProcessUiAutomationElements(*automation.get(), application.processId());
        }
        require(screenshotControl.get() != nullptr,
                "could not find the Screenshot control through UIA");
        require(invoke(*screenshotControl.get()), "could not start screenshot capture through UIA");

        // The capture overlay owns the selection input surface and needs to be
        // fully shown before the scripted rectangle drag begins.
        std::this_thread::sleep_for(2s);

        dragSelect500By500();

        std::this_thread::sleep_for(1s);

        ComPtr<IUIAutomationElement> scrollingControl =
            waitForElement([&automation, &application]() {
                return findProcessDescendantByAutomationIdSuffix(
                    *automation.get(), application.processId(),
                    kScrollingScreenshotControlAutomationIdSuffix);
            });
        if (scrollingControl.get() == nullptr) {
            reportProcessUiAutomationElements(*automation.get(), application.processId());
        }
        require(scrollingControl.get() != nullptr,
                "could not find the Scrolling screenshot control through UIA");
        require(invoke(*scrollingControl.get()),
                "could not enter scrolling screenshot through UIA");

        ComPtr<IUIAutomationElement> thumbnail = waitForElement([&automation, &application]() {
            ComPtr<IUIAutomationElement> element = findProcessDescendantByAutomationIdSuffix(
                *automation.get(), application.processId(), kScrollingThumbnailAutomationIdSuffix);
            if (element.get() == nullptr) {
                return element;
            }
            BOOL offscreen = TRUE;
            RECT bounds{};
            if (FAILED(element.get()->get_CurrentIsOffscreen(&offscreen)) || offscreen != FALSE ||
                FAILED(element.get()->get_CurrentBoundingRectangle(&bounds)) ||
                bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
                return ComPtr<IUIAutomationElement>{};
            }
            return element;
        });
        require(thumbnail.get() != nullptr,
                "could not find the visible scrolling thumbnail through UIA");
        RECT thumbnailBounds{};
        require(SUCCEEDED(thumbnail.get()->get_CurrentBoundingRectangle(&thumbnailBounds)) &&
                    thumbnailBounds.right > thumbnailBounds.left &&
                    thumbnailBounds.bottom > thumbnailBounds.top,
                "could not determine the scrolling thumbnail bounds through UIA");
        const bool thumbnailVisibleBeforeClicks =
            thumbnailTrimControlVisible(kThumbnailBeforeClicksFile, thumbnailBounds);
        std::cout << "thumbnail crop control before clicks: "
                  << (thumbnailVisibleBeforeClicks ? "detected" : "not detected") << '\n';

        moveCursor(250, 250);
        leftClick();
        moveCursor((thumbnailBounds.left + thumbnailBounds.right) / 2,
                   (thumbnailBounds.top + thumbnailBounds.bottom) / 2);
        leftClick();

        std::this_thread::sleep_for(1s);
        const bool thumbnailVisibleAfterClicks =
            thumbnailTrimControlVisible(kThumbnailAfterClicksFile, thumbnailBounds);
        std::cout << "thumbnail crop control after clicks: "
                  << (thumbnailVisibleAfterClicks ? "detected" : "not detected") << '\n';

        const std::optional<LONG> thumbnailRowBeforeDrag =
            thumbnailTrimControlRow(kThumbnailBeforeDragFile, thumbnailBounds);
        if (thumbnailRowBeforeDrag.has_value()) {
            const LONG targetY = std::min(thumbnailBounds.bottom - 1, thumbnailBounds.top + 100);
            dragThumbnailHeadHandle((thumbnailBounds.left + thumbnailBounds.right) / 2,
                                    *thumbnailRowBeforeDrag, targetY);
        }
        std::this_thread::sleep_for(1s);
        const std::optional<LONG> thumbnailRowAfterDrag =
            thumbnailTrimControlRow(kThumbnailAfterDragFile, thumbnailBounds);
        const bool thumbnailHandleMoved = thumbnailRowBeforeDrag.has_value() &&
                                          thumbnailRowAfterDrag.has_value() &&
                                          *thumbnailRowAfterDrag >= *thumbnailRowBeforeDrag + 50;
        std::cout << "thumbnail crop control row before drag: "
                  << (thumbnailRowBeforeDrag.has_value() ? std::to_string(*thumbnailRowBeforeDrag)
                                                         : "not detected")
                  << '\n';
        std::cout << "thumbnail crop control row after drag: "
                  << (thumbnailRowAfterDrag.has_value() ? std::to_string(*thumbnailRowAfterDrag)
                                                        : "not detected")
                  << '\n';
        std::this_thread::sleep_for(1s);
        require(thumbnailVisibleBeforeClicks && thumbnailVisibleAfterClicks && thumbnailHandleMoved,
                "the scrolling screenshot thumbnail did not repaint automatically when shown or "
                "when its crop handle moved");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
