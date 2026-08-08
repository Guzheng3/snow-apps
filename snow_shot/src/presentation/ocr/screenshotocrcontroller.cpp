#include "snow_shot/presentation/screenshotocrcontroller.h"

#include "snow_shot/presentation/screenshotcapturestate.h"
#include "snow_shot/presentation/screenshotdisplaysession.h"
#include "snow_shot/presentation/screenshotgeometry.h"
#include "snow_shot/presentation/screenshotocrpresentation.h"
#include "snow_shot/presentation/screenshotrecognitionwindow.h"
#include "snow_shot/presentation/screenshotrecognitionsessioncontroller.h"
#include "snow_shot/presentation/screenshotocrtexteditingsession.h"
#include "snow_shot/presentation/screenshotocrtexttransform.h"
#include "snow_shot/presentation/screenshottabledocument.h"
#include "snow_shot/presentation/screenshottableeditor.h"
#include "snow_shot/presentation/screenshotoverlaycoordinator.h"
#include "snow_shot/presentation/screenshotoverlaywindow.h"
#include "snow_shot/presentation/screenshotselectionmodel.h"
#include "snow_shot/presentation/screenshotsourceimagecomposer.h"
#include "snow_shot/presentation/screenshottoolbarwindow.h"

#include "snow_draw_engine_qt/snow_canvas_widget.h"
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QImage>
#include <QMimeData>
#include <QScreen>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>

#include <utility>

namespace {
constexpr auto kRecognitionMessageKey = "screenshot-ocr-recognition";
constexpr auto kStatusMessageKey = "screenshot-ocr-status";

ScreenshotToolPalette::Tool paletteTool(ScreenshotActiveTool tool) {
    switch (tool) {
    case ScreenshotActiveTool::Select:
        return ScreenshotToolPalette::Tool::Select;
    case ScreenshotActiveTool::Shape:
        return ScreenshotToolPalette::Tool::Shape;
    case ScreenshotActiveTool::Arrow:
        return ScreenshotToolPalette::Tool::Arrow;
    case ScreenshotActiveTool::Line:
        return ScreenshotToolPalette::Tool::Line;
    case ScreenshotActiveTool::FreeDraw:
        return ScreenshotToolPalette::Tool::FreeDraw;
    case ScreenshotActiveTool::RectangleHighlight:
        return ScreenshotToolPalette::Tool::RectangleHighlight;
    case ScreenshotActiveTool::PenHighlight:
        return ScreenshotToolPalette::Tool::PenHighlight;
    case ScreenshotActiveTool::Eraser:
        return ScreenshotToolPalette::Tool::Eraser;
    case ScreenshotActiveTool::RectangleFilter:
        return ScreenshotToolPalette::Tool::RectangleFilter;
    case ScreenshotActiveTool::PenFilter:
        return ScreenshotToolPalette::Tool::PenFilter;
    case ScreenshotActiveTool::Text:
        return ScreenshotToolPalette::Tool::Text;
    case ScreenshotActiveTool::SerialNumber:
        return ScreenshotToolPalette::Tool::SerialNumber;
    case ScreenshotActiveTool::Ocr:
        return ScreenshotToolPalette::Tool::Ocr;
    case ScreenshotActiveTool::Table:
        return ScreenshotToolPalette::Tool::Table;
    case ScreenshotActiveTool::Qr:
        return ScreenshotToolPalette::Tool::Qr;
    case ScreenshotActiveTool::Move:
    default:
        return ScreenshotToolPalette::Tool::Move;
    }
}

QRect recognitionGeometryForDisplay(const ScreenshotGeometryMapper& geometry,
                                    const CapturedDisplayModel& display,
                                    const QRectF& canvasSelection) {
    const QRectF canvasRect = ScreenshotGeometryMapper::displayCanvasRect(display);
    if (!canvasRect.isValid() || canvasRect.isEmpty() || !display.logicalRect.isValid() ||
        display.logicalRect.isEmpty() || !canvasSelection.isValid() ||
        canvasSelection.isEmpty()) {
        return {};
    }
    return QRectF(geometry.logicalPositionForCanvasPoint(display, canvasSelection.topLeft()),
                  geometry.logicalPositionForCanvasPoint(display, canvasSelection.bottomRight()))
        .normalized()
        .toAlignedRect();
}

} // namespace

