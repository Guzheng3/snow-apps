#include "snow_shot/storage/applicationstorage.h"
#include "snow_shot/storage/configurationschema.h"
#include "snow_shot/storage/configurationstore.h"
#include "snow_shot/storage/persistedselectioncodec.h"
#include "snow_shot/storage/settingsadapters.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfoList>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace storage = snow_shot::storage;

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void writeBytes(const QString& path, const QByteArray& bytes) {
    QFile file(path);
    require(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "failed to open test file");
    require(file.write(bytes) == bytes.size(), "failed to write test file");
}

QByteArray readBytes(const QString& path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "failed to read test file");
    return file.readAll();
}

QJsonObject readObject(const QString& path) {
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(readBytes(path), &error);
    require(error.error == QJsonParseError::NoError && document.isObject(),
            "stored configuration is not valid JSON");
    return document.object();
}

storage::ApplicationStorage& initialize(const QString& executableDirectory,
                                        const QString& appDataDirectory,
                                        int debounceMilliseconds = 60000) {
    auto& applicationStorage = storage::ApplicationStorage::instance();
    static_cast<void>(applicationStorage.initialize(
        {executableDirectory, appDataDirectory, debounceMilliseconds}));
    return applicationStorage;
}

void markerResolutionAndStatus() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "failed to create marker test directory");
    const QString executable = QDir(temporary.path()).filePath(QStringLiteral("bin"));
    const QString fallback = QDir(temporary.path()).filePath(QStringLiteral("fallback"));
    require(QDir().mkpath(executable), "failed to create executable directory");

    auto& missing = initialize(executable, fallback);
    require(missing.configurationDirectory() == QDir::cleanPath(fallback) &&
                missing.status().effectiveMode == storage::StorageMode::ApplicationData,
            "missing marker did not select application data storage");

    const QString marker = QDir(executable).filePath(QStringLiteral("__data_directory"));
    writeBytes(marker, QByteArrayLiteral("portable"));
    auto& portable = initialize(executable, fallback);
    require(portable.configurationDirectory() ==
                    QDir(executable).filePath(QStringLiteral("portable")) &&
                portable.status().effectiveMode == storage::StorageMode::Portable,
            "relative marker did not select portable storage");

    const QString blocking = QDir(temporary.path()).filePath(QStringLiteral("file-target"));
    writeBytes(blocking, QByteArrayLiteral("file"));
    writeBytes(marker, blocking.toUtf8());
    auto& fallbackStorage = initialize(executable, fallback);
    require(fallbackStorage.configurationDirectory() == QDir::cleanPath(fallback) &&
                !fallbackStorage.status().fallbackReason.isEmpty(),
            "unwritable portable target did not report fallback");
}

