#include "snow_shot/presentation/screenshotrecognitionsessioncontroller.h"

#include "snow_shot/presentation/screenshotocrpresentation.h"
#include "snow_shot/presentation/screenshotocrtexteditingsession.h"
#include "snow_shot/presentation/screenshotocrtexttransform.h"
#include "snow_shot/presentation/screenshotrecognitionwindow.h"
#include "snow_shot/presentation/screenshottabledocument.h"
#include "snow_shot/presentation/screenshottableeditor.h"

#include <QDesktopServices>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>

#include <utility>

namespace {
constexpr auto kRecognitionMessageKey = "screenshot-recognition-session";
}

ScreenshotRecognitionSessionController::ScreenshotRecognitionSessionController(
    ScreenshotOcrRecognitionPort* recognition, ScreenshotQrRecognitionPort* qrRecognition,
    SnowShotApiClient* tableRecognition, ScreenshotRecognitionSessionActions actions,
    QObject* parent)
    : QObject(parent),
      m_recognition(recognition),
      m_qrRecognition(qrRecognition),
      m_tableRecognition(tableRecognition),
      m_actions(std::move(actions)) {
    if (recognition != nullptr) {
        connect(recognition, &QObject::destroyed, this,
                [this]() { handleRecognitionProviderDestroyed(Mode::Text); });
    }
    if (tableRecognition != nullptr) {
        connect(tableRecognition, &QObject::destroyed, this,
                [this]() { handleRecognitionProviderDestroyed(Mode::Table); });
    }
    if (qrRecognition != nullptr) {
        connect(qrRecognition, &QObject::destroyed, this,
                [this]() { handleRecognitionProviderDestroyed(Mode::Qr); });
    }
}

ScreenshotRecognitionSessionController::~ScreenshotRecognitionSessionController() {
    invalidate();
}

void ScreenshotRecognitionSessionController::setTarget(ScreenshotRecognitionTarget target) {
    if (target.key == m_target.key && target.canvasRect == m_target.canvasRect &&
        target.image.cacheKey() == m_target.image.cacheKey()) {
        return;
    }
    invalidate();
    m_target = std::move(target);
}

bool ScreenshotRecognitionSessionController::hasTarget() const {
    return m_target.isValid();
}

void ScreenshotRecognitionSessionController::prefetchText() {
    if (!hasTarget() || m_textCache.contains(m_target.key) || m_textRequestToken != 0) {
        return;
    }
    startTextRecognition();
}

void ScreenshotRecognitionSessionController::activate(Mode mode) {
    if (!hasTarget()) {
        showStatus(tr("Unable to read the selected screenshot"), true);
        return;
    }
    clearTextEditingState();
    m_mode = mode;
    m_active = true;
    ensureContent();
    if (m_actions.setRecognitionVisualState) {
        m_actions.setRecognitionVisualState(true);
    }
    if (m_actions.setActiveMode) {
        m_actions.setActiveMode(static_cast<int>(mode));
    }

    if (ScreenshotRecognitionWindow* window = content()) {
        window->clearOcrPresentation();
        window->clearTableSession();
        window->clearQrContents();
    }
    m_presentation.reset();
    m_tableSession.reset();
    m_qrContents.clear();
    m_textCacheKey.clear();
    m_tableCacheKey.clear();
    m_qrCacheKey.clear();
    clearContent();
    if (m_actions.clearOcrBackground) {
        m_actions.clearOcrBackground();
    }
    if (mode == Mode::Text) {
        const auto cached = m_textCache.constFind(m_target.key);
        if (cached != m_textCache.cend()) {
            m_textCacheKey = m_target.key;
            m_presentation = cached->presentation;
            applyPresentation(m_presentation);
            emit textResultChanged(true);
            if (cached->editing) {
                m_editing = true;
                m_editingKey = m_target.key;
                beginTextEditing();
            }
        } else {
            startTextRecognition();
        }
    } else if (mode == Mode::Table) {
        const auto cached = m_tableCache.constFind(m_target.key);
        if (cached != m_tableCache.cend()) {
            m_tableCacheKey = m_target.key;
            applyTableSession(cached.value());
        } else {
            startTableRecognition();
        }
    } else {
        const auto cached = m_qrCache.constFind(m_target.key);
        if (cached != m_qrCache.cend()) {
            m_qrCacheKey = m_target.key;
            applyQrContents(cached.value());
        } else {
            startQrRecognition();
        }
    }
    updateBusyState();
    updateTextState();
}