ScreenshotOcrController::ScreenshotOcrController(ScreenshotOcrControllerContext context,
                                                 QObject* parent)
    : QObject(parent), m_context(std::move(context)),
      m_messages(std::make_unique<ScreenshotMessageService>(
          m_context.displaySession, m_context.geometry, m_context.selection,
          [this]() { return m_context.overlayCoordinator.toolbar(); })) {
    m_session = std::make_unique<ScreenshotRecognitionSessionController>(
        &m_context.recognition, &m_context.qrRecognition, m_context.tableRecognition,
        ScreenshotRecognitionSessionActions{
            [this]() -> ScreenshotRecognitionWindow* {
                return ensureRecognitionWindow() ? m_recognitionWindow.data() : nullptr;
            },
            [this]() { destroyRecognitionWindow(); },
            [this](std::shared_ptr<ScreenshotOcrPresentation> presentation) {
                if (m_recognitionWindow != nullptr) {
                    m_recognitionWindow->setOcrPresentation(presentation);
                }
            },
            [this](std::shared_ptr<ScreenshotOcrPresentation> presentation) {
                m_presentation = presentation;
                applyOcrBackgroundToOverlays(presentation);
            },
            [this]() { clearOcrBackgroundFromOverlays(); },
            [](bool) {},
            [this](int mode) {
                if (ScreenshotToolbarWindow* toolbar = m_context.overlayCoordinator.toolbar()) {
                    const auto tool = mode == static_cast<int>(ScreenshotRecognitionSessionController::Mode::Text)
                                          ? ScreenshotActiveTool::Ocr
                                          : mode == static_cast<int>(ScreenshotRecognitionSessionController::Mode::Table)
                                                ? ScreenshotActiveTool::Table
                                                : ScreenshotActiveTool::Qr;
                    toolbar->setActiveTool(paletteTool(tool));
                }
            },
            [this](bool available, bool editing, bool canUndo, bool canRedo) {
                if (ScreenshotToolbarWindow* toolbar = m_context.overlayCoordinator.toolbar()) {
                    toolbar->setTextEditingState(available, editing, canUndo, canRedo);
                }
            },
            [this](bool available, bool canUndo, bool canRedo, bool canMerge, bool canSplit,
                   bool canReset) {
                if (ScreenshotToolbarWindow* toolbar = m_context.overlayCoordinator.toolbar()) {
                    toolbar->setTableEditingState(available, canUndo, canRedo, canMerge, canSplit,
                                                  canReset);
                }
            },
            [this](bool textBusy, bool tableBusy, bool qrBusy) {
                if (ScreenshotToolbarWindow* toolbar = m_context.overlayCoordinator.toolbar()) {
                    toolbar->setOcrBusy(textBusy);
                    toolbar->setTableBusy(tableBusy);
                    toolbar->setQrBusy(qrBusy);
                }
            },
            [this](const QString& message) {
                QWidget* preferredOwner = m_recognitionWindow.data();
                m_messages->loading(QString::fromLatin1(kRecognitionMessageKey), message, {},
                                    preferredOwner);
            },
            [this]() { hideRecognitionMessage(); },
            [this](const QString& message, bool error) { showStatus(message, error); },
            [this](const QUrl& url) { handleQrLinkActivated(url); },
        },
        this);
    connect(m_session.get(), &ScreenshotRecognitionSessionController::textEditingChanged, this,
            &ScreenshotOcrController::textEditingChanged);
    connect(m_session.get(), &ScreenshotRecognitionSessionController::textResultChanged, this,
            &ScreenshotOcrController::textResultChanged);
    connect(m_session.get(), &ScreenshotRecognitionSessionController::textDraftChanged, this,
            &ScreenshotOcrController::textDraftChanged);
}

ScreenshotOcrController::~ScreenshotOcrController() {
    invalidateSession();
}

bool ScreenshotOcrController::active() const {
    return m_active;
}

bool ScreenshotOcrController::busy() const {
    return busy(Mode::Text) || busy(Mode::Table) || busy(Mode::Qr);
}

bool ScreenshotOcrController::busy(Mode mode) const {
    if (m_session != nullptr) {
        return m_session->busy(static_cast<ScreenshotRecognitionSessionController::Mode>(mode));
    }
    switch (mode) {
    case Mode::Text:
        return m_requestToken != 0;
    case Mode::Table:
        return m_tableRequestToken != 0;
    case Mode::Qr:
        return m_qrRequestToken != 0;
    }
    return false;
}

ScreenshotOcrController::Mode ScreenshotOcrController::mode() const {
    return m_mode;
}

bool ScreenshotOcrController::tableModeActive() const {
    return m_session != nullptr ? m_session->tableModeActive() : m_active && m_mode == Mode::Table;
}

bool ScreenshotOcrController::qrModeActive() const {
    return m_session != nullptr ? m_session->qrModeActive() : m_active && m_mode == Mode::Qr;
}

void ScreenshotOcrController::activate() {
    activateMode(Mode::Text);
}

void ScreenshotOcrController::activateTable() {
    activateMode(Mode::Table);
}

void ScreenshotOcrController::activateQr() {
    activateMode(Mode::Qr);
}

void ScreenshotOcrController::setMode(Mode mode) {
    activateMode(mode);
}

QString ScreenshotOcrController::currentCacheKey() const {
    const QRect selection = m_context.selection.pixelSelection();
    return QStringLiteral("%1:%2,%3,%4,%5")
        .arg(m_context.captureState.sessionId)
        .arg(selection.x())
        .arg(selection.y())
        .arg(selection.width())
        .arg(selection.height());
}

