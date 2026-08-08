#include "snow_shot/storage/configurationschema.h"

#include "snow_shot/storage/capturehistorytypes.h"
#include "snow_shot/storage/persistedselectioncodec.h"

#include <QJsonArray>
#include <QKeySequence>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace snow_shot::storage {
namespace {
const QVector<ConfigurationSchemaEntry> kEntries = {
    {QStringLiteral("storage/schema_version"), 1, ConfigurationValueKind::Integer,
     ConfigurationIntegerRange{1, 1, 1}},
    {QStringLiteral("interface/theme_mode"),
     QStringLiteral("system"),
     ConfigurationValueKind::String,
     std::nullopt,
     {QStringLiteral("system"), QStringLiteral("light"), QStringLiteral("dark")}},
    {QStringLiteral("interface/language"), QStringLiteral("system"),
     ConfigurationValueKind::String},
    {QStringLiteral("system/application_priority"), QStringLiteral("above_normal"),
     ConfigurationValueKind::String, std::nullopt,
     {QStringLiteral("normal"), QStringLiteral("above_normal"), QStringLiteral("high"),
      QStringLiteral("real_time")}},
    {QStringLiteral("interface/sidebar_collapsed"), false, ConfigurationValueKind::Boolean},
    {QStringLiteral("global_shortcuts/screenshot"),
     QJsonArray(),
     ConfigurationValueKind::StringList,
     std::nullopt,
     {},
     2},
    {QStringLiteral("global_shortcuts/open_settings"),
     QJsonArray(),
     ConfigurationValueKind::StringList,
     std::nullopt,
     {},
     2},
    {QStringLiteral("video_recording/enable_microphone"), false, ConfigurationValueKind::Boolean},
    {QStringLiteral("video_recording/enable_system_audio"), true, ConfigurationValueKind::Boolean},
    {QStringLiteral("screenshot_toolbar/arrow_line_tool"), QStringLiteral("arrow"),
     ConfigurationValueKind::String, std::nullopt,
     {QStringLiteral("arrow"), QStringLiteral("line")}},
    {QStringLiteral("screenshot_toolbar/highlight_tool"), QStringLiteral("pen_highlight"),
     ConfigurationValueKind::String, std::nullopt,
     {QStringLiteral("pen_highlight"), QStringLiteral("highlight"), QStringLiteral("spotlight")}},
    {QStringLiteral("screenshot_toolbar/table_qr_tool"), QStringLiteral("table"),
     ConfigurationValueKind::String, std::nullopt,
     {QStringLiteral("table"), QStringLiteral("qr")}},
    {QStringLiteral("screenshot_selection/previous_selection"), QJsonValue::Null,
     ConfigurationValueKind::Structured},
    {QStringLiteral("screenshot_selection/smart_selection"), true, ConfigurationValueKind::Boolean},
    {QStringLiteral("screenshot_selection/selection_rect_presets"), QJsonArray(),
     ConfigurationValueKind::Structured},
    {QStringLiteral("capture_history/enabled"), true, ConfigurationValueKind::Boolean},
    {QStringLiteral("capture_history/retention_days"), 7, ConfigurationValueKind::Integer,
     ConfigurationIntegerRange{CaptureHistoryPolicy::MinimumRetentionDays,
                               CaptureHistoryPolicy::MaximumRetentionDays, 1}},
    {QStringLiteral("capture_history/max_entries"), 100, ConfigurationValueKind::Integer,
     ConfigurationIntegerRange{CaptureHistoryPolicy::MinimumEntries,
                               CaptureHistoryPolicy::MaximumEntries, 1}},
    {QStringLiteral("capture_history/max_disk_mib"), 1024, ConfigurationValueKind::Integer,
     ConfigurationIntegerRange{CaptureHistoryPolicy::MinimumDiskMiB,
                               CaptureHistoryPolicy::MaximumDiskMiB, 1}},
};

bool isInteger(const QJsonValue& value, int* result = nullptr) {
    if (!value.isDouble() || !std::isfinite(value.toDouble()) ||
        std::floor(value.toDouble()) != value.toDouble()) {
        return false;
    }
    if (value.toDouble() < static_cast<double>(std::numeric_limits<int>::min()) ||
        value.toDouble() > static_cast<double>(std::numeric_limits<int>::max())) {
        return false;
    }
    if (result != nullptr) {
        *result = value.toInt();
    }
    return true;
}

ConfigurationNormalization exactType(const QJsonValue& value, QJsonValue::Type type) {
    return {value, value.type() == type, false};
}

ConfigurationNormalization normalizeIntegerRange(const QJsonValue& value, int minimum,
                                                 int maximum) {
    int integer = 0;
    if (!isInteger(value, &integer) || integer < minimum || integer > maximum) {
        return {};
    }
    return {integer, true, false};
}

ConfigurationNormalization normalizeTheme(const QJsonValue& value) {
    if (!value.isString()) {
        return {};
    }
    const QString normalized = value.toString().trimmed().toLower();
    if (normalized != QStringLiteral("system") && normalized != QStringLiteral("light") &&
        normalized != QStringLiteral("dark")) {
        return {};
    }
    return {normalized, true, normalized != value.toString()};
}

ConfigurationNormalization normalizeLanguage(const QJsonValue& value) {
    if (!value.isString()) {
        return {};
    }
    QString normalized = value.toString().trimmed();
    if (normalized.compare(QStringLiteral("system"), Qt::CaseInsensitive) == 0) {
        normalized = QStringLiteral("system");
    } else {
        normalized.replace(u'-', u'_');
        if (normalized.compare(QStringLiteral("en"), Qt::CaseInsensitive) == 0) {
            normalized = QStringLiteral("en_US");
        } else {
            static const QRegularExpression localePattern(
                QStringLiteral("^[A-Za-z]{2,3}(?:_[A-Za-z0-9]{2,8})*$"));
            if (!localePattern.match(normalized).hasMatch()) {
                return {};
            }
            const QLocale locale(normalized);
            if (locale.language() == QLocale::AnyLanguage || locale.name() == QStringLiteral("C")) {
                return {};
            }
            normalized = locale.name();
        }
    }
    return {normalized, true, normalized != value.toString()};
}

QString canonicalShortcut(const QString& input) {
    const QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    QKeySequence sequence = QKeySequence::fromString(trimmed, QKeySequence::PortableText);
    if (sequence.isEmpty()) {
        sequence = QKeySequence::fromString(trimmed, QKeySequence::NativeText);
    }
    if (sequence.count() != 1) {
        return trimmed;
    }
    const QKeyCombination combination = sequence[0];
    const Qt::Key key = combination.key();
    if (key == Qt::Key_unknown || key == Qt::Key_Control || key == Qt::Key_Alt ||
        key == Qt::Key_Shift || key == Qt::Key_Meta || key == Qt::Key_AltGr ||
        key == Qt::Key_Super_L || key == Qt::Key_Super_R) {
        return trimmed;
    }
    const QString portable = sequence.toString(QKeySequence::PortableText).trimmed();
    return portable;
}

ConfigurationNormalization normalizeShortcuts(const QJsonValue& value, int maximumItems) {
    if (!value.isArray()) {
        return {};
    }
    QJsonArray normalized;
    QSet<QString> seen;
    bool changed = false;
    for (const QJsonValue& item : value.toArray()) {
        if (!item.isString()) {
            changed = true;
            continue;
        }
        const QString shortcut = canonicalShortcut(item.toString());
        if (shortcut.isEmpty() || seen.contains(shortcut) ||
            (maximumItems >= 0 && normalized.size() >= maximumItems)) {
            changed = true;
            continue;
        }
        seen.insert(shortcut);
        normalized.push_back(shortcut);
        changed = changed || shortcut != item.toString();
    }
    return {normalized, true, changed};
}

ConfigurationNormalization normalizeSelection(const QJsonValue& value) {
    if (value.isNull()) {
        return {QJsonValue::Null, true, false};
    }
    const PersistedSelectionNormalization normalized = normalizePersistedSelection(value);
    if (!normalized.valid) {
        return {};
    }
    return {persistedSelectionToJson(normalized.value), true, normalized.changed};
}

ConfigurationNormalization normalizePresets(const QJsonValue& value) {
    if (!value.isArray()) {
        return {};
    }
    QJsonArray result;
    bool changed = false;
    for (const QJsonValue& item : value.toArray()) {
        if (!item.isObject()) {
            changed = true;
            continue;
        }
        const QJsonObject itemObject = item.toObject();
        const QString name = itemObject.value(QStringLiteral("name")).toString().trimmed();
        const PersistedSelectionNormalization normalized = normalizePersistedSelection(item);
        if (name.isEmpty() || !normalized.valid) {
            changed = true;
            continue;
        }
        QJsonObject normalizedSelection = persistedSelectionToJson(normalized.value);
        normalizedSelection.insert(QStringLiteral("name"), name);
        result.push_back(normalizedSelection);
        changed = changed || normalized.changed || normalizedSelection != itemObject;
    }
    return {result, true, changed};
}

void insertPath(QJsonObject* root, const QString& path, const QJsonValue& value) {
    const QStringList parts = path.split(u'/');
    if (root == nullptr || parts.size() != 2) {
        return;
    }
    QJsonObject group = root->value(parts[0]).toObject();
    group.insert(parts[1], value);
    root->insert(parts[0], group);
}
} // namespace