void ScreenshotRecognitionSessionController::deactivate() {
    if (!m_active && m_content == nullptr) {
        return;
    }
    clearTextEditingState();
    m_active = false;
    m_presentation.reset();
    m_tableSession.reset();
    m_qrContents.clear();
    clearContent();
    if (m_actions.clearOcrBackground) {
        m_actions.clearOcrBackground();
    }
    if (m_actions.setRecognitionVisualState) {
        m_actions.setRecognitionVisualState(false);
    }
    if (m_actions.setActiveMode) {
        m_actions.setActiveMode(-1);
    }
    updateBusyState();
    updateTextState();
    updateTableState({});
}

void ScreenshotRecognitionSessionController::invalidate() {
    deactivate();
    cancelOutstandingRequests();
    ++m_textGeneration;
    ++m_tableGeneration;
    ++m_qrGeneration;
    m_textCache.clear();
    m_tableCache.clear();
    m_qrCache.clear();
    m_textCacheKey.clear();
    m_tableCacheKey.clear();
    m_qrCacheKey.clear();
    m_editingKey.clear();
    m_target = {};
    emit textResultChanged(false);
}

bool ScreenshotRecognitionSessionController::active() const {
    return m_active;
}

bool ScreenshotRecognitionSessionController::busy() const {
    return busy(Mode::Text) || busy(Mode::Table) || busy(Mode::Qr);
}

bool ScreenshotRecognitionSessionController::busy(Mode mode) const {
    switch (mode) {
    case Mode::Text:
        return m_textRequestToken != 0;
    case Mode::Table:
        return m_tableRequestToken != 0;
    case Mode::Qr:
        return m_qrRequestToken != 0;
    }
    return false;
}

ScreenshotRecognitionSessionController::Mode ScreenshotRecognitionSessionController::mode() const {
    return m_mode;
}

bool ScreenshotRecognitionSessionController::tableModeActive() const {
    return m_active && m_mode == Mode::Table;
}

bool ScreenshotRecognitionSessionController::qrModeActive() const {
    return m_active && m_mode == Mode::Qr;
}

void ScreenshotRecognitionSessionController::mergeTableSelection() {
    if (tableModeActive() && content() != nullptr) {
        content()->mergeTableSelection();
    }
}

void ScreenshotRecognitionSessionController::splitTableSelection() {
    if (tableModeActive() && content() != nullptr) {
        content()->splitTableSelection();
    }
}

void ScreenshotRecognitionSessionController::resetTable() {
    if (tableModeActive() && content() != nullptr) {
        content()->resetTable();
    }
}

void ScreenshotRecognitionSessionController::undoTableEdit() {
    if (tableModeActive() && content() != nullptr) {
        content()->undoTableEdit();
    }
}

void ScreenshotRecognitionSessionController::redoTableEdit() {
    if (tableModeActive() && content() != nullptr) {
        content()->redoTableEdit();
    }
}

void ScreenshotRecognitionSessionController::undoTextEdit() {
    if (!m_active || !m_editing || m_textDocument == nullptr) {
        return;
    }
    const auto session = m_textCache.value(m_editingKey).editingSession;
    if (session != nullptr) {
        session->undo();
    }
}

void ScreenshotRecognitionSessionController::redoTextEdit() {
    if (!m_active || !m_editing || m_textDocument == nullptr) {
        return;
    }
    const auto session = m_textCache.value(m_editingKey).editingSession;
    if (session != nullptr) {
        session->redo();
    }
}

