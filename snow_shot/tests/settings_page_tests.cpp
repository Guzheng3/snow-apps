#include "snow_shot/presentation/components/applicationsearchwidget.h"
#include "snow_shot/presentation/components/contentcardwidget.h"
#include "snow_shot/presentation/components/drawingtoolbareditorsettingswidget.h"
#include "snow_shot/presentation/components/infotooltipicon.h"
#include "snow_shot/presentation/components/maincontentheaderwidget.h"
#include "snow_shot/presentation/components/pagecontainerwidget.h"
#include "snow_shot/presentation/components/sectionheaderwidget.h"
#include "snow_shot/presentation/components/screenshothistorypagewidget.h"
#include "snow_shot/presentation/components/settingspagewidget.h"
#include "snow_shot/presentation/components/shortcutkeyrow.h"
#include "snow_shot/presentation/components/sidebarwidget.h"
#include "snow_shot/presentation/components/storagestatussettingswidget.h"
#include "snow_shot/presentation/settings/settingsruntimebindings.h"
#include "snow_shot/presentation/screenshottoolbarlayoutmodel.h"
#include "snow_shot/presentation/styles/thememanager.h"
#include "snow_shot/storage/applicationstorage.h"
#include "snow_shot/storage/capturehistoryrepository.h"

#include "antd_icons.h"
#include "widgets/button.h"
#include "widgets/carousel.h"
#include "widgets/date_picker.h"
#include "widgets/descriptions.h"
#include "widgets/input_number.h"
#include "widgets/modal.h"
#include "widgets/multi_select.h"
#include "widgets/navigation_menu.h"
#include "widgets/pagination.h"
#include "widgets/popconfirm.h"
#include "widgets/radio.h"
#include "widgets/scroll_area.h"
#include "widgets/select.h"
#include "widgets/switch.h"
#include "widgets/tabs.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QImage>
#include <QMimeData>
#include <QScrollBar>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QTranslator>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QUuid>

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace settings = snow_shot::presentation::settings;

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void flushEvents() {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::PolishRequest);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QCoreApplication::processEvents();
}

class FakeRuntimeBindings final : public settings::SettingsRuntimeBindings {
  public:
    FakeRuntimeBindings() {
        m_storageStatus.writeAvailable = true;
        m_storageStatus.effectiveMode = snow_shot::storage::StorageMode::ApplicationData;
        m_storageStatus.effectiveDirectory = QStringLiteral("C:/settings-test-storage");
        m_storageStatus.historyUsage.entryCount = 2;
        m_storageStatus.historyUsage.totalBytes = 2048;
        m_shortcutStates.insert(snow_shot::presentation::GlobalShortcutAction::Screenshot,
                                {snow_shot::presentation::GlobalShortcutAction::Screenshot,
                                 {QStringLiteral("Ctrl+Shift+S")},
                                 snow_shot::presentation::GlobalShortcutStatus::Registered,
                                 {}});
        m_shortcutStates.insert(snow_shot::presentation::GlobalShortcutAction::OpenSettings,
                                {snow_shot::presentation::GlobalShortcutAction::OpenSettings,
                                 {},
                                 snow_shot::presentation::GlobalShortcutStatus::Unset,
                                 {}});
        m_selectValues = {
            {settings::SettingsSelectBinding::ScreenshotOcrAction,
             QStringLiteral("no_action")},
            {settings::SettingsSelectBinding::ScreenshotDoubleClickAction,
             QStringLiteral("copy")},
            {settings::SettingsSelectBinding::ScreenshotMiddleClickAction,
             QStringLiteral("pin")},
            {settings::SettingsSelectBinding::PinMouseWheelZoomMode,
             QStringLiteral("mouse_position")},
            {settings::SettingsSelectBinding::VideoClarity, QStringLiteral("1080p")},
            {settings::SettingsSelectBinding::VideoFrameRate, 30},
            {settings::SettingsSelectBinding::AnimatedImageClarity,
             QStringLiteral("1080p")},
            {settings::SettingsSelectBinding::AnimatedImageFrameRate, 10},
            {settings::SettingsSelectBinding::AnimatedImageFormat, QStringLiteral("gif")},
            {settings::SettingsSelectBinding::VideoEncoder, QStringLiteral("h264")},
            {settings::SettingsSelectBinding::VideoEncodingPreset,
             QStringLiteral("veryfast")},
            {settings::SettingsSelectBinding::TrayLeftClickAction,
             QStringLiteral("screenshot")},
        };
        m_switchValues = {
            {settings::SettingsSwitchBinding::ScreenshotAutoSaveAfterCopy, false},
            {settings::SettingsSwitchBinding::ScreenshotCopyImageFileToClipboard, false},
            {settings::SettingsSwitchBinding::PinAutomaticTextRecognition, true},
            {settings::SettingsSwitchBinding::PinAutoResizeWindow, true},
            {settings::SettingsSwitchBinding::VideoHideToolbarInRecording, true},
            {settings::SettingsSwitchBinding::DisableHotkeysOnFocusedFullscreen, false},
            {settings::SettingsSwitchBinding::AutoStartAtBoot, false},
        };
        m_multiSelectValues.insert(
            settings::SettingsMultiSelectBinding::DrawingQuickSelectionDisabledTools,
            {QStringLiteral("free-draw"), QStringLiteral("pen-filter")});
        m_localShortcuts = {
            {QStringLiteral("shape"), {QStringLiteral("1"), QStringLiteral("S")}},
            {QStringLiteral("arrow"), {QStringLiteral("2"), QStringLiteral("A")}},
            {QStringLiteral("brush"), {QStringLiteral("3"), QStringLiteral("P")}},
            {QStringLiteral("highlight"), {QStringLiteral("4"), QStringLiteral("H")}},
            {QStringLiteral("text"), {QStringLiteral("5"), QStringLiteral("T")}},
            {QStringLiteral("serial_number"), {QStringLiteral("6"), QStringLiteral("N")}},
            {QStringLiteral("filter"), {QStringLiteral("7"), QStringLiteral("F")}},
            {QStringLiteral("eraser"), {QStringLiteral("8"), QStringLiteral("E")}},
            {QStringLiteral("watermark"), {QStringLiteral("9"), QStringLiteral("W")}},
        };
    }

    QVariant selectValue(settings::SettingsSelectBinding binding) const override {
        switch (binding) {
        case settings::SettingsSelectBinding::Theme:
            return m_theme;
        case settings::SettingsSelectBinding::Language:
            return m_language;
        case settings::SettingsSelectBinding::ApplicationPriority:
            return m_applicationPriority;
        case settings::SettingsSelectBinding::ScreenshotToolbarSize:
            return m_toolbarSize;
        case settings::SettingsSelectBinding::ColorPickerDisplayMode:
            return m_colorPickerDisplayMode;
        default:
            return m_selectValues.value(binding);
        }
        return {};
    }

    QVector<settings::SettingsRuntimeOption>
    dynamicSelectOptions(settings::SettingsSelectBinding binding) const override {
        if (binding != settings::SettingsSelectBinding::Language) {
            return {};
        }
        return {{QStringLiteral("en_US"), QStringLiteral("English")}};
    }

    bool applySelectValue(settings::SettingsSelectBinding binding, const QVariant& value) override {
        if (!acceptWrites) {
            return false;
        }
        if (binding == settings::SettingsSelectBinding::Theme) {
            m_theme = value.toString();
        } else if (binding == settings::SettingsSelectBinding::Language) {
            m_language = value.toString();
        } else if (binding == settings::SettingsSelectBinding::ApplicationPriority) {
            m_applicationPriority = value.toString();
        } else if (binding == settings::SettingsSelectBinding::ScreenshotToolbarSize) {
            m_toolbarSize = value.toString();
        } else if (binding == settings::SettingsSelectBinding::ColorPickerDisplayMode) {
            m_colorPickerDisplayMode = value.toString();
        } else {
            m_selectValues.insert(binding, value);
        }
        emit synchronized();
        return true;
    }

    bool switchValue(settings::SettingsSwitchBinding binding) const override {
        switch (binding) {
        case settings::SettingsSwitchBinding::HistoryEnabled:
            return m_historyEnabled;
        case settings::SettingsSwitchBinding::SmartSelection:
            return m_smartSelection;
        case settings::SettingsSwitchBinding::DirectMlAcceleration:
            return m_directMlAcceleration;
        case settings::SettingsSwitchBinding::SelectionTransitionAnimation:
            return m_selectionTransitionAnimation;
        case settings::SettingsSwitchBinding::TrayEnabled:
            return m_trayEnabled;
        default:
            return m_switchValues.value(binding, false);
        }
    }

    bool switchEnabled(settings::SettingsSwitchBinding binding) const override {
        return binding != settings::SettingsSwitchBinding::DirectMlAcceleration ||
               directMlSupported;
    }

    bool applySwitchValue(settings::SettingsSwitchBinding binding, bool value) override {
        if (!acceptWrites) {
            return false;
        }
        ++switchApplyCount;
        if (binding == settings::SettingsSwitchBinding::SmartSelection) {
            m_smartSelection = value;
        } else if (binding == settings::SettingsSwitchBinding::DirectMlAcceleration) {
            m_directMlAcceleration = value;
        } else if (binding == settings::SettingsSwitchBinding::SelectionTransitionAnimation) {
            m_selectionTransitionAnimation = value;
        } else if (binding == settings::SettingsSwitchBinding::TrayEnabled) {
            m_trayEnabled = value;
        } else if (binding == settings::SettingsSwitchBinding::HistoryEnabled) {
            m_historyEnabled = value;
        } else {
            m_switchValues.insert(binding, value);
        }
        emit synchronized();
        return true;
    }

    QVariantList multiSelectValue(settings::SettingsMultiSelectBinding binding) const override {
        return m_multiSelectValues.value(binding);
    }

    bool applyMultiSelectValue(settings::SettingsMultiSelectBinding binding,
                               const QVariantList& value) override {
        if (!acceptWrites) {
            return false;
        }
        m_multiSelectValues.insert(binding, value);
        emit synchronized();
        return true;
    }

