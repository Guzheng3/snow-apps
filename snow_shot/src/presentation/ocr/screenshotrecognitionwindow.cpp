#include "snow_shot/presentation/screenshotrecognitionwindow.h"

#include "snow_shot/presentation/screenshotocrpresentation.h"
#include "snow_shot/presentation/screenshotocrtextlayer.h"
#include "snow_shot/presentation/screenshottableeditor.h"
#include "theme/theme_manager.h"
#include "widgets/input_text_edit.h"
#include "widgets/scroll_area.h"
#include "widgets/spin.h"

#include <QApplication>
#include <QClipboard>
#include <QFocusEvent>
#include <QFrame>
#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>
#include <QScreen>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStackedLayout>
#include <QTextBrowser>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QWindow>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
QMargins adTextAreaContentMargins(const adqt::theme::ThemeMapToken& theme) {
    const int borderInset = std::max(1, qRound(theme.lineWidth));
    const int horizontalPadding = std::max(8, qRound(theme.sizeSM - theme.lineWidth));
    const double baseVerticalPadding =
        (theme.controlHeight - theme.fontSize * theme.lineHeight) / 2.0;
    const double roundedVerticalPadding = std::round(baseVerticalPadding * 10.0) / 10.0;
    const int verticalPadding =
        std::max(0, qRound(roundedVerticalPadding - theme.lineWidth));
    return QMargins(borderInset + horizontalPadding, borderInset + verticalPadding,
                    borderInset + horizontalPadding, borderInset + verticalPadding);
}

void applyTextEditorContainerBackground(QWidget* container) {
    if (container == nullptr) {
        return;
    }
    const adqt::theme::ThemeMapToken theme =
        adqt::theme::ThemeManager::instance().resolveTheme(container);
    QPalette palette = container->palette();
    palette.setColor(QPalette::Window, theme.colorBgContainer);
    palette.setColor(QPalette::Base, theme.colorBgContainer);
    container->setPalette(palette);
    container->setAutoFillBackground(true);
}

bool isHttpUrl(const QString& text, QUrl* result = nullptr) {
    const QUrl url(text, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    const bool valid = url.isValid() && !url.isRelative() && !url.host().isEmpty() &&
                       (scheme == QStringLiteral("http") ||
                        scheme == QStringLiteral("https"));
    if (valid && result != nullptr) {
        *result = url;
    }
    return valid;
}
}  // namespace

class ScreenshotFormattedTextLayer final : public QGraphicsView {
  public:
    explicit ScreenshotFormattedTextLayer(QWidget* parent = nullptr)
        : QGraphicsView(parent), m_scene(new QGraphicsScene(this)),
          m_textItem(new QGraphicsTextItem) {
        setObjectName(QStringLiteral("screenshotClipboardText"));
        setScene(m_scene);
        m_scene->addItem(m_textItem);
        setFrameShape(QFrame::NoFrame);
        setContentsMargins(0, 0, 0, 0);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setAlignment(Qt::AlignLeft | Qt::AlignTop);
        const QColor background = QGuiApplication::palette().color(QPalette::Base);
        QPalette layerPalette = palette();
        layerPalette.setColor(QPalette::Base, background);
        layerPalette.setColor(QPalette::Window, background);
        setPalette(layerPalette);
        viewport()->setPalette(layerPalette);
        viewport()->setAutoFillBackground(true);
        setBackgroundBrush(background);
        setFocusPolicy(Qt::StrongFocus);
        setInteractive(true);
        setOptimizationFlag(QGraphicsView::DontSavePainterState, true);
        setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing |
                       QPainter::SmoothPixmapTransform);
        setStyleSheet(
            QStringLiteral("QGraphicsView#screenshotClipboardText { border: none; }"));