void defaultsAndTypedRoundTrip() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "failed to create defaults directory");
    const QString config = QDir(temporary.path()).filePath(QStringLiteral("config.json"));
    storage::ConfigurationStore store(config, true, true, 60000);
    require(store.isDirty() && store.flushNow().success,
            "default configuration was not materialized");
    const QJsonObject root = readObject(config);
    const QJsonObject history = root.value(QStringLiteral("capture_history")).toObject();
    const QJsonObject screenshotUi = root.value(QStringLiteral("screenshot_ui")).toObject();
    const QJsonObject toolbarLayout = root.value(QStringLiteral("screenshot_toolbar"))
                                          .toObject()
                                          .value(QStringLiteral("layout"))
                                          .toObject();
    const QJsonArray toolbarPositions = toolbarLayout.value(QStringLiteral("positions")).toArray();
    const QJsonObject tray = root.value(QStringLiteral("tray")).toObject();
    require(
        root.value(QStringLiteral("storage"))
                    .toObject()
                    .value(QStringLiteral("schema_version"))
                    .toInt() == 1 &&
            root.value(QStringLiteral("screenshot_selection"))
                .toObject()
                .value(QStringLiteral("smart_selection"))
                .toBool() &&
            history.value(QStringLiteral("enabled")).toBool() &&
            history.value(QStringLiteral("retention_days")).toInt() == 7 &&
            history.value(QStringLiteral("max_entries")).toInt() == 100 &&
            history.value(QStringLiteral("max_disk_mib")).toInt() == 1024 &&
            screenshotUi.value(QStringLiteral("toolbar_size")).toString() ==
                QStringLiteral("normal") &&
            screenshotUi.value(QStringLiteral("selection_transition_animation")).toBool() &&
            screenshotUi.value(QStringLiteral("selection_mask_color")).toString() ==
                QStringLiteral("#00000080") &&
            screenshotUi.value(QStringLiteral("shortcut_hint_opacity")).toInt() == 100 &&
            toolbarLayout.size() == 2 &&
            toolbarLayout.value(QStringLiteral("hidden")).toArray().isEmpty() &&
            toolbarPositions ==
                QJsonArray{
                    QJsonArray{QStringLiteral("shape")},
                    QJsonArray{QStringLiteral("line"), QStringLiteral("arrow")},
                    QJsonArray{QStringLiteral("free-draw")},
                    QJsonArray{QStringLiteral("spotlight"),
                               QStringLiteral("highlighter")},
                    QJsonArray{QStringLiteral("text")},
                    QJsonArray{QStringLiteral("serial-number")},
                    QJsonArray{QStringLiteral("filter")},
                    QJsonArray{QStringLiteral("eraser")},
                    QJsonArray{QStringLiteral("watermark")},
                } &&
            tray.value(QStringLiteral("enabled")).toBool() &&
            tray.value(QStringLiteral("icon")).toString() == QStringLiteral("default") &&
            tray.value(QStringLiteral("custom_icon")).toString().isEmpty() &&
            !history.contains(QStringLiteral("records")),
        "schema-v1 defaults are incomplete");
    require(readBytes(config).endsWith('\n'), "configuration has no final newline");

    require(
        store.setValues({
            {QStringLiteral("interface/theme_mode"), QStringLiteral("DARK")},
            {QStringLiteral("interface/language"), QStringLiteral("zh-CN")},
            {QStringLiteral("capture_history/retention_days"), 30},
            {QStringLiteral("capture_history/max_entries"), 250},
            {QStringLiteral("capture_history/max_disk_mib"), 2048},
            {QStringLiteral("screenshot_selection/smart_selection"), false},
            {QStringLiteral("screenshot_ui/selection_mask_color"), QStringLiteral(" #12ab34cd ")},
            {QStringLiteral("screenshot_ui/shortcut_hint_opacity"), 42},
            {QStringLiteral("tray/icon"), QStringLiteral("snow-dark")},
        }) &&
            store.flushNow().success,
        "typed configuration mutation failed");
    storage::ConfigurationStore reloaded(config, true, true, 60000);
    require(
        reloaded.value(QStringLiteral("interface/theme_mode")).toString() ==
                QStringLiteral("dark") &&
            reloaded.value(QStringLiteral("interface/language")).toString() ==
                QStringLiteral("zh_CN") &&
            reloaded.value(QStringLiteral("capture_history/retention_days")).toInt() == 30 &&
            !reloaded.value(QStringLiteral("screenshot_selection/smart_selection")).toBool() &&
            reloaded.value(QStringLiteral("screenshot_ui/selection_mask_color")).toString() ==
                QStringLiteral("#12AB34CD") &&
            reloaded.value(QStringLiteral("screenshot_ui/shortcut_hint_opacity")).toInt() == 42 &&
            reloaded.value(QStringLiteral("tray/icon")).toString() == QStringLiteral("snow-dark"),
        "typed values did not normalize and round-trip");
}

