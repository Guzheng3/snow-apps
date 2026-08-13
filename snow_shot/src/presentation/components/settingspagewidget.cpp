#include "snow_shot/presentation/components/settingspagewidget.h"

#include "snow_shot/presentation/components/pagecontainerwidget.h"
#include "snow_shot/presentation/components/sectionheaderwidget.h"
#include "snow_shot/presentation/components/settingscustomwidget.h"
#include "snow_shot/presentation/components/settingspageutils.h"
#include "snow_shot/presentation/components/shortcutkeyrow.h"
#include "snow_shot/presentation/settings/settingsruntimebindings.h"
#include "snow_shot/presentation/styles/mainwindowcomponenttoken.h"
#include "snow_shot/presentation/styles/thememanager.h"
#include "snow_shot/storage/configurationschema.h"

#include "widgets/button.h"
#include "widgets/color_picker.h"
#include "widgets/input_number.h"
#include "widgets/input_search_edit.h"
#include "widgets/modal.h"
#include "widgets/radio.h"
#include "widgets/radio_button_group.h"
#include "widgets/scroll_area.h"
#include "widgets/select.h"
#include "widgets/slider.h"
#include "widgets/switch.h"

#include <QAbstractButton>
#include <QEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPointer>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QScopedValueRollback>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <type_traits>
#include <utility>

namespace {
namespace settings = snow_shot::presentation::settings;
namespace settings_ui = snow_shot::presentation::components;

adqt::widgets::AdSelect::Option selectOption(const QVariant& value, const QString& label) {
    adqt::widgets::AdSelect::Option result;
    result.value = value;
    result.label = label;
    return result;
}

} // namespace

class SettingsPageWidget::Impl {
  public:
    struct RuntimeItem {
        const settings::SettingsItemDefinition* definition = nullptr;
        QWidget* anchor = nullptr;
        QWidget* focusTarget = nullptr;
        QLabel* title = nullptr;
        QLabel* description = nullptr;
        adqt::widgets::AdSelect* select = nullptr;
        adqt::widgets::AdSwitch* switchControl = nullptr;
        adqt::widgets::AdInputNumber* integerControl = nullptr;
        QWidget* sliderContainer = nullptr;
        adqt::widgets::AdSlider* sliderControl = nullptr;
        QLabel* sliderValue = nullptr;
        adqt::widgets::AdColorPicker* colorControl = nullptr;
        QWidget* radioContainer = nullptr;
        adqt::widgets::AdRadioButtonGroup* radioGroup = nullptr;
        QVector<adqt::widgets::AdRadio*> radioButtons;
        QVector<QVariant> radioValues;
        adqt::widgets::AdSearchEdit* filePathControl = nullptr;
        ShortcutKeyRow* shortcutControl = nullptr;
        adqt::widgets::AdButton* actionControl = nullptr;
        SettingsCustomWidget* customControl = nullptr;
        QPointer<adqt::widgets::AdModal> modal;
    };

    struct RuntimeSection {
        const settings::SettingsSectionDefinition* definition = nullptr;
        SectionHeaderWidget* header = nullptr;
    };

    Impl(SettingsPageWidget& owner, const settings::SettingsCatalog& sourceCatalog,
          const QString& sourcePageId,
          settings::SettingsRuntimeBindings& sourceRuntimeBindings)
        : q(owner), catalog(sourceCatalog), runtimeBindings(sourceRuntimeBindings),
          page(catalog.page(sourcePageId)),
          colorScheme(snow_shot::presentation::styles::ThemeManager::instance()
                          .themeColorScheme()) {
        Q_ASSERT(page != nullptr);
        build();
        connectServices();
        retranslateUi();
        syncValues();
        applyTheme(colorScheme);
    }

    RuntimeItem* runtimeItem(const QString& itemId) {
        for (RuntimeItem& item : items) {
            if (item.definition != nullptr && item.definition->id == itemId) {
                return &item;
            }
        }
        return nullptr;
    }

    RuntimeSection* runtimeSection(const QString& sectionId) {
        for (RuntimeSection& section : sections) {
            if (section.definition != nullptr && section.definition->id == sectionId) {
                return &section;
            }
        }
        return nullptr;
    }

    void build() {
        const auto metric = colorScheme.metricAlias;
        q.setObjectName(settings::generatedObjectName(QStringLiteral("settings-page"), page->id));
        q.setAutoFillBackground(false);

        auto* pageLayout = new QVBoxLayout(&q);
        pageLayout->setContentsMargins(0, 0, 0, 0);
        pageLayout->setSpacing(0);

        auto* pageContainer = new PageContainerWidget(metric, &q);
        pageContainer->setObjectName(
            settings::generatedObjectName(QStringLiteral("settings-container"), page->id));
        scrollArea = pageContainer->scrollArea();
        scrollArea->setObjectName(
            settings::generatedObjectName(QStringLiteral("settings-scroll"), page->id));

        contentWidget = pageContainer->contentWidget();
        contentWidget->setObjectName(
            settings::generatedObjectName(QStringLiteral("settings-content"), page->id));
        contentLayout = pageContainer->contentLayout();
        contentLayout->setSpacing(0);

        for (const settings::SettingsSectionDefinition& sectionDefinition : page->sections) {
            RuntimeSection runtimeSection;
            runtimeSection.definition = &sectionDefinition;
            runtimeSection.header =
                new SectionHeaderWidget(sectionDefinition.title.translated(), metric, contentWidget);
            runtimeSection.header->setObjectName(settings::generatedObjectName(
                QStringLiteral("settings-section"),
                QStringLiteral("%1-%2").arg(page->id, sectionDefinition.id)));
            runtimeSection.header->setResetVisible(
                sectionDefinition.reset != settings::SettingsSectionReset::None);
            contentLayout->addWidget(runtimeSection.header);
            sections.push_back(runtimeSection);

            auto* list = new QWidget(contentWidget);
            list->setObjectName(settings::generatedObjectName(
                QStringLiteral("settings-section-list"),
                QStringLiteral("%1-%2").arg(page->id, sectionDefinition.id)));
            list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            auto* listLayout = new QVBoxLayout(list);
            listLayout->setContentsMargins(0, 0, 0, 0);
            listLayout->setSpacing(page->id == QStringLiteral("quick-functions")
                                       ? metric.padding
                                       : metric.paddingLG);

            for (const settings::SettingsItemDefinition& itemDefinition :
                 sectionDefinition.items) {
                buildItem(itemDefinition, list, listLayout);
            }
            contentLayout->addWidget(list);
        }
        trailingScrollSpace = new QWidget(contentWidget);
        trailingScrollSpace->setObjectName(settings::generatedObjectName(
            QStringLiteral("settings-section-scroll-space"), page->id));
        trailingScrollSpace->setFixedHeight(0);
        contentLayout->addWidget(trailingScrollSpace);
        contentLayout->addStretch(1);
        pageLayout->addWidget(pageContainer, 1);
        scrollMarginX = metric.paddingSM;
        scrollMarginY = metric.paddingSM;
        if (scrollArea->viewport() != nullptr) {
            scrollArea->viewport()->installEventFilter(&q);
        }
        requestScrollGeometryUpdate();
    }

