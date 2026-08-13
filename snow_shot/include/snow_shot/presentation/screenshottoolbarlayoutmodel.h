#ifndef SNOW_SHOT_PRESENTATION_SCREENSHOTTOOLBARLAYOUTMODEL_H
#define SNOW_SHOT_PRESENTATION_SCREENSHOTTOOLBARLAYOUTMODEL_H

#include "snow_shot/presentation/components/icons/snowshoticons.h"
#include "snow_shot/storage/settingsadapters.h"

#include "antd_icons.h"

#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace snow_shot::presentation::toolbar_layout {

enum class Item {
    Move,
    Select,
    Shape,
    ArrowLine,
    FreeDraw,
    Highlight,
    Text,
    SerialNumber,
    Filter,
    Eraser,
    Watermark,
    History,
    TableQr,
    VideoRecord,
    Pin,
    Ocr,
    ScrollingScreenshot,
};

enum class Icon {
    Move,
    Select,
    Shape,
    Arrow,
    Line,
    FreeDraw,
    Highlight,
    Spotlight,
    Text,
    SerialNumber,
    Filter,
    Eraser,
    Watermark,
    Undo,
    Redo,
    Table,
    Qr,
    VideoRecord,
    Pin,
    Ocr,
    ScrollingScreenshot,
};

struct ChildDescriptor {
    const char* label = nullptr;
    Icon icon = Icon::Move;
};

struct Descriptor {
    Item item = Item::Move;
    const char* id = nullptr;
    const char* label = nullptr;
    Icon icon = Icon::Move;
    QVector<ChildDescriptor> children;
};

[[nodiscard]] inline const QVector<Descriptor>& descriptors() {
    static const QVector<Descriptor> value{
        {Item::Move, "move",
         QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Edit selection"), Icon::Move,
         {}},
        {Item::Select, "select",
         QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Select elements"),
         Icon::Select, {}},
        {Item::Shape, "shape", QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Shape"),
         Icon::Shape, {}},
        {Item::ArrowLine,
         "arrow-line",
         QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Arrow and Line"),
         Icon::Arrow,
         {{QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Arrow"), Icon::Arrow},
          {QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Line"), Icon::Line}}},
        {Item::FreeDraw, "free-draw",
         QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Pen"), Icon::FreeDraw, {}},
        {Item::Highlight,
         "highlight",
         QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Highlight"),
         Icon::Highlight,
         {{QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Pen highlight"),
           Icon::Highlight},
          {QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Rectangle highlight"),
           Icon::Highlight},
          {QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Spotlight"),
           Icon::Spotlight}}},
        {Item::Text, "text", QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Text"),
         Icon::Text, {}},
        {Item::SerialNumber, "serial-number",
         QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Serial number"),
         Icon::SerialNumber, {}},
        {Item::Filter, "filter",
         QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Filter"), Icon::Filter, {}},
        {Item::Eraser, "eraser",
         QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Eraser"), Icon::Eraser, {}},
        {Item::Watermark, "watermark",
         QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Watermark"), Icon::Watermark,
         {}},
        {Item::History,
         "history",
         QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Undo and Redo"),
         Icon::Undo,
         {{QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Undo"), Icon::Undo},
          {QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Redo"), Icon::Redo}}},
        {Item::TableQr,
         "table-qr",
         QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Table and QR"),
         Icon::Table,
         {{QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Table recognition"),
           Icon::Table},
          {QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "QR code recognition"),
           Icon::Qr}}},
        {Item::VideoRecord, "video-record",
         QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Record video"),
         Icon::VideoRecord, {}},
        {Item::Pin, "pin",
         QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Pin to screen"), Icon::Pin,
         {}},
        {Item::Ocr, "ocr",
         QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Text recognition"), Icon::Ocr,
         {}},
        {Item::ScrollingScreenshot,
         "scrolling-screenshot",
         QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Scrolling screenshot"),
         Icon::ScrollingScreenshot,
         {}},
    };
    return value;
}

[[nodiscard]] inline const Descriptor& descriptor(Item item) {
    for (const Descriptor& candidate : descriptors()) {
        if (candidate.item == item) {
            return candidate;
        }
    }
    return descriptors().constFirst();
}

[[nodiscard]] inline const Descriptor* descriptor(const QString& id) {
    for (const Descriptor& candidate : descriptors()) {
        if (id == QLatin1String(candidate.id)) {
            return &candidate;
        }
    }
    return nullptr;
}

[[nodiscard]] inline QString id(Item item) {
    return QString::fromLatin1(descriptor(item).id);
}

[[nodiscard]] inline QStringList defaultOrder() {
    QStringList result;
    result.reserve(descriptors().size());
    for (const Descriptor& candidate : descriptors()) {
        result.push_back(QString::fromLatin1(candidate.id));
    }
    return result;
}

[[nodiscard]] inline storage::ScreenshotToolbarLayout
normalizedLayout(const storage::ScreenshotToolbarLayout& input) {
    const QStringList defaults = defaultOrder();
    const QSet<QString> known(defaults.cbegin(), defaults.cend());
    QSet<QString> ordered;
    storage::ScreenshotToolbarLayout result;
    for (const QString& itemId : input.order) {
        if (known.contains(itemId) && !ordered.contains(itemId)) {
            result.order.push_back(itemId);
            ordered.insert(itemId);
        }
    }
    for (const QString& itemId : defaults) {
        if (!ordered.contains(itemId)) {
            result.order.push_back(itemId);
        }
    }

    const QSet<QString> requestedHidden(input.hidden.cbegin(), input.hidden.cend());
    for (const QString& itemId : result.order) {
        if (known.contains(itemId) && requestedHidden.contains(itemId)) {
            result.hidden.push_back(itemId);
        }
    }
    return result;
}

[[nodiscard]] inline adqt::icons::IconRef icon(Icon semantic) {
    namespace custom = snow_shot::presentation::icons::custom::outlined;
    namespace ant = adqt::icons::antd::outlined;
    switch (semantic) {
    case Icon::Move:
        return custom::ToolMove();
    case Icon::Select:
        return custom::ToolSelect();
    case Icon::Shape:
        return custom::ToolRectangle();
    case Icon::Arrow:
        return custom::ToolArrow();
    case Icon::Line:
        return custom::ToolLine();
    case Icon::FreeDraw:
        return custom::ToolFreeDraw();
    case Icon::Highlight:
        return custom::ToolHighlight();
    case Icon::Spotlight:
        return custom::ToolSpotlight();
    case Icon::Text:
        return custom::ToolText();
    case Icon::SerialNumber:
        return custom::ToolSerialNumber();
    case Icon::Filter:
        return custom::ToolFilter();
    case Icon::Eraser:
        return custom::ToolEraser();
    case Icon::Watermark:
        return custom::ToolWatermark();
    case Icon::Undo:
        return ant::Undo();
    case Icon::Redo:
        return ant::Redo();
    case Icon::Table:
        return custom::TableRecognition();
    case Icon::Qr:
        return custom::ScanQrcode();
    case Icon::VideoRecord:
        return custom::RecordVideo();
    case Icon::Pin:
        return custom::PinToScreen();
    case Icon::Ocr:
        return custom::ToolRecognizeText();
    case Icon::ScrollingScreenshot:
        return custom::ScrollingScreenshot();
    }
    return {};
}

} // namespace snow_shot::presentation::toolbar_layout

#endif // SNOW_SHOT_PRESENTATION_SCREENSHOTTOOLBARLAYOUTMODEL_H
