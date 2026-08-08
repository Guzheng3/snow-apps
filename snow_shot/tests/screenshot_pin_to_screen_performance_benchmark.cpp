#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QSysInfo>
#include <QVector>
#include <QStringList>

#include <Windows.h>
#include <UIAutomation.h>
#include <dwmapi.h>
#include <objbase.h>
#include <psapi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <cwchar>

namespace {
using namespace std::chrono_literals;

template <typename T> class ComPtr final {
  public:
    ComPtr() = default;
    ComPtr(ComPtr&& other) noexcept : m_value(other.m_value) { other.m_value = nullptr; }
    ComPtr& operator=(ComPtr&& other) noexcept { if (this != &other) { reset(other.m_value); other.m_value = nullptr; } return *this; }
    ~ComPtr() { reset(); }
    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;
    T* get() const { return m_value; }
    T** put() { reset(); return &m_value; }
    void reset(T* value = nullptr) { if (m_value) m_value->Release(); m_value = value; }
  private:
    T* m_value = nullptr;
};

class ScopedCom final {
  public:
    ScopedCom() : m_result(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ScopedCom() { if (SUCCEEDED(m_result)) CoUninitialize(); }
    HRESULT result() const { return m_result; }
  private: HRESULT m_result;
};

class ChildProcess final {
  public:
    ~ChildProcess() { stop(); }
    bool start(const QString& executable) {
        std::wstring path = executable.toStdWString();
        std::wstring command = L"\"" + path + L"\" --show-main-window --e2e-allow-overlay-capture --e2e-instance-id=" + std::to_wstring(GetCurrentProcessId());
        std::vector<wchar_t> commandLine(command.begin(), command.end()); commandLine.push_back(L'\0');
        STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION process{};
        if (!CreateProcessW(path.c_str(), commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) return false;
        CloseHandle(process.hThread); m_process = process.hProcess; m_pid = process.dwProcessId; return true;
    }
    DWORD pid() const { return m_pid; }
    bool alive() const { return m_process && WaitForSingleObject(m_process, 0) == WAIT_TIMEOUT; }
    qint64 workingSetBytes(bool peak) const {
        PROCESS_MEMORY_COUNTERS_EX counters{};
        counters.cb = sizeof(counters);
        if (!m_process || !GetProcessMemoryInfo(m_process,
                                                reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                                                sizeof(counters))) {
            return 0;
        }
        return static_cast<qint64>(peak ? counters.PeakWorkingSetSize
                                       : counters.WorkingSetSize);
    }
    void stop() { if (!m_process) return; if (alive()) { TerminateProcess(m_process, 1); WaitForSingleObject(m_process, 5000); } CloseHandle(m_process); m_process = nullptr; }
  private: HANDLE m_process = nullptr; DWORD m_pid = 0;
};

void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

ComPtr<IUIAutomation> createAutomation() {
    ComPtr<IUIAutomation> automation;
    require(SUCCEEDED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(automation.put()))), "UI Automation initialization failed");
    return automation;
}

ComPtr<IUIAutomationElement> findByAutomationIdSuffix(IUIAutomation& automation, DWORD pid,
                                                       const wchar_t* suffix) {
    ComPtr<IUIAutomationElement> root;
    if (FAILED(automation.GetRootElement(root.put()))) return {};
    VARIANT value{}; value.vt = VT_I4; value.lVal = static_cast<LONG>(pid);
    ComPtr<IUIAutomationCondition> condition;
    if (FAILED(automation.CreatePropertyCondition(UIA_ProcessIdPropertyId, value, condition.put()))) return {};
    ComPtr<IUIAutomationElementArray> elements;
    if (FAILED(root.get()->FindAll(TreeScope_Descendants, condition.get(), elements.put()))) return {};
    int length = 0; elements.get()->get_Length(&length);
    for (int i = 0; i < length; ++i) {
        ComPtr<IUIAutomationElement> element; if (FAILED(elements.get()->GetElement(i, element.put()))) continue;
        BSTR id = nullptr; if (FAILED(element.get()->get_CurrentAutomationId(&id))) continue;
        const bool match = id != nullptr && wcsstr(id, suffix) != nullptr; SysFreeString(id);
        if (match) return element;
    }
    return {};
}

template <typename Finder> ComPtr<IUIAutomationElement> waitFor(Finder finder, int timeoutMs) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline) {
        auto result = finder(); if (result.get() != nullptr) return result;
        std::this_thread::sleep_for(25ms);
    }
    return {};
}