    void buildItem(const settings::SettingsItemDefinition& definition, QWidget* list,
                   QVBoxLayout* listLayout) {
        RuntimeItem runtime;
        runtime.definition = &definition;

        std::visit(
            [&](const auto& payload) {
                using Payload = std::decay_t<decltype(payload)>;
                if constexpr (std::is_same_v<Payload, settings::SettingsSelectDefinition>) {
                    auto* select = new adqt::widgets::AdSelect(list);
                    select->setMode(adqt::widgets::AdSelect::Mode::Single);
                    select->setControlSize(adqt::widgets::AdSelect::ControlSize::Middle);
                    select->setFixedWidth(settings_ui::settingsControlWidth(colorScheme.metricAlias));
                    runtime.select = select;
                    runtime.focusTarget = select;
                    runtime.anchor = settings_ui::createSettingItemRow(
                        list, colorScheme.metricAlias, &runtime.title, &runtime.description, select,
                        settings::generatedObjectName(QStringLiteral("settings-item"), definition.id));
                    listLayout->addWidget(runtime.anchor);
                    connect(select, &adqt::widgets::AdSelect::currentValueChanged, &q,
                            [this, itemId = definition.id](const QVariant& value) {
                                applySelectValue(itemId, value);
                            });
                } else if constexpr (std::is_same_v<Payload,
                                                    settings::SettingsSwitchDefinition>) {
                    auto* control = new adqt::widgets::AdSwitch(list);
                    control->setControlSize(adqt::widgets::AdSwitch::ControlSize::Medium);
                    runtime.switchControl = control;
                    runtime.focusTarget = control;
                    runtime.anchor = settings_ui::createSettingItemRow(
                        list, colorScheme.metricAlias, &runtime.title, &runtime.description, control,
                        settings::generatedObjectName(QStringLiteral("settings-item"), definition.id));
                    listLayout->addWidget(runtime.anchor);
                    connect(control, &QAbstractButton::toggled, &q,
                            [this, binding = payload.binding](bool checked) {
                                applySwitchValue(binding, checked);
                            });
                } else if constexpr (std::is_same_v<Payload,
                                                     settings::SettingsIntegerDefinition>) {
                    auto* control = new adqt::widgets::AdInputNumber(list);
                    control->setControlSize(adqt::widgets::AdInputNumber::ControlSize::Medium);
                    control->setVariant(adqt::widgets::AdInputNumber::Variant::Outlined);
                    control->setStepButtonLayout(
                        adqt::widgets::AdInputNumber::StepButtonLayout::Compact);
                    control->setDecimals(0);
                    control->setFixedWidth(settings_ui::settingsControlWidth(colorScheme.metricAlias));
                    const auto* schemaEntry = snow_shot::storage::ConfigurationSchema::entry(
                        definition.configurationKey);
                    Q_ASSERT(schemaEntry != nullptr && schemaEntry->integerRange.has_value());
                    control->setRange(schemaEntry->integerRange->minimum,
                                      schemaEntry->integerRange->maximum);
                    control->setSingleStep(schemaEntry->integerRange->step);
                    runtime.integerControl = control;
                    runtime.focusTarget = control;
                    runtime.anchor = settings_ui::createSettingItemRow(
                        list, colorScheme.metricAlias, &runtime.title, &runtime.description, control,
                        settings::generatedObjectName(QStringLiteral("settings-item"), definition.id));
                    listLayout->addWidget(runtime.anchor);
                    connect(control, &adqt::widgets::AdInputNumber::valueChanged, &q,
                            [this, binding = payload.binding](double value) {
                                applyIntegerValue(binding, static_cast<int>(value));
                            });
                } else if constexpr (std::is_same_v<Payload,
                                                     settings::SettingsSliderDefinition>) {
                    auto* container = new QWidget(list);
                    auto* layout = new QHBoxLayout(container);
                    layout->setContentsMargins(0, 0, 0, 0);
                    layout->setSpacing(0);
                    auto* control = new adqt::widgets::AdSlider(container);
                    const auto* schemaEntry = snow_shot::storage::ConfigurationSchema::entry(
                        definition.configurationKey);
                    Q_ASSERT(schemaEntry != nullptr && schemaEntry->integerRange.has_value());
                    control->setRange(schemaEntry->integerRange->minimum,
                                      schemaEntry->integerRange->maximum);
                    control->setSingleStep(schemaEntry->integerRange->step);
                    control->setPageStep(std::max(schemaEntry->integerRange->step, 10));
                    control->setTooltipEnabled(true);
                    auto* valueLabel = new QLabel(container);
                    valueLabel->setMinimumWidth(colorScheme.metricAlias.controlHeightLG);
                    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    layout->addWidget(control, 1);
                    layout->addWidget(valueLabel);
                    container->setFixedWidth(
                        settings_ui::settingsControlWidth(colorScheme.metricAlias));
                    runtime.sliderContainer = container;
                    runtime.sliderControl = control;
                    runtime.sliderValue = valueLabel;
                    runtime.focusTarget = control;
                    runtime.anchor = settings_ui::createSettingItemRow(
                        list, colorScheme.metricAlias, &runtime.title, &runtime.description,
                        container,
                        settings::generatedObjectName(QStringLiteral("settings-item"),
                                                      definition.id));
                    listLayout->addWidget(runtime.anchor);
                    connect(control, &adqt::widgets::AdSlider::valueChanged, &q,
                            [this, binding = payload.binding, valueLabel,
                             suffix = payload.suffix](double value) {
                                const int integerValue = qRound(value);
                                valueLabel->setText(
                                    QStringLiteral("%1%2")
                                        .arg(integerValue)
                                        .arg(suffix.translated()));
                                if (!synchronizingValues &&
                                    !runtimeBindings.applySliderValue(binding, integerValue)) {
                                    syncValues();
                                }
                            });
                } else if constexpr (std::is_same_v<Payload,
                                                     settings::SettingsColorDefinition>) {
                    auto* control = new adqt::widgets::AdColorPicker(list);
                    control->setSize(adqt::widgets::AdColorPicker::Size::Middle);
                    control->setModeOptions({adqt::widgets::AdColorPicker::Mode::Solid});
                    control->setMode(adqt::widgets::AdColorPicker::Mode::Solid);
                    control->setFormat(adqt::widgets::AdColorPicker::Format::Hex);
                    control->setAlphaChannelEnabled(payload.alphaChannelEnabled);
                    control->setAllowClear(false);
                    control->setTriggerTextVisible(true);
                    control->setFixedWidth(
                        settings_ui::settingsControlWidth(colorScheme.metricAlias));
                    if (auto* layout = qobject_cast<QHBoxLayout*>(control->layout());
                        layout != nullptr && layout->count() > 0 &&
                        layout->itemAt(0)->widget() != nullptr) {
                        layout->setAlignment(layout->itemAt(0)->widget(),
                                             Qt::AlignRight | Qt::AlignVCenter);
                    }
                    runtime.colorControl = control;
                    runtime.focusTarget = control;
                    runtime.anchor = settings_ui::createSettingItemRow(
                        list, colorScheme.metricAlias, &runtime.title, &runtime.description,
                        control,
                        settings::generatedObjectName(QStringLiteral("settings-item"),
                                                      definition.id));
                    listLayout->addWidget(runtime.anchor);
                    connect(control, &adqt::widgets::AdColorPicker::valueChanged, &q,
                            [this, binding = payload.binding](
                                const adqt::widgets::AdColorValue& value) {
                                if (!synchronizingValues && value.isSolid() &&
                                    !runtimeBindings.applyColorValue(binding,
                                                                    value.solidColor)) {
                                    syncValues();
                                }
                            });
                } else if constexpr (std::is_same_v<Payload,
                                                     settings::SettingsRadioDefinition>) {
                    auto* container = new QWidget(list);
                    auto* layout = new QHBoxLayout(container);
                    layout->setContentsMargins(0, 0, 0, 0);
                    layout->setSpacing(0);
                    auto* radioList = new QWidget(container);
                    radioList->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                    auto* radioLayout = new QVBoxLayout(radioList);
                    radioLayout->setContentsMargins(0, 0, 0, 0);
                    radioLayout->setSpacing(colorScheme.metricAlias.marginXS);
                    auto* group = new adqt::widgets::AdRadioButtonGroup(container);
                    group->setManagedLayout(radioLayout);
                    group->setControlSize(adqt::widgets::AdRadio::ControlSize::Small);
                    for (int index = 0; index < payload.options.size(); ++index) {
                        const settings::SettingsRadioOptionDefinition& option =
                            payload.options.at(index);
                        auto* radio = new adqt::widgets::AdRadio(radioList);
                        radio->setIcon(QIcon(option.iconResource));
                        radio->setIconSize(QSize(24, 24));
                        group->addButton(radio, index);
                        radioLayout->addWidget(radio);
                        runtime.radioButtons.push_back(radio);
                        runtime.radioValues.push_back(option.value);
                    }
                    layout->addStretch(1);
                    layout->addWidget(radioList, 0, Qt::AlignRight);
                    container->setFixedWidth(
                        settings_ui::settingsControlWidth(colorScheme.metricAlias));
                    runtime.radioContainer = container;
                    runtime.radioGroup = group;
                    runtime.focusTarget = runtime.radioButtons.isEmpty()
                                              ? static_cast<QWidget*>(container)
                                              : runtime.radioButtons.constFirst();
                    runtime.anchor = settings_ui::createSettingItemRow(
                        list, colorScheme.metricAlias, &runtime.title, &runtime.description,
                        container,
                        settings::generatedObjectName(QStringLiteral("settings-item"),
                                                      definition.id));
                    listLayout->addWidget(runtime.anchor);
                    connect(group, &adqt::widgets::AdRadioButtonGroup::checkedIdChanged, &q,
                            [this, binding = payload.binding,
                             values = runtime.radioValues](int id) {
                                if (!synchronizingValues && id >= 0 && id < values.size() &&
                                    !runtimeBindings.applyRadioValue(binding,
                                                                    values.at(id))) {
                                    syncValues();
                                }
                            });
                } else if constexpr (std::is_same_v<Payload,
                                                     settings::SettingsFilePathDefinition>) {
                    auto* control = new adqt::widgets::AdSearchEdit(list);
                    control->setControlSize(adqt::widgets::AdSearchEdit::ControlSize::Medium);
                    control->setAllowClear(true);
                    control->setFixedWidth(
                        settings_ui::settingsControlWidth(colorScheme.metricAlias));
                    runtime.filePathControl = control;
                    runtime.focusTarget = control;
                    runtime.anchor = settings_ui::createSettingItemRow(
                        list, colorScheme.metricAlias, &runtime.title, &runtime.description,
                        control,
                        settings::generatedObjectName(QStringLiteral("settings-item"),
                                                      definition.id));
                    listLayout->addWidget(runtime.anchor);
                    connect(control, &adqt::widgets::AdSearchEdit::editingFinished, &q,
                            [this, control, binding = payload.binding]() {
                                if (!synchronizingValues &&
                                    !runtimeBindings.applyFilePathValue(binding,
                                                                        control->text())) {
                                    syncValues();
                                }
                            });
                    connect(control, &adqt::widgets::AdSearchEdit::searchRequested, &q,
                            [this, control, binding = payload.binding,
                             dialogTitle = payload.dialogTitle,
                             fileFilter = payload.fileFilter](
                                const QString& text,
                                adqt::widgets::AdSearchEdit::SearchReason reason) {
                                if (reason ==
                                    adqt::widgets::AdSearchEdit::SearchReason::ButtonClick) {
                                    const QString path = QFileDialog::getOpenFileName(
                                        &q, dialogTitle.translated(), text,
                                        fileFilter.translated());
                                    if (!path.isEmpty()) {
                                        control->setText(path);
                                        if (!runtimeBindings.applyFilePathValue(binding, path)) {
                                            syncValues();
                                        }
                                    }
                                } else if (reason ==
                                           adqt::widgets::AdSearchEdit::SearchReason::ClearAction) {
                                    if (!runtimeBindings.applyFilePathValue(binding, QString())) {
                                        syncValues();
                                    }
                                }
                            });
                } else if constexpr (std::is_same_v<
                                         Payload, settings::SettingsShortcutActionDefinition>) {
                    const auto shortcutState = runtimeBindings.shortcutState(payload.shortcutAction);
                    const auto metric = colorScheme.metricAlias;
                    const auto mainWindowMetric =
                        snow_shot::presentation::styles::buildMainWindowComponentMetricToken(
                            colorScheme);
                    ShortcutKeyRowConfig config{
                        definition.title.translated(),
                        payload.iconFactory ? payload.iconFactory() : adqt::icons::IconRef(),
                        shortcutState.shortcuts,
                        shortcutState,
                        QStringLiteral("normal"),
                        true,
                        2,
                        [this](const QString& shortcut) {
                            return runtimeBindings.validateShortcut(shortcut);
                        },
                        payload.adjustment == settings::SettingsShortcutAdjustment::ScreenshotDelaySeconds,
                        payload.adjustment == settings::SettingsShortcutAdjustment::ScreenshotDelaySeconds
                            ? runtimeBindings.integerValue(
                                  settings::SettingsIntegerBinding::ScreenshotDelaySeconds)
                            : 3,
                        [this](int value) {
                            return runtimeBindings.applyIntegerValue(
                                settings::SettingsIntegerBinding::ScreenshotDelaySeconds, value);
                        },
                    };
                    auto* control = new ShortcutKeyRow(config, metric, mainWindowMetric, list);
                    control->setObjectName(settings::generatedObjectName(
                        QStringLiteral("settings-item"), definition.id));
                    runtime.shortcutControl = control;
                    runtime.focusTarget = control;
                    runtime.anchor = control;
                    listLayout->addWidget(control);
                    connect(control, &ShortcutKeyRow::clicked, &q,
                            [this, command = payload.command]() { emit q.commandRequested(command); });
                    connect(control, &ShortcutKeyRow::shortcutsChanged, &q,
                            [this, action = payload.shortcutAction](const QStringList& shortcuts) {
                                 if (!runtimeBindings.applyShortcuts(action, shortcuts)) {
                                     syncValues();
                                 }
                            });
                } else if constexpr (std::is_same_v<Payload,
                                                    settings::SettingsActionDefinition>) {
                    auto* control = new adqt::widgets::AdButton(list);
                    control->setButtonStyle(adqt::widgets::AdButton::ButtonStyle::Outline);
                    control->setAccentRole(
                        payload.accent == settings::SettingsActionAccent::Danger
                            ? adqt::widgets::AdButton::AccentRole::Danger
                            : adqt::widgets::AdButton::AccentRole::Neutral);
                    control->setSizeClass(adqt::widgets::AdButton::SizeClass::Medium);
                    if (payload.iconFactory) {
                        control->setIconRef(payload.iconFactory());
                    }
                    runtime.actionControl = control;
                    runtime.focusTarget = control;
                    runtime.anchor = settings_ui::createSettingItemRow(
                        list, colorScheme.metricAlias, &runtime.title, &runtime.description, control,
                        settings::generatedObjectName(QStringLiteral("settings-item"), definition.id));
                    listLayout->addWidget(runtime.anchor);
                    connect(control, &QAbstractButton::clicked, &q,
                            [this, itemId = definition.id]() { triggerAction(itemId); });
                } else if constexpr (std::is_same_v<Payload,
                                                    settings::SettingsCustomDefinition>) {
                    auto* control =
                        createSettingsCustomWidget(payload.renderer, runtimeBindings, list);
                    Q_ASSERT(control != nullptr);
                    if (control == nullptr) {
                        return;
                    }
                    control->setObjectName(settings::generatedObjectName(
                        QStringLiteral("settings-item"), definition.id));
                    runtime.customControl = control;
                    runtime.anchor = control;
                    runtime.focusTarget = control;
                    listLayout->addWidget(control);
                }
            },
            definition.payload);

        if (runtime.focusTarget != nullptr && runtime.focusTarget != runtime.anchor) {
            runtime.focusTarget->setObjectName(settings::generatedObjectName(
                QStringLiteral("settings-control"), definition.id));
        }
        items.push_back(runtime);
    }

    void connectServices() {
        auto& themeManager = snow_shot::presentation::styles::ThemeManager::instance();
        QObject::connect(&themeManager,
                         &snow_shot::presentation::styles::ThemeManager::themeChanged, &q,
                         [this](const auto& scheme) { applyTheme(scheme); });
        QObject::connect(&themeManager,
                         &snow_shot::presentation::styles::ThemeManager::themeModeChanged, &q,
                         [this](auto) { syncValues(); });
        QObject::connect(&runtimeBindings,
                         &settings::SettingsRuntimeBindings::shortcutStateChanged, &q,
                         [this](snow_shot::presentation::GlobalShortcutAction action,
                                const snow_shot::presentation::GlobalShortcutRegistrationState& state) {
                             for (RuntimeItem& item : items) {
                                 const auto* shortcut =
                                     item.definition == nullptr
                                         ? nullptr
                                         : std::get_if<settings::SettingsShortcutActionDefinition>(
                                               &item.definition->payload);
                                 if (shortcut != nullptr && shortcut->shortcutAction == action &&
                                     item.shortcutControl != nullptr) {
                                     item.shortcutControl->setRegistrationState(state);
                                 }
                             }
                         });
        QObject::connect(&runtimeBindings, &settings::SettingsRuntimeBindings::synchronized, &q,
                         [this]() { syncValues(); });

        if (scrollArea != nullptr && scrollArea->verticalScrollBar() != nullptr) {
            QObject::connect(scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, &q,
                             [this](int) { syncVisibleSection(); });
            QObject::connect(scrollArea->verticalScrollBar(), &QScrollBar::rangeChanged, &q,
                             [this](int, int) {
                                 requestScrollGeometryUpdate();
                                 syncVisibleSection();
                             });
        }

        for (RuntimeSection& section : sections) {
            QObject::connect(section.header, &SectionHeaderWidget::resetRequested, &q,
                             [this, reset = section.definition->reset]() { resetSection(reset); });
        }
    }

