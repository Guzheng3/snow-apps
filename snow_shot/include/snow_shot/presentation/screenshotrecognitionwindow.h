#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTRECOGNITIONWINDOW_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTRECOGNITIONWINDOW_H

#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QStringList>
#include <QTransform>
#include <QWidget>

#include <functional>
#include <memory>

class QKeyEvent;
class QFocusEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QScreen;
class QStackedLayout;
class QTextDocument;
class QTextBrowser;
class QTextEdit;
class QUrl;
class ScreenshotOcrPresentation;
class ScreenshotOcrTextLayer;
class ScreenshotTableEditingSession;
class ScreenshotTableEditor;
struct ScreenshotTableCommandState;
namespace adqt::widgets {
class AdSpin;
}

struct ScreenshotRecognitionWindowActions {
    std::function<void()> handleCancel = []() {};
    std::function<void(const QString&)> handleTextEdited = [](const QString&) {};
    std::function<void(const ScreenshotTableCommandState&)> handleTableCommandStateChanged;
    std::function<void(const QString&)> handleTableOperationRejected = [](const QString&) {};
    std::function<void(const QUrl&)> handleLinkActivated = [](const QUrl&) {};
    std::function<void()> handleUndoTextEdit = []() {};
    std::function<void()> handleRedoTextEdit = []() {};
};

class ScreenshotRecognitionWindow final : public QWidget {
    Q_OBJECT

  public:
    enum class PresentationMode {
        TopLevelWindow,
        EmbeddedChild,
    };

    struct Config {
        QScreen* screen = nullptr;
        QWidget* transientOwner = nullptr;
        QRect geometry;
        QRectF canvasSelection;
        PresentationMode presentationMode = PresentationMode::TopLevelWindow;
    };

    explicit ScreenshotRecognitionWindow(
        ScreenshotRecognitionWindowActions actions,
        QWidget* parent = nullptr,
        PresentationMode presentationMode = PresentationMode::TopLevelWindow);
    ~ScreenshotRecognitionWindow() override;

    [[nodiscard]] bool present(const Config& config);
    [[nodiscard]] PresentationMode presentationMode() const;

    void setOcrPresentation(std::shared_ptr<ScreenshotOcrPresentation> presentation);
    void clearOcrPresentation();
    void showFormattedText(std::shared_ptr<QTextDocument> document);
    void clearFormattedText();

    void setTableSession(std::shared_ptr<ScreenshotTableEditingSession> session);
    void clearTableSession();
    [[nodiscard]] ScreenshotTableCommandState tableCommandState() const;
    void mergeTableSelection();
    void splitTableSelection();
    void resetTable();
    void undoTableEdit();
    void redoTableEdit();
    void commitActiveTableEdit();

    void showTextEditor(QTextDocument* document, bool readOnly = false,
                        bool streaming = false);
    void setTextEditorStreaming(bool streaming);
    void hideTextEditor();

    void showQrContents(const QStringList& contents);
    void clearQrContents();

  protected:
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    [[nodiscard]] QPointF canvasPositionForLocalPoint(const QPointF& localPosition) const;
    [[nodiscard]] QTransform canvasToLocalTransform() const;
    bool handleRecognitionKeyPress(QKeyEvent* event);
    void synchronizeTextLayer();
    void updateTextEditorSpinGeometry();

    ScreenshotRecognitionWindowActions m_actions;
    std::shared_ptr<ScreenshotOcrPresentation> m_ocrPresentation;
    QStackedLayout* m_stack = nullptr;
    ScreenshotOcrTextLayer* m_textLayer = nullptr;
    QWidget* m_textEditorContainer = nullptr;
    QTextEdit* m_textEditor = nullptr;
    adqt::widgets::AdSpin* m_textEditorSpin = nullptr;
    QTextBrowser* m_qrBrowser = nullptr;
    QTextBrowser* m_formattedTextBrowser = nullptr;
    std::shared_ptr<QTextDocument> m_formattedTextDocument;
    ScreenshotTableEditor* m_tableEditor = nullptr;
    QRectF m_canvasSelection;
    PresentationMode m_presentationMode = PresentationMode::TopLevelWindow;
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTRECOGNITIONWINDOW_H
