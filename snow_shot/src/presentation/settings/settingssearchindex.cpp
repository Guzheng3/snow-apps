#include "snow_shot/presentation/settings/settingssearchindex.h"

#include "snow_shot/presentation/languagemanager.h"

#include <QCoreApplication>
#include <QRegularExpression>

#include <algorithm>

namespace snow_shot::presentation::settings {
namespace {
constexpr const char* PAGES_SOURCE = QT_TRANSLATE_NOOP("SettingsCatalog", "Pages");

QString normalized(QString value) {
    value = value.normalized(QString::NormalizationForm_KC).toCaseFolded().trimmed();
    value.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return value;
}

QStringList tokens(const QString& query) {
    return normalized(query).split(u' ', Qt::SkipEmptyParts);
}

int fieldScore(const QString& token, const QString& field, int exact, int prefix,
               int contains) {
    if (field.isEmpty()) {
        return 0;
    }
    if (field == token) {
        return exact;
    }
    if (field.startsWith(token)) {
        return prefix;
    }
    return field.contains(token) ? contains : 0;
}

int entryScore(const SettingsSearchIndex::NormalizedFields& fields,
               const QStringList& queryTokens,
               const QString& normalizedQuery) {
    if (queryTokens.isEmpty()) {
        return 0;
    }

    int total = fields.title == normalizedQuery ? 2000 : 0;
    for (const QString& token : queryTokens) {
        int score = fieldScore(token, fields.title, 1000, 800, 600);
        for (const QString& alias : fields.aliases) {
            score = std::max(score, fieldScore(token, alias, 550, 500, 450));
        }
        for (const QString& optionLabel : fields.optionLabels) {
            score = std::max(score, fieldScore(token, optionLabel, 500, 450, 400));
        }
        score = std::max(score, fieldScore(token, fields.path, 400, 350, 300));
        score = std::max(score, fieldScore(token, fields.description, 150, 125, 100));
        if (score == 0) {
            return -1;
        }
        total += score;
    }
    return total;
}

QStringList normalizedList(const QStringList& values) {
    QStringList result;
    result.reserve(values.size());
    for (const QString& value : values) {
        result.push_back(normalized(value));
    }
    return result;
}

QStringList translatedAliases(const QVector<TranslatableText>& aliases) {
    QStringList result;
    result.reserve(aliases.size());
    for (const TranslatableText& alias : aliases) {
        if (alias.isValid()) {
            result.push_back(alias.translated());
        }
    }
    return result;
}

QStringList selectOptionLabels(const SettingsSelectDefinition& select) {
    QStringList result;
    result.reserve(select.options.size());
    for (const SettingsOptionDefinition& option : select.options) {
        result.push_back(option.label.translated());
    }
    if (select.source == SettingsSelectSource::LanguageCatalog) {
        for (const LanguageCatalog& catalog : LanguageManager::instance().availableLanguages()) {
            result.push_back(catalog.nativeName);
        }
    }
    return result;
}

QStringList radioOptionLabels(const SettingsRadioDefinition& radio) {
    QStringList result;
    result.reserve(radio.options.size());
    for (const SettingsRadioOptionDefinition& option : radio.options) {
        result.push_back(option.label.translated());
    }
    return result;
}
} // namespace

SettingsSearchIndex::SettingsSearchIndex(const SettingsCatalog& catalog) : m_catalog(catalog) {
    rebuild();
}

void SettingsSearchIndex::rebuild() {
    m_entries.clear();
    m_normalizedEntries.clear();
    int order = 0;
    const QString pages = QCoreApplication::translate("SettingsCatalog", PAGES_SOURCE);
    for (const SettingsPageDefinition& page : m_catalog.pages()) {
        m_entries.push_back({
            QStringLiteral("page:%1").arg(page.id),
            SettingsSearchNodeKind::Page,
            {page.id, {}, {}},
            page.title.translated(),
            page.description.translated(),
            pages,
            {},
            {},
            order++,
        });

        for (const SettingsSectionDefinition& section : page.sections) {
            m_entries.push_back({
                QStringLiteral("section:%1/%2").arg(page.id, section.id),
                SettingsSearchNodeKind::Section,
                {page.id, section.id, {}},
                section.title.translated(),
                section.searchDescription.translated(),
                page.title.translated(),
                {},
                {},
                order++,
            });

            const QString itemPath = QStringLiteral("%1 / %2")
                                         .arg(page.title.translated(),
                                              section.title.translated());
            for (const SettingsItemDefinition& item : section.items) {
                QStringList optionLabels;
                if (const auto* select = std::get_if<SettingsSelectDefinition>(&item.payload)) {
                    optionLabels = selectOptionLabels(*select);
                } else if (const auto* radio =
                               std::get_if<SettingsRadioDefinition>(&item.payload)) {
                    optionLabels = radioOptionLabels(*radio);
                }
                m_entries.push_back({
                    QStringLiteral("item:%1").arg(item.id),
                    SettingsSearchNodeKind::Item,
                    {page.id, section.id, item.id},
                    item.title.translated(),
                    item.description.translated(),
                    itemPath,
                    translatedAliases(item.aliases),
                    optionLabels,
                    order++,
                });
            }
        }
    }
    m_normalizedEntries.reserve(m_entries.size());
    for (const SettingsSearchEntry& entry : m_entries) {
        m_normalizedEntries.push_back({
            normalized(entry.title),
            normalized(entry.description),
            normalized(entry.path),
            normalizedList(entry.aliases),
            normalizedList(entry.optionLabels),
        });
    }
}

const QVector<SettingsSearchEntry>& SettingsSearchIndex::entries() const {
    return m_entries;
}

QVector<SettingsSearchEntry> SettingsSearchIndex::search(const QString& query) const {
    const QString normalizedQuery = normalized(query);
    const QStringList queryTokens = tokens(query);
    if (queryTokens.isEmpty()) {
        return m_entries;
    }

    struct RankedEntry {
        SettingsSearchEntry entry;
        int score = 0;
    };
    QVector<RankedEntry> ranked;
    for (qsizetype index = 0; index < m_entries.size(); ++index) {
        const SettingsSearchEntry& entry = m_entries.at(index);
        const int score = entryScore(m_normalizedEntries.at(index), queryTokens,
                                     normalizedQuery);
        if (score >= 0) {
            ranked.push_back({entry, score});
        }
    }
    std::sort(ranked.begin(), ranked.end(), [](const RankedEntry& first,
                                               const RankedEntry& second) {
        if (first.score != second.score) {
            return first.score > second.score;
        }
        if (first.entry.catalogOrder != second.entry.catalogOrder) {
            return first.entry.catalogOrder < second.entry.catalogOrder;
        }
        return first.entry.id < second.entry.id;
    });

    QVector<SettingsSearchEntry> result;
    result.reserve(ranked.size());
    for (const RankedEntry& entry : ranked) {
        result.push_back(entry.entry);
    }
    return result;
}

} // namespace snow_shot::presentation::settings