    void applySelectValue(const QString& itemId, const QVariant& value) {
        const RuntimeItem* item = runtimeItem(itemId);
        if (item == nullptr || item->definition == nullptr) {
            return;
        }
        const auto* select =
            std::get_if<settings::SettingsSelectDefinition>(&item->definition->payload);
        const bool accepted =
            select != nullptr && runtimeBindings.applySelectValue(select->binding, value);
        if (!accepted) {
            syncValues();
        }
    }

    void applySwitchValue(settings::SettingsSwitchBinding binding, bool checked) {
        if (synchronizingValues) {
            return;
        }
        if (!runtimeBindings.applySwitchValue(binding, checked)) {
            syncValues();
        }
    }

    void applyIntegerValue(settings::SettingsIntegerBinding binding, int value) {
        if (!runtimeBindings.applyIntegerValue(binding, value)) {
            syncValues();
        }
    }

    void triggerAction(const QString& itemId) {
        RuntimeItem* item = runtimeItem(itemId);
        if (item == nullptr || item->definition == nullptr || item->modal != nullptr) {
            return;
        }
        const auto* action =
            std::get_if<settings::SettingsActionDefinition>(&item->definition->payload);
        if (action == nullptr) {
            return;
        }
        if (!action->confirmation.has_value()) {
            if (!runtimeBindings.triggerAction(action->binding)) {
                syncValues();
            }
            return;
        }

        const settings::SettingsConfirmationDefinition& confirmation = *action->confirmation;
        auto* modal = new adqt::widgets::AdModal(&q);
        item->modal = modal;
        modal->setObjectName(settings::generatedObjectName(QStringLiteral("settings-modal"),
                                                           item->definition->id));
        modal->setMode(adqt::widgets::AdModal::Mode::Window);
        modal->setOwnerWindow(q.window());
        modal->setPreset(adqt::widgets::AdModal::Preset::Confirm);
        modal->setWindowTitle(confirmation.title.translated());
        modal->setText(confirmation.message.translated());
        modal->setAcceptText(confirmation.acceptText.translated());
        modal->setRejectText(confirmation.rejectText.translated());
        modal->setAcceptAccentRole(adqt::widgets::AdButton::AccentRole::Danger);
        modal->setStandardButtons(adqt::widgets::AdModal::StandardButton::Ok |
                                  adqt::widgets::AdModal::StandardButton::Cancel);
        QObject::connect(modal, &adqt::widgets::AdModal::accepted, &q, [this, binding = action->binding]() {
            if (!runtimeBindings.triggerAction(binding)) {
                syncValues();
            }
        });
        QObject::connect(modal, &adqt::widgets::AdModal::finished, &q,
                         [this, itemId](adqt::widgets::AdModal::DialogCode) {
                             RuntimeItem* finishedItem = runtimeItem(itemId);
                             if (finishedItem != nullptr && finishedItem->modal != nullptr) {
                                 finishedItem->modal->deleteLater();
                                 finishedItem->modal = nullptr;
                             }
                         });
        modal->setOpen(true);
    }