const QVector<ConfigurationSchemaEntry>& ConfigurationSchema::entries() {
    return kEntries;
}

const ConfigurationSchemaEntry* ConfigurationSchema::entry(const QString& key) {
    const auto found = std::find_if(kEntries.cbegin(), kEntries.cend(),
                                    [&key](const auto& item) { return item.key == key; });
    return found == kEntries.cend() ? nullptr : &*found;
}

bool ConfigurationSchema::contains(const QString& key) {
    return entry(key) != nullptr;
}

QJsonValue ConfigurationSchema::defaultValue(const QString& key) {
    const ConfigurationSchemaEntry* found = entry(key);
    return found == nullptr ? QJsonValue() : found->defaultValue;
}

ConfigurationNormalization ConfigurationSchema::normalize(const QString& key,
                                                          const QJsonValue& value) {
    const ConfigurationSchemaEntry* schemaEntry = entry(key);
    if (schemaEntry == nullptr) {
        return {};
    }
    if (key == QStringLiteral("interface/theme_mode")) {
        return normalizeTheme(value);
    }
    if (key == QStringLiteral("interface/language")) {
        return normalizeLanguage(value);
    }
    if (key == QStringLiteral("screenshot_selection/previous_selection")) {
        return normalizeSelection(value);
    }
    if (key == QStringLiteral("screenshot_selection/selection_rect_presets")) {
        return normalizePresets(value);
    }
    switch (schemaEntry->valueKind) {
    case ConfigurationValueKind::Boolean:
        return exactType(value, QJsonValue::Bool);
    case ConfigurationValueKind::Integer:
        if (schemaEntry->integerRange.has_value()) {
            return normalizeIntegerRange(value, schemaEntry->integerRange->minimum,
                                         schemaEntry->integerRange->maximum);
        }
        return isInteger(value) ? ConfigurationNormalization{value, true, false}
                                : ConfigurationNormalization{};
    case ConfigurationValueKind::String: {
        if (!value.isString()) {
            return {};
        }
        const QString normalizedValue = value.toString().trimmed();
        if (!schemaEntry->allowedStringValues.isEmpty() &&
            !schemaEntry->allowedStringValues.contains(normalizedValue)) {
            return {};
        }
        return {normalizedValue, true, normalizedValue != value.toString()};
    }
    case ConfigurationValueKind::StringList:
        return normalizeShortcuts(value, schemaEntry->maximumListItems);
    case ConfigurationValueKind::Structured:
        break;
    }
    return {};
}

QJsonObject ConfigurationSchema::completeDefaultDocument() {
    QJsonObject root;
    for (const ConfigurationSchemaEntry& entry : kEntries) {
        insertPath(&root, entry.key, entry.defaultValue);
    }
    return root;
}
} // namespace snow_shot::storage