    int integerValue(settings::SettingsIntegerBinding binding) const override {
        switch (binding) {
        case settings::SettingsIntegerBinding::HistoryRetentionDays:
            return m_retentionDays;
        case settings::SettingsIntegerBinding::HistoryMaxEntries:
            return m_maxEntries;
        case settings::SettingsIntegerBinding::HistoryMaxDiskMiB:
            return m_maxDiskMiB;
        case settings::SettingsIntegerBinding::ScreenshotDelaySeconds:
            return m_screenshotDelaySeconds;
        }
        return 0;
    }

    bool applyIntegerValue(settings::SettingsIntegerBinding binding, int value) override {
        if (!acceptWrites) {
            return false;
        }
        switch (binding) {
        case settings::SettingsIntegerBinding::HistoryRetentionDays:
            m_retentionDays = value;
            break;
        case settings::SettingsIntegerBinding::HistoryMaxEntries:
            m_maxEntries = value;
            break;
        case settings::SettingsIntegerBinding::HistoryMaxDiskMiB:
            m_maxDiskMiB = value;
            break;
        case settings::SettingsIntegerBinding::ScreenshotDelaySeconds:
            m_screenshotDelaySeconds = value;
            break;
        }
        emit synchronized();
        return true;
    }

    int sliderValue(settings::SettingsSliderBinding) const override {
        return m_shortcutHintOpacity;
    }

    bool applySliderValue(settings::SettingsSliderBinding, int value) override {
        if (!acceptWrites) {
            return false;
        }
        m_shortcutHintOpacity = value;
        emit synchronized();
        return true;
    }

    QColor colorValue(settings::SettingsColorBinding binding) const override {
        return m_colors.value(binding, QColor(QStringLiteral("#1677FFFF")));
    }

    bool applyColorValue(settings::SettingsColorBinding binding, const QColor& value) override {
        if (!acceptWrites) {
            return false;
        }
        m_colors.insert(binding, value);
        emit synchronized();
        return true;
    }

    QVariant radioValue(settings::SettingsRadioBinding) const override {
        return m_trayIcon;
    }

    bool applyRadioValue(settings::SettingsRadioBinding, const QVariant& value) override {
        if (!acceptWrites) {
            return false;
        }
        m_trayIcon = value.toString();
        emit synchronized();
        return true;
    }

    QString filePathValue(settings::SettingsFilePathBinding) const override {
        return m_trayCustomIcon;
    }

    bool applyFilePathValue(settings::SettingsFilePathBinding, const QString& value) override {
        if (!acceptWrites) {
            return false;
        }
        m_trayCustomIcon = value;
        emit synchronized();
        return true;
    }

    snow_shot::storage::ScreenshotToolbarLayout toolbarLayout() const override {
        return m_toolbarLayout;
    }

    bool applyToolbarLayout(const snow_shot::storage::ScreenshotToolbarLayout& layout) override {
        if (!acceptWrites) {
            return false;
        }
        m_toolbarLayout = snow_shot::presentation::toolbar_layout::normalizedLayout(layout);
        ++toolbarLayoutApplyCount;
        emit synchronized();
        return true;
    }

    snow_shot::presentation::GlobalShortcutRegistrationState
    shortcutState(snow_shot::presentation::GlobalShortcutAction action) const override {
        return m_shortcutStates.value(action);
    }

    snow_shot::presentation::GlobalShortcutValidationResult
    validateShortcut(const QString& shortcut) const override {
        return {shortcut, true, snow_shot::presentation::GlobalShortcutFailureReason::None};
    }

    bool applyShortcuts(snow_shot::presentation::GlobalShortcutAction action,
                        const QStringList& shortcuts) override {
        if (!acceptWrites) {
            return false;
        }
        auto state = m_shortcutStates.value(action);
        state.shortcuts = shortcuts;
        m_shortcutStates.insert(action, state);
        emit shortcutStateChanged(action, state);
        return true;
    }

    QStringList localShortcuts(const QString& toolId) const override {
        return m_localShortcuts.value(toolId);
    }

    snow_shot::presentation::GlobalShortcutValidationResult
    validateLocalShortcut(const QString&, const QString& shortcut) const override {
        return {shortcut, true, snow_shot::presentation::GlobalShortcutFailureReason::None};
    }

    bool applyLocalShortcuts(const QString& toolId, const QStringList& shortcuts) override {
        if (!acceptWrites) {
            return false;
        }
        m_localShortcuts.insert(toolId, shortcuts);
        emit synchronized();
        return true;
    }

    settings::SettingsActionState actionState(settings::SettingsActionBinding) const override {
        return {m_storageStatus.writeAvailable && !m_storageStatus.historyClearing &&
                    m_storageStatus.historyUsage.entryCount > 0,
                m_storageStatus.historyClearing};
    }

    bool triggerAction(settings::SettingsActionBinding binding) override {
        triggeredAction = binding;
        actionTriggered = acceptWrites;
        return acceptWrites;
    }

    snow_shot::storage::StorageStatus storageStatus() const override {
        return m_storageStatus;
    }

    bool resetSection(settings::SettingsSectionReset reset) override {
        resetRequested = reset;
        if (!acceptWrites) {
            return false;
        }
        if (reset == settings::SettingsSectionReset::HistoryPolicy) {
            m_historyEnabled = true;
            m_retentionDays = 7;
            m_maxEntries = 100;
            m_maxDiskMiB = 1024;
        } else if (reset == settings::SettingsSectionReset::ScreenshotSettings) {
            m_smartSelection = true;
        }
        emit synchronized();
        return true;
    }

    void setStorageState(bool writeAvailable, bool historyClearing) {
        m_storageStatus.writeAvailable = writeAvailable;
        m_storageStatus.historyClearing = historyClearing;
        emit synchronized();
    }

    bool acceptWrites = true;
    bool directMlSupported = true;
    int switchApplyCount = 0;
    int toolbarLayoutApplyCount = 0;
    bool actionTriggered = false;
    settings::SettingsActionBinding triggeredAction =
        settings::SettingsActionBinding::ClearCaptureHistory;
    settings::SettingsSectionReset resetRequested = settings::SettingsSectionReset::None;

  private:
    QString m_theme = QStringLiteral("system");
    QString m_language = QStringLiteral("en_US");
    QString m_applicationPriority = QStringLiteral("above_normal");
    QString m_toolbarSize = QStringLiteral("normal");
    QString m_colorPickerDisplayMode = QStringLiteral("hex");
    QHash<settings::SettingsSelectBinding, QVariant> m_selectValues;
    QHash<settings::SettingsMultiSelectBinding, QVariantList> m_multiSelectValues;
    QHash<settings::SettingsSwitchBinding, bool> m_switchValues;
    bool m_historyEnabled = true;
    bool m_smartSelection = true;
    bool m_directMlAcceleration = true;
    bool m_selectionTransitionAnimation = true;
    bool m_trayEnabled = true;
    int m_retentionDays = 7;
    int m_maxEntries = 100;
    int m_maxDiskMiB = 1024;
    int m_screenshotDelaySeconds = 3;
    int m_shortcutHintOpacity = 80;
    QHash<settings::SettingsColorBinding, QColor> m_colors;
    QString m_trayIcon = QStringLiteral("default");
    QString m_trayCustomIcon;
    snow_shot::storage::ScreenshotToolbarLayout m_toolbarLayout{
        snow_shot::presentation::toolbar_layout::defaultPositions()};
    snow_shot::storage::StorageStatus m_storageStatus;
    QHash<snow_shot::presentation::GlobalShortcutAction,
          snow_shot::presentation::GlobalShortcutRegistrationState>
        m_shortcutStates;
    QHash<QString, QStringList> m_localShortcuts;
};

class PageTranslator final : public QTranslator {
  public:
    QString translate(const char* context, const char* sourceText, const char*,
                      int) const override {
        if (QString::fromLatin1(context) == QStringLiteral("SettingsCatalog") &&
            QString::fromUtf8(sourceText) == QStringLiteral("Theme")) {
            return QStringLiteral("Localized Theme");
        }
        if (QString::fromLatin1(context) == QStringLiteral("SettingsCatalog") &&
            QString::fromUtf8(sourceText) == QStringLiteral("Follow System")) {
            return QStringLiteral("Localized System Theme");
        }
        return {};
    }
};

settings::TranslatableText text(const char* source) {
    return {"SettingsPageTests", source};
}

snow_shot::storage::CaptureHistoryDraft
historyDraft(const QDateTime& createdUtc, snow_shot::storage::CaptureHistorySource source) {
    snow_shot::storage::CaptureHistoryDraft draft;
    draft.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    draft.createdUtc = createdUtc;
    draft.canvasBounds = QRect(0, 0, 96, 64);
    draft.selection.rectangle = QRect(8, 10, 72, 42);
    draft.selection.cornerRadius = 4;
    draft.selection.shadowWidth = 2;
    draft.selection.shadowColor = QColor(0, 0, 0, 96);
    draft.canvasHistory = QByteArrayLiteral("{\"schemaVersion\":1,\"document\":{},\"history\":{}}");
    draft.source = source;
    QImage image(96, 64, QImage::Format_RGBA8888);
    image.fill(source == snow_shot::storage::CaptureHistorySource::PinnedToScreen
                   ? QColor(QStringLiteral("#52C41A"))
                   : QColor(QStringLiteral("#1677FF")));
    draft.displays.push_back(
        {QStringLiteral("display-1"), QStringLiteral("Primary display"), image});
    return draft;
}

class FakeHistoryDataSource final : public ScreenshotHistoryPageDataSource {
  public:
    using ScreenshotHistoryPageDataSource::ScreenshotHistoryPageDataSource;

    QVector<snow_shot::storage::CaptureHistoryRecord> records() const override {
        ++recordsCalls;
        return currentRecords;
    }

    std::optional<snow_shot::storage::CaptureHistoryAssetSet>
    displayAssets(const snow_shot::storage::CaptureHistoryRecord& record) const override {
        ++assetCalls;
        return assets.value(record.id);
    }