    void resetSection(settings::SettingsSectionReset reset) {
        if (!runtimeBindings.resetSection(reset)) {
            syncValues();
        }
    }

    QList<adqt::widgets::AdSelect::Option>
    selectOptions(const settings::SettingsSelectDefinition& definition) const {
        QList<adqt::widgets::AdSelect::Option> options;
        for (const settings::SettingsOptionDefinition& option : definition.options) {
            options.push_back(selectOption(option.value, option.label.translated()));
        }
        for (const settings::SettingsRuntimeOption& option :
             runtimeBindings.dynamicSelectOptions(definition.binding)) {
            options.push_back(selectOption(option.value, option.label));
        }
        return options;
    }

    void syncValues() {
        const QScopedValueRollback<bool> synchronizationGuard(synchronizingValues, true);
        const auto storageStatus = runtimeBindings.storageStatus();
        for (RuntimeItem& runtime : items) {
            if (runtime.definition == nullptr) {
                continue;
            }
            if (runtime.select != nullptr) {
                const QSignalBlocker blocker(runtime.select);
                const auto* definition = std::get_if<settings::SettingsSelectDefinition>(
                    &runtime.definition->payload);
                if (definition != nullptr) {
                    runtime.select->setCurrentValue(runtimeBindings.selectValue(definition->binding));
                }
                runtime.select->setEnabled(storageStatus.writeAvailable);
            }
            if (runtime.switchControl != nullptr) {
                // AdSwitch refreshes its rendered thumb from toggled; the sync guard prevents
                // this programmatic update from being written back as a user change.
                const auto* definition = std::get_if<settings::SettingsSwitchDefinition>(
                    &runtime.definition->payload);
                if (definition != nullptr) {
                    runtime.switchControl->setChecked(
                        runtimeBindings.switchValue(definition->binding));
                }
                runtime.switchControl->setEnabled(
                    storageStatus.writeAvailable && !storageStatus.historyPolicyUpdating &&
                    runtimeBindings.switchEnabled(definition->binding));
            }
            if (runtime.integerControl != nullptr) {
                const QSignalBlocker blocker(runtime.integerControl);
                const auto* definition = std::get_if<settings::SettingsIntegerDefinition>(
                    &runtime.definition->payload);
                if (definition != nullptr) {
                    runtime.integerControl->setValue(
                        runtimeBindings.integerValue(definition->binding));
                }
                runtime.integerControl->setEnabled(storageStatus.writeAvailable &&
                                                   !storageStatus.historyPolicyUpdating);
            }
            if (runtime.sliderControl != nullptr) {
                const QSignalBlocker blocker(runtime.sliderControl);
                const auto* definition = std::get_if<settings::SettingsSliderDefinition>(
                    &runtime.definition->payload);
                if (definition != nullptr) {
                    const int value = runtimeBindings.sliderValue(definition->binding);
                    runtime.sliderControl->setValue(value);
                    runtime.sliderValue->setText(
                        QStringLiteral("%1%2").arg(value).arg(definition->suffix.translated()));
                }
                runtime.sliderControl->setEnabled(storageStatus.writeAvailable);
            }
            if (runtime.colorControl != nullptr) {
                const QSignalBlocker blocker(runtime.colorControl);
                const auto* definition = std::get_if<settings::SettingsColorDefinition>(
                    &runtime.definition->payload);
                if (definition != nullptr) {
                    runtime.colorControl->setValue(adqt::widgets::AdColorValue::solid(
                        runtimeBindings.colorValue(definition->binding)));
                }
                runtime.colorControl->setDisabled(!storageStatus.writeAvailable);
            }
            if (runtime.radioGroup != nullptr) {
                const QSignalBlocker blocker(runtime.radioGroup);
                const auto* definition = std::get_if<settings::SettingsRadioDefinition>(
                    &runtime.definition->payload);
                if (definition != nullptr) {
                    const QVariant current = runtimeBindings.radioValue(definition->binding);
                    runtime.radioGroup->setCheckedId(runtime.radioValues.indexOf(current));
                }
                for (adqt::widgets::AdRadio* button : std::as_const(runtime.radioButtons)) {
                    button->setEnabled(storageStatus.writeAvailable);
                }
            }
            if (runtime.filePathControl != nullptr) {
                const QSignalBlocker blocker(runtime.filePathControl);
                const auto* definition = std::get_if<settings::SettingsFilePathDefinition>(
                    &runtime.definition->payload);
                if (definition != nullptr) {
                    runtime.filePathControl->setText(
                        runtimeBindings.filePathValue(definition->binding));
                }
                runtime.filePathControl->setEnabled(storageStatus.writeAvailable);
            }
            if (runtime.shortcutControl != nullptr) {
                const auto* definition = std::get_if<settings::SettingsShortcutActionDefinition>(
                    &runtime.definition->payload);
                if (definition != nullptr &&
                    definition->adjustment ==
                        settings::SettingsShortcutAdjustment::ScreenshotDelaySeconds) {
                    runtime.shortcutControl->setDelaySeconds(
                        runtimeBindings.integerValue(
                            settings::SettingsIntegerBinding::ScreenshotDelaySeconds));
                }
            }
            if (runtime.actionControl != nullptr) {
                const auto* definition = std::get_if<settings::SettingsActionDefinition>(
                    &runtime.definition->payload);
                if (definition != nullptr) {
                    const settings::SettingsActionState state =
                        runtimeBindings.actionState(definition->binding);
                    runtime.actionControl->setBusy(state.busy);
                    runtime.actionControl->setEnabled(state.enabled);
                }
            }
        }
        for (RuntimeSection& runtime : sections) {
            if (runtime.definition->reset != settings::SettingsSectionReset::None) {
                runtime.header->setResetEnabled(storageStatus.writeAvailable);
            }
        }
    }