bool invoke(IUIAutomationElement& element) {
    ComPtr<IUIAutomationInvokePattern> pattern;
    if (SUCCEEDED(element.GetCurrentPatternAs(UIA_InvokePatternId, IID_PPV_ARGS(pattern.put()))))
        return SUCCEEDED(pattern.get()->Invoke());
    return false;
}

RECT bounds(IUIAutomationElement& element) {
    RECT result{}; element.get_CurrentBoundingRectangle(&result); return result;
}

LONG absoluteCoordinate(LONG value, LONG origin, LONG extent) {
    return extent <= 1 ? 0 : static_cast<LONG>((static_cast<double>(value - origin) * 65535.0) / (extent - 1));
}

void sendMouse(int x, int y, DWORD flags) {
    const LONG left = GetSystemMetrics(SM_XVIRTUALSCREEN), top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const LONG width = GetSystemMetrics(SM_CXVIRTUALSCREEN), height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    INPUT input{}; input.type = INPUT_MOUSE; input.mi.dx = absoluteCoordinate(x, left, width); input.mi.dy = absoluteCoordinate(y, top, height);
    input.mi.dwFlags = flags | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    require(SendInput(1, &input, sizeof(input)) == 1, "SendInput failed");
}

void dragSelection(const RECT& monitor, int fraction) {
    const int width = std::max(64, static_cast<int>((monitor.right - monitor.left) * fraction / 100));
    const int height = std::max(64, static_cast<int>((monitor.bottom - monitor.top) * fraction / 100));
    const int x0 = monitor.left + (monitor.right - monitor.left - width) / 2;
    const int y0 = monitor.top + (monitor.bottom - monitor.top - height) / 2;
    sendMouse(x0, y0, MOUSEEVENTF_MOVE); sendMouse(x0, y0, MOUSEEVENTF_LEFTDOWN);
    std::this_thread::sleep_for(20ms); sendMouse(x0 + width, y0 + height, MOUSEEVENTF_MOVE);
    sendMouse(x0 + width, y0 + height, MOUSEEVENTF_LEFTUP);
}

QVector<RECT> monitors() {
    QVector<RECT> result;
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
        MONITORINFO info{}; info.cbSize = sizeof(info); GetMonitorInfoW(monitor, &info);
        static_cast<QVector<RECT>*>(reinterpret_cast<void*>(data))->push_back(info.rcWork); return TRUE;
    }, reinterpret_cast<LPARAM>(&result));
    return result;
}

int traceLineCount(const QString& path) {
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) return 0;
    return file.readAll().count('\n');
}

QJsonObject readTraceLine(const QString& path, int expectedLine) {
    QFile file(path); require(file.open(QIODevice::ReadOnly), "could not open app trace");
    const QList<QByteArray> lines = file.readAll().split('\n');
    require(expectedLine > 0 && expectedLine <= lines.size(), "trace line was unavailable");
    QJsonParseError error{}; const QJsonDocument document = QJsonDocument::fromJson(lines.at(expectedLine - 1), &error);
    require(error.error == QJsonParseError::NoError && document.isObject(), "trace JSON was invalid");
    return document.object();
}

QJsonObject statistics(const QVector<double>& source,
                       const QString& unit = QStringLiteral("ms")) {
    if (source.isEmpty()) return {};
    QVector<double> values = source; std::sort(values.begin(), values.end());
    auto at = [&values](double p) { return values[std::min(values.size() - 1, static_cast<qsizetype>(std::ceil(p * values.size()) - 1))]; };
    const double mean = std::accumulate(values.cbegin(), values.cend(), 0.0) / values.size();
    double variance = 0.0; for (double value : values) variance += (value - mean) * (value - mean);
    const auto key = [&unit](const char* name) {
        return QString::fromLatin1(name) + QLatin1Char('_') + unit;
    };
    return {{QStringLiteral("count"), values.size()}, {key("min"), values.first()},
            {key("mean"), mean}, {key("p50"), at(.50)}, {key("p90"), at(.90)},
            {key("p95"), at(.95)}, {key("p99"), at(.99)}, {key("max"), values.last()},
            {key("stddev"), std::sqrt(variance / values.size())}};
}