        m_textItem->setObjectName(QStringLiteral("screenshotClipboardTextItem"));
        m_textItem->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                            Qt::TextSelectableByKeyboard);
        m_textItem->setOpenExternalLinks(false);
        m_textItem->setFlag(QGraphicsItem::ItemIsFocusable, true);
        m_textItem->setAcceptedMouseButtons(Qt::LeftButton);
        m_textItem->hide();
        hide();
    }

    ~ScreenshotFormattedTextLayer() override {
        clearDocument();
    }

    void setDocument(std::shared_ptr<QTextDocument> document, const QRectF& canvasRect,
                     qreal devicePixelRatio) {
        clearDocument();
        if (document == nullptr || !canvasRect.isValid() || canvasRect.isEmpty() ||
            !std::isfinite(devicePixelRatio) || devicePixelRatio <= 0.0) {
            return;
        }
        m_document = std::move(document);
        m_canvasRect = canvasRect.normalized();
        m_textItem->setDocument(m_document.get());
        m_textItem->setPos(m_canvasRect.topLeft());
        m_textItem->setScale(devicePixelRatio);
        m_textItem->show();
    }

    void clearDocument() {
        if (m_textItem != nullptr) {
            m_textItem->clearFocus();
            m_textItem->hide();
            m_textItem->setScale(1.0);
            m_textItem->setDocument(nullptr);
        }
        m_document.reset();
        m_canvasRect = {};
        hide();
    }

    void synchronize(const QTransform& canvasToViewTransform, const QRect& viewportRect) {
        if (m_document == nullptr || m_canvasRect.isEmpty() || viewportRect.isEmpty()) {
            hide();
            return;
        }
        if (geometry() != viewportRect) {
            setGeometry(viewportRect);
        }
        setSceneRect(m_canvasRect);
        setTransform(canvasToViewTransform, false);
        show();
        raise();
        viewport()->update();
    }

    void focusText() {
        setFocus(Qt::OtherFocusReason);
        m_textItem->setFocus(Qt::OtherFocusReason);
    }

  private:
    QGraphicsScene* m_scene = nullptr;
    QGraphicsTextItem* m_textItem = nullptr;
    std::shared_ptr<QTextDocument> m_document;
    QRectF m_canvasRect;
};

ScreenshotRecognitionWindow::ScreenshotRecognitionWindow(
    ScreenshotRecognitionWindowActions actions, QWidget* parent,
    PresentationMode presentationMode)
    : QWidget(parent,
               presentationMode == PresentationMode::EmbeddedChild
                   ? Qt::Widget
                   : Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint),
      m_actions(std::move(actions)),
      m_stack(new QStackedLayout(this)),
      m_textLayer(new ScreenshotOcrTextLayer(this)),
      m_presentationMode(presentationMode) {
    setObjectName(QStringLiteral("screenshotRecognitionWindow"));
    if (m_presentationMode == PresentationMode::TopLevelWindow) {
        setAttribute(Qt::WA_TranslucentBackground, true);
    }
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    m_stack->setContentsMargins(0, 0, 0, 0);
    // This window is an exact overlay for the screenshot selection. Child pages such as
    // QGraphicsView and AdTextEdit have useful standalone minimum size hints, but those hints
    // must never enlarge the overlay and desynchronize it from the selected pixels.
    m_stack->setSizeConstraint(QLayout::SetNoConstraint);
    m_stack->setStackingMode(QStackedLayout::StackOne);
    m_stack->addWidget(m_textLayer);
    m_textLayer->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_textLayer->viewport()->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    auto* cancelShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    cancelShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(cancelShortcut, &QShortcut::activated, this, [this]() {
        if (m_tableEditor != nullptr && m_tableEditor->cancelActiveEdit()) {
            return;
        }
        m_actions.handleCancel();
    });
}

ScreenshotRecognitionWindow::~ScreenshotRecognitionWindow() {
    clearFormattedText();
    if (m_textEditor != nullptr) {
        const QSignalBlocker blocker(m_textEditor);
        m_textEditor->setDocument(nullptr);
    }
    if (m_tableEditor != nullptr) {
        m_tableEditor->clearSession();
    }
}