    void retranslateUi() {
        for (RuntimeSection& runtime : sections) {
            runtime.header->setTitle(runtime.definition->title.translated());
        }
        for (RuntimeItem& runtime : items) {
            const settings::SettingsItemDefinition& definition = *runtime.definition;
            const QString title = definition.title.translated();
            const QString description = definition.description.translated();
            if (runtime.title != nullptr) {
                runtime.title->setText(title);
            }
            if (runtime.description != nullptr) {
                runtime.description->setText(description);
            }
            if (runtime.focusTarget != nullptr) {
                runtime.focusTarget->setAccessibleName(title);
                runtime.focusTarget->setAccessibleDescription(description);
            }
            if (runtime.select != nullptr) {
                const auto* select =
                    std::get_if<settings::SettingsSelectDefinition>(&definition.payload);
                Q_ASSERT(select != nullptr);
                const QSignalBlocker blocker(runtime.select);
                runtime.select->setOptions(selectOptions(*select));
            }
            if (runtime.integerControl != nullptr) {
                const auto* integer =
                    std::get_if<settings::SettingsIntegerDefinition>(&definition.payload);
                Q_ASSERT(integer != nullptr);
                runtime.integerControl->setSuffixText(integer->suffix.translated());
            }
            if (runtime.sliderControl != nullptr) {
                const auto* slider =
                    std::get_if<settings::SettingsSliderDefinition>(&definition.payload);
                Q_ASSERT(slider != nullptr);
                const int value = qRound(runtime.sliderControl->value());
                runtime.sliderValue->setText(
                    QStringLiteral("%1%2").arg(value).arg(slider->suffix.translated()));
                runtime.sliderControl->setTooltipFormatter(
                    [suffix = slider->suffix](double current) {
                        return QStringLiteral("%1%2")
                            .arg(qRound(current))
                            .arg(suffix.translated());
                    });
            }
            if (runtime.radioGroup != nullptr) {
                const auto* radio =
                    std::get_if<settings::SettingsRadioDefinition>(&definition.payload);
                Q_ASSERT(radio != nullptr);
                for (int index = 0;
                     index < radio->options.size() && index < runtime.radioButtons.size();
                     ++index) {
                    const QString label = radio->options.at(index).label.translated();
                    runtime.radioButtons.at(index)->setText(label);
                    runtime.radioButtons.at(index)->setAccessibleName(label);
                    runtime.radioButtons.at(index)->setAccessibleDescription(description);
                }
            }
            if (runtime.filePathControl != nullptr) {
                const auto* filePath =
                    std::get_if<settings::SettingsFilePathDefinition>(&definition.payload);
                Q_ASSERT(filePath != nullptr);
                runtime.filePathControl->setSearchButtonText(filePath->buttonText.translated());
                runtime.filePathControl->setPlaceholderText(
                    filePath->fileFilter.translated().section(QStringLiteral(";;"), 0, 0));
            }
            if (runtime.shortcutControl != nullptr) {
                runtime.shortcutControl->setTitle(title);
                runtime.shortcutControl->setAccessibleDescription(description);
                runtime.shortcutControl->retranslateUi();
            }
            if (runtime.actionControl != nullptr) {
                const auto* action =
                    std::get_if<settings::SettingsActionDefinition>(&definition.payload);
                Q_ASSERT(action != nullptr);
                runtime.actionControl->setText(action->buttonText.translated());
                if (runtime.modal != nullptr && action->confirmation.has_value()) {
                    runtime.modal->setWindowTitle(action->confirmation->title.translated());
                    runtime.modal->setText(action->confirmation->message.translated());
                    runtime.modal->setAcceptText(action->confirmation->acceptText.translated());
                    runtime.modal->setRejectText(action->confirmation->rejectText.translated());
                }
            }
            if (runtime.customControl != nullptr) {
                runtime.customControl->retranslateUi();
            }
        }
        syncValues();
        requestScrollGeometryUpdate();
    }