void ScreenshotOcrController::activateMode(Mode mode) {
    const QRect selection = m_context.selection.pixelSelection();
    if (selection.width() < 1 || selection.height() < 1) {
        if (ScreenshotToolbarWindow* toolbar = m_context.overlayCoordinator.toolbar()) {
            toolbar->setActiveTool(paletteTool(m_context.interaction.activeTool()));
        }
        showStatus(tr("Select an area to recognize"), false);
        return;
    }

    clearTextEditingState();

    if (!m_active) {
        m_previousTool = m_context.interaction.activeTool();
        m_canvasStates.clear();
        m_context.displaySession.forEachOverlay(
            [this](qsizetype, ScreenshotOverlayWindow* overlay) {
                if (overlay == nullptr || overlay->canvas() == nullptr) {
                    return;
                }
                SnowCanvasWidget* canvas = overlay->canvas();
                m_canvasStates.push_back(CanvasState{
                    overlay,
                    canvas,
                    canvas->canvasContentVisible(),
                    canvas->interactionEnabled(),
                    overlay->hasScreenshotSelection(),
                    overlay->screenshotSelectionHandlesVisible(),
                    overlay->screenshotSelectionBorderVisible(),
                });
                canvas->setInteractionEnabled(false);
                canvas->setCanvasContentVisible(false);
                overlay->setScreenshotSelection(m_context.selection.normalizedSelection(), false,
                                                m_context.selection.cornerRadius());
                overlay->setScreenshotSelectionBorderVisible(false);
            });
        m_active = true;
        m_context.captureState.sessionState = ScreenshotSessionState::Editing;
        m_context.hideColorPicker();
    }

    m_mode = mode;
    clearOcrBackgroundFromOverlays();
    if (!ensureRecognitionWindow()) {
        restorePreviousToolAfterFailure();
        return;
    }
    m_recognitionWindow->clearOcrPresentation();
    m_recognitionWindow->clearTableSession();
    m_recognitionWindow->clearQrContents();
    emit textEditingChanged(false);
    updateToolbarTextState();
    if (mode != Mode::Table) {
        handleTableCommandStateChanged({});
    }
    if (mode == Mode::Text) {
        m_context.interaction.setOcrTool();
    } else if (mode == Mode::Table) {
        m_context.interaction.setTableTool();
    } else {
        m_context.interaction.setQrTool();
    }
    if (ScreenshotToolbarWindow* toolbar = m_context.overlayCoordinator.toolbar()) {
        const ScreenshotActiveTool activeTool =
            mode == Mode::Text    ? ScreenshotActiveTool::Ocr
            : mode == Mode::Table ? ScreenshotActiveTool::Table
                                  : ScreenshotActiveTool::Qr;
        toolbar->setActiveTool(paletteTool(activeTool));
    }

    const QString key = currentCacheKey();
    if (m_session != nullptr) {
        QImage source = m_surfaceKey == key
                            ? m_surfaceImage
                            : composeScreenshotSourceSelection(m_context.displaySession, selection);
        if (source.isNull()) {
            showStatus(tr("Unable to read the selected screenshot"), true);
            restorePreviousToolAfterFailure();
            return;
        }
        m_session->setTarget(ScreenshotRecognitionTarget{key, std::move(source), QRectF(selection)});
        m_session->activate(static_cast<ScreenshotRecognitionSessionController::Mode>(mode));
        return;
    }
    if (mode == Mode::Text) {
        const auto cached = m_textCache.constFind(key);
        if (cached != m_textCache.cend()) {
            m_textCacheKey = key;
            m_presentation = cached->presentation;
            applyPresentation(m_presentation);
            if (cached->editing) {
                m_editing = true;
                m_editingKey = key;
                m_recognitionWindow->clearOcrPresentation();
                showTextEditor();
                emit textEditingChanged(true);
            }
            updateToolbarTextState();
        } else {
            startRecognition();
        }
    } else if (mode == Mode::Table) {
        const auto cached = m_tableCache.constFind(key);
        if (cached != m_tableCache.cend()) {
            m_tableCacheKey = key;
            applyTableSession(cached.value());
        } else {
            m_tableSession.reset();
            handleTableCommandStateChanged({});
            startTableRecognition();
        }
    } else {
        const auto cached = m_qrCache.constFind(key);
        if (cached != m_qrCache.cend()) {
            m_qrCacheKey = key;
            applyQrContents(cached.value());
        } else {
            m_qrContents.clear();
            startQrRecognition();
        }
    }
}

bool ScreenshotOcrController::copyRecognitionToClipboard() {
    if (!m_active || QApplication::clipboard() == nullptr) {
        return false;
    }
    if (m_session != nullptr) {
        QString text;
        if (m_session->editing()) {
            text = m_session->textDraft();
        } else if (m_presentation != nullptr) {
            text = m_presentation->hasTextSelection()
                       ? m_presentation->selectedText()
                       : snow_shot::presentation::originalOcrText(*m_presentation);
        }
        if (m_session->qrModeActive()) {
            const QStringList contents = m_session->qrContents();
            if (contents.isEmpty()) {
                return false;
            }
            QApplication::clipboard()->setText(contents.join(QLatin1Char('\n')));
            m_context.cancelCapture();
            return true;
        }
        if (m_session->tableModeActive()) {
            if (m_recognitionWindow != nullptr) {
                m_recognitionWindow->commitActiveTableEdit();
            }
            const auto session = m_session->tableSession();
            if (session == nullptr || session->document.empty()) {
                return false;
            }
            auto* mimeData = new QMimeData;
            mimeData->setData(QStringLiteral("text/html"), session->document.toHtml().toUtf8());
            mimeData->setText(session->document.toPlainText());
            QApplication::clipboard()->setMimeData(mimeData, QClipboard::Clipboard);
            m_context.cancelCapture();
            return true;
        }
        if (text.isEmpty()) {
            return false;
        }
        QApplication::clipboard()->setText(text);
        m_context.cancelCapture();
        return true;
    }
    if (m_mode == Mode::Text) {
        QString text;
        if (m_editing) {
            const auto session = m_textCache.value(m_editingKey).editingSession;
            text = session != nullptr ? session->text() : QString{};
        } else if (m_presentation != nullptr) {
            text = m_presentation->hasTextSelection()
                       ? m_presentation->selectedText()
                       : snow_shot::presentation::originalOcrText(*m_presentation);
        }
        if (text.isEmpty()) {
            return false;
        }
        QApplication::clipboard()->setText(text);
        m_context.cancelCapture();
        return true;
    }
    if (m_mode == Mode::Qr) {
        if (m_qrContents.isEmpty()) {
            return false;
        }
        QApplication::clipboard()->setText(m_qrContents.join(QLatin1Char('\n')));
        m_context.cancelCapture();
        return true;
    }
    if (m_recognitionWindow != nullptr) {
        m_recognitionWindow->commitActiveTableEdit();
    }
    if (m_tableSession == nullptr || m_tableSession->document.empty()) {
        return false;
    }
    auto* mimeData = new QMimeData;
    mimeData->setData(QStringLiteral("text/html"),
                      m_tableSession->document.toHtml().toUtf8());
    mimeData->setText(m_tableSession->document.toPlainText());
    QApplication::clipboard()->setMimeData(mimeData, QClipboard::Clipboard);
    m_context.cancelCapture();
    return true;
}

void ScreenshotOcrController::mergeTableSelection() {
    if (m_session != nullptr) {
        m_session->mergeTableSelection();
        return;
    }
    if (tableModeActive() && m_recognitionWindow != nullptr) {
        m_recognitionWindow->mergeTableSelection();
    }
}

void ScreenshotOcrController::splitTableSelection() {
    if (m_session != nullptr) {
        m_session->splitTableSelection();
        return;
    }
    if (tableModeActive() && m_recognitionWindow != nullptr) {
        m_recognitionWindow->splitTableSelection();
    }
}