void screenshotUiSchemaRepairsStructuredValues() {
    const auto validColor = storage::ConfigurationSchema::normalize(
        QStringLiteral("screenshot_ui/cursor_guide_line_color"), QStringLiteral("#abcdef80"));
    require(validColor.valid && validColor.changed &&
                validColor.value.toString() == QStringLiteral("#ABCDEF80"),
            "RGBA colors were not normalized canonically");
    require(!storage::ConfigurationSchema::normalize(
                 QStringLiteral("screenshot_ui/cursor_guide_line_color"), QStringLiteral("#ABCDEF"))
                 .valid,
            "RGBA color schema accepted an incomplete value");

    const QJsonObject malformedLayout{
        {QStringLiteral("positions"),
         QJsonArray{
             QJsonArray{QStringLiteral("watermark"), QStringLiteral("shape"),
                        QStringLiteral("unknown"), QStringLiteral("watermark")},
             QJsonArray{QStringLiteral("line"), QStringLiteral("shape")},
             QStringLiteral("not-a-position"),
             QJsonArray{QStringLiteral("rectangle-highlight"),
                        QStringLiteral("pen-highlight")},
         }},
        {QStringLiteral("hidden"),
         QJsonArray{QStringLiteral("shape"), QStringLiteral("arrow"),
                    QStringLiteral("free-draw"), QStringLiteral("pen-highlight"),
                    QStringLiteral("arrow")}},
    };
    const auto normalized = storage::ConfigurationSchema::normalize(
        QStringLiteral("screenshot_toolbar/layout"), malformedLayout);
    const QJsonObject layout = normalized.value.toObject();
    const QJsonArray positions = layout.value(QStringLiteral("positions")).toArray();
    require(
        normalized.valid && normalized.changed && layout.size() == 2 &&
            positions ==
                QJsonArray{
                    QJsonArray{QStringLiteral("watermark"), QStringLiteral("shape")},
                    QJsonArray{QStringLiteral("line")},
                    QJsonArray{QStringLiteral("highlighter")},
                    QJsonArray{QStringLiteral("spotlight")},
                    QJsonArray{QStringLiteral("text")},
                    QJsonArray{QStringLiteral("serial-number")},
                    QJsonArray{QStringLiteral("filter")},
                    QJsonArray{QStringLiteral("eraser")},
                } &&
            layout.value(QStringLiteral("hidden")).toArray() ==
                QJsonArray{QStringLiteral("arrow"), QStringLiteral("free-draw")},
        "toolbar layout normalization did not preserve hidden nested membership");

    const QJsonObject legacyLayout{
        {QStringLiteral("order"),
         QJsonArray{QStringLiteral("move"), QStringLiteral("highlight"), QStringLiteral("shape"),
                    QStringLiteral("arrow-line"), QStringLiteral("watermark"),
                    QStringLiteral("highlight")}},
        {QStringLiteral("hidden"),
         QJsonArray{QStringLiteral("highlight"), QStringLiteral("arrow-line"),
                    QStringLiteral("watermark"), QStringLiteral("free-draw")}},
    };
    const auto migrated = storage::ConfigurationSchema::normalize(
        QStringLiteral("screenshot_toolbar/layout"), legacyLayout);
    require(migrated.valid && migrated.changed &&
                migrated.value.toObject() ==
                    QJsonObject{
                        {QStringLiteral("positions"),
                         QJsonArray{
                             QJsonArray{QStringLiteral("shape")},
                             QJsonArray{QStringLiteral("text")},
                             QJsonArray{QStringLiteral("serial-number")},
                             QJsonArray{QStringLiteral("filter")},
                             QJsonArray{QStringLiteral("eraser")},
                         }},
                        {QStringLiteral("hidden"),
                         QJsonArray{QStringLiteral("spotlight"),
                                    QStringLiteral("highlighter"),
                                    QStringLiteral("line"), QStringLiteral("arrow"),
                                    QStringLiteral("watermark"),
                                    QStringLiteral("free-draw")}}},
            "legacy hidden toolbar groups were not migrated into hidden drawing entries");
}

void screenshotUiAdaptersRoundTripTypedValues() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "failed to create screenshot UI adapter directory");
    const QString executable = QDir(temporary.path()).filePath(QStringLiteral("bin"));
    require(QDir().mkpath(executable), "failed to create screenshot UI executable directory");
    static_cast<void>(initialize(executable, temporary.path()));

    const storage::ScreenshotUiSettings screenshot;
    require(screenshot.setSelectionMaskColor(QColor(18, 52, 86, 120)) &&
                screenshot.selectionMaskColor() == QColor(18, 52, 86, 120) &&
                storage::colorToRgbaString(screenshot.selectionMaskColor()) ==
                    QStringLiteral("#12345678") &&
                storage::colorFromRgbaString(QStringLiteral("#ABCDEF01")) ==
                    QColor(171, 205, 239, 1),
            "typed RGBA settings did not round-trip");

    storage::ScreenshotToolbarLayout layout;
    layout.positions = {
        {QStringLiteral("watermark"), QStringLiteral("shape"), QStringLiteral("watermark"),
         QStringLiteral("unknown")},
        {QStringLiteral("line")},
        {QStringLiteral("rectangle-highlight")},
        {QStringLiteral("shape")},
    };
    layout.hidden = {QStringLiteral("shape"), QStringLiteral("arrow"),
                     QStringLiteral("free-draw"), QStringLiteral("pen-highlight"),
                     QStringLiteral("arrow")};
    const storage::ScreenshotToolbarSettings toolbar;
    const QVector<QStringList> expectedPositions{
        {QStringLiteral("watermark"), QStringLiteral("shape")},
        {QStringLiteral("line")},
        {QStringLiteral("highlighter")},
        {QStringLiteral("spotlight")},
        {QStringLiteral("text")},
        {QStringLiteral("serial-number")},
        {QStringLiteral("filter")},
        {QStringLiteral("eraser")},
    };
    const storage::ScreenshotToolbarLayout expectedLayout{
        expectedPositions,
        {QStringLiteral("arrow"), QStringLiteral("free-draw")},
    };
    require(toolbar.setLayout(layout) && toolbar.layout() == expectedLayout,
            "typed toolbar layout did not preserve normalized visible and hidden entries");
}