QString htmlEscape(QString value) { return value.replace('&', "&amp;").replace('<', "&lt;").replace('>', "&gt;"); }

bool runSelfTest() {
    const QVector<double> values{1.0, 2.0, 3.0, 4.0, 5.0};
    return qFuzzyCompare(statistics(values).value(QStringLiteral("p50_ms")).toDouble() + 1.0, 4.0);
}

int run(const QCommandLineParser& parser) {
    const QString appPath = parser.value(QStringLiteral("app"));
    const QString output = QDir::cleanPath(parser.value(QStringLiteral("output")));
    const int warmups = parser.value(QStringLiteral("warmups")).toInt();
    const int samples = parser.value(QStringLiteral("samples")).toInt();
    const int timeout = parser.value(QStringLiteral("timeout-ms")).toInt();
    const int screenIndex = parser.value(QStringLiteral("screen-index")).toInt();
    require(!appPath.isEmpty() && warmups >= 0 && samples > 0 && timeout > 0, "invalid benchmark arguments");
    const QVector<RECT> displayList = monitors(); require(screenIndex >= 0 && screenIndex < displayList.size(), "monitor index unavailable");
    QDir().mkpath(output); const QString tracePath = QDir(output).filePath(QStringLiteral("app-trace.jsonl")); QFile::remove(tracePath);
    _putenv_s("SNOW_SHOT_PIN_PERF_TRACE", tracePath.toLocal8Bit().constData());
    ScopedCom com; require(SUCCEEDED(com.result()), "COM initialization failed");
    auto automation = createAutomation(); ChildProcess child; require(child.start(appPath), "could not start snow_shot");
    const QStringList scenarios{QStringLiteral("small"), QStringLiteral("medium"), QStringLiteral("large")};
    QVector<QJsonObject> records; int line = 0;
    for (const QString& scenario : scenarios) {
        const int fraction = scenario == QStringLiteral("small") ? 25 : scenario == QStringLiteral("medium") ? 50 : 90;
        for (int iteration = -warmups; iteration < samples; ++iteration) {
            auto screenshot = waitFor([&]() { return findByAutomationIdSuffix(*automation.get(), child.pid(), L"settings-item-quick-screenshot"); }, timeout);
            require(screenshot.get() != nullptr && invoke(*screenshot.get()), "could not invoke screenshot capture");
            std::this_thread::sleep_for(300ms); dragSelection(displayList.at(screenIndex), fraction);
            auto pin = waitFor([&]() { return findByAutomationIdSuffix(*automation.get(), child.pid(), L"screenshotPinToScreenButton"); }, timeout);
            require(pin.get() != nullptr, "pin button did not appear"); const RECT pinRect = bounds(*pin.get());
            sendMouse((pinRect.left + pinRect.right) / 2, (pinRect.top + pinRect.bottom) / 2, MOUSEEVENTF_MOVE);
            sendMouse((pinRect.left + pinRect.right) / 2, (pinRect.top + pinRect.bottom) / 2, MOUSEEVENTF_LEFTDOWN);
            sendMouse((pinRect.left + pinRect.right) / 2, (pinRect.top + pinRect.bottom) / 2, MOUSEEVENTF_LEFTUP);
            const int targetLine = line + 1;
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
            while (traceLineCount(tracePath) < targetLine && child.alive() && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(25ms);
            require(traceLineCount(tracePath) >= targetLine, "pin sample timed out"); line = targetLine;
            QJsonObject record = readTraceLine(tracePath, line);
            record.insert(QStringLiteral("scenario"), scenario);
            record.insert(QStringLiteral("iteration"), iteration);
            record.insert(QStringLiteral("warmup"), iteration < 0);
            record.insert(QStringLiteral("working_set_bytes"), child.workingSetBytes(false));
            record.insert(QStringLiteral("peak_working_set_bytes"), child.workingSetBytes(true));
            records.push_back(record);
            if (iteration >= 0 && scenario != scenarios.last()) std::this_thread::sleep_for(100ms);
            const qint64 hwndValue = record.value(QStringLiteral("counters")).toObject().value(QStringLiteral("window.hwnd")).toInteger();
            if (hwndValue != 0) PostMessageW(reinterpret_cast<HWND>(static_cast<quintptr>(hwndValue)), WM_CLOSE, 0, 0);
        }
    }
    child.stop();
    QFile raw(QDir(output).filePath(QStringLiteral("raw.jsonl"))); require(raw.open(QIODevice::WriteOnly | QIODevice::Truncate), "could not write raw report");
    for (const QJsonObject& record : records) { raw.write(QJsonDocument(record).toJson(QJsonDocument::Compact)); raw.write("\n"); } raw.close();
    QJsonArray scenarioReports;
    for (const QString& scenario : scenarios) {
        QVector<double> values;
        QVector<double> workingSet;
        QVector<double> peakWorkingSet;
        for (const QJsonObject& record : records) {
            if (record.value(QStringLiteral("scenario")).toString() != scenario ||
                record.value(QStringLiteral("warmup")).toBool()) {
                continue;
            }
            values.push_back(record.value(QStringLiteral("end_to_end_ns")).toDouble() / 1e6);
            workingSet.push_back(record.value(QStringLiteral("working_set_bytes")).toDouble() /
                                 (1024.0 * 1024.0));
            peakWorkingSet.push_back(
                record.value(QStringLiteral("peak_working_set_bytes")).toDouble() /
                (1024.0 * 1024.0));
        }
        scenarioReports.append(QJsonObject{{QStringLiteral("id"), scenario},
                                           {QStringLiteral("controller_to_composited_frame"),
                                            statistics(values)},
                                           {QStringLiteral("working_set"),
                                            statistics(workingSet, QStringLiteral("mb"))},
                                           {QStringLiteral("peak_working_set"),
                                            statistics(peakWorkingSet, QStringLiteral("mb"))}});
    }
    const QJsonObject report{{QStringLiteral("schema_version"), 1}, {QStringLiteral("benchmark"), QStringLiteral("screenshot_pin_to_screen")},
                             {QStringLiteral("generated_utc"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}, {QStringLiteral("warmups"), warmups}, {QStringLiteral("samples"), samples}, {QStringLiteral("screen_index"), screenIndex}, {QStringLiteral("scenarios"), scenarioReports}, {QStringLiteral("environment"), QJsonObject{{QStringLiteral("os"), QSysInfo::prettyProductName()}, {QStringLiteral("qt"), QString::fromLatin1(qVersion())}, {QStringLiteral("architecture"), QSysInfo::currentCpuArchitecture()}}}};
    QFile reportFile(QDir(output).filePath(QStringLiteral("report.json"))); require(reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate), "could not write report.json"); reportFile.write(QJsonDocument(report).toJson(QJsonDocument::Indented)); reportFile.close();
    QFile html(QDir(output).filePath(QStringLiteral("report.html"))); require(html.open(QIODevice::WriteOnly | QIODevice::Truncate), "could not write report.html"); html.write(("<!doctype html><meta charset=utf-8><title>Snow Shot pin performance</title><h1>Pin-to-screen performance</h1><pre>" + htmlEscape(QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented))) + "</pre>").toUtf8()); html.close();
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv); QCoreApplication::setApplicationName(QStringLiteral("snow-shot-pin-to-screen-performance-benchmark"));
    QCommandLineParser parser; parser.setApplicationDescription(QStringLiteral("Native Windows pin-to-screen performance benchmark")); parser.addHelpOption();
    parser.addOption({QStringLiteral("app"), QStringLiteral("snow_shot executable"), QStringLiteral("path")});
    parser.addOption({QStringLiteral("output"), QStringLiteral("output directory"), QStringLiteral("directory"), QStringLiteral("pin-to-screen-performance")});
    parser.addOption({QStringLiteral("screen-index"), QStringLiteral("monitor index"), QStringLiteral("index"), QStringLiteral("0")});
    parser.addOption({QStringLiteral("warmups"), QStringLiteral("warmup samples"), QStringLiteral("count"), QStringLiteral("3")});
    parser.addOption({QStringLiteral("samples"), QStringLiteral("measured samples"), QStringLiteral("count"), QStringLiteral("20")});
    parser.addOption({QStringLiteral("timeout-ms"), QStringLiteral("sample timeout"), QStringLiteral("milliseconds"), QStringLiteral("30000")});
    parser.addOption({QStringLiteral("self-test"), QStringLiteral("run report self-tests")}); parser.process(application);
    try { if (parser.isSet(QStringLiteral("self-test"))) return runSelfTest() ? 0 : 1; return run(parser); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