void ScreenshotOcrController::resetTable() {
    if (m_session != nullptr) {
        m_session->resetTable();
        return;
    }
    if (tableModeActive() && m_recognitionWindow != nullptr) {
        m_recognitionWindow->resetTable();
    }
}

void ScreenshotOcrController::undoTableEdit() {
    if (m_session != nullptr) {
        m_session->undoTableEdit();
        return;
    }
    if (tableModeActive() && m_recognitionWindow != nullptr) {
        m_recognitionWindow->undoTableEdit();
    }
}

void ScreenshotOcrController::redoTableEdit() {
    if (m_session != nullptr) {
        m_session->redoTableEdit();
        return;
    }
    if (tableModeActive() && m_recognitionWindow != nullptr) {
        m_recognitionWindow->redoTableEdit();
    }
}

void ScreenshotOcrController::undoTextEdit() {
    if (m_session != nullptr) {
        m_session->undoTextEdit();
        return;
    }
    if (m_active && m_mode == Mode::Text && m_editing && m_textDocument != nullptr) {
        const auto session = m_textCache.value(m_editingKey).editingSession;
        if (session != nullptr) {
            session->undo();
        }
    }
}

void ScreenshotOcrController::redoTextEdit() {
    if (m_session != nullptr) {
        m_session->redoTextEdit();
        return;
    }
    if (m_active && m_mode == Mode::Text && m_editing && m_textDocument != nullptr) {
        const auto session = m_textCache.value(m_editingKey).editingSession;
        if (session != nullptr) {
            session->redo();
        }
    }
}

ScreenshotTableCommandState ScreenshotOcrController::tableCommandState() const {
    if (m_session != nullptr) {
        return m_session->tableModeActive() && m_recognitionWindow != nullptr
                   ? m_recognitionWindow->tableCommandState()
                   : ScreenshotTableCommandState{};
    }
    return tableModeActive() && m_recognitionWindow != nullptr
               ? m_recognitionWindow->tableCommandState()
               : ScreenshotTableCommandState{};
}

void ScreenshotOcrController::startRecognition() {
    const QRect selection = m_context.selection.pixelSelection();
    const QString key = currentCacheKey();
    if (m_textCache.contains(key)) {
        m_textCacheKey = key;
        m_presentation = m_textCache.value(key).presentation;
        if (m_active && m_mode == Mode::Text) {
            applyPresentation(m_presentation);
        }
        return;
    }
    if (m_requestToken != 0 && m_textCacheKey == key) {
        showRecognitionMessage();
        return;
    }
    if (!screenshotOcrImageWithinPixelLimit(selection.size())) {
        showStatus(tr("Text recognition is unavailable for screenshots larger than 4K"), false);
        return;
    }
    QImage source = m_surfaceKey == key
                        ? m_surfaceImage
                        : composeScreenshotSourceSelection(m_context.displaySession, selection);
    if (source.isNull()) {
        showStatus(tr("Unable to read the selected screenshot"), true);
        return;
    }

    if (m_requestToken != 0) {
        m_context.recognition.cancel(m_requestToken);
        m_requestToken = 0;
    }
    const quint64 generation = ++m_textGeneration;
    const quint64 sessionId = m_context.captureState.sessionId;
    m_textCacheKey = key;
    updateToolbarBusy();
    showRecognitionMessage();
    const auto callbackCompleted = std::make_shared<bool>(false);
    m_requestToken = m_context.recognition.recognize(
        std::move(source), QRectF(selection), this,
        [this, generation, sessionId, selection, key,
         callbackCompleted](ScreenshotOcrRecognitionResult output) mutable {
            *callbackCompleted = true;
            if (generation == m_textGeneration) {
                m_requestToken = 0;
            }
            handleWorkerOutput(generation, sessionId, selection, key, std::move(output));
        });
    if (*callbackCompleted) {
        m_requestToken = 0;
    }
    updateToolbarBusy();
    if (m_requestToken == 0) {
        updateToolbarBusy();
        hideRecognitionMessage();
        showStatus(tr("Text recognition request could not be prepared"), true);
    }
}

void ScreenshotOcrController::startTableRecognition() {
    if (m_context.tableRecognition == nullptr) {
        showStatus(tr("Table recognition service is unavailable"), true);
        return;
    }
    const QRect selection = m_context.selection.pixelSelection();
    const QString key = currentCacheKey();
    if (m_tableCache.contains(key)) {
        m_tableCacheKey = key;
        if (m_active && m_mode == Mode::Table) {
            applyTableSession(m_tableCache.value(key));
        }
        return;
    }
    if (m_tableRequestToken != 0 && m_tableCacheKey == key) {
        showRecognitionMessage();
        return;
    }
    QImage source = m_surfaceKey == key
                        ? m_surfaceImage
                        : composeScreenshotSourceSelection(m_context.displaySession, selection);
    if (source.isNull()) {
        showStatus(tr("Unable to read the selected screenshot"), true);
        return;
    }
    if (m_tableRequestToken != 0) {
        m_context.tableRecognition->cancel(m_tableRequestToken);
        m_tableRequestToken = 0;
    }
    const quint64 generation = ++m_tableGeneration;
    const quint64 sessionId = m_context.captureState.sessionId;
    m_tableCacheKey = key;
    updateToolbarBusy();
    showRecognitionMessage();
    const auto callbackCompleted = std::make_shared<bool>(false);
    m_tableRequestToken = m_context.tableRecognition->extractTable(
        source, this,
        [this, generation, sessionId, selection, key,
         callbackCompleted](SnowShotTableResult result) {
            *callbackCompleted = true;
            if (generation == m_tableGeneration) {
                m_tableRequestToken = 0;
            }
            handleTableOutput(generation, sessionId, selection, key, std::move(result));
        });
    if (*callbackCompleted) {
        m_tableRequestToken = 0;
    }
    updateToolbarBusy();
    if (m_tableRequestToken == 0) {
        updateToolbarBusy();
        hideRecognitionMessage();
        showStatus(tr("Table recognition request could not be prepared"), true);
    }
}