void screenshotTranslationSettingsRoundTripSupportedValues() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "failed to create translation settings directory");
    const QString executable = QDir(temporary.path()).filePath(QStringLiteral("bin"));
    require(QDir().mkpath(executable), "failed to create translation settings executable directory");
    static_cast<void>(initialize(executable, temporary.path()));

    const storage::ScreenshotTranslationSettings translation;
    require(translation.configuration() ==
                storage::ScreenshotTranslationConfiguration{QStringLiteral("auto"), {}, {}},
            "translation settings should default to Auto source and runtime-derived target/model");
    const storage::ScreenshotTranslationConfiguration selected{
        QStringLiteral("ja"), QStringLiteral("zh-Hant"), QStringLiteral("model-a")};
    require(translation.setConfiguration(selected) && translation.configuration() == selected,
            "translation language and model selections should persist together");

    const auto unsupportedTarget = storage::ConfigurationSchema::normalize(
        QStringLiteral("screenshot_translation/target_language"), QStringLiteral("auto"));
    require(!unsupportedTarget.valid,
            "Auto Detect should be accepted only for the source translation language");
    const auto normalizedSource = storage::ConfigurationSchema::normalize(
        QStringLiteral("screenshot_translation/source_language"), QStringLiteral(" ZH-HANS "));
    require(normalizedSource.valid && normalizedSource.changed &&
                normalizedSource.value.toString() == QStringLiteral("zh-Hans"),
            "translation language codes should normalize to their canonical persisted form");
}

void smartSelectionAccessorAndSignal() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "failed to create smart-selection directory");
    const QString executable = QDir(temporary.path()).filePath(QStringLiteral("bin"));
    require(QDir().mkpath(executable), "failed to create smart-selection executable directory");
    auto& applicationStorage = initialize(executable, temporary.path());
    bool changed = false;
    QObject::connect(&applicationStorage, &storage::ApplicationStorage::smartSelectionChanged,
                     [&changed](bool enabled) { changed = !enabled; });
    require(applicationStorage.smartSelectionEnabled() &&
                applicationStorage.requestSmartSelection(false) &&
                !applicationStorage.smartSelectionEnabled() && changed,
            "smart-selection accessor did not persist or signal changes");
    require(applicationStorage.requestSmartSelection(true) &&
                applicationStorage.smartSelectionEnabled(),
            "smart-selection setting did not restore its enabled default");
}

void unknownFieldsArePreserved() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "failed to create unknown-field directory");
    const QString config = QDir(temporary.path()).filePath(QStringLiteral("config.json"));
    writeBytes(config,
               QByteArrayLiteral("{\n"
                                 "  \"storage\": {\"schema_version\": 1, \"future_flag\": true},\n"
                                 "  \"interface\": {\"theme_mode\": \"light\"},\n"
                                 "  \"extension\": {\"nested\": [1, 2, 3]}\n"
                                 "}\n"));
    storage::ConfigurationStore store(config, true, true, 60000);
    require(store.setValue(QStringLiteral("interface/sidebar_collapsed"), true) &&
                store.flushNow().success,
            "failed to update document containing unknown fields");
    const QJsonObject root = readObject(config);
    require(root.value(QStringLiteral("storage"))
                    .toObject()
                    .value(QStringLiteral("future_flag"))
                    .toBool() &&
                root.value(QStringLiteral("extension"))
                        .toObject()
                        .value(QStringLiteral("nested"))
                        .toArray()
                        .size() == 3,
            "schema-v1 unknown fields were erased");
}