bool ScreenshotRecognitionWindow::present(const Config& config) {
    if (config.screen == nullptr || !config.geometry.isValid() || config.geometry.isEmpty() ||
        !config.canvasSelection.isValid() ||
        config.canvasSelection.isEmpty() ||
        !std::isfinite(config.formattedTextDevicePixelRatio) ||
        config.formattedTextDevicePixelRatio <= 0.0) {
        return false;
    }

    m_canvasSelection = config.canvasSelection.normalized();
    m_formattedTextDevicePixelRatio = config.formattedTextDevicePixelRatio;
    m_presentationMode = config.presentationMode;
    if (m_presentationMode == PresentationMode::TopLevelWindow) {
        static_cast<void>(winId());
        if (QWindow* handle = windowHandle()) {
            handle->setScreen(config.screen);
            if (config.transientOwner != nullptr) {
                static_cast<void>(config.transientOwner->winId());
                handle->setTransientParent(config.transientOwner->windowHandle());
            }
        }
    }
    setGeometry(config.geometry);
    show();
    if (m_presentationMode == PresentationMode::TopLevelWindow) {
        raise();
        activateWindow();
    }
    setFocus(Qt::OtherFocusReason);
    synchronizeTextLayer();
    return true;
}

ScreenshotRecognitionWindow::PresentationMode ScreenshotRecognitionWindow::presentationMode()
    const {
    return m_presentationMode;
}

void ScreenshotRecognitionWindow::setOcrPresentation(
    std::shared_ptr<ScreenshotOcrPresentation> presentation) {
    hideTextEditor();
    clearFormattedText();
    clearTableSession();
    clearQrContents();
    m_ocrPresentation = std::move(presentation);
    m_textLayer->setPresentation(m_ocrPresentation);
    m_stack->setCurrentWidget(m_textLayer);
    synchronizeTextLayer();
    setFocus(Qt::OtherFocusReason);
}

void ScreenshotRecognitionWindow::clearOcrPresentation() {
    m_ocrPresentation.reset();
    m_textLayer->clearPresentation();
    unsetCursor();
}

void ScreenshotRecognitionWindow::showFormattedText(std::shared_ptr<QTextDocument> document) {
    if (document == nullptr) {
        return;
    }
    hideTextEditor();
    clearOcrPresentation();
    clearTableSession();
    clearQrContents();
    if (m_formattedTextLayer == nullptr) {
        m_formattedTextLayer = new ScreenshotFormattedTextLayer(this);
        m_stack->addWidget(m_formattedTextLayer);
    }
    m_formattedTextLayer->setDocument(std::move(document), m_canvasSelection,
                                      m_formattedTextDevicePixelRatio);
    m_stack->setCurrentWidget(m_formattedTextLayer);
    synchronizeTextLayer();
    m_formattedTextLayer->focusText();
}

void ScreenshotRecognitionWindow::clearFormattedText() {
    if (m_formattedTextLayer == nullptr) {
        return;
    }
    m_formattedTextLayer->clearDocument();
    m_stack->removeWidget(m_formattedTextLayer);
    delete m_formattedTextLayer;
    m_formattedTextLayer = nullptr;
    m_stack->setCurrentWidget(m_textLayer);
}

void ScreenshotRecognitionWindow::setTableSession(
    std::shared_ptr<ScreenshotTableEditingSession> session) {
    hideTextEditor();
    clearFormattedText();
    clearOcrPresentation();
    clearQrContents();
    if (m_tableEditor == nullptr) {
        m_tableEditor = new ScreenshotTableEditor(this);
        m_stack->addWidget(m_tableEditor);
        connect(m_tableEditor, &ScreenshotTableEditor::commandStateChanged, this,
                [this](const ScreenshotTableCommandState& state) {
                    if (m_actions.handleTableCommandStateChanged) {
                        m_actions.handleTableCommandStateChanged(state);
                    }
                });
        connect(m_tableEditor, &ScreenshotTableEditor::operationRejected, this,
                [this](const QString& message) {
                    m_actions.handleTableOperationRejected(message);
                });
    }
    m_tableEditor->setSession(std::move(session));
    m_stack->setCurrentWidget(m_tableEditor);
    m_tableEditor->setFocus(Qt::OtherFocusReason);
}