void ScreenshotRecognitionSessionController::beginTextEditing() {
    if (!m_active || m_mode != Mode::Text || !hasTextResult()) {
        return;
    }
    m_editing = true;
    m_editingKey = m_textCacheKey.isEmpty() ? m_target.key : m_textCacheKey;
    auto it = m_textCache.find(m_editingKey);
    if (it != m_textCache.end()) {
        it->editing = true;
        m_textDocument = it->editingSession != nullptr ? it->editingSession->document() : nullptr;
    }
    if (content() != nullptr && m_textDocument != nullptr) {
        if (m_actions.clearOcrBackground) {
            m_actions.clearOcrBackground();
        }
        content()->clearOcrPresentation();
        content()->showTextEditor(m_textDocument.data());
    }
    emit textEditingChanged(true);
    updateTextState();
}

void ScreenshotRecognitionSessionController::endTextEditing() {
    if (!m_editing || !hasTextResult()) {
        return;
    }
    auto it = m_textCache.find(m_editingKey);
    if (it != m_textCache.end()) {
        it->editing = false;
    }
    m_editing = false;
    m_editingKey.clear();
    m_textDocument = nullptr;
    if (content() != nullptr) {
        content()->hideTextEditor();
    }
    applyPresentation(m_presentation);
    emit textEditingChanged(false);
    updateTextState();
}

void ScreenshotRecognitionSessionController::resetTextEditing() {
    if (m_editing) {
        setTextDraft(originalText());
    }
}

void ScreenshotRecognitionSessionController::applyRemoveLineBreaks() {
    beginTextEditing();
    if (m_editing) {
        setTextDraft(snow_shot::presentation::removeOcrLineBreaks(originalText()));
    }
}

void ScreenshotRecognitionSessionController::applyHalfWidthPunctuation() {
    beginTextEditing();
    if (m_editing) {
        setTextDraft(snow_shot::presentation::convertOcrPunctuation(originalText(), false));
    }
}

void ScreenshotRecognitionSessionController::applyFullWidthPunctuation() {
    beginTextEditing();
    if (m_editing) {
        setTextDraft(snow_shot::presentation::convertOcrPunctuation(originalText(), true));
    }
}

bool ScreenshotRecognitionSessionController::editing() const {
    return m_editing;
}

bool ScreenshotRecognitionSessionController::hasTextResult() const {
    return !m_textCacheKey.isEmpty() && m_textCache.contains(m_textCacheKey);
}

QString ScreenshotRecognitionSessionController::textDraft() const {
    const QString key = m_editingKey.isEmpty() ? m_textCacheKey : m_editingKey;
    const auto session = m_textCache.value(key).editingSession;
    return session != nullptr ? session->text() : QString{};
}

QString ScreenshotRecognitionSessionController::originalText() const {
    const QString key = m_editingKey.isEmpty() ? m_textCacheKey : m_editingKey;
    const auto session = m_textCache.value(key).editingSession;
    return session != nullptr ? session->originalText() : QString{};
}

std::shared_ptr<ScreenshotTableEditingSession>
ScreenshotRecognitionSessionController::tableSession() const {
    return m_tableSession;
}

QStringList ScreenshotRecognitionSessionController::qrContents() const {
    return m_qrContents;
}

void ScreenshotRecognitionSessionController::setTextDraft(const QString& text) {
    const QString key = m_editingKey.isEmpty() ? m_textCacheKey : m_editingKey;
    auto it = m_textCache.find(key);
    if (it == m_textCache.end() || it->editingSession == nullptr) {
        return;
    }
    static_cast<void>(it->editingSession->replaceText(text));
}