void malformedConfigurationIsCopiedAndReplaced() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "failed to create malformed directory");
    const QString config = QDir(temporary.path()).filePath(QStringLiteral("config.json"));
    writeBytes(config, QByteArrayLiteral("{broken"));
    storage::ConfigurationStore store(config, true, true, 60000);
    require(store.compatibility() == storage::ConfigurationCompatibility::RecoveredDefaults &&
                store.isDirty(),
            "malformed configuration did not load recoverable defaults");
    require(!QDir(temporary.path())
                 .entryInfoList({QStringLiteral("config.json.corrupt.*.json")}, QDir::Files)
                 .isEmpty(),
            "malformed configuration was not copied to a corrupt backup");
    require(store.flushNow().success && readObject(config)
                                                .value(QStringLiteral("storage"))
                                                .toObject()
                                                .value(QStringLiteral("schema_version"))
                                                .toInt() == 1,
            "malformed configuration was not replaced cleanly");

    const QString expiredBackup =
        QDir(temporary.path())
            .filePath(QStringLiteral("config.json.corrupt.20000101T000000000Z.json"));
    writeBytes(expiredBackup, QByteArrayLiteral("old"));
    QFile expiredFile(expiredBackup);
    require(expiredFile.open(QIODevice::ReadWrite), "failed to open aged corrupt backup");
    require(expiredFile.setFileTime(QDateTime::currentDateTimeUtc().addDays(-31),
                                    QFileDevice::FileModificationTime),
            "failed to age corrupt configuration backup");
    expiredFile.close();
    storage::ConfigurationStore cleanup(config, true, true, 60000);
    require(!QFileInfo::exists(expiredBackup) &&
                !QDir(temporary.path())
                     .entryInfoList({QStringLiteral("config.json.corrupt.*.json")}, QDir::Files)
                     .isEmpty(),
            "expired corrupt configuration backups were not cleaned up");

    writeBytes(config, QByteArrayLiteral("{\"storage\": {}}"));
    storage::ConfigurationStore missingVersion(config, true, true, 60000);
    require(missingVersion.compatibility() ==
                storage::ConfigurationCompatibility::RecoveredDefaults,
            "missing schema version was not treated as corrupt");
}

void futureVersionIsReadOnly() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "failed to create future-version directory");
    const QString config = QDir(temporary.path()).filePath(QStringLiteral("config.json"));
    writeBytes(config, QByteArrayLiteral("{\n"
                                         "  \"storage\": {\"schema_version\": 2},\n"
                                         "  \"interface\": {\"theme_mode\": \"dark\"},\n"
                                         "  \"future\": {\"value\": 42}\n"
                                         "}\n"));
    const QByteArray original = readBytes(config);
    storage::ConfigurationStore store(config, true, true, 60000);
    bool rejected = false;
    QObject::connect(&store, &storage::ConfigurationStore::mutationRejected,
                     [&rejected](const QString&, const QString&) { rejected = true; });
    require(store.compatibility() == storage::ConfigurationCompatibility::FutureVersion &&
                !store.isWritable() &&
                store.value(QStringLiteral("interface/theme_mode")).toString() ==
                    QStringLiteral("dark") &&
                !store.setValue(QStringLiteral("interface/theme_mode"), QStringLiteral("light")) &&
                rejected && store.flushNow().success && readBytes(config) == original,
            "future configuration was not loaded conservatively in read-only mode");

    const QString executable = QDir(temporary.path()).filePath(QStringLiteral("bin"));
    require(QDir().mkpath(executable), "failed to create future-version executable directory");
    auto& applicationStorage = initialize(executable, temporary.path());
    const storage::StorageStatus status = applicationStorage.status();
    require(status.effectiveMode == storage::StorageMode::FutureVersionReadOnly &&
                !status.writeAvailable &&
                status.configurationCompatibility ==
                    storage::ConfigurationCompatibility::FutureVersion &&
                !applicationStorage.requestCaptureHistoryClear(),
            "application storage did not propagate future-version read-only mode");
}

void failedWriteCanBeRetried() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "failed to create retry directory");
    const QString config = QDir(temporary.path()).filePath(QStringLiteral("config.json"));
    require(QDir().mkpath(config), "failed to create blocking config directory");
    storage::ConfigurationStore store(config, true, true, 60000);
    require(!store.flushNow().success && store.isDirty(),
            "failed configuration write did not remain dirty");
    require(QDir(config).removeRecursively(), "failed to remove blocking config directory");
    require(store.flushNow().success && !store.isDirty() && QFileInfo::exists(config),
            "configuration write did not recover on retry");
}