    void remove(const QString& id) override {
        removedIds.push_back(id);
    }

    bool requestClear() override {
        ++clearCalls;
        return true;
    }

    void notifyChanged() {
        emit historyChanged();
    }

    mutable int recordsCalls = 0;
    mutable int assetCalls = 0;
    int clearCalls = 0;
    QVector<QString> removedIds;
    QVector<snow_shot::storage::CaptureHistoryRecord> currentRecords;
    QHash<QString, snow_shot::storage::CaptureHistoryAssetSet> assets;
};

void screenshotHistoryLifecycleAndIdentityDiff() {
    using namespace snow_shot::storage;
    CaptureHistoryRecord record;
    record.id = QStringLiteral("history-row-1");
    record.createdUtc = QDateTime::currentDateTimeUtc();
    record.canvasBounds = QRect(0, 0, 100, 80);
    record.selection.rectangle = QRect(5, 6, 70, 50);
    record.displays.push_back(
        {QStringLiteral("display-1"), QStringLiteral("Primary"), QSize(100, 80), 128});
    record.totalBytes = 256;

    CaptureHistoryAssetSet assetSet;
    assetSet.recordId = record.id;
    assetSet.displays.push_back({record.id, QStringLiteral("display-1"), QStringLiteral("Primary"),
                                 QSize(100, 80),
                                 QUrl::fromLocalFile(QStringLiteral("C:/missing-history.png"))});

    FakeHistoryDataSource source;
    source.currentRecords = {record};
    source.assets.insert(record.id, assetSet);
    ScreenshotHistoryPageWidget page(&source, nullptr);
    page.resize(760, 520);
    require(source.recordsCalls == 0 && source.assetCalls == 0,
            "history construction queried its data source");
    source.notifyChanged();
    flushEvents();
    require(source.recordsCalls == 0 && source.assetCalls == 0,
            "inactive history changes triggered reconciliation");

    page.show();
    flushEvents();
    require(source.recordsCalls == 1 && source.assetCalls == 1 && page.totalCount() == 1,
            "first history activation did not reconcile metadata once");
    QWidget* initialRow =
        page.findChild<QWidget*>(QStringLiteral("screenshotHistoryEntry-history-row-1"));
    require(initialRow != nullptr, "history activation did not create the visible row");
    auto* editButton = initialRow->findChild<adqt::widgets::AdButton*>(
        QStringLiteral("screenshotHistoryEntryEdit"));
    auto* deleteButton = initialRow->findChild<adqt::widgets::AdButton*>(
        QStringLiteral("screenshotHistoryEntryDelete"));
    QString editedRecordId;
    QObject::connect(&page, &ScreenshotHistoryPageWidget::editRequested, &page,
                     [&editedRecordId](const QString& recordId) { editedRecordId = recordId; });
    require(editButton != nullptr && deleteButton != nullptr &&
                editButton->buttonStyle() == adqt::widgets::AdButton::ButtonStyle::Text &&
                editButton->accentRole() == adqt::widgets::AdButton::AccentRole::Primary &&
                adqt::icons::describeIcon(editButton->iconRef()).key.name ==
                    QStringLiteral("edit") &&
                editButton->mapTo(initialRow, QPoint()).x() <
                    deleteButton->mapTo(initialRow, QPoint()).x(),
            "history Edit must be a primary text action placed before Delete");
    editButton->click();
    require(editedRecordId == record.id, "history Edit did not emit the selected record ID");

    source.notifyChanged();
    source.notifyChanged();
    flushEvents();
    QWidget* retainedRow =
        page.findChild<QWidget*>(QStringLiteral("screenshotHistoryEntry-history-row-1"));
    require(source.recordsCalls == 2 && source.assetCalls == 1 && retainedRow == initialRow,
            "active changes were not coalesced, revalidated assets, or recreated an unchanged row");

    page.hide();
    flushEvents();
    source.notifyChanged();
    flushEvents();
    require(source.recordsCalls == 2, "hidden history changes performed a metadata reconciliation");
    page.show();
    flushEvents();
    require(source.recordsCalls == 3, "dirty history metadata was not refreshed on reactivation");
}

void screenshotHistoryEmptyToPopulatedGeometryIsStable() {
    using namespace snow_shot::storage;
    CaptureHistoryRecord record;
    record.id = QStringLiteral("geometry-row-1");
    record.createdUtc = QDateTime::currentDateTimeUtc();
    record.canvasBounds = QRect(0, 0, 100, 80);
    record.selection.rectangle = QRect(5, 6, 70, 50);
    record.displays.push_back(
        {QStringLiteral("display-1"), QStringLiteral("Primary"), QSize(100, 80), 128});
    record.totalBytes = 256;

    FakeHistoryDataSource source;
    ScreenshotHistoryPageWidget page(&source, nullptr);
    page.resize(760, 520);
    page.show();
    flushEvents();
    auto* container =
        page.findChild<PageContainerWidget*>(QStringLiteral("screenshotHistoryPageContainer"));
    auto* content = container != nullptr ? container->contentWidget() : nullptr;
    auto* entries = page.findChild<QWidget*>(QStringLiteral("screenshotHistoryEntries"));
    auto* scroll = container != nullptr ? container->scrollArea() : nullptr;
    require(container != nullptr && content != nullptr && entries != nullptr && scroll != nullptr,
            "history geometry test could not find its shared page container");
    const int emptyContentHeight = content->height();
    const int emptyEntriesHeight = entries->height();
    source.currentRecords = {record};
    source.notifyChanged();
    flushEvents();
    require(content->height() == emptyContentHeight && entries->height() == emptyEntriesHeight &&
                page.findChild<QWidget*>(QStringLiteral("screenshotHistoryEntry-geometry-row-1")) !=
                    nullptr,
            "history empty-to-populated transition must preserve its layout footprint");
    source.currentRecords.clear();
    source.notifyChanged();
    flushEvents();
    require(content->height() == emptyContentHeight && entries->height() == emptyEntriesHeight &&
                page.findChild<QWidget*>(QStringLiteral("screenshotHistoryEntry-geometry-row-1")) ==
                    nullptr,
            "history populated-to-empty transition must preserve its layout footprint");

    page.resize(520, 520);
    flushEvents();
    const int narrowEmptyContentHeight = content->height();
    const int narrowEmptyEntriesHeight = entries->height();
    source.currentRecords = {record};
    source.notifyChanged();
    flushEvents();
    require(content->height() == narrowEmptyContentHeight &&
                entries->height() == narrowEmptyEntriesHeight,
            "history narrow empty-to-populated transition must preserve its layout footprint");

    FakeHistoryDataSource initiallyPopulatedSource;
    initiallyPopulatedSource.currentRecords = {record};
    ScreenshotHistoryPageWidget initiallyPopulatedPage(&initiallyPopulatedSource, nullptr);
    initiallyPopulatedPage.resize(760, 520);
    initiallyPopulatedPage.show();
    flushEvents();
    auto* initiallyPopulatedContainer = initiallyPopulatedPage.findChild<PageContainerWidget*>(
        QStringLiteral("screenshotHistoryPageContainer"));
    auto* initiallyPopulatedContent = initiallyPopulatedContainer != nullptr
                                          ? initiallyPopulatedContainer->contentWidget()
                                          : nullptr;
    auto* initiallyPopulatedEntries =
        initiallyPopulatedPage.findChild<QWidget*>(QStringLiteral("screenshotHistoryEntries"));
    require(initiallyPopulatedContent != nullptr && initiallyPopulatedEntries != nullptr,
            "initially populated history geometry test could not find its content");
    const int initiallyPopulatedContentHeight = initiallyPopulatedContent->height();
    const int initiallyPopulatedEntriesHeight = initiallyPopulatedEntries->height();
    initiallyPopulatedSource.currentRecords.clear();
    initiallyPopulatedSource.notifyChanged();
    flushEvents();
    require(initiallyPopulatedContent->height() == initiallyPopulatedContentHeight &&
                initiallyPopulatedEntries->height() == initiallyPopulatedEntriesHeight,
            "history initially populated-to-empty transition must preserve its layout footprint");
}