void ScreenshotOcrController::startQrRecognition() {
    const QRect selection = m_context.selection.pixelSelection();
    const QString key = currentCacheKey();
    if (m_qrCache.contains(key)) {
        m_qrCacheKey = key;
        if (m_active && m_mode == Mode::Qr) {
            applyQrContents(m_qrCache.value(key));
        }
        return;
    }
    if (m_qrRequestToken != 0 && m_qrCacheKey == key) {
        showRecognitionMessage();
        return;
    }
    if (!screenshotOcrImageWithinPixelLimit(selection.size())) {
        showStatus(tr("QR code recognition is unavailable for screenshots larger than 4K"),
                   false);
        return;
    }
    QImage source = m_surfaceKey == key
                        ? m_surfaceImage
                        : composeScreenshotSourceSelection(m_context.displaySession, selection);
    if (source.isNull()) {
        showStatus(tr("Unable to read the selected screenshot"), true);
        return;
    }
    if (m_qrRequestToken != 0) {
        m_context.qrRecognition.cancel(m_qrRequestToken);
        m_qrRequestToken = 0;
    }
    const quint64 generation = ++m_qrGeneration;
    const quint64 sessionId = m_context.captureState.sessionId;
    m_qrCacheKey = key;
    updateToolbarBusy();
    showRecognitionMessage();
    const auto callbackCompleted = std::make_shared<bool>(false);
    m_qrRequestToken = m_context.qrRecognition.recognize(
        std::move(source), this,
        [this, generation, sessionId, selection, key,
         callbackCompleted](ScreenshotQrRecognitionResult result) mutable {
            *callbackCompleted = true;
            if (generation == m_qrGeneration) {
                m_qrRequestToken = 0;
            }
            handleQrOutput(generation, sessionId, selection, key, std::move(result));
        });
    if (*callbackCompleted) {
        m_qrRequestToken = 0;
    }
    updateToolbarBusy();
    if (m_qrRequestToken == 0) {
        updateToolbarBusy();
        hideRecognitionMessage();
        showStatus(tr("QR code recognition request could not be prepared"), true);
    }
}

void ScreenshotOcrController::handleWorkerOutput(quint64 generation, quint64 sessionId,
                                                 const QRect& selection, const QString& key,
                                                 ScreenshotOcrRecognitionResult output) {
    if (generation != m_textGeneration || sessionId != m_context.captureState.sessionId ||
        key != QStringLiteral("%1:%2,%3,%4,%5")
                    .arg(sessionId)
                    .arg(selection.x())
                    .arg(selection.y())
                    .arg(selection.width())
                    .arg(selection.height())) {
        return;
    }
    updateToolbarBusy();
    if (!output.error.isEmpty() || output.presentation == nullptr) {
        if (m_active && m_mode == Mode::Text) {
            showStatus(output.error.isEmpty() ? tr("Text recognition failed") : output.error, true);
        }
        hideRecognitionMessage();
        return;
    }
    const QString original = snow_shot::presentation::originalOcrText(*output.presentation);
    auto editingSession = std::make_shared<ScreenshotOcrTextEditingSession>(original);
    connect(editingSession->document(), &QTextDocument::contentsChanged, this,
            [this, key]() { handleTextDocumentChanged(key); });
    TextCacheEntry entry{std::move(output.presentation), std::move(editingSession), false};
    m_textCache.insert(key, entry);
    if (m_active && m_mode == Mode::Text && currentCacheKey() == key) {
        m_textCacheKey = key;
        m_presentation = entry.presentation;
        applyPresentation(m_presentation);
        emit textResultChanged(true);
        updateToolbarTextState();
    }
    if (!busy()) {
        hideRecognitionMessage();
    }
}

void ScreenshotOcrController::handleTableOutput(quint64 generation, quint64 sessionId,
                                                const QRect& selection, const QString& key,
                                                SnowShotTableResult result) {
    const QString expectedKey = QStringLiteral("%1:%2,%3,%4,%5")
                                    .arg(sessionId)
                                    .arg(selection.x())
                                    .arg(selection.y())
                                    .arg(selection.width())
                                    .arg(selection.height());
    if (generation != m_tableGeneration || sessionId != m_context.captureState.sessionId ||
        key != expectedKey) {
        return;
    }
    updateToolbarBusy();
    if (!result.succeeded()) {
        if (m_active && m_mode == Mode::Table) {
            showStatus(result.error.isEmpty() ? tr("Table recognition failed") : result.error,
                       true);
        }
        hideRecognitionMessage();
        return;
    }
    ScreenshotTableDocument document = ScreenshotTableDocument::fromHtml(result.html);
    if (document.empty()) {
        if (m_active && m_mode == Mode::Table) {
            showStatus(tr("No table cells were recognized"), false);
        }
        hideRecognitionMessage();
        return;
    }
    auto session = std::make_shared<ScreenshotTableEditingSession>(std::move(document));
    m_tableCache.insert(key, session);
    if (m_active && m_mode == Mode::Table && currentCacheKey() == key) {
        m_tableCacheKey = key;
        applyTableSession(std::move(session));
    }
    if (!busy()) {
        hideRecognitionMessage();
    }
}

void ScreenshotOcrController::handleQrOutput(quint64 generation, quint64 sessionId,
                                             const QRect& selection, const QString& key,
                                             ScreenshotQrRecognitionResult result) {
    const QString expectedKey = QStringLiteral("%1:%2,%3,%4,%5")
                                    .arg(sessionId)
                                    .arg(selection.x())
                                    .arg(selection.y())
                                    .arg(selection.width())
                                    .arg(selection.height());
    if (generation != m_qrGeneration || sessionId != m_context.captureState.sessionId ||
        key != expectedKey) {
        return;
    }
    updateToolbarBusy();
    if (!result.error.isEmpty()) {
        if (m_active && m_mode == Mode::Qr) {
            showStatus(result.error, true);
        }
        hideRecognitionMessage();
        return;
    }
    if (result.contents.isEmpty()) {
        if (m_active && m_mode == Mode::Qr) {
            showStatus(tr("No QR code was recognized"), false);
        }
        hideRecognitionMessage();
        return;
    }
    m_qrCache.insert(key, result.contents);
    if (m_active && m_mode == Mode::Qr && currentCacheKey() == key) {
        m_qrCacheKey = key;
        applyQrContents(result.contents);
    }
    if (!busy()) {
        hideRecognitionMessage();
    }
}