void ScreenshotRecognitionWindow::clearTableSession() {
    if (m_tableEditor == nullptr) {
        return;
    }
    m_tableEditor->clearSession();
    m_stack->removeWidget(m_tableEditor);
    delete m_tableEditor;
    m_tableEditor = nullptr;
    m_stack->setCurrentWidget(m_textLayer);
}

ScreenshotTableCommandState ScreenshotRecognitionWindow::tableCommandState() const {
    return m_tableEditor != nullptr ? m_tableEditor->commandState()
                                    : ScreenshotTableCommandState{};
}

void ScreenshotRecognitionWindow::mergeTableSelection() {
    if (m_tableEditor != nullptr) {
        m_tableEditor->mergeSelection();
    }
}

void ScreenshotRecognitionWindow::splitTableSelection() {
    if (m_tableEditor != nullptr) {
        m_tableEditor->splitSelection();
    }
}

void ScreenshotRecognitionWindow::resetTable() {
    if (m_tableEditor != nullptr) {
        m_tableEditor->resetDocument();
    }
}

void ScreenshotRecognitionWindow::undoTableEdit() {
    if (m_tableEditor != nullptr) {
        m_tableEditor->undoEdit();
    }
}

void ScreenshotRecognitionWindow::redoTableEdit() {
    if (m_tableEditor != nullptr) {
        m_tableEditor->redoEdit();
    }
}

void ScreenshotRecognitionWindow::commitActiveTableEdit() {
    if (m_tableEditor != nullptr) {
        static_cast<void>(m_tableEditor->commitActiveEdit());
    }
}

void ScreenshotRecognitionWindow::showTextEditor(QTextDocument* document, bool readOnly,
                                                 bool streaming) {
    if (document == nullptr) {
        return;
    }
    clearOcrPresentation();
    clearFormattedText();
    clearTableSession();
    clearQrContents();
    if (m_textEditor == nullptr) {
        m_textEditorContainer = new QWidget(this);
        m_textEditorContainer->setObjectName(QStringLiteral("screenshotOcrEditorContainer"));
        m_textEditorContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        auto* editorLayout = new QVBoxLayout(m_textEditorContainer);
        editorLayout->setContentsMargins(0, 0, 0, 0);
        auto* editor = new adqt::widgets::AdTextEdit(m_textEditorContainer);
        m_textEditor = editor;
        m_textEditor->setObjectName(QStringLiteral("screenshotOcrEditor"));
        editor->setHeightMode(adqt::widgets::AdTextEdit::HeightMode::FixedGeometry);
        editor->setVariant(adqt::widgets::AdTextEdit::Variant::Borderless);
        m_textEditor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_textEditor->setAcceptRichText(false);
        m_textEditor->installEventFilter(this);
        editorLayout->addWidget(editor);
        m_textEditorSpin = new adqt::widgets::AdSpin(m_textEditorContainer);
        m_textEditorSpin->setObjectName(QStringLiteral("screenshotOcrTranslationSpin"));
        m_textEditorSpin->setSizeClass(adqt::widgets::AdSpin::SizeClass::Small);
        m_textEditorSpin->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        m_textEditorSpin->hide();
        applyTextEditorContainerBackground(m_textEditorContainer);
        connect(&adqt::theme::ThemeManager::instance(), &adqt::theme::ThemeManager::themeChanged,
                m_textEditorContainer, [container = m_textEditorContainer]() {
                    applyTextEditorContainerBackground(container);
                });
        m_stack->addWidget(m_textEditorContainer);
        connect(editor, &adqt::widgets::AdTextEdit::textEdited, this,
                [this](const QString& text) { m_actions.handleTextEdited(text); });
    }
    const QSignalBlocker blocker(m_textEditor);
    m_textEditor->setDocument(document);
    m_textEditor->setReadOnly(readOnly);
    const adqt::theme::ThemeMapToken theme =
        adqt::theme::ThemeManager::instance().resolveTheme(m_textEditor);
    QFont editorFont = theme.appFont.family().isEmpty() ? m_textEditor->font() : theme.appFont;
    editorFont.setPixelSize(qRound(theme.fontSize));
    m_textEditor->setFont(editorFont);
    document->setDefaultFont(editorFont);
    m_stack->setCurrentWidget(m_textEditorContainer);
    setTextEditorStreaming(streaming);
    if (!readOnly) {
        static_cast<adqt::widgets::AdTextEdit*>(m_textEditor)->focusEditor();
    }
}