void concurrentFlushKeepsLatestRevision() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "failed to create concurrency directory");
    const QString config = QDir(temporary.path()).filePath(QStringLiteral("config.json"));
    storage::ConfigurationStore store(config, true, true, 60000);
    require(store.flushNow().success, "failed to write concurrency defaults");

    std::atomic<bool> start{false};
    std::thread first([&store, &start]() {
        while (!start.load()) {
            std::this_thread::yield();
        }
        for (int index = 0; index < 50; ++index) {
            static_cast<void>(
                store.setValue(QStringLiteral("interface/sidebar_collapsed"), index % 2 == 0));
            static_cast<void>(store.flushNow());
        }
    });
    std::thread second([&store, &start]() {
        while (!start.load()) {
            std::this_thread::yield();
        }
        for (int index = 0; index < 50; ++index) {
            static_cast<void>(
                store.setValue(QStringLiteral("interface/theme_mode"),
                               index % 2 == 0 ? QStringLiteral("dark") : QStringLiteral("light")));
            static_cast<void>(store.flushNow());
        }
    });
    start = true;
    first.join();
    second.join();
    require(store.setValues({
                {QStringLiteral("interface/sidebar_collapsed"), true},
                {QStringLiteral("interface/theme_mode"), QStringLiteral("dark")},
            }) &&
                store.flushNow().success,
            "failed to flush final concurrent revision");
    const QJsonObject interface = readObject(config).value(QStringLiteral("interface")).toObject();
    require(interface.value(QStringLiteral("sidebar_collapsed")).toBool() &&
                interface.value(QStringLiteral("theme_mode")).toString() == QStringLiteral("dark"),
            "an older concurrent snapshot overwrote the latest revision");
}

void persistedSelectionCodecIsCanonicalAndStrict() {
    storage::PersistedSelection selection;
    selection.rectangle = QRect(4, 5, 120, 80);
    selection.cornerRadius = 8;
    selection.shadowWidth = 3;
    selection.shadowColor = QColor(10, 20, 30, 120);
    selection.lockAspectRatio = true;
    const QJsonObject encoded = storage::persistedSelectionToJson(selection);
    const auto decoded = storage::normalizePersistedSelection(encoded);
    require(decoded.valid && decoded.value == selection && !decoded.changed,
            "persisted selection codec did not round-trip canonically");

    QJsonObject malformed = encoded;
    malformed.insert(QStringLiteral("corner_radius"), 257);
    require(!storage::normalizePersistedSelection(malformed).valid,
            "persisted selection codec accepted an out-of-range radius");
}

void asynchronousMutationResultsAreObservable() {
    QTemporaryDir temporary;
    require(temporary.isValid(), "failed to create async mutation directory");
    const QString executable = QDir(temporary.path()).filePath(QStringLiteral("bin"));
    require(QDir().mkpath(executable), "failed to create async mutation executable directory");
    auto& applicationStorage = initialize(executable, temporary.path(), 60000);

    storage::CaptureHistoryPolicy policy = applicationStorage.captureHistoryPolicy();
    policy.maxEntries = 2;
    const auto policyResult = applicationStorage.requestCaptureHistoryPolicyAsync(policy);
    require(policyResult.valid() && policyResult.get().success,
            "asynchronous policy mutation did not complete successfully");
    QCoreApplication::processEvents();
    require(!applicationStorage.status().historyPolicyUpdating,
            "policy mutation remained busy after completion");

    const auto clearResult = applicationStorage.requestCaptureHistoryClearAsync();
    require(clearResult.valid() && clearResult.get().success,
            "asynchronous history clear did not complete successfully");
    QCoreApplication::processEvents();
    require(!applicationStorage.status().historyClearing,
            "history clear remained busy after completion");
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SnowShotTests"));
    QCoreApplication::setApplicationName(QStringLiteral("storage-tests"));
    markerResolutionAndStatus();
    defaultsAndTypedRoundTrip();
    screenshotUiSchemaRepairsStructuredValues();
    screenshotUiAdaptersRoundTripTypedValues();
    screenshotTranslationSettingsRoundTripSupportedValues();
    smartSelectionAccessorAndSignal();
    unknownFieldsArePreserved();
    malformedConfigurationIsCopiedAndReplaced();
    futureVersionIsReadOnly();
    failedWriteCanBeRetried();
    concurrentFlushKeepsLatestRevision();
    persistedSelectionCodecIsCanonicalAndStrict();
    asynchronousMutationResultsAreObservable();
    storage::ApplicationStorage::instance().shutdown();
    return 0;
}