    void applyTheme(const snow_shot::presentation::styles::ThemeColorScheme& scheme) {
        colorScheme = scheme;
        for (RuntimeSection& runtime : sections) {
            runtime.header->applyTheme(scheme);
        }
        for (RuntimeItem& runtime : items) {
            if (runtime.title != nullptr && runtime.description != nullptr) {
                settings_ui::applySettingItemTheme(runtime.title, runtime.description, scheme);
            }
            if (runtime.shortcutControl != nullptr) {
                runtime.shortcutControl->applyTheme(scheme);
            }
            if (runtime.customControl != nullptr) {
                runtime.customControl->applyTheme(scheme);
            }
        }
        requestScrollGeometryUpdate();
        q.update();
    }

    int sectionTop(const RuntimeSection& section) const {
        if (section.header == nullptr || contentWidget == nullptr) {
            return 0;
        }
        return section.header->mapTo(contentWidget, QPoint(0, 0)).y();
    }

    int sectionViewportInset() const {
        return contentLayout != nullptr ? contentLayout->contentsMargins().top() : 0;
    }

    void updateTrailingScrollSpace() {
        if (updatingScrollGeometry || scrollArea == nullptr || contentWidget == nullptr ||
            contentLayout == nullptr || trailingScrollSpace == nullptr || sections.isEmpty() ||
            scrollArea->viewport() == nullptr) {
            return;
        }

        updatingScrollGeometry = true;
        contentLayout->activate();
        const int existingReserve = trailingScrollSpace->height();
        const int naturalHeight =
            qMax(0, contentWidget->sizeHint().height() - existingReserve);
        const int viewportHeight = scrollArea->viewport()->height();
        int reserve = 0;
        // A scrollable page needs enough tail room for its final section to align at the top.
        if (viewportHeight > 0 && naturalHeight > viewportHeight) {
            const int lastSectionTop = sectionTop(sections.constLast());
            const int requiredHeight =
                lastSectionTop - sectionViewportInset() + viewportHeight;
            reserve = qMax(0, requiredHeight - naturalHeight);
        }

        if (trailingScrollSpace->height() != reserve) {
            trailingScrollSpace->setFixedHeight(reserve);
            contentLayout->activate();
            contentWidget->resize(contentWidget->width(), contentWidget->sizeHint().height());
        }
        updatingScrollGeometry = false;
    }

    void requestScrollGeometryUpdate() {
        if (scrollGeometryUpdatePending) {
            return;
        }
        scrollGeometryUpdatePending = true;
        QTimer::singleShot(0, &q, [this]() {
            scrollGeometryUpdatePending = false;
            updateTrailingScrollSpace();
            syncVisibleSection();
        });
    }

