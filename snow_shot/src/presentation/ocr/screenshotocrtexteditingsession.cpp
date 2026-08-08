#include "snow_shot/presentation/screenshotocrtexteditingsession.h"

#include <utility>

ScreenshotOcrTextEditingSession::ScreenshotOcrTextEditingSession(QString originalText)
    : m_originalText(std::move(originalText)) {
    m_document.setUndoRedoEnabled(false);
    m_document.setPlainText(m_originalText);
    m_history.push_back(m_originalText);
    QObject::connect(&m_document, &QTextDocument::contentsChanged, [this]() {
        recordCurrentText();
    });
}

const QString& ScreenshotOcrTextEditingSession::originalText() const {
    return m_originalText;
}

QString ScreenshotOcrTextEditingSession::text() const {
    return m_document.toPlainText();
}

QTextDocument* ScreenshotOcrTextEditingSession::document() {
    return &m_document;
}

const QTextDocument* ScreenshotOcrTextEditingSession::document() const {
    return &m_document;
}

bool ScreenshotOcrTextEditingSession::replaceText(const QString& text) {
    if (text == this->text()) {
        return false;
    }
    if (m_historyIndex + 1 < m_history.size()) {
        m_history.resize(m_historyIndex + 1);
    }
    m_history.push_back(text);
    ++m_historyIndex;
    applyText(text);
    return true;
}

bool ScreenshotOcrTextEditingSession::reset() {
    return replaceText(m_originalText);
}

void ScreenshotOcrTextEditingSession::recordCurrentText() {
    if (m_applying) {
        return;
    }
    const QString current = text();
    if (current == m_history.at(m_historyIndex)) {
        return;
    }
    if (m_historyIndex + 1 < m_history.size()) {
        m_history.resize(m_historyIndex + 1);
    }
    m_history.push_back(current);
    ++m_historyIndex;
}

void ScreenshotOcrTextEditingSession::applyText(const QString& text) {
    m_applying = true;
    m_document.setPlainText(text);
    m_applying = false;
}

void ScreenshotOcrTextEditingSession::undo() {
    if (!canUndo()) {
        return;
    }
    --m_historyIndex;
    applyText(m_history.at(m_historyIndex));
}

void ScreenshotOcrTextEditingSession::redo() {
    if (!canRedo()) {
        return;
    }
    ++m_historyIndex;
    applyText(m_history.at(m_historyIndex));
}

bool ScreenshotOcrTextEditingSession::canUndo() const {
    return m_historyIndex > 0;
}

bool ScreenshotOcrTextEditingSession::canRedo() const {
    return m_historyIndex + 1 < m_history.size();
}