void ScreenshotOcrController::applyPresentation(
    std::shared_ptr<ScreenshotOcrPresentation> presentation) {
    m_presentation = std::move(presentation);
    if (m_recognitionWindow != nullptr) {
        m_recognitionWindow->setOcrPresentation(m_presentation);
    }
    applyOcrBackgroundToOverlays(m_presentation);
}

void ScreenshotOcrController::applyTableSession(
    std::shared_ptr<ScreenshotTableEditingSession> session) {
    if (session == nullptr || session->document.empty()) {
        return;
    }
    m_tableSession = std::move(session);
    if (m_recognitionWindow != nullptr) {
        m_recognitionWindow->setTableSession(m_tableSession);
        handleTableCommandStateChanged(m_recognitionWindow->tableCommandState());
    }
}

void ScreenshotOcrController::applyQrContents(const QStringList& contents) {
    m_qrContents = contents;
    if (m_recognitionWindow != nullptr) {
        m_recognitionWindow->showQrContents(m_qrContents);
    }
}

void ScreenshotOcrController::handleQrLinkActivated(const QUrl& url) {
    const QString scheme = url.scheme().toLower();
    if (!qrModeActive() || !url.isValid() || url.isRelative() || url.host().isEmpty() ||
        (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))) {
        return;
    }
    if (!QDesktopServices::openUrl(url)) {
        showStatus(tr("Unable to open the recognized link"), true);
        return;
    }
    // The link activation signal is emitted by the recognition window. Canceling
    // synchronously destroys that window while Qt is still dispatching the signal,
    // which can leave QTextBrowser's event handling with a dangling sender.
    QTimer::singleShot(0, this, [this]() {
        if (qrModeActive()) {
            m_context.cancelCapture();
        }
    });
}

void ScreenshotOcrController::deactivate() {
    if (m_session != nullptr) {
        m_session->deactivate();
    }
    clearTextEditingState();
    if (!m_active && m_canvasStates.isEmpty() && m_recognitionWindow == nullptr) {
        return;
    }
    m_active = false;
    m_qrContents.clear();
    clearOcrBackgroundFromOverlays();
    destroyRecognitionWindow();
    emit textEditingChanged(false);
    updateToolbarTextState();
    handleTableCommandStateChanged({});
    updateToolbarBusy();
    hideRecognitionMessage();
    m_context.displaySession.forEachOverlay([](qsizetype, ScreenshotOverlayWindow* overlay) {
        if (overlay != nullptr && overlay->canvas() != nullptr) {
            overlay->canvas()->unsetCursor();
        }
    });
    for (const CanvasState& state : std::as_const(m_canvasStates)) {
        if (state.overlay != nullptr) {
            state.overlay->setScreenshotSelectionBorderVisible(state.selectionBorderVisible);
            if (state.hadSelection) {
                state.overlay->setScreenshotSelection(m_context.selection.normalizedSelection(),
                                                      state.selectionHandlesVisible,
                                                      m_context.selection.cornerRadius());
            } else {
                state.overlay->clearScreenshotSelection();
            }
        }
        if (state.canvas != nullptr) {
            state.canvas->setCanvasContentVisible(state.contentVisible);
            state.canvas->setInteractionEnabled(state.interactionEnabled);
        }
    }
    m_canvasStates.clear();
}

void ScreenshotOcrController::invalidateSession() {
    if (m_session != nullptr) {
        m_session->invalidate();
    }
    deactivate();
    ++m_sessionGeneration;
    ++m_textGeneration;
    ++m_tableGeneration;
    ++m_qrGeneration;
    if (m_requestToken != 0) {
        m_context.recognition.cancel(m_requestToken);
        m_requestToken = 0;
    }
    if (m_tableRequestToken != 0 && m_context.tableRecognition != nullptr) {
        m_context.tableRecognition->cancel(m_tableRequestToken);
        m_tableRequestToken = 0;
    }
    if (m_qrRequestToken != 0) {
        m_context.qrRecognition.cancel(m_qrRequestToken);
        m_qrRequestToken = 0;
    }
    updateToolbarBusy();
    hideRecognitionMessage();
    m_textCache.clear();
    m_tableCache.clear();
    m_qrCache.clear();
    m_presentation.reset();
    m_tableSession.reset();
    m_textCacheKey.clear();
    m_tableCacheKey.clear();
    m_qrCacheKey.clear();
    m_qrContents.clear();
    m_editingKey.clear();
    m_surfaceKey.clear();
    m_surfaceImage = QImage();
    emit textResultChanged(false);
    updateToolbarTextState();
}

void ScreenshotOcrController::restorePreviousToolAfterFailure() {
    const ScreenshotActiveTool previousTool = m_previousTool;
    deactivate();
    if (previousTool == ScreenshotActiveTool::Move) {
        m_context.interaction.setMoveTool(m_context.selection.hasPixelSelection(), false);
    } else {
        m_context.interaction.setCanvasTool(previousTool);
    }
    if (ScreenshotToolbarWindow* toolbar = m_context.overlayCoordinator.toolbar()) {
        toolbar->setActiveTool(paletteTool(previousTool));
    }
    updateOverlays();
}

void ScreenshotOcrController::beginTextEditing() {
    if (m_session != nullptr) {
        m_session->beginTextEditing();
        return;
    }
    if (!m_active || m_mode != Mode::Text || !hasTextResult()) {
        return;
    }
    m_editing = true;
    m_editingKey = m_textCacheKey.isEmpty() ? currentCacheKey() : m_textCacheKey;
    auto cached = m_textCache.find(m_editingKey);
    if (cached != m_textCache.end()) {
        cached->editing = true;
    }
    if (m_recognitionWindow != nullptr) {
        m_recognitionWindow->clearOcrPresentation();
    }
    showTextEditor();
    emit textEditingChanged(true);
    updateToolbarTextState();
}