void ScreenshotRecognitionSessionController::startTextRecognition() {
    if (!hasTarget() || m_recognition == nullptr || m_textRequestToken != 0 ||
        !screenshotOcrImageWithinPixelLimit(m_target.image.size())) {
        if (hasTarget() && !screenshotOcrImageWithinPixelLimit(m_target.image.size())) {
            showStatus(tr("Text recognition is unavailable for screenshots larger than 4K"), false);
        }
        return;
    }
    const quint64 generation = ++m_textGeneration;
    const QString key = m_target.key;
    showRecognitionMessage();
    const auto callbackCompleted = std::make_shared<bool>(false);
    m_textRequestToken = m_recognition->recognize(
        m_target.image, m_target.canvasRect, this,
        [this, generation, key, callbackCompleted](ScreenshotOcrRecognitionResult output) {
            *callbackCompleted = true;
            if (generation == m_textGeneration) {
                m_textRequestToken = 0;
            }
            handleTextOutput(generation, key, std::move(output));
        });
    if (*callbackCompleted) {
        m_textRequestToken = 0;
    }
    updateBusyState();
    if (m_textRequestToken == 0 && !*callbackCompleted) {
        showStatus(tr("Text recognition request could not be prepared"), true);
        hideRecognitionMessage();
    }
}

void ScreenshotRecognitionSessionController::startTableRecognition() {
    if (!hasTarget() || m_tableRecognition == nullptr || m_tableRequestToken != 0) {
        if (m_tableRecognition == nullptr) {
            showStatus(tr("Table recognition service is unavailable"), true);
        }
        return;
    }
    const quint64 generation = ++m_tableGeneration;
    const QString key = m_target.key;
    showRecognitionMessage();
    const auto callbackCompleted = std::make_shared<bool>(false);
    m_tableRequestToken = m_tableRecognition->extractTable(
        m_target.image, this,
        [this, generation, key, callbackCompleted](SnowShotTableResult result) {
            *callbackCompleted = true;
            if (generation == m_tableGeneration) {
                m_tableRequestToken = 0;
            }
            handleTableOutput(generation, key, std::move(result));
        });
    if (*callbackCompleted) {
        m_tableRequestToken = 0;
    }
    updateBusyState();
    if (m_tableRequestToken == 0 && !*callbackCompleted) {
        showStatus(tr("Table recognition request could not be prepared"), true);
        hideRecognitionMessage();
    }
}

void ScreenshotRecognitionSessionController::startQrRecognition() {
    if (!hasTarget() || m_qrRecognition == nullptr || m_qrRequestToken != 0) {
        if (m_qrRecognition == nullptr) {
            showStatus(tr("QR code recognition is unavailable"), true);
        }
        return;
    }
    const quint64 generation = ++m_qrGeneration;
    const QString key = m_target.key;
    showRecognitionMessage();
    const auto callbackCompleted = std::make_shared<bool>(false);
    m_qrRequestToken = m_qrRecognition->recognize(
        m_target.image, this,
        [this, generation, key, callbackCompleted](ScreenshotQrRecognitionResult result) {
            *callbackCompleted = true;
            if (generation == m_qrGeneration) {
                m_qrRequestToken = 0;
            }
            handleQrOutput(generation, key, std::move(result));
        });
    if (*callbackCompleted) {
        m_qrRequestToken = 0;
    }
    updateBusyState();
    if (m_qrRequestToken == 0 && !*callbackCompleted) {
        showStatus(tr("QR code recognition request could not be prepared"), true);
        hideRecognitionMessage();
    }
}

void ScreenshotRecognitionSessionController::handleTextOutput(
    quint64 generation, const QString& key, ScreenshotOcrRecognitionResult output) {
    if (generation != m_textGeneration || key != m_target.key) {
        return;
    }
    if (!output.error.isEmpty() || output.presentation == nullptr) {
        showStatus(output.error.isEmpty() ? tr("Text recognition failed") : output.error, true);
        hideRecognitionMessage();
        updateBusyState();
        return;
    }
    const QString original = snow_shot::presentation::originalOcrText(*output.presentation);
    auto editingSession = std::make_shared<ScreenshotOcrTextEditingSession>(original);
    connect(editingSession->document(), &QTextDocument::contentsChanged, this,
            [this, key]() { handleTextDocumentChanged(key); });
    m_textCache.insert(key, TextCacheEntry{output.presentation, std::move(editingSession), false});
    if (m_active && m_mode == Mode::Text) {
        m_textCacheKey = key;
        m_presentation = m_textCache.value(key).presentation;
        applyPresentation(m_presentation);
        emit textResultChanged(true);
    }
    hideRecognitionMessage();
    updateBusyState();
    updateTextState();
}

void ScreenshotRecognitionSessionController::handleTableOutput(
    quint64 generation, const QString& key, SnowShotTableResult result) {
    if (generation != m_tableGeneration || key != m_target.key) {
        return;
    }
    if (!result.succeeded()) {
        showStatus(result.error.isEmpty() ? tr("Table recognition failed") : result.error, true);
        hideRecognitionMessage();
        updateBusyState();
        return;
    }
    ScreenshotTableDocument document = ScreenshotTableDocument::fromHtml(result.html);
    if (document.empty()) {
        showStatus(tr("No table cells were recognized"), false);
        hideRecognitionMessage();
        updateBusyState();
        return;
    }
    auto session = std::make_shared<ScreenshotTableEditingSession>(std::move(document));
    m_tableCache.insert(key, session);
    if (m_active && m_mode == Mode::Table) {
        m_tableCacheKey = key;
        applyTableSession(session);
    }
    hideRecognitionMessage();
    updateBusyState();
}

void ScreenshotRecognitionSessionController::handleQrOutput(
    quint64 generation, const QString& key, ScreenshotQrRecognitionResult result) {
    if (generation != m_qrGeneration || key != m_target.key) {
        return;
    }
    if (!result.error.isEmpty()) {
        showStatus(result.error, true);
        hideRecognitionMessage();
        updateBusyState();
        return;
    }
    if (result.contents.isEmpty()) {
        showStatus(tr("No QR code was recognized"), false);
        hideRecognitionMessage();
        updateBusyState();
        return;
    }
    m_qrCache.insert(key, result.contents);
    if (m_active && m_mode == Mode::Qr) {
        m_qrCacheKey = key;
        applyQrContents(result.contents);
    }
    hideRecognitionMessage();
    updateBusyState();
}

void ScreenshotRecognitionSessionController::ensureContent() {
    if (m_content == nullptr && m_actions.ensureContent) {
        m_content = m_actions.ensureContent();
    }
}

void ScreenshotRecognitionSessionController::clearContent() {
    if (m_content != nullptr) {
        m_content->clearOcrPresentation();
        m_content->clearTableSession();
        m_content->clearQrContents();
    }
}

void ScreenshotRecognitionSessionController::applyPresentation(
    const std::shared_ptr<ScreenshotOcrPresentation>& presentation) {
    m_presentation = presentation;
    ensureContent();
    if (m_actions.applyOcrPresentation) {
        m_actions.applyOcrPresentation(presentation);
    } else if (content() != nullptr) {
        content()->setOcrPresentation(presentation);
    }
    if (m_actions.applyOcrBackground) {
        m_actions.applyOcrBackground(presentation);
    }
}

void ScreenshotRecognitionSessionController::applyTableSession(
    const std::shared_ptr<ScreenshotTableEditingSession>& session) {
    if (session == nullptr || session->document.empty()) {
        return;
    }
    m_tableSession = session;
    ensureContent();
    if (content() != nullptr) {
        content()->setTableSession(session);
        updateTableState(content()->tableCommandState());
    }
}

void ScreenshotRecognitionSessionController::applyQrContents(const QStringList& contents) {
    m_qrContents = contents;
    ensureContent();
    if (content() != nullptr) {
        content()->showQrContents(contents);
    }
}

void ScreenshotRecognitionSessionController::handleTextDocumentChanged(const QString& key) {
    auto it = m_textCache.find(key);
    if (it == m_textCache.end() || it->editingSession == nullptr) {
        return;
    }
    it->editingSession->recordCurrentText();
    if (key == (m_editingKey.isEmpty() ? m_textCacheKey : m_editingKey)) {
        emit textDraftChanged(it->editingSession->text());
    }
    updateTextState();
}

