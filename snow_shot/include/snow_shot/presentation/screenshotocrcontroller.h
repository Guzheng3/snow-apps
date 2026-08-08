#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTOCRCONTROLLER_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTOCRCONTROLLER_H

#include "snow_shot/presentation/screenshotinteractionstate.h"
#include "snow_shot/presentation/screenshotmessageservice.h"
#include "snow_shot/presentation/screenshotocrrecognitionservice.h"
#include "snow_shot/presentation/screenshotqrrecognitionservice.h"
#include "snow_shot/network/snowshotapiclient.h"

#include <QObject>
#include <QPointer>
#include <QHash>
#include <QImage>
#include <QRect>
#include <QVector>

#include <functional>
#include <memory>

class ScreenshotDisplaySession;
class ScreenshotGeometryMapper;
class ScreenshotOcrPresentation;
class ScreenshotOcrTextEditingSession;
class ScreenshotRecognitionWindow;
class ScreenshotRecognitionSessionController;
class ScreenshotTableEditingSession;
class ScreenshotOverlayCoordinator;
class ScreenshotOverlayWindow;
class ScreenshotSelectionModel;
class SnowCanvasRuntime;
class SnowCanvasWidget;
class QTextDocument;
class QWidget;
class QUrl;
struct ScreenshotCaptureState;
struct ScreenshotTableCommandState;

struct ScreenshotOcrControllerContext {
    ScreenshotCaptureState& captureState;
    ScreenshotInteractionState& interaction;
    ScreenshotSelectionModel& selection;
    ScreenshotDisplaySession& displaySession;
    ScreenshotGeometryMapper& geometry;
    ScreenshotOverlayCoordinator& overlayCoordinator;
    ScreenshotOcrRecognitionPort& recognition;
    ScreenshotQrRecognitionPort& qrRecognition;
    SnowShotApiClient* tableRecognition = nullptr;
    std::function<void()> hideColorPicker = []() {};
    std::function<void()> cancelCapture = []() {};
};

class ScreenshotOcrController final : public QObject {
    Q_OBJECT

  public:
    enum class Mode { Text, Table, Qr };

    explicit ScreenshotOcrController(ScreenshotOcrControllerContext context,
                                     QObject* parent = nullptr);
    ~ScreenshotOcrController() override;

    void activate();
    void activateTable();
    void activateQr();
    // Leaves the visible recognition tool but deliberately keeps requests and cache entries alive.
    void deactivate();
    // Invalidates the capture session and cancels all recognition work.
    void invalidateSession();
    [[nodiscard]] bool active() const;
    [[nodiscard]] bool busy() const;
    [[nodiscard]] bool busy(Mode mode) const;
    [[nodiscard]] Mode mode() const;
    [[nodiscard]] bool tableModeActive() const;
    [[nodiscard]] bool qrModeActive() const;
    void setMode(Mode mode);
    [[nodiscard]] bool copyRecognitionToClipboard();
    void mergeTableSelection();
    void splitTableSelection();
    void resetTable();
    void undoTableEdit();
    void redoTableEdit();
    [[nodiscard]] ScreenshotTableCommandState tableCommandState() const;
    void undoTextEdit();
    void redoTextEdit();

    void beginTextEditing();
    void endTextEditing();
    void resetTextEditing();
    void applyRemoveLineBreaks();
    void applyHalfWidthPunctuation();
    void applyFullWidthPunctuation();
    [[nodiscard]] bool editing() const;
    [[nodiscard]] bool hasTextResult() const;
    [[nodiscard]] QString textDraft() const;
    [[nodiscard]] QString originalText() const;
    void setTextDraft(const QString& text);

  signals:
    void textEditingChanged(bool editing);
    void textResultChanged(bool available);
    void textDraftChanged(const QString& text);

  private:
    struct CanvasState {
        QPointer<ScreenshotOverlayWindow> overlay;
        QPointer<SnowCanvasWidget> canvas;
        bool contentVisible = true;
        bool interactionEnabled = true;
        bool hadSelection = false;
        bool selectionHandlesVisible = true;
        bool selectionBorderVisible = true;
    };
    struct TextCacheEntry {
        std::shared_ptr<ScreenshotOcrPresentation> presentation;
        std::shared_ptr<ScreenshotOcrTextEditingSession> editingSession;
        bool editing = false;
    };

    void activateMode(Mode mode);
    void startRecognition();
    void startTableRecognition();
    void startQrRecognition();
    void handleWorkerOutput(quint64 generation, quint64 sessionId, const QRect& selection,
                            const QString& key, ScreenshotOcrRecognitionResult output);
    void handleTableOutput(quint64 generation, quint64 sessionId, const QRect& selection,
                           const QString& key, SnowShotTableResult result);
    void handleQrOutput(quint64 generation, quint64 sessionId, const QRect& selection,
                        const QString& key, ScreenshotQrRecognitionResult result);
    void applyPresentation(std::shared_ptr<ScreenshotOcrPresentation> presentation);
    void applyTableSession(std::shared_ptr<ScreenshotTableEditingSession> session);
    void applyQrContents(const QStringList& contents);
    void handleQrLinkActivated(const QUrl& url);
    void handleTableCommandStateChanged(const ScreenshotTableCommandState& state) const;
    void updateOverlays() const;
    void applyOcrBackgroundToOverlays(
        const std::shared_ptr<ScreenshotOcrPresentation>& presentation) const;
    void clearOcrBackgroundFromOverlays() const;
    void restorePreviousToolAfterFailure();
    void showStatus(const QString& message, bool error) const;
    void updateToolbarBusy() const;
    void updateToolbarTextState() const;
    void showTextEditor();
    void hideTextEditors();
    void clearTextEditingState();
    void handleTextDocumentChanged(const QString& key);
    void showRecognitionMessage();
    void hideRecognitionMessage();
    [[nodiscard]] bool ensureRecognitionWindow();
    void destroyRecognitionWindow();
    [[nodiscard]] QString currentCacheKey() const;

    ScreenshotOcrControllerContext m_context;
    QVector<CanvasState> m_canvasStates;
    QPointer<QTextDocument> m_textDocument;
    QHash<QString, TextCacheEntry> m_textCache;
    QHash<QString, std::shared_ptr<ScreenshotTableEditingSession>> m_tableCache;
    QHash<QString, QStringList> m_qrCache;
    std::shared_ptr<ScreenshotOcrPresentation> m_presentation;
    ScreenshotActiveTool m_previousTool = ScreenshotActiveTool::Move;
    std::unique_ptr<ScreenshotMessageService> m_messages;
    std::unique_ptr<ScreenshotRecognitionSessionController> m_session;
    QPointer<ScreenshotRecognitionWindow> m_recognitionWindow;
    std::shared_ptr<ScreenshotTableEditingSession> m_tableSession;
    QString m_textCacheKey;
    QString m_tableCacheKey;
    QString m_qrCacheKey;
    QStringList m_qrContents;
    QString m_editingKey;
    QString m_surfaceKey;
    QImage m_surfaceImage;
    SnowShotApiClient::RequestToken m_tableRequestToken = 0;
    ScreenshotQrRecognitionPort::RequestToken m_qrRequestToken = 0;
    Mode m_mode = Mode::Text;
    quint64 m_textGeneration = 0;
    quint64 m_tableGeneration = 0;
    quint64 m_qrGeneration = 0;
    quint64 m_sessionGeneration = 0;
    ScreenshotOcrRecognitionPort::RequestToken m_requestToken = 0;
    bool m_active = false;
    bool m_editing = false;
};

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTOCRCONTROLLER_H