void ScreenshotRecognitionWindow::setTextEditorStreaming(bool streaming) {
    if (m_textEditor != nullptr) {
        m_textEditor->setReadOnly(streaming);
    }
    if (m_textEditorSpin != nullptr) {
        m_textEditorSpin->setSpinning(streaming);
        m_textEditorSpin->setVisible(streaming);
        if (streaming) {
            updateTextEditorSpinGeometry();
            m_textEditorSpin->raise();
        }
    }
}

void ScreenshotRecognitionWindow::hideTextEditor() {
    if (m_textEditor == nullptr) {
        return;
    }
    // Detaching the document can emit textChanged with an empty editor. This is
    // teardown, not a user edit, so do not forward it to the OCR draft cache.
    {
        const QSignalBlocker blocker(m_textEditor);
        m_textEditor->setDocument(nullptr);
    }
    m_stack->removeWidget(m_textEditorContainer);
    delete m_textEditorContainer;
    m_textEditorContainer = nullptr;
    m_textEditor = nullptr;
    m_textEditorSpin = nullptr;
    m_stack->setCurrentWidget(m_textLayer);
}

void ScreenshotRecognitionWindow::showQrContents(const QStringList& contents) {
    hideTextEditor();
    clearFormattedText();
    clearOcrPresentation();
    clearTableSession();
    if (m_qrBrowser == nullptr) {
        m_qrBrowser = new QTextBrowser(this);
        m_qrBrowser->setObjectName(QStringLiteral("screenshotQrContents"));
        m_qrBrowser->setFrameStyle(QFrame::NoFrame);
        m_qrBrowser->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_qrBrowser->setContentsMargins(0, 0, 0, 0);
        m_qrBrowser->setLineWrapMode(QTextEdit::WidgetWidth);
        m_qrBrowser->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_qrBrowser->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        m_qrBrowser->setVerticalScrollBar(
            new adqt::widgets::AdScrollBar(Qt::Vertical, m_qrBrowser));
        m_qrBrowser->setOpenLinks(false);
        m_qrBrowser->setOpenExternalLinks(false);
        m_qrBrowser->setTextInteractionFlags(Qt::TextBrowserInteraction);
        m_stack->addWidget(m_qrBrowser);
        connect(m_qrBrowser, &QTextBrowser::anchorClicked, this,
                [this](const QUrl& url) { m_actions.handleLinkActivated(url); });
    }

    const adqt::theme::ThemeMapToken theme =
        adqt::theme::ThemeManager::instance().resolveTheme(m_qrBrowser);
    const QMargins contentMargins = adTextAreaContentMargins(theme);
    m_qrBrowser->setStyleSheet(
        QStringLiteral("QTextBrowser#screenshotQrContents { border: none; border-radius: 0; "
                       "padding: %1px %2px; background-color: palette(base); }")
            .arg(contentMargins.top())
            .arg(contentMargins.left()));

    QTextDocument* document = m_qrBrowser->document();
    document->clear();
    document->setDocumentMargin(0.0);
    QFont browserFont = theme.appFont.family().isEmpty() ? m_qrBrowser->font() : theme.appFont;
    browserFont.setPixelSize(qRound(theme.fontSize));
    m_qrBrowser->setFont(browserFont);
    document->setDefaultFont(browserFont);

    QTextCursor cursor(document);
    QTextCharFormat plainFormat;
    plainFormat.setFont(browserFont);
    QTextCharFormat linkFormat = plainFormat;
    linkFormat.setAnchor(true);
    linkFormat.setFontUnderline(true);
    linkFormat.setForeground(theme.colorLink);

    for (qsizetype index = 0; index < contents.size(); ++index) {
        if (index > 0) {
            cursor.insertBlock();
        }
        const QString content = contents.at(index);
        const QString trimmed = content.trimmed();
        QUrl url;
        if (!trimmed.isEmpty() && isHttpUrl(trimmed, &url)) {
            const qsizetype start = content.indexOf(trimmed);
            cursor.insertText(content.left(start), plainFormat);
            linkFormat.setAnchorHref(url.toString(QUrl::FullyEncoded));
            cursor.insertText(trimmed, linkFormat);
            cursor.insertText(content.mid(start + trimmed.size()), plainFormat);
        } else {
            cursor.insertText(content, plainFormat);
        }
    }
    cursor.movePosition(QTextCursor::Start);
    m_qrBrowser->setTextCursor(cursor);
    m_stack->setCurrentWidget(m_qrBrowser);
    m_qrBrowser->setFocus(Qt::OtherFocusReason);
}

void ScreenshotRecognitionWindow::clearQrContents() {
    if (m_qrBrowser == nullptr) {
        return;
    }
    m_stack->removeWidget(m_qrBrowser);
    delete m_qrBrowser;
    m_qrBrowser = nullptr;
    m_stack->setCurrentWidget(m_textLayer);
}

void ScreenshotRecognitionWindow::focusOutEvent(QFocusEvent* event) {
    if (m_ocrPresentation != nullptr) {
        const quint64 previousRevision = m_ocrPresentation->selectionRevision();
        m_ocrPresentation->clearTextSelection();
        if (m_ocrPresentation->selectionRevision() != previousRevision) {
            m_textLayer->updateSelection();
        }
    }
    QWidget::focusOutEvent(event);
}

void ScreenshotRecognitionWindow::keyPressEvent(QKeyEvent* event) {
    if (handleRecognitionKeyPress(event)) {
        return;
    }
    QWidget::keyPressEvent(event);
}

void ScreenshotRecognitionWindow::mousePressEvent(QMouseEvent* event) {
    if (event != nullptr && event->button() == Qt::LeftButton && m_ocrPresentation != nullptr) {
        const quint64 previousRevision = m_ocrPresentation->selectionRevision();
        m_ocrPresentation->beginTextSelection(
            m_textLayer->textPositionAt(canvasPositionForLocalPoint(event->position()), false));
        if (m_ocrPresentation->selectionRevision() != previousRevision) {
            m_textLayer->updateSelection();
        }
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ScreenshotRecognitionWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event != nullptr && m_ocrPresentation != nullptr) {
        const QPointF canvasPosition = canvasPositionForLocalPoint(event->position());
        const ScreenshotOcrTextPosition exactPosition =
            m_textLayer->textPositionAt(canvasPosition, false);
        if (m_ocrPresentation->textSelectionActive()) {
            const quint64 previousRevision = m_ocrPresentation->selectionRevision();
            m_ocrPresentation->updateTextSelection(
                exactPosition.valid() ? exactPosition
                                      : m_textLayer->textPositionAt(canvasPosition, true));
            if (m_ocrPresentation->selectionRevision() != previousRevision) {
                m_textLayer->updateSelection();
            }
        }
        setCursor(exactPosition.valid() ? Qt::IBeamCursor : Qt::ArrowCursor);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void ScreenshotRecognitionWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event != nullptr && event->button() == Qt::LeftButton && m_ocrPresentation != nullptr) {
        const quint64 previousRevision = m_ocrPresentation->selectionRevision();
        m_ocrPresentation->updateTextSelection(m_textLayer->textPositionAt(
            canvasPositionForLocalPoint(event->position()), true));
        m_ocrPresentation->finishTextSelection();
        if (m_ocrPresentation->selectionRevision() != previousRevision) {
            m_textLayer->updateSelection();
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ScreenshotRecognitionWindow::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    if (m_presentationMode == PresentationMode::EmbeddedChild) {
        return;
    }
    // Windows excludes fully zero-alpha layered pixels from mouse hit testing.
    QPainter painter(this);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), QColor(0, 0, 0, 2));
}