void ScreenshotOcrController::endTextEditing() {
    if (m_session != nullptr) {
        m_session->endTextEditing();
        return;
    }
    if (!m_editing || !hasTextResult()) {
        return;
    }
    auto cached = m_textCache.find(m_editingKey);
    if (cached != m_textCache.end()) {
        cached->editing = false;
    }
    m_editing = false;
    m_editingKey.clear();
    hideTextEditors();
    applyPresentation(m_presentation);
    emit textEditingChanged(false);
    updateToolbarTextState();
}

void ScreenshotOcrController::resetTextEditing() {
    if (m_session != nullptr) {
        m_session->resetTextEditing();
        return;
    }
    if (!m_editing || !hasTextResult()) {
        return;
    }
    setTextDraft(originalText());
}

void ScreenshotOcrController::applyRemoveLineBreaks() {
    if (m_session != nullptr) {
        m_session->applyRemoveLineBreaks();
        return;
    }
    beginTextEditing();
    if (m_editing) {
        setTextDraft(snow_shot::presentation::removeOcrLineBreaks(originalText()));
    }
}

void ScreenshotOcrController::applyHalfWidthPunctuation() {
    if (m_session != nullptr) {
        m_session->applyHalfWidthPunctuation();
        return;
    }
    beginTextEditing();
    if (m_editing) {
        setTextDraft(snow_shot::presentation::convertOcrPunctuation(originalText(), false));
    }
}

void ScreenshotOcrController::applyFullWidthPunctuation() {
    if (m_session != nullptr) {
        m_session->applyFullWidthPunctuation();
        return;
    }
    beginTextEditing();
    if (m_editing) {
        setTextDraft(snow_shot::presentation::convertOcrPunctuation(originalText(), true));
    }
}

bool ScreenshotOcrController::editing() const {
    return m_session != nullptr ? m_session->editing() : m_editing;
}

bool ScreenshotOcrController::hasTextResult() const {
    return m_session != nullptr ? m_session->hasTextResult()
                                : !m_textCacheKey.isEmpty() && m_textCache.contains(m_textCacheKey);
}

QString ScreenshotOcrController::textDraft() const {
    if (m_session != nullptr) {
        return m_session->textDraft();
    }
    const QString key = m_editingKey.isEmpty() ? m_textCacheKey : m_editingKey;
    const auto session = m_textCache.value(key).editingSession;
    return session != nullptr ? session->text() : QString{};
}

QString ScreenshotOcrController::originalText() const {
    if (m_session != nullptr) {
        return m_session->originalText();
    }
    const QString key = m_editingKey.isEmpty() ? m_textCacheKey : m_editingKey;
    const auto session = m_textCache.value(key).editingSession;
    return session != nullptr ? session->originalText() : QString{};
}

void ScreenshotOcrController::setTextDraft(const QString& text) {
    if (m_session != nullptr) {
        m_session->setTextDraft(text);
        return;
    }
    const QString key = m_editingKey.isEmpty() ? m_textCacheKey : m_editingKey;
    auto it = m_textCache.find(key);
    if (it == m_textCache.end() || it->editingSession == nullptr) {
        return;
    }
    static_cast<void>(it->editingSession->replaceText(text));
}

void ScreenshotOcrController::showTextEditor() {
    hideTextEditors();
    clearOcrBackgroundFromOverlays();
    const QString key = m_editingKey.isEmpty() ? m_textCacheKey : m_editingKey;
    const auto cached = m_textCache.constFind(key);
    if (cached == m_textCache.cend() || cached->editingSession == nullptr) {
        return;
    }
    m_textDocument = cached->editingSession->document();
    if (m_recognitionWindow != nullptr && m_textDocument != nullptr) {
        m_recognitionWindow->showTextEditor(m_textDocument.data());
    }
}

void ScreenshotOcrController::hideTextEditors() {
    if (m_recognitionWindow != nullptr) {
        m_recognitionWindow->hideTextEditor();
    }
    m_textDocument = nullptr;
}

void ScreenshotOcrController::clearTextEditingState() {
    for (auto it = m_textCache.begin(); it != m_textCache.end(); ++it) {
        it->editing = false;
    }
    m_editing = false;
    m_editingKey.clear();
    hideTextEditors();
}

void ScreenshotOcrController::handleTextDocumentChanged(const QString& key) {
    auto cached = m_textCache.find(key);
    if (cached == m_textCache.end() || cached->editingSession == nullptr) {
        return;
    }
    cached->editingSession->recordCurrentText();
    if (key == (m_editingKey.isEmpty() ? m_textCacheKey : m_editingKey)) {
        emit textDraftChanged(cached->editingSession->text());
    }
    updateToolbarTextState();
}

void ScreenshotOcrController::updateOverlays() const {
    m_context.displaySession.forEachOverlay([](qsizetype, ScreenshotOverlayWindow* overlay) {
        if (overlay != nullptr && overlay->canvas() != nullptr) {
            overlay->canvas()->update();
        }
    });
}

void ScreenshotOcrController::applyOcrBackgroundToOverlays(
    const std::shared_ptr<ScreenshotOcrPresentation>& presentation) const {
    m_context.displaySession.forEachOverlay(
        [&presentation](qsizetype, ScreenshotOverlayWindow* overlay) {
            if (overlay != nullptr) {
                overlay->setScreenshotOcrBackground(presentation);
            }
        });
}

void ScreenshotOcrController::clearOcrBackgroundFromOverlays() const {
    m_context.displaySession.forEachOverlay([](qsizetype, ScreenshotOverlayWindow* overlay) {
        if (overlay != nullptr) {
            overlay->clearScreenshotOcrBackground();
        }
    });
}