void screenshotHistoryPageUsesRepositoryAndAntDesignComponents() {
    auto& repository = snow_shot::storage::ApplicationStorage::instance().captureHistory();
    require(repository.requestClear().get().success, "history test setup must clear storage");
    const QDateTime today = QDateTime::currentDateTimeUtc();
    require(
        repository
                .publish(historyDraft(today.addSecs(-1),
                                      snow_shot::storage::CaptureHistorySource::CopiedToClipboard))
                .get()
                .storage.success &&
            repository
                .publish(
                    historyDraft(today, snow_shot::storage::CaptureHistorySource::PinnedToScreen))
                .get()
                .storage.success,
        "history fixtures must publish");

    ScreenshotHistoryPageWidget page;
    page.resize(760, 520);
    page.show();
    flushEvents();

    auto* sourceFilter =
        page.findChild<adqt::widgets::AdSelect*>(QStringLiteral("screenshotHistorySourceFilter"));
    auto* dateRangeFilter = page.findChild<adqt::widgets::AdDateRangePicker*>(
        QStringLiteral("screenshotHistoryDateRangeFilter"));
    auto* pagination =
        page.findChild<adqt::widgets::AdPagination*>(QStringLiteral("screenshotHistoryPagination"));
    auto* deleteAll =
        page.findChild<adqt::widgets::AdButton*>(QStringLiteral("screenshotHistoryDeleteAll"));
    auto* refresh =
        page.findChild<adqt::widgets::AdButton*>(QStringLiteral("screenshotHistoryRefresh"));
    auto* title = page.findChild<QLabel*>(QStringLiteral("screenshotHistoryTitle"));
    auto* countLabel = page.findChild<QLabel*>(QStringLiteral("screenshotHistoryCountLabel"));
    auto* filters = page.findChild<QWidget*>(QStringLiteral("screenshotHistoryFilters"));
    auto* historyContainer =
        page.findChild<PageContainerWidget*>(QStringLiteral("screenshotHistoryPageContainer"));
    auto* confirmation = page.findChild<adqt::widgets::AdPopconfirm*>(
        QStringLiteral("screenshotHistoryDeleteAllConfirm"));
    auto* displayDescription =
        page.findChild<QLabel*>(QStringLiteral("screenshotHistoryDisplayName"));
    bool carouselSlidesAvailable = true;
    const auto carousels = page.findChildren<adqt::widgets::AdCarousel*>();
    for (adqt::widgets::AdCarousel* carousel : carousels) {
        carouselSlidesAvailable = carouselSlidesAvailable && carousel->count() == 1;
    }
    require(sourceFilter != nullptr && dateRangeFilter != nullptr && pagination != nullptr &&
                deleteAll != nullptr && refresh != nullptr && confirmation != nullptr &&
                title != nullptr && countLabel != nullptr && filters != nullptr,
            "history page must expose its filters, actions, labels, and pagination");
    require(page.totalCount() == 2 && page.filteredCount() == 2 &&
                sourceFilter->options().size() == 4 && sourceFilter->currentValues().isEmpty() &&
                pagination->total() == 2 && deleteAll->isEnabled() &&
                displayDescription == nullptr && historyContainer != nullptr &&
                title->parentWidget()->layout() != nullptr &&
                title->parentWidget()->layout()->spacing() ==
                    snow_shot::presentation::styles::ThemeManager::instance()
                        .themeColorScheme()
                        .metricAlias.marginXXS &&
                historyContainer->contentLayout()->itemAt(1)->spacerItem() != nullptr &&
                historyContainer->contentLayout()->itemAt(1)->spacerItem()->sizeHint().height() ==
                    snow_shot::presentation::styles::ThemeManager::instance()
                        .themeColorScheme()
                        .metricAlias.marginSM &&
                filters->mapTo(&page, QPoint()).y() >=
                    countLabel->mapTo(&page, QPoint()).y() + countLabel->height() &&
                carousels.size() == 2 && carouselSlidesAvailable,
            "history page must expose filters below its title plus management, carousels, and "
            "pagination");

    const auto metric =
        snow_shot::presentation::styles::ThemeManager::instance().themeColorScheme().metricAlias;
    require(title->font().pixelSize() == metric.fontSizeHeading4 &&
                title->font().weight() == QFont::DemiBold &&
                countLabel->font().pixelSize() == metric.fontSize,
            "history title typography must use Ant Design heading and body tokens");

    FakeRuntimeBindings shortcutBindings;
    SettingsPageWidget shortcutPage(settings::builtInSettingsCatalog(),
                                    QStringLiteral("quick-functions"), shortcutBindings);
    auto* resetButton =
        shortcutPage.findChild<adqt::widgets::AdButton*>(QStringLiteral("sectionResetButton"));
    require(resetButton != nullptr && deleteAll->size() == resetButton->size() &&
                refresh->size() == resetButton->size() &&
                deleteAll->buttonStyle() == resetButton->buttonStyle() &&
                refresh->buttonStyle() == resetButton->buttonStyle() &&
                refresh->accentRole() == resetButton->accentRole() &&
                deleteAll->accentRole() == adqt::widgets::AdButton::AccentRole::Danger &&
                deleteAll->shape() == resetButton->shape() &&
                refresh->shape() == resetButton->shape(),
            "history actions must match the quick-functions reset button styling while keeping "
            "the delete danger color");

    page.resize(520, 520);
    flushEvents();
    require(dateRangeFilter->mapTo(filters, QPoint()).y() ==
                sourceFilter->mapTo(filters, QPoint()).y(),
            "history filters must remain tiled in one row at narrow widths");
    page.resize(760, 520);
    flushEvents();
    require(dateRangeFilter->mapTo(filters, QPoint()).y() ==
                sourceFilter->mapTo(filters, QPoint()).y(),
            "history filters must share a row inside their own region at wide widths");

    sourceFilter->setCurrentValues({QStringLiteral("pinned")});
    flushEvents();
    require(page.filteredCount() == 1 && pagination->total() == 1,
            "source filtering must rebuild the visible repository records");

    sourceFilter->setCurrentValues({});
    const QDate captureDate = today.toLocalTime().date();
    dateRangeFilter->setRange(captureDate.addDays(-1), captureDate);
    flushEvents();
    require(page.filteredCount() == 2,
            "date range filtering must include both local calendar date boundaries");

    dateRangeFilter->setRange(captureDate.addDays(1), captureDate.addDays(2));
    flushEvents();
    require(page.filteredCount() == 0,
            "date range filtering must exclude captures outside the selected local dates");

    dateRangeFilter->clear();
    flushEvents();
    require(page.filteredCount() == 2,
            "clearing the date range must restore all source-matching captures");

    require(repository.requestClear().get().success, "history test cleanup must clear storage");
}

void screenshotHistorySurvivesSidebarWidthTransitions() {
    auto& repository = snow_shot::storage::ApplicationStorage::instance().captureHistory();
    require(repository.requestClear().get().success,
            "history collapse test setup must clear storage");
    require(repository
                .publish(historyDraft(QDateTime::currentDateTimeUtc(),
                                      snow_shot::storage::CaptureHistorySource::CopiedToClipboard))
                .get()
                .storage.success,
            "history collapse test fixture must publish");

    const auto& builtInCatalog = settings::builtInSettingsCatalog();
    snow_shot::presentation::GlobalShortcutManager shortcutManager;
    QWidget window;
    auto* rootLayout = new QHBoxLayout(&window);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto* sidebar = new SidebarWidget(builtInCatalog, &window);
    rootLayout->addWidget(sidebar, 0);

    auto* contentShell = new QWidget(&window);
    auto* contentLayout = new QVBoxLayout(contentShell);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    auto* header = new MainContentHeaderWidget(
        builtInCatalog,
        snow_shot::presentation::styles::ThemeManager::instance().themeColorScheme().metricAlias,
        contentShell);
    auto* content = new ContentCardWidget(builtInCatalog, shortcutManager, contentShell);
    contentLayout->addWidget(header, 0);
    contentLayout->addWidget(content, 1);
    rootLayout->addWidget(contentShell, 1);

    content->setCurrentRoute(QStringLiteral("/history"));
    sidebar->setCurrentRoute(QStringLiteral("/history"));
    header->setSections(content->currentSections());
    window.resize(900, 556);
    window.show();
    flushEvents();

    auto* tabs = header->findChild<adqt::widgets::AdTabs*>(QStringLiteral("mainSectionTabs"));
    auto* historyPage =
        content->findChild<ScreenshotHistoryPageWidget*>(QStringLiteral("screenshotHistoryPage"));
    require(tabs != nullptr && tabs->count() == 0 && tabs->isHidden() && historyPage != nullptr &&
                historyPage->totalCount() == 1,
            "history route must remove its empty section tabs before responsive layout changes");

    sidebar->setCollapsed(true);
    flushEvents();
    require(sidebar->isCollapsed() && tabs->isHidden() && historyPage->totalCount() == 1,
            "collapsing the sidebar must keep populated screenshot history stable");

    sidebar->setCollapsed(false);
    flushEvents();
    require(!sidebar->isCollapsed() && tabs->isHidden() && historyPage->totalCount() == 1,
            "expanding the sidebar must keep populated screenshot history stable");

    window.hide();
    require(repository.requestClear().get().success,
            "history collapse test cleanup must clear storage");
}

void allPagesShareBaseContainerSpacingAndScrollbar() {
    FakeRuntimeBindings bindings;
    const auto& builtInCatalog = settings::builtInSettingsCatalog();
    const auto metric =
        snow_shot::presentation::styles::ThemeManager::instance().themeColorScheme().metricAlias;
    SettingsPageWidget settingsPage(builtInCatalog, QStringLiteral("interface-settings"), bindings);
    ScreenshotHistoryPageWidget historyPage;

    auto* settingsContainer = settingsPage.findChild<PageContainerWidget*>(
        QStringLiteral("settings-container-interface-settings"));
    auto* historyContainer = historyPage.findChild<PageContainerWidget*>(
        QStringLiteral("screenshotHistoryPageContainer"));
    require(settingsContainer != nullptr && historyContainer != nullptr,
            "every page must render inside the shared page container");

    const QMargins expectedMargins(metric.paddingLG, metric.paddingXXS, metric.paddingLG,
                                   metric.paddingLG);
    require(settingsContainer->contentLayout()->contentsMargins() == expectedMargins &&
                historyContainer->contentLayout()->contentsMargins() == expectedMargins,
            "all pages must use the same theme-driven base inner spacing");

    auto* settingsHeader = settingsPage.findChild<SectionHeaderWidget*>(
        QStringLiteral("settings-section-interface-settings-general"));
    QLayout* historyHeaderLayout = historyContainer->contentLayout()->itemAt(0)->layout();
    require(!historyPage.autoFillBackground() && settingsHeader != nullptr &&
                settingsHeader->layout() != nullptr && historyHeaderLayout != nullptr &&
                settingsHeader->layout()->contentsMargins() ==
                    historyHeaderLayout->contentsMargins(),
            "history must preserve the rounded card background and match the first-title inset");

    auto* settingsScrollArea = settingsContainer->scrollArea();
    auto* historyScrollArea = historyContainer->scrollArea();
    require(settingsScrollArea != nullptr && historyScrollArea != nullptr &&
                settingsScrollArea->scrollBarThickness() == metric.scrollbarThickness &&
                historyScrollArea->scrollBarThickness() == metric.scrollbarThickness &&
                settingsScrollArea->overlayVerticalScrollBar() != nullptr &&
                historyScrollArea->overlayVerticalScrollBar() != nullptr &&
                settingsScrollArea->overlayVerticalScrollBar()->overlayMargins() ==
                    historyScrollArea->overlayVerticalScrollBar()->overlayMargins(),
            "all pages must use the same themed overlay scrollbar");
}