void ScreenshotRecognitionWindow::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    synchronizeTextLayer();
    updateTextEditorSpinGeometry();
}

bool ScreenshotRecognitionWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_textEditor && event != nullptr && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->matches(QKeySequence::Undo)) {
            m_actions.handleUndoTextEdit();
            return true;
        }
        if (keyEvent->matches(QKeySequence::Redo)) {
            m_actions.handleRedoTextEdit();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

QPointF ScreenshotRecognitionWindow::canvasPositionForLocalPoint(
    const QPointF& localPosition) const {
    bool invertible = false;
    const QTransform localToCanvas = canvasToLocalTransform().inverted(&invertible);
    return invertible ? localToCanvas.map(localPosition) : m_canvasSelection.topLeft();
}

QTransform ScreenshotRecognitionWindow::canvasToLocalTransform() const {
    const QRectF localRect(QPointF(0.0, 0.0), QSizeF(size()));
    if (!m_canvasSelection.isValid() || m_canvasSelection.isEmpty() || localRect.isEmpty()) {
        return {};
    }
    const QPolygonF canvasQuad({
        m_canvasSelection.topLeft(),
        m_canvasSelection.topRight(),
        m_canvasSelection.bottomRight(),
        m_canvasSelection.bottomLeft(),
    });
    const QPolygonF localQuad({
        localRect.topLeft(),
        localRect.topRight(),
        localRect.bottomRight(),
        localRect.bottomLeft(),
    });
    QTransform transform;
    return QTransform::quadToQuad(canvasQuad, localQuad, transform) ? transform : QTransform();
}

bool ScreenshotRecognitionWindow::handleRecognitionKeyPress(QKeyEvent* event) {
    if (event == nullptr || m_ocrPresentation == nullptr) {
        return false;
    }
    const bool command = event->modifiers().testFlag(Qt::ControlModifier) ||
                         event->modifiers().testFlag(Qt::MetaModifier);
    if (command && event->key() == Qt::Key_A) {
        const quint64 previousRevision = m_ocrPresentation->selectionRevision();
        m_ocrPresentation->selectAll();
        if (m_ocrPresentation->selectionRevision() != previousRevision) {
            m_textLayer->updateSelection();
        }
        event->accept();
        return true;
    }
    if (command && event->key() == Qt::Key_C) {
        QString text;
        if (m_ocrPresentation->hasTextSelection()) {
            text = m_ocrPresentation->selectedText();
        } else {
            QStringList lines;
            lines.reserve(m_ocrPresentation->lines.size());
            for (const ScreenshotOcrLine& line : m_ocrPresentation->lines) {
                lines.push_back(line.text);
            }
            text = lines.join(QLatin1Char('\n'));
        }
        if (!text.isEmpty() && QApplication::clipboard() != nullptr) {
            QApplication::clipboard()->setText(text);
        }
        event->accept();
        return true;
    }
    return false;
}

void ScreenshotRecognitionWindow::synchronizeTextLayer() {
    if (m_textLayer != nullptr) {
        m_textLayer->synchronize(canvasToLocalTransform(), rect());
    }
    if (m_formattedTextLayer != nullptr) {
        m_formattedTextLayer->synchronize(canvasToLocalTransform(), rect());
    }
}

void ScreenshotRecognitionWindow::updateTextEditorSpinGeometry() {
    if (m_textEditorContainer == nullptr || m_textEditorSpin == nullptr) {
        return;
    }
    const QSize spinSize = m_textEditorSpin->sizeHint().expandedTo(QSize(20, 20));
    constexpr int margin = 12;
    m_textEditorSpin->setGeometry(m_textEditorContainer->width() - spinSize.width() - margin,
                                  m_textEditorContainer->height() - spinSize.height() - margin,
                                  spinSize.width(), spinSize.height());
}
