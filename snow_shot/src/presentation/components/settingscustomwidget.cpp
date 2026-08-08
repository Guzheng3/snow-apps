#include "snow_shot/presentation/components/settingscustomwidget.h"

#include "snow_shot/presentation/components/storagestatussettingswidget.h"
#include "snow_shot/presentation/settings/settingsruntimebindings.h"

SettingsCustomWidget* createSettingsCustomWidget(
    snow_shot::presentation::settings::SettingsCustomRenderer renderer,
    snow_shot::presentation::settings::SettingsRuntimeBindings& runtimeBindings,
    QWidget* parent) {
    using snow_shot::presentation::settings::SettingsCustomRenderer;
    switch (renderer) {
    case SettingsCustomRenderer::StorageStatus:
        return new StorageStatusSettingsWidget(runtimeBindings, parent);
    }
    return nullptr;
}