settings::SettingsCatalog expandedCatalog() {
    const auto& builtIn = settings::builtInSettingsCatalog();
    QVector<settings::SettingsPageDefinition> pages = builtIn.pages();
    settings::SettingsSelectDefinition select;
    select.options = {
        {QStringLiteral("system"), text("Follow System")},
        {QStringLiteral("light"), text("Light")},
        {QStringLiteral("dark"), text("Dark")},
    };
    pages.push_back({QStringLiteral("extra-page"),
                     QStringLiteral("/extra"),
                     text("Extra Page"),
                     text("Extra page description"),
                     {{QStringLiteral("extra-section"),
                       text("Extra Section"),
                       text("Extra section description"),
                       settings::SettingsSectionReset::None,
                       {{QStringLiteral("extra.item"),
                         text("Extra Item"),
                         text("Extra item description"),
                         {},
                         QStringLiteral("interface/theme_mode"),
                         select}}}}});
    QVector<settings::SettingsNavigationNode> navigation = builtIn.navigation();
    navigation.push_back(settings::SettingsNavigationPageDefinition{
        QStringLiteral("nav.extra-page"), QStringLiteral("extra-page"),
        []() { return adqt::icons::antd::outlined::Appstore(); }});
    return {std::move(pages), std::move(navigation), builtIn.defaultLocation()};
}