    QString visibleSectionId() const {
        if (sections.isEmpty() || scrollArea == nullptr ||
            scrollArea->verticalScrollBar() == nullptr) {
            return {};
        }

        const QScrollBar* scrollBar = scrollArea->verticalScrollBar();
        int activeIndex = 0;
        if (scrollBar->maximum() > scrollBar->minimum() &&
            scrollBar->value() >= scrollBar->maximum()) {
            activeIndex = sections.size() - 1;
        } else {
            const int activationLine =
                scrollBar->value() + sectionViewportInset() + 1;
            for (int index = 1; index < sections.size(); ++index) {
                if (sectionTop(sections.at(index)) > activationLine) {
                    break;
                }
                activeIndex = index;
            }
        }

        const RuntimeSection& section = sections.at(activeIndex);
        return section.definition != nullptr ? section.definition->id : QString();
    }

    void syncVisibleSection() {
        if (suppressVisibleSectionTracking || updatingScrollGeometry || scrollArea == nullptr ||
            scrollArea->verticalScrollBar() == nullptr ||
            scrollArea->verticalScrollBar()->maximum() <=
                scrollArea->verticalScrollBar()->minimum()) {
            return;
        }
        const QString sectionId = visibleSectionId();
        if (sectionId.isEmpty() || sectionId == lastVisibleSectionId) {
            return;
        }
        lastVisibleSectionId = sectionId;
        emit q.visibleSectionChanged(sectionId);
    }

    void scrollToSection(const RuntimeSection& section) {
        if (scrollArea == nullptr || scrollArea->verticalScrollBar() == nullptr) {
            return;
        }
        updateTrailingScrollSpace();
        QScrollBar* scrollBar = scrollArea->verticalScrollBar();
        const int target = sectionTop(section) - sectionViewportInset();
        scrollBar->setValue(qBound(scrollBar->minimum(), target, scrollBar->maximum()));
    }

    void handleWatchedEvent(QObject* watched, QEvent* event) {
        if (event == nullptr || scrollArea == nullptr || watched != scrollArea->viewport()) {
            return;
        }
        switch (event->type()) {
        case QEvent::Resize:
        case QEvent::Show:
        case QEvent::LayoutRequest:
            requestScrollGeometryUpdate();
            break;
        default:
            break;
        }
    }

    void reveal(const settings::SettingsLocation& requested) {
        const settings::SettingsLocation location = catalog.resolveLocation(requested);
        if (location.pageId != page->id || scrollArea == nullptr) {
            return;
        }
        QWidget* target = nullptr;
        QWidget* focus = nullptr;
        RuntimeSection* targetSection = nullptr;
        if (!location.itemId.isEmpty()) {
            if (RuntimeItem* item = runtimeItem(location.itemId)) {
                target = item->anchor;
                focus = item->focusTarget;
            }
        }
        if (target == nullptr) {
            targetSection = runtimeSection(location.sectionId);
            if (targetSection != nullptr) {
                target = targetSection->header;
            }
        }
        const bool previousSuppression = suppressVisibleSectionTracking;
        suppressVisibleSectionTracking = true;
        if (target != nullptr) {
            if (targetSection != nullptr) {
                scrollToSection(*targetSection);
            } else {
                scrollArea->ensureWidgetVisible(target, scrollMarginX, scrollMarginY);
            }
        }
        if (focus != nullptr) {
            QWidget* focusWidget = focus->focusProxy() != nullptr ? focus->focusProxy() : focus;
            if (focusWidget->focusPolicy() != Qt::NoFocus) {
                focusWidget->setFocus(Qt::ShortcutFocusReason);
            }
        }
        suppressVisibleSectionTracking = previousSuppression;
        lastVisibleSectionId = visibleSectionId();
    }

    SettingsPageWidget& q;
    const settings::SettingsCatalog& catalog;
    settings::SettingsRuntimeBindings& runtimeBindings;
    const settings::SettingsPageDefinition* page = nullptr;
    adqt::widgets::AdScrollArea* scrollArea = nullptr;
    QWidget* contentWidget = nullptr;
    QVBoxLayout* contentLayout = nullptr;
    QWidget* trailingScrollSpace = nullptr;
    QVector<RuntimeSection> sections;
    QVector<RuntimeItem> items;
    snow_shot::presentation::styles::ThemeColorScheme colorScheme;
    int scrollMarginX = 0;
    int scrollMarginY = 0;
    QString lastVisibleSectionId;
    bool suppressVisibleSectionTracking = false;
    bool synchronizingValues = false;
    bool updatingScrollGeometry = false;
    bool scrollGeometryUpdatePending = false;
};

SettingsPageWidget::SettingsPageWidget(
    const snow_shot::presentation::settings::SettingsCatalog& catalog, const QString& pageId,
    snow_shot::presentation::settings::SettingsRuntimeBindings& runtimeBindings,
    QWidget* parent)
    : QWidget(parent),
      m_impl(std::make_unique<Impl>(*this, catalog, pageId, runtimeBindings)) {}

SettingsPageWidget::~SettingsPageWidget() = default;

QString SettingsPageWidget::pageId() const {
    return m_impl->page->id;
}

void SettingsPageWidget::reveal(
    const snow_shot::presentation::settings::SettingsLocation& location) {
    m_impl->reveal(location);
}

void SettingsPageWidget::applyTheme(
    const snow_shot::presentation::styles::ThemeColorScheme& scheme) {
    m_impl->applyTheme(scheme);
}

void SettingsPageWidget::retranslateUi() {
    m_impl->retranslateUi();
}

void SettingsPageWidget::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
}

bool SettingsPageWidget::eventFilter(QObject* watched, QEvent* event) {
    if (m_impl != nullptr) {
        m_impl->handleWatchedEvent(watched, event);
    }
    return QWidget::eventFilter(watched, event);
}