void ScreenshotRecognitionSessionController::clearTextEditingState() {
    for (auto it = m_textCache.begin(); it != m_textCache.end(); ++it) {
        it->editing = false;
    }
    m_editing = false;
    m_editingKey.clear();
    m_textDocument = nullptr;
    if (content() != nullptr) {
        content()->hideTextEditor();
    }
    emit textEditingChanged(false);
}

void ScreenshotRecognitionSessionController::handleTableCommandState(
    const ScreenshotTableCommandState& state) {
    updateTableState(state);
}

void ScreenshotRecognitionSessionController::updateBusyState() const {
    if (m_actions.setBusyState) {
        m_actions.setBusyState(busy(Mode::Text), busy(Mode::Table), busy(Mode::Qr));
    }
}

void ScreenshotRecognitionSessionController::updateTextState() const {
    if (!m_actions.setTextEditingState) {
        return;
    }
    const auto session = m_textCache.value(m_editingKey).editingSession;
    m_actions.setTextEditingState(hasTextResult() && m_active && m_mode == Mode::Text,
                                  m_editing,
                                  m_editing && session != nullptr && session->canUndo(),
                                  m_editing && session != nullptr && session->canRedo());
}

void ScreenshotRecognitionSessionController::updateTableState(
    const ScreenshotTableCommandState& state) const {
    if (!m_actions.setTableEditingState) {
        return;
    }
    const bool available = m_active && m_mode == Mode::Table && m_tableSession != nullptr;
    m_actions.setTableEditingState(available, available && state.canUndo, available && state.canRedo,
                                   available && state.canMerge, available && state.canSplit,
                                   available && state.canReset);
}

void ScreenshotRecognitionSessionController::showRecognitionMessage() const {
    const QString message = m_mode == Mode::Table ? tr("Recognizing table")
                        : m_mode == Mode::Qr ? tr("Recognizing QR code")
                                             : tr("Recognizing text");
    if (m_actions.showLoading) {
        m_actions.showLoading(message);
    } else if (m_actions.showStatus) {
        m_actions.showStatus(message, false);
    }
}

void ScreenshotRecognitionSessionController::hideRecognitionMessage() const {
    if (!busy() && m_actions.hideLoading) {
        m_actions.hideLoading();
    }
}

void ScreenshotRecognitionSessionController::showStatus(const QString& message, bool error) const {
    if (!message.isEmpty() && m_actions.showStatus) {
        m_actions.showStatus(message, error);
    }
}

void ScreenshotRecognitionSessionController::cancelOutstandingRequests() {
    if (m_recognition != nullptr && m_textRequestToken != 0) {
        m_recognition->cancel(m_textRequestToken);
    }
    if (m_qrRecognition != nullptr && m_qrRequestToken != 0) {
        m_qrRecognition->cancel(m_qrRequestToken);
    }
    if (m_tableRecognition != nullptr && m_tableRequestToken != 0) {
        m_tableRecognition->cancel(m_tableRequestToken);
    }
    m_textRequestToken = 0;
    m_qrRequestToken = 0;
    m_tableRequestToken = 0;
    hideRecognitionMessage();
}

void ScreenshotRecognitionSessionController::handleRecognitionProviderDestroyed(Mode mode) {
    bool requestWasPending = false;
    switch (mode) {
    case Mode::Text:
        requestWasPending = m_textRequestToken != 0;
        m_textRequestToken = 0;
        ++m_textGeneration;
        break;
    case Mode::Table:
        requestWasPending = m_tableRequestToken != 0;
        m_tableRequestToken = 0;
        ++m_tableGeneration;
        break;
    case Mode::Qr:
        requestWasPending = m_qrRequestToken != 0;
        m_qrRequestToken = 0;
        ++m_qrGeneration;
        break;
    }

    updateBusyState();
    hideRecognitionMessage();
    if (requestWasPending) {
        const QString message = mode == Mode::Text    ? tr("Text recognition failed")
                                : mode == Mode::Table ? tr("Table recognition failed")
                                                      : tr("QR code recognition failed");
        showStatus(message, true);
    }
}

ScreenshotRecognitionWindow* ScreenshotRecognitionSessionController::content() const {
    return m_content;
}