void generatedPagesRenderEveryItemTypeAndResynchronize() {
    FakeRuntimeBindings bindings;
    const auto& catalog = settings::builtInSettingsCatalog();

    SettingsPageWidget quick(catalog, QStringLiteral("quick-functions"), bindings);
    SettingsPageWidget interfacePage(catalog, QStringLiteral("interface-settings"), bindings);
    SettingsPageWidget storagePage(catalog, QStringLiteral("storage-and-privacy"), bindings);
    SettingsPageWidget functionPage(catalog, QStringLiteral("function-settings"), bindings);
    SettingsPageWidget systemPage(catalog, QStringLiteral("system-settings"), bindings);
    SettingsPageWidget hotkeyPage(catalog, QStringLiteral("hotkey-settings"), bindings);
    interfacePage.resize(720, 360);
    quick.resize(720, 520);
    storagePage.resize(720, 480);
    functionPage.resize(720, 240);
    systemPage.resize(720, 240);
    hotkeyPage.resize(720, 360);
    interfacePage.show();
    quick.show();
    storagePage.show();
    functionPage.show();
    systemPage.show();
    hotkeyPage.show();
    flushEvents();

    auto* theme = interfacePage.findChild<adqt::widgets::AdSelect*>(
        QStringLiteral("settings-control-interface-theme"));
    auto* language = interfacePage.findChild<adqt::widgets::AdSelect*>(
        QStringLiteral("settings-control-interface-language"));
    require(theme != nullptr && language != nullptr && theme->options().size() == 3 &&
                language->options().size() == 2 &&
                theme->currentValue() == QStringLiteral("system") &&
                language->currentValue() == QStringLiteral("en_US"),
            "select renderers must use catalog options and binding values");
    require(!theme->accessibleName().isEmpty() && !theme->accessibleDescription().isEmpty() &&
                !language->accessibleName().isEmpty(),
            "generated controls must expose catalog accessibility metadata");

    auto* ocrAction = functionPage.findChild<adqt::widgets::AdSelect*>(
        QStringLiteral("settings-control-screenshot-auto-execute-after-text-recognition"));
    auto* doubleClickAction = functionPage.findChild<adqt::widgets::AdSelect*>(
        QStringLiteral("settings-control-screenshot-double-click-action"));
    auto* middleClickAction = functionPage.findChild<adqt::widgets::AdSelect*>(
        QStringLiteral("settings-control-screenshot-middle-mouse-button-action"));
    auto* videoClarity = functionPage.findChild<adqt::widgets::AdSelect*>(
        QStringLiteral("settings-control-video-recording-video-clarity"));
    auto* videoFrameRate = functionPage.findChild<adqt::widgets::AdSelect*>(
        QStringLiteral("settings-control-video-recording-frame-rate"));
    auto* animatedFormat = functionPage.findChild<adqt::widgets::AdSelect*>(
        QStringLiteral("settings-control-video-recording-animated-image-format"));
    auto* drawingExclusions = interfacePage.findChild<adqt::widgets::AdMultiSelect*>(
        QStringLiteral("settings-control-drawing-quick-selection-disabled-tools"));
    auto* pinZoomMode = interfacePage.findChild<adqt::widgets::AdSelect*>(
        QStringLiteral("settings-control-pin-to-screen-mouse-wheel-zoom-mode"));
    auto* trayLeftClick = interfacePage.findChild<adqt::widgets::AdSelect*>(
        QStringLiteral("settings-control-tray-left-click-action"));
    require(ocrAction != nullptr && ocrAction->options().size() == 6 &&
                doubleClickAction != nullptr && doubleClickAction->options().size() == 4 &&
                middleClickAction != nullptr && middleClickAction->options().size() == 4 &&
                videoClarity != nullptr && videoClarity->options().size() == 5 &&
                videoFrameRate != nullptr && videoFrameRate->options().size() == 7 &&
                animatedFormat != nullptr && animatedFormat->options().size() == 3 &&
                drawingExclusions != nullptr && drawingExclusions->options().size() == 13 &&
                pinZoomMode != nullptr && pinZoomMode->options().size() == 6 &&
                trayLeftClick != nullptr && trayLeftClick->options().size() == 2,
            "new select and multi-select controls must render every advertised option");

    ocrAction->setCurrentValue(QStringLiteral("copy_text"));
    videoFrameRate->setCurrentValue(83);
    pinZoomMode->setCurrentValue(QStringLiteral("top_right"));
    trayLeftClick->setCurrentValue(QStringLiteral("show_main_window"));
    drawingExclusions->setSelectedValues(
        {QStringLiteral("free-draw"), QStringLiteral("pen-filter")});
    require(bindings.selectValue(settings::SettingsSelectBinding::ScreenshotOcrAction) ==
                    QStringLiteral("copy_text") &&
                bindings.selectValue(settings::SettingsSelectBinding::VideoFrameRate).toInt() ==
                    83 &&
                bindings.selectValue(settings::SettingsSelectBinding::PinMouseWheelZoomMode) ==
                    QStringLiteral("top_right") &&
                bindings.selectValue(settings::SettingsSelectBinding::TrayLeftClickAction) ==
                    QStringLiteral("show_main_window") &&
                bindings.multiSelectValue(
                    settings::SettingsMultiSelectBinding::DrawingQuickSelectionDisabledTools) ==
                    QVariantList{QStringLiteral("free-draw"), QStringLiteral("pen-filter")},
            "new select and multi-select values must flow through runtime bindings");

    const auto drawingShortcutRows = hotkeyPage.findChildren<ShortcutKeyRow*>();
    require(drawingShortcutRows.size() == 9 &&
                std::all_of(drawingShortcutRows.cbegin(), drawingShortcutRows.cend(),
                            [](const ShortcutKeyRow* row) {
                                const auto* status =
                                    row != nullptr
                                        ? row->findChild<InfoTooltipIcon*>(QStringLiteral(
                                              "shortcutRegistrationStatusTooltipTrigger"))
                                        : nullptr;
                                return status != nullptr && status->isHidden();
                            }),
            "Hotkey Settings must render nine local drawing shortcut rows without global status");

    auto* generalList = interfacePage.findChild<QWidget*>(
        QStringLiteral("settings-section-list-interface-settings-general"));
    auto* screenshotActions = quick.findChild<QWidget*>(
        QStringLiteral("settings-section-list-quick-functions-screenshot"));
    auto* themeRow =
        interfacePage.findChild<QWidget*>(QStringLiteral("settings-item-interface-theme"));
    auto* languageRow =
        interfacePage.findChild<QWidget*>(QStringLiteral("settings-item-interface-language"));
    const auto settingMetrics =
        snow_shot::presentation::styles::ThemeManager::instance().themeColorScheme().metricAlias;
    const auto contentHeight = [](const QWidget* row) {
        int height = 0;
        if (row != nullptr && row->layout() != nullptr) {
            for (int index = 0; index < row->layout()->count(); ++index) {
                if (QWidget* child = row->layout()->itemAt(index)->widget(); child != nullptr) {
                    height = std::max(height, child->height());
                }
            }
        }
        return height;
    };
    require(generalList != nullptr && generalList->layout() != nullptr &&
                generalList->layout()->count() == 2 && screenshotActions != nullptr &&
                screenshotActions->layout() != nullptr &&
                screenshotActions->layout()->count() == 7 &&
                generalList->layout()->spacing() == settingMetrics.paddingLG &&
                screenshotActions->layout()->spacing() == settingMetrics.padding &&
                themeRow != nullptr && themeRow->layout() != nullptr &&
                themeRow->layout()->contentsMargins().top() == 0 &&
                themeRow->layout()->contentsMargins().bottom() == 0 &&
                themeRow->height() == contentHeight(themeRow) && languageRow != nullptr &&
                languageRow->height() == contentHeight(languageRow),
            "quick actions and settings must use list spacing without divider components");

    auto* trayIconRadio = interfacePage.findChild<adqt::widgets::AdRadio*>(
        QStringLiteral("settings-control-interface-tray-icon"));
    auto* trayIconRow =
        interfacePage.findChild<QWidget*>(QStringLiteral("settings-item-interface-tray-icon"));
    const auto trayIconRadios = trayIconRadio != nullptr && trayIconRadio->parentWidget() != nullptr
                                    ? trayIconRadio->parentWidget()->findChildren<adqt::widgets::AdRadio*>()
                                    : QList<adqt::widgets::AdRadio*>();
    int rightmostRadioEdge = 0;
    int leftmostRadioEdge = 0;
    if (!trayIconRadios.isEmpty() && trayIconRow != nullptr) {
        leftmostRadioEdge = trayIconRadios.constFirst()->mapTo(
                                trayIconRow, QPoint(0, 0))
                                .x();
        for (const auto* radio : trayIconRadios) {
            rightmostRadioEdge = std::max(
                rightmostRadioEdge,
                radio->mapTo(trayIconRow, QPoint(radio->width() - 1, 0)).x());
        }
    }
    require(trayIconRadio != nullptr && trayIconRow != nullptr &&
                trayIconRadios.size() > 1 && rightmostRadioEdge == trayIconRow->contentsRect().right() &&
                std::all_of(trayIconRadios.cbegin(), trayIconRadios.cend(),
                            [trayIconRow, leftmostRadioEdge](const auto* radio) {
                                return radio->mapTo(trayIconRow, QPoint(0, 0)).x() ==
                                       leftmostRadioEdge;
                            }),
            "radio group must align to the right while its controls share a left edge");

    theme->setCurrentValue(QStringLiteral("light"));
    require(bindings.selectValue(settings::SettingsSelectBinding::Theme) == QStringLiteral("light"),
            "accepted select writes must flow through runtime bindings");
    bindings.acceptWrites = false;
    theme->setCurrentValue(QStringLiteral("dark"));
    require(theme->currentValue() == QStringLiteral("light"),
            "rejected select writes must restore the bound value");
    bindings.acceptWrites = true;

    auto* applicationPriority = systemPage.findChild<adqt::widgets::AdSelect*>(
        QStringLiteral("settings-control-system-application-priority"));
    require(applicationPriority != nullptr && applicationPriority->options().size() == 4 &&
                applicationPriority->currentValue() == QStringLiteral("above_normal") &&
                applicationPriority->currentText() == QStringLiteral("Above normal") &&
                applicationPriority->selectedModelIndexes().size() == 1,
            "application priority must resolve its stored value to the labeled selected option");
    applicationPriority->setCurrentValue(QStringLiteral("high"));
    require(bindings.selectValue(settings::SettingsSelectBinding::ApplicationPriority) ==
                    QStringLiteral("high") &&
                applicationPriority->currentText() == QStringLiteral("High") &&
                applicationPriority->selectedModelIndexes().size() == 1,
            "application priority changes must update both its label and dropdown selection");

    auto* directMlAcceleration = systemPage.findChild<adqt::widgets::AdSwitch*>(
        QStringLiteral("settings-control-text-recognition-direct-ml-acceleration"));
    require(directMlAcceleration != nullptr && directMlAcceleration->isEnabled() &&
                directMlAcceleration->isChecked(),
            "supported Direct ML acceleration must render enabled and on by default");
    directMlAcceleration->setChecked(false);
    require(!bindings.switchValue(settings::SettingsSwitchBinding::DirectMlAcceleration),
            "Direct ML acceleration changes must flow through runtime bindings");
    bindings.directMlSupported = false;
    emit bindings.synchronized();
    flushEvents();
    require(!directMlAcceleration->isEnabled(),
            "Direct ML acceleration must be disabled when the environment is unsupported");

    auto* historySwitch = storagePage.findChild<adqt::widgets::AdSwitch*>(
        QStringLiteral("settings-control-history-enabled"));
    auto* retention = storagePage.findChild<adqt::widgets::AdInputNumber*>(
        QStringLiteral("settings-control-history-retention-days"));
    auto* entries = storagePage.findChild<adqt::widgets::AdInputNumber*>(
        QStringLiteral("settings-control-history-max-entries"));
    auto* disk = storagePage.findChild<adqt::widgets::AdInputNumber*>(
        QStringLiteral("settings-control-history-max-disk-mib"));
    auto* clear = storagePage.findChild<adqt::widgets::AdButton*>(
        QStringLiteral("settings-control-history-clear"));
    require(historySwitch != nullptr && retention != nullptr && entries != nullptr &&
                disk != nullptr && clear != nullptr,
            "switch, integer, and action renderers must all be generated");
    auto* smartSelection = functionPage.findChild<adqt::widgets::AdSwitch*>(
        QStringLiteral("settings-control-screenshot-smart-selection"));
    require(smartSelection != nullptr && smartSelection->isChecked(),
            "Smart Selection must render as an enabled switch by default");
    require(retention->minimum() == 1 && retention->maximum() == 365 && entries->minimum() == 1 &&
                entries->maximum() == 1000 && disk->minimum() == 128 && disk->maximum() == 10240,
            "integer constraints must come from ConfigurationSchema metadata");
    historySwitch->setChecked(false);
    smartSelection->setChecked(false);
    retention->setValue(30);
    require(!bindings.switchValue(settings::SettingsSwitchBinding::HistoryEnabled) &&
                !bindings.switchValue(settings::SettingsSwitchBinding::SmartSelection) &&
                bindings.integerValue(settings::SettingsIntegerBinding::HistoryRetentionDays) == 30,
            "generated switch and integer controls must submit changes through runtime bindings");

    auto* historyHeader = storagePage.findChild<SectionHeaderWidget*>(
        QStringLiteral("settings-section-storage-and-privacy-history"));
    auto* statusHeader = storagePage.findChild<SectionHeaderWidget*>(
        QStringLiteral("settings-section-storage-and-privacy-storage-status"));
    require(
        historyHeader != nullptr && statusHeader != nullptr &&
            historyHeader->layout()->contentsMargins().top() ==
                historyHeader->layout()->contentsMargins().bottom() &&
            statusHeader->layout()->contentsMargins().top() ==
                statusHeader->layout()->contentsMargins().bottom() &&
            historyHeader->findChild<adqt::widgets::AdButton*>(QStringLiteral("sectionResetButton"))
                ->isVisible() &&
            statusHeader->findChild<adqt::widgets::AdButton*>(QStringLiteral("sectionResetButton"))
                ->isHidden(),
        "section headers must use equal vertical spacing and catalog reset visibility");
    const int switchApplyCountBeforeHistoryReset = bindings.switchApplyCount;
    require(QMetaObject::invokeMethod(historyHeader, "resetRequested", Qt::DirectConnection) &&
                bindings.resetRequested == settings::SettingsSectionReset::HistoryPolicy &&
                historySwitch->isChecked() && retention->value() == 7 &&
                bindings.switchApplyCount == switchApplyCountBeforeHistoryReset,
            "section reset must be catalog-configured and resynchronize all policy controls");
    auto* functionHeader = functionPage.findChild<SectionHeaderWidget*>(
        QStringLiteral("settings-section-function-settings-screenshot-settings"));
    require(functionHeader != nullptr &&
                QMetaObject::invokeMethod(functionHeader, "resetRequested", Qt::DirectConnection) &&
                bindings.resetRequested == settings::SettingsSectionReset::ScreenshotSettings &&
                smartSelection->isChecked(),
            "Screenshot settings reset must restore Smart Selection to enabled");

    bindings.setStorageState(false, true);
    flushEvents();
    auto* historyReset =
        historyHeader->findChild<adqt::widgets::AdButton*>(QStringLiteral("sectionResetButton"));
    require(!historySwitch->isEnabled() && !retention->isEnabled() && !entries->isEnabled() &&
                !disk->isEnabled() && clear->busy() && !clear->isEnabled() &&
                historyReset != nullptr && !historyReset->isEnabled(),
            "read-only and busy binding state must resynchronize generated history controls");
    bindings.setStorageState(true, false);
    flushEvents();

    auto* status = storagePage.findChild<StorageStatusSettingsWidget*>(
        QStringLiteral("settings-item-storage-status"));
    auto* descriptions = status != nullptr
                             ? status->findChild<adqt::widgets::AdDescriptions*>(
                                   QStringLiteral("settings-storage-status-descriptions"))
                             : nullptr;
    auto* entryCount =
        storagePage.findChild<QLabel*>(QStringLiteral("settings-status-value-entries"));
    auto* diskUsage =
        storagePage.findChild<QLabel*>(QStringLiteral("settings-status-value-disk-usage"));
    QLabel* entriesLabel = nullptr;
    if (descriptions != nullptr) {
        const auto labels = descriptions->findChildren<QLabel*>();
        for (QLabel* label : labels) {
            if (label->text() == QStringLiteral("Entries") &&
                label->accessibleDescription() == QStringLiteral("Description label")) {
                entriesLabel = label;
                break;
            }
        }
    }
    const int descriptionFontSize = snow_shot::presentation::styles::ThemeManager::instance()
                                        .themeColorScheme()
                                        .metricAlias.fontSize;
    require(status != nullptr && descriptions != nullptr && descriptions->column() == 1 &&
                descriptions->count() == 5 && entryCount != nullptr &&
                entryCount->text() == QStringLiteral("2") && diskUsage != nullptr &&
                diskUsage->text() == QStringLiteral("2.00 KiB") && entriesLabel != nullptr &&
                entryCount->font().pixelSize() == descriptionFontSize &&
                entryCount->font().pixelSize() == entriesLabel->font().pixelSize(),
            "custom Storage Status rendering must use the Descriptions content typography");

    clear->click();
    flushEvents();
    auto* modal = storagePage.findChild<adqt::widgets::AdModal*>(
        QStringLiteral("settings-modal-history-clear"));
    require(modal != nullptr && modal->isOpen() && !bindings.actionTriggered,
            "destructive actions must wait for configured confirmation");
    modal->accept();
    flushEvents();
    require(bindings.actionTriggered,
            "confirmed action rows must invoke their runtime action binding");

    auto* screenshot =
        quick.findChild<ShortcutKeyRow*>(QStringLiteral("settings-item-quick-screenshot"));
    auto* screenshotDelay =
        quick.findChild<ShortcutKeyRow*>(QStringLiteral("settings-item-quick-screenshot-delay"));
    bool delayTitleIsRendered = false;
    if (screenshotDelay != nullptr) {
        for (const QLabel* label : screenshotDelay->findChildren<QLabel*>()) {
            if (label->text() == QStringLiteral("Delay 3s to Execute")) {
                delayTitleIsRendered = true;
                break;
            }
        }
    }
    require(screenshot != nullptr && screenshotDelay != nullptr &&
                screenshotDelay->delaySeconds() == 3 && delayTitleIsRendered &&
                screenshotDelay->cursor().shape() == Qt::SplitVCursor,
            "shortcut/action items and the adjustable 3-second delay must render from the catalog");

    QWheelEvent increaseDelay(
        QPointF(screenshotDelay->rect().center()),
        QPointF(screenshotDelay->mapToGlobal(screenshotDelay->rect().center())), QPoint(),
        QPoint(0, 120), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QCoreApplication::sendEvent(screenshotDelay, &increaseDelay);
    require(screenshotDelay->delaySeconds() == 4 &&
                bindings.integerValue(settings::SettingsIntegerBinding::ScreenshotDelaySeconds) ==
                    4,
            "delay-row wheel adjustments must persist through runtime bindings");
    settings::SettingsCommand command;
    bool commandEmitted = false;
    QObject::connect(&quick, &SettingsPageWidget::commandRequested, &quick,
                     [&command, &commandEmitted](const auto& requested) {
                         command = requested;
                         commandEmitted = true;
                     });
    screenshot->click();
    require(commandEmitted && command.kind == settings::SettingsCommandKind::CaptureScreenshot,
            "shortcut row clicks must emit their configured command");

    PageTranslator translator;
    require(QCoreApplication::installTranslator(&translator), "page translator must install");
    QEvent languageChange(QEvent::LanguageChange);
    QCoreApplication::sendEvent(&interfacePage, &languageChange);
    require(theme->accessibleName() == QStringLiteral("Localized Theme") &&
                theme->options().constFirst().label == QStringLiteral("Localized System Theme"),
            "generated controls and options must retranslate catalog metadata");
    QCoreApplication::removeTranslator(&translator);
    QCoreApplication::sendEvent(&interfacePage, &languageChange);

    interfacePage.raise();
    interfacePage.activateWindow();
    interfacePage.reveal({QStringLiteral("interface-settings"), QStringLiteral("general"),
                          QStringLiteral("interface.language")});
    flushEvents();
    QWidget* focused = QApplication::focusWidget();
    require(focused == language || (focused != nullptr && language->isAncestorOf(focused)),
            "structured item navigation must reveal and focus an appropriate generated control");
}

void quickActionCommandsDispatchThroughContentCard() {
    using Action = snow_shot::presentation::GlobalShortcutAction;

    const auto& catalog = settings::builtInSettingsCatalog();
    snow_shot::presentation::GlobalShortcutManager shortcutManager;
    ContentCardWidget content(catalog, shortcutManager);
    content.setCurrentRoute(QStringLiteral("/"));

    QVector<Action> requestedActions;
    int screenshotRequests = 0;
    QObject::connect(&content, &ContentCardWidget::quickActionRequested, &content,
                     [&requestedActions](Action action) { requestedActions.push_back(action); });
    QObject::connect(&content, &ContentCardWidget::screenshotRequested, &content,
                     [&screenshotRequests]() { ++screenshotRequests; });

    const QVector<QPair<QString, Action>> genericActions{
        {QStringLiteral("quick-screenshot-delay"), Action::ScreenshotDelay},
        {QStringLiteral("quick-screenshot-fixed"), Action::ScreenshotFixed},
        {QStringLiteral("quick-screenshot-ocr"), Action::ScreenshotOcr},
        {QStringLiteral("quick-screenshot-copy"), Action::ScreenshotCopy},
        {QStringLiteral("quick-screenshot-full-screen"), Action::ScreenshotFullScreen},
        {QStringLiteral("quick-screenshot-focused-window"), Action::ScreenshotFocusedWindow},
        {QStringLiteral("quick-video-record"), Action::VideoRecord},
        {QStringLiteral("quick-video-record-copy"), Action::VideoRecordCopy},
        {QStringLiteral("quick-show-or-hide-main-window"), Action::ShowOrHideMainWindow},
        {QStringLiteral("quick-open-capture-history"), Action::OpenCaptureHistory},
    };
    for (const auto& [objectId, action] : genericActions) {
        auto* row = content.findChild<ShortcutKeyRow*>(QStringLiteral("settings-item-") + objectId);
        require(row != nullptr, "every generic quick action must render a shortcut row");
        row->click();
        require(!requestedActions.isEmpty() && requestedActions.constLast() == action,
                "ContentCardWidget must emit the action encoded by each generic command");
    }
    require(requestedActions.size() == genericActions.size() && screenshotRequests == 0,
            "generic quick actions must use only the typed quick-action signal");

    auto* screenshot =
        content.findChild<ShortcutKeyRow*>(QStringLiteral("settings-item-quick-screenshot"));
    require(screenshot != nullptr, "the standard screenshot action must render");
    screenshot->click();
    require(screenshotRequests == 1 && requestedActions.size() == genericActions.size(),
            "the standard screenshot command must preserve its dedicated signal");

    require(content.findChild<ShortcutKeyRow*>(
                QStringLiteral("settings-item-quick-open-interface-settings")) == nullptr,
            "Open Interface Settings must not render in Quick Functions");
    content.showInterfaceSettings();
    require(content.currentLocation() ==
                    settings::SettingsLocation{
                        QStringLiteral("interface-settings"), QStringLiteral("general"), {}} &&
                requestedActions.size() == genericActions.size() && screenshotRequests == 1,
            "the external settings action must navigate without emitting execution signals");
}

void actionsMayExecuteWithoutConfirmation() {
    FakeRuntimeBindings bindings;
    settings::SettingsActionDefinition action;
    action.buttonText = text("Run action");
    action.iconFactory = []() { return adqt::icons::antd::outlined::Rest(); };
    const settings::SettingsCatalog catalog(
        {{QStringLiteral("actions"),
          QStringLiteral("/actions"),
          text("Actions"),
          text("Actions page"),
          {{QStringLiteral("commands"),
            text("Commands"),
            text("Action commands"),
            settings::SettingsSectionReset::None,
            {{QStringLiteral("action.direct"),
              text("Direct action"),
              text("Execute immediately"),
              {},
              {},
              action}}}}}},
        {settings::SettingsNavigationPageDefinition{
            QStringLiteral("nav.actions"), QStringLiteral("actions"),
            []() { return adqt::icons::antd::outlined::Appstore(); }}},
        {QStringLiteral("actions"), QStringLiteral("commands"), QStringLiteral("action.direct")});
    require(catalog.validationErrors().isEmpty(),
            "an action without confirmation metadata must be valid");

    SettingsPageWidget page(catalog, QStringLiteral("actions"), bindings);
    auto* button =
        page.findChild<adqt::widgets::AdButton*>(QStringLiteral("settings-control-action-direct"));
    require(button != nullptr, "an action without confirmation must render");
    button->click();
    require(bindings.actionTriggered && page.findChild<adqt::widgets::AdModal*>() == nullptr,
            "an action without confirmation must execute directly");
}

void catalogExpansionUpdatesAllConsumers() {
    settings::SettingsCatalog catalog = expandedCatalog();
    require(catalog.validationErrors().isEmpty(), "expanded integration catalog must validate");

    snow_shot::presentation::GlobalShortcutManager shortcutManager;
    ContentCardWidget content(catalog, shortcutManager);
    SidebarWidget sidebar(catalog);
    MainContentHeaderWidget header(
        catalog,
        snow_shot::presentation::styles::ThemeManager::instance().themeColorScheme().metricAlias);
    content.resize(720, 420);
    content.show();
    sidebar.show();
    header.show();
    flushEvents();

    auto* stack = content.findChild<QStackedWidget*>();
    auto* menu = sidebar.findChild<adqt::widgets::AdNavigationMenu*>();
    auto* search = header.findChild<ApplicationSearchWidget*>(QStringLiteral("globalTopSearchBar"));
    auto* searchSelect =
        search != nullptr ? search->findChild<adqt::widgets::AdSelect*>() : nullptr;
    require(stack != nullptr && stack->count() == 8,
            "route stack must add catalog pages automatically");
    require(content.findChild<ScreenshotHistoryPageWidget*>(
                QStringLiteral("screenshotHistoryPage")) == nullptr,
            "main-content construction eagerly instantiated screenshot history");
    require(menu != nullptr && menu->model() != nullptr && menu->model()->rowCount() == 5,
            "sidebar must add a catalog navigation node automatically");
    require(searchSelect != nullptr && searchSelect->options().size() == 8,
            "application search must add every catalog page to its default results");

    content.setCurrentRoute(QStringLiteral("/history"));
    flushEvents();
    require(content.currentRoute() == QStringLiteral("/history") &&
                content.findChild<ScreenshotHistoryPageWidget*>(
                    QStringLiteral("screenshotHistoryPage")) != nullptr,
            "the custom screenshot history route must participate in the shared content stack");

    content.setCurrentRoute(QStringLiteral("/extra"));
    header.setSections(content.currentSections());
    header.setCurrentSection(content.currentLocation().sectionId);
    flushEvents();
    auto* tabs = header.findChild<adqt::widgets::AdTabs*>(QStringLiteral("mainSectionTabs"));
    require(content.currentRoute() == QStringLiteral("/extra") &&
                content.currentLocation() ==
                    settings::SettingsLocation{
                        QStringLiteral("extra-page"), QStringLiteral("extra-section"), {}} &&
                content.findChild<SettingsPageWidget*>(
                    QStringLiteral("settings-page-extra-page")) != nullptr &&
                content.findChild<adqt::widgets::AdSelect*>(
                    QStringLiteral("settings-control-extra-item")) != nullptr,
            "content routes, generated page, and item anchors must follow the expanded catalog");
    require(tabs != nullptr && tabs->count() == 1 &&
                tabs->tabKey(0) == QStringLiteral("extra-section"),
            "header tabs must follow the current generated page sections");
}

void sectionTabsAndScrollingStaySynchronized() {
    const auto& catalog = settings::builtInSettingsCatalog();
    snow_shot::presentation::GlobalShortcutManager shortcutManager;
    ContentCardWidget content(catalog, shortcutManager);
    MainContentHeaderWidget header(
        catalog,
        snow_shot::presentation::styles::ThemeManager::instance().themeColorScheme().metricAlias);

    QObject::connect(&header, &MainContentHeaderWidget::sectionRequested, &content,
                     &ContentCardWidget::activateSection);
    QObject::connect(&content, &ContentCardWidget::sectionListChanged, &header,
                     [&content, &header]() { header.setSections(content.currentSections()); });
    QObject::connect(&content, &ContentCardWidget::locationChanged, &header,
                     [&header](const settings::SettingsLocation& location) {
                         header.setCurrentSection(location.sectionId);
                     });

    content.resize(720, 260);
    content.show();
    header.show();
    content.setCurrentRoute(QStringLiteral("/settings/storageAndPrivacy"));
    header.setSections(content.currentSections());
    header.setCurrentSection(content.currentLocation().sectionId);
    flushEvents();

    auto* tabs = header.findChild<adqt::widgets::AdTabs*>(QStringLiteral("mainSectionTabs"));
    auto* scrollArea = content.findChild<adqt::widgets::AdScrollArea*>(
        QStringLiteral("settings-scroll-storage-and-privacy"));
    auto* storageSection = content.findChild<SectionHeaderWidget*>(
        QStringLiteral("settings-section-storage-and-privacy-storage-status"));
    require(tabs != nullptr && scrollArea != nullptr && storageSection != nullptr &&
                scrollArea->contentWidget() != nullptr &&
                scrollArea->verticalScrollBar() != nullptr,
            "section navigation integration must expose tabs, anchors, and a scrollbar");

    QScrollBar* scrollBar = scrollArea->verticalScrollBar();
    const int topInset = scrollArea->contentWidget()->layout()->contentsMargins().top();
    const int storageSectionTop =
        storageSection->mapTo(scrollArea->contentWidget(), QPoint(0, 0)).y();
    require(scrollBar->maximum() >= storageSectionTop - topInset,
            "scrollable pages must reserve enough trailing space to align the last section");

    tabs->tabClicked(QStringLiteral("storage-status"));
    flushEvents();
    require(scrollBar->value() == storageSectionTop - topInset &&
                storageSection->mapTo(scrollArea->viewport(), QPoint(0, 0)).y() == topInset,
            "clicking a tab must top-align its section without an ensure-visible offset");
    require(header.currentSection() == QStringLiteral("storage-status") &&
                content.currentLocation().sectionId == QStringLiteral("storage-status"),
            "tab navigation must keep the header and content location synchronized");

    scrollBar->setValue(0);
    flushEvents();
    require(header.currentSection() == QStringLiteral("history") &&
                content.currentLocation().sectionId == QStringLiteral("history"),
            "scrolling back to the first section must select its tab automatically");

    scrollBar->setValue(storageSectionTop - topInset);
    flushEvents();
    require(header.currentSection() == QStringLiteral("storage-status") &&
                content.currentLocation().sectionId == QStringLiteral("storage-status"),
            "scrolling to a later section must select its tab automatically");
}

void drawingToolbarEditorPersistsDropsAndRestoresRejectedChanges() {
    FakeRuntimeBindings runtime;
    DrawingToolbarEditorSettingsWidget editor(runtime);
    editor.resize(960, 320);
    editor.show();
    flushEvents();

    QWidget* surface =
        editor.findChild<QWidget*>(QStringLiteral("settings-drawing-toolbar-surface"));
    QWidget* hiddenZone =
        editor.findChild<QWidget*>(QStringLiteral("settings-drawing-toolbar-hidden-zone"));
    auto* hiddenTitle =
        editor.findChild<QLabel*>(QStringLiteral("settings-drawing-toolbar-hidden-title"));
    const auto drawingButtons = editor.findChildren<adqt::widgets::AdButton*>(
        QRegularExpression(QStringLiteral("^settings-drawing-toolbar-item-")));
    const auto metric =
        snow_shot::presentation::styles::ThemeManager::instance().themeColorScheme().metricAlias;
    require(surface != nullptr && hiddenZone != nullptr && hiddenTitle != nullptr &&
                hiddenTitle->font().pixelSize() == metric.fontSizeLG &&
                hiddenTitle->font().weight() == QFont::DemiBold && drawingButtons.size() == 11 &&
                editor.findChild<adqt::widgets::AdButton*>(
                    QStringLiteral("settings-drawing-toolbar-item-highlighter")) != nullptr &&
                editor.findChild<adqt::widgets::AdButton*>(
                    QStringLiteral("settings-drawing-toolbar-item-pen-highlight")) == nullptr &&
                editor.findChild<adqt::widgets::AdButton*>(
                    QStringLiteral("settings-drawing-toolbar-item-rectangle-highlight")) == nullptr &&
                editor.findChildren<adqt::widgets::AdPopover*>().isEmpty(),
            "drawing toolbar settings should use themed hidden-title typography and expose one "
            "generic Highlighter Tool among eleven direct tools");

    const auto drop = [](QWidget* target, const QString& itemId, const QPointF& position) {
        QMimeData mimeData;
        mimeData.setData("application/x-snow-shot-toolbar-item", itemId.toUtf8());
        QDragEnterEvent enter(position.toPoint(), Qt::MoveAction, &mimeData, Qt::LeftButton,
                              Qt::NoModifier);
        QApplication::sendEvent(target, &enter);
        QDropEvent event(position, Qt::MoveAction, &mimeData, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(target, &event);
        flushEvents();
        return event.isAccepted();
    };

    QWidget* shapePosition =
        editor.findChild<QWidget*>(QStringLiteral("settings-drawing-toolbar-position-0"));
    require(shapePosition != nullptr,
            "drawing toolbar editor should expose stable position widgets");
    const QPoint stackAboveShape =
        shapePosition->mapTo(surface, QPoint(shapePosition->width() / 2, 0));
    require(drop(surface, QStringLiteral("watermark"), stackAboveShape) &&
                runtime.toolbarLayout().positions.constFirst() ==
                    QStringList{QStringLiteral("watermark"), QStringLiteral("shape")},
            "dropping any drawing tool above another should stack it in that position");

    shapePosition =
        editor.findChild<QWidget*>(QStringLiteral("settings-drawing-toolbar-position-0"));
    require(shapePosition != nullptr && shapePosition->height() > 32,
            "stacked drawing tools should render as vertical direct buttons");
    const QPoint unstackAtEnd(surface->width() - 2, surface->height() - 20);
    require(drop(surface, QStringLiteral("watermark"), unstackAtEnd) &&
                runtime.toolbarLayout().positions.constFirst() ==
                    QStringList{QStringLiteral("shape")} &&
                runtime.toolbarLayout().positions.constLast() ==
                    QStringList{QStringLiteral("watermark")},
            "dropping a stacked tool beside the toolbar should unstack it into a position");

    require(drop(hiddenZone, QStringLiteral("watermark"), hiddenZone->rect().center()) &&
                runtime.toolbarLayout().hidden ==
                    QStringList{QStringLiteral("watermark")} &&
                std::none_of(runtime.toolbarLayout().positions.cbegin(),
                             runtime.toolbarLayout().positions.cend(),
                             [](const QStringList& position) {
                                 return position.contains(QStringLiteral("watermark"));
                             }),
            "dropping a visible tool into the hidden well should remove its toolbar position");
    auto* hiddenWatermark = editor.findChild<adqt::widgets::AdButton*>(
        QStringLiteral("settings-drawing-toolbar-item-watermark"));
    require(hiddenWatermark != nullptr && hiddenWatermark->parentWidget() == hiddenZone &&
                hiddenWatermark->isVisibleTo(&editor),
            "hidden tools should remain directly visible and draggable in settings");

    require(drop(surface, QStringLiteral("watermark"), unstackAtEnd) &&
                runtime.toolbarLayout().hidden.isEmpty() &&
                runtime.toolbarLayout().positions.constLast() ==
                    QStringList{QStringLiteral("watermark")},
            "dragging a hidden tool back to the preview should restore its toolbar position");

    const snow_shot::storage::ScreenshotToolbarLayout accepted = runtime.toolbarLayout();
    runtime.acceptWrites = false;
    require(drop(hiddenZone, QStringLiteral("watermark"), hiddenZone->rect().center()) &&
                runtime.toolbarLayout() == accepted && hiddenWatermark->parentWidget() != hiddenZone,
            "rejected toolbar persistence should leave the prior layout intact");
}
} // namespace

int main(int argc, char** argv) {
    bool drawingToolbarEditorOnly = false;
    for (int argumentIndex = 1; argumentIndex < argc; ++argumentIndex) {
        if (QString::fromLocal8Bit(argv[argumentIndex]) ==
            QStringLiteral("--drawing-toolbar-editor-only")) {
            drawingToolbarEditorOnly = true;
            break;
        }
    }
#if defined(Q_OS_WIN)
    if (drawingToolbarEditorOnly) {
        qunsetenv("QT_QPA_PLATFORM");
    } else {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
#else
    qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SnowShotTests"));
    QCoreApplication::setApplicationName(QStringLiteral("settings_page_tests"));
    QTemporaryDir storageDirectory;
    require(storageDirectory.isValid(), "temporary storage directory must be available");
    require(snow_shot::storage::ApplicationStorage::instance()
                .initialize({storageDirectory.path(), storageDirectory.path(), 60000})
                .success,
            "application storage must initialize for generated page integration tests");
    snow_shot::presentation::styles::ThemeManager::instance().initialize(application);

    if (drawingToolbarEditorOnly) {
        drawingToolbarEditorPersistsDropsAndRestoresRejectedChanges();
        snow_shot::storage::ApplicationStorage::instance().shutdown();
        return 0;
    }

    generatedPagesRenderEveryItemTypeAndResynchronize();
    quickActionCommandsDispatchThroughContentCard();
    actionsMayExecuteWithoutConfirmation();
    screenshotHistoryPageUsesRepositoryAndAntDesignComponents();
    screenshotHistorySurvivesSidebarWidthTransitions();
    screenshotHistoryLifecycleAndIdentityDiff();
    screenshotHistoryEmptyToPopulatedGeometryIsStable();
    catalogExpansionUpdatesAllConsumers();
    sectionTabsAndScrollingStaySynchronized();
    drawingToolbarEditorPersistsDropsAndRestoresRejectedChanges();

    snow_shot::storage::ApplicationStorage::instance().shutdown();
    return 0;
}