bool ScreenshotOcrController::ensureRecognitionWindow() {
    const QRect selection = m_context.selection.pixelSelection();
    const QString key = currentCacheKey();
    const QPointF center = QRectF(selection).center();
    const CapturedDisplayModel* display =
        m_context.geometry.displayForCanvasPoint(m_context.displaySession, center);
    if (display == nullptr) {
        display = m_context.geometry.displayForCanvasRect(m_context.displaySession,
                                                         QRectF(selection));
    }
    if (display == nullptr) {
        showStatus(tr("Unable to read the selected screenshot"), true);
        return false;
    }

    ScreenshotOverlayWindow* overlay = m_context.displaySession.overlayForDisplay(display);
    if (overlay == nullptr) {
        showStatus(tr("Unable to read the selected screenshot"), true);
        return false;
    }
    QScreen* screen = ScreenshotGeometryMapper::screenForCaptureDisplay(display->name,
                                                                        display->physicalRect);
    if (screen == nullptr) {
        screen = overlay->screen();
    }
    if (screen == nullptr) {
        showStatus(tr("Unable to read the selected screenshot"), true);
        return false;
    }

    // Keep every recognition result on top of the screenshot region that the
    // user selected. QR used to use a fixed, screen-centered window here,
    // which made its result appear unrelated to the selected screenshot.
    const QRect windowGeometry =
        recognitionGeometryForDisplay(m_context.geometry, *display, QRectF(selection));
    const ScreenshotRecognitionWindow::Config config{
        screen,
        overlay,
        windowGeometry,
        QRectF(selection),
    };
    if (m_recognitionWindow != nullptr && m_surfaceKey == key) {
        if (!m_recognitionWindow->present(config)) {
            showStatus(tr("Unable to read the selected screenshot"), true);
            return false;
        }
        if (ScreenshotToolbarWindow* toolbar = m_context.overlayCoordinator.toolbar()) {
            toolbar->raise();
        }
        return true;
    }

    destroyRecognitionWindow();
    QImage source = composeScreenshotSourceSelection(m_context.displaySession, selection);
    if (source.isNull()) {
        showStatus(tr("Unable to read the selected screenshot"), true);
        return false;
    }

    auto* window = new ScreenshotRecognitionWindow(ScreenshotRecognitionWindowActions{
        [this]() { m_context.cancelCapture(); },
        [this](const QString& text) { setTextDraft(text); },
        [this](const ScreenshotTableCommandState& state) {
            handleTableCommandStateChanged(state);
        },
        [this](const QString& message) { showStatus(message, false); },
        [this](const QUrl& url) { handleQrLinkActivated(url); },
        [this]() { undoTextEdit(); },
        [this]() { redoTextEdit(); },
    });
    if (!window->present(config)) {
        delete window;
        showStatus(tr("Unable to read the selected screenshot"), true);
        return false;
    }

    m_surfaceKey = key;
    m_surfaceImage = std::move(source);
    m_recognitionWindow = window;
    if (ScreenshotToolbarWindow* toolbar = m_context.overlayCoordinator.toolbar()) {
        toolbar->raise();
    }
    return true;
}

void ScreenshotOcrController::destroyRecognitionWindow() {
    if (m_recognitionWindow != nullptr) {
        delete m_recognitionWindow.data();
        m_recognitionWindow = nullptr;
    }
    m_surfaceKey.clear();
    m_surfaceImage = QImage();
}

void ScreenshotOcrController::showStatus(const QString& message, bool error) const {
    if (message.isEmpty()) {
        return;
    }
    QWidget* preferredOwner = m_recognitionWindow.data();
    if (error) {
        m_messages->error(QString::fromLatin1(kStatusMessageKey), message, {}, preferredOwner);
    } else {
        m_messages->warning(QString::fromLatin1(kStatusMessageKey), message, {}, preferredOwner);
    }
}

void ScreenshotOcrController::updateToolbarBusy() const {
    if (ScreenshotToolbarWindow* toolbar = m_context.overlayCoordinator.toolbar()) {
        toolbar->setOcrBusy(busy(Mode::Text));
        toolbar->setTableBusy(busy(Mode::Table));
        toolbar->setQrBusy(busy(Mode::Qr));
    }
}

void ScreenshotOcrController::updateToolbarTextState() const {
    if (ScreenshotToolbarWindow* toolbar = m_context.overlayCoordinator.toolbar()) {
        toolbar->setTextEditingState(hasTextResult() && m_active && m_mode == Mode::Text,
                                     m_editing,
                                     m_editing &&
                                         m_textCache.value(m_editingKey).editingSession != nullptr &&
                                         m_textCache.value(m_editingKey).editingSession->canUndo(),
                                     m_editing &&
                                         m_textCache.value(m_editingKey).editingSession != nullptr &&
                                         m_textCache.value(m_editingKey).editingSession->canRedo());
    }
}

void ScreenshotOcrController::handleTableCommandStateChanged(
    const ScreenshotTableCommandState& state) const {
    if (ScreenshotToolbarWindow* toolbar = m_context.overlayCoordinator.toolbar()) {
        const bool available = m_active && m_mode == Mode::Table && m_tableSession != nullptr;
        toolbar->setTableEditingState(available, available && state.canUndo,
                                      available && state.canRedo, available && state.canMerge,
                                      available && state.canSplit, available && state.canReset);
    }
}

void ScreenshotOcrController::showRecognitionMessage() {
    QWidget* preferredOwner = m_recognitionWindow.data();
    const QString message =
        m_mode == Mode::Table ? tr("Recognizing table")
        : m_mode == Mode::Qr  ? tr("Recognizing QR code")
                              : tr("Recognizing text");
    m_messages->loading(QString::fromLatin1(kRecognitionMessageKey), message, {}, preferredOwner);
}

void ScreenshotOcrController::hideRecognitionMessage() {
    if (busy()) {
        return;
    }
    m_messages->destroy(QString::fromLatin1(kRecognitionMessageKey));
}
