#include "snow_shot/presentation/components/settingscustomwidget.h"

#include "snow_shot/presentation/components/drawingtoolbareditorsettingswidget.h"
#include "snow_shot/presentation/components/storagestatussettingswidget.h"
#include "snow_shot/presentation/screenshottoolbarlayoutmodel.h"
#include "snow_shot/presentation/settings/settingsruntimebindings.h"
#include "snow_shot/presentation/styles/thememanager.h"

#include "widgets/button.h"
#include "widgets/popover.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFrame>
#include <QHash>
#include <QLabel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPalette>
#include <QSet>
#include <QStyle>
#include <QStyleHints>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <utility>

namespace {
namespace toolbar_layout = snow_shot::presentation::toolbar_layout;
namespace storage = snow_shot::storage;

constexpr char kToolbarItemMimeType[] = "application/x-snow-shot-toolbar-item";
constexpr char kToolbarItemProperty[] = "screenshotToolbarItemId";
[[maybe_unused]] constexpr const char* kEditorTranslations[] = {
    QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Visible toolbar"),
    QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Hidden tools"),
    QT_TRANSLATE_NOOP(
        "DrawingToolbarEditorSettingsWidget",
        "Drag tools to change their order or move them between the toolbar and hidden area."),
    QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Drop tools here to show them"),
    QT_TRANSLATE_NOOP("DrawingToolbarEditorSettingsWidget", "Drag tools here to hide them"),
};

enum class ToolbarDropTarget {
    Visible,
    Hidden,
};

QString translatedToolbarText(const char* sourceText) {
    return QApplication::translate("DrawingToolbarEditorSettingsWidget", sourceText);
}

QString cssColor(const QColor& color) {
    if (color.alpha() == 255) {
        return color.name(QColor::HexRgb);
    }
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

class ToolbarDragButton final : public adqt::widgets::AdButton {
  public:
    explicit ToolbarDragButton(const QString& itemId, QWidget* parent)
        : adqt::widgets::AdButton(parent), m_itemId(itemId) {
        setProperty(kToolbarItemProperty, itemId);
        setObjectName(QStringLiteral("settings-drawing-toolbar-item-%1").arg(itemId));
        setButtonStyle(ButtonStyle::Text);
        setShape(Shape::Circle);
        setCursor(Qt::OpenHandCursor);
        setFixedSize(40, 40);
        setIconSize(QSize(22, 22));
    }

  protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event != nullptr && event->button() == Qt::LeftButton) {
            m_pressPosition = event->position().toPoint();
        }
        adqt::widgets::AdButton::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (event == nullptr || !(event->buttons() & Qt::LeftButton) ||
            (event->position().toPoint() - m_pressPosition).manhattanLength() <
                QApplication::startDragDistance()) {
            adqt::widgets::AdButton::mouseMoveEvent(event);
            return;
        }

        auto* mimeData = new QMimeData();
        mimeData->setData(kToolbarItemMimeType, m_itemId.toUtf8());
        auto* drag = new QDrag(this);
        drag->setMimeData(mimeData);
        QPixmap preview(size());
        preview.fill(Qt::transparent);
        render(&preview);
        drag->setPixmap(preview);
        drag->setHotSpot(m_pressPosition);
        setCursor(Qt::ClosedHandCursor);
        static_cast<void>(drag->exec(Qt::MoveAction));
        setCursor(Qt::OpenHandCursor);
    }

  private:
    QString m_itemId;
    QPoint m_pressPosition;
};

class ToolbarDropZone final : public QFrame {
  public:
    using DropHandler = std::function<void(const QString&, ToolbarDropTarget, int)>;

    ToolbarDropZone(ToolbarDropTarget target, DropHandler handler, QWidget* parent)
        : QFrame(parent), m_target(target), m_dropHandler(std::move(handler)) {
        setAcceptDrops(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setMinimumHeight(56);
        m_layout = new QHBoxLayout(this);
        m_layout->setContentsMargins(8, 8, 8, 8);
        m_layout->setSpacing(4);
        m_layout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    }

    [[nodiscard]] QBoxLayout* contentLayout() const {
        return m_layout;
    }

  protected:
    void dragEnterEvent(QDragEnterEvent* event) override {
        if (hasToolbarItem(event != nullptr ? event->mimeData() : nullptr)) {
            setProperty("dragActive", true);
            style()->unpolish(this);
            style()->polish(this);
            event->acceptProposedAction();
            return;
        }
        QFrame::dragEnterEvent(event);
    }

    void dragMoveEvent(QDragMoveEvent* event) override {
        if (hasToolbarItem(event != nullptr ? event->mimeData() : nullptr)) {
            event->acceptProposedAction();
            return;
        }
        QFrame::dragMoveEvent(event);
    }

    void dragLeaveEvent(QDragLeaveEvent* event) override {
        setProperty("dragActive", false);
        style()->unpolish(this);
        style()->polish(this);
        QFrame::dragLeaveEvent(event);
    }

    void dropEvent(QDropEvent* event) override {
        setProperty("dragActive", false);
        style()->unpolish(this);
        style()->polish(this);
        if (!hasToolbarItem(event != nullptr ? event->mimeData() : nullptr)) {
            QFrame::dropEvent(event);
            return;
        }
        const QString itemId = QString::fromUtf8(event->mimeData()->data(kToolbarItemMimeType));
        if (toolbar_layout::descriptor(itemId) == nullptr) {
            event->ignore();
            return;
        }
        if (m_dropHandler) {
            m_dropHandler(itemId, m_target, insertionIndex(event->position().toPoint()));
        }
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }

  private:
    static bool hasToolbarItem(const QMimeData* mimeData) {
        return mimeData != nullptr && mimeData->hasFormat(kToolbarItemMimeType);
    }

    int insertionIndex(const QPoint& position) const {
        int itemIndex = 0;
        if (m_layout == nullptr) {
            return itemIndex;
        }
        for (int layoutIndex = 0; layoutIndex < m_layout->count(); ++layoutIndex) {
            QWidget* widget = m_layout->itemAt(layoutIndex)->widget();
            if (widget == nullptr || !widget->property(kToolbarItemProperty).isValid()) {
                continue;
            }
            if (position.x() < widget->geometry().center().x()) {
                return itemIndex;
            }
            ++itemIndex;
        }
        return itemIndex;
    }

    ToolbarDropTarget m_target;
    DropHandler m_dropHandler;
    QBoxLayout* m_layout = nullptr;
};

void clearLayoutItems(QBoxLayout* layout) {
    if (layout == nullptr) {
        return;
    }
    while (QLayoutItem* item = layout->takeAt(0)) {
        delete item;
    }
}
} // namespace

struct DrawingToolbarEditorSettingsWidget::Private {
    Private(DrawingToolbarEditorSettingsWidget& sourceOwner,
            snow_shot::presentation::settings::SettingsRuntimeBindings& sourceRuntimeBindings)
        : owner(sourceOwner), runtimeBindings(sourceRuntimeBindings),
          colorScheme(
              snow_shot::presentation::styles::ThemeManager::instance().themeColorScheme()) {}

    void initialize() {
        owner.setObjectName(QStringLiteral("settings-drawing-toolbar-editor"));
        auto* rootLayout = new QVBoxLayout(&owner);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(8);

        visibleLabel = new QLabel(&owner);
        visibleLabel->setObjectName(QStringLiteral("settings-drawing-toolbar-visible-label"));
        rootLayout->addWidget(visibleLabel);
        visibleZone = new ToolbarDropZone(
            ToolbarDropTarget::Visible,
            [this](const QString& itemId, ToolbarDropTarget target, int index) {
                applyDrop(itemId, target, index);
            },
            &owner);
        visibleZone->setObjectName(QStringLiteral("settings-drawing-toolbar-visible-zone"));
        rootLayout->addWidget(visibleZone);

        hiddenLabel = new QLabel(&owner);
        hiddenLabel->setObjectName(QStringLiteral("settings-drawing-toolbar-hidden-label"));
        rootLayout->addWidget(hiddenLabel);
        hiddenZone = new ToolbarDropZone(
            ToolbarDropTarget::Hidden,
            [this](const QString& itemId, ToolbarDropTarget target, int index) {
                applyDrop(itemId, target, index);
            },
            &owner);
        hiddenZone->setObjectName(QStringLiteral("settings-drawing-toolbar-hidden-zone"));
        rootLayout->addWidget(hiddenZone);

        instructionLabel = new QLabel(&owner);
        instructionLabel->setObjectName(
            QStringLiteral("settings-drawing-toolbar-instruction"));
        instructionLabel->setWordWrap(true);
        rootLayout->addWidget(instructionLabel);

        visibleEmptyLabel = new QLabel(visibleZone);
        visibleEmptyLabel->setObjectName(
            QStringLiteral("settings-drawing-toolbar-visible-empty"));
        hiddenEmptyLabel = new QLabel(hiddenZone);
        hiddenEmptyLabel->setObjectName(QStringLiteral("settings-drawing-toolbar-hidden-empty"));

        for (const toolbar_layout::Descriptor& descriptor : toolbar_layout::descriptors()) {
            const QString itemId = QString::fromLatin1(descriptor.id);
            auto* button = new ToolbarDragButton(itemId, &owner);
            button->setIconRef(toolbar_layout::icon(descriptor.icon));
            buttons.insert(itemId, button);
            if (!descriptor.children.isEmpty()) {
                addGroupPopover(button, descriptor);
            }
        }

        QObject::connect(
            &runtimeBindings,
            &snow_shot::presentation::settings::SettingsRuntimeBindings::synchronized, &owner,
            [this]() { syncFromRuntime(); });

        layout = toolbar_layout::normalizedLayout(runtimeBindings.toolbarLayout());
        owner.retranslateUi();
        owner.applyTheme(colorScheme);
        rebuild();
    }

    void addGroupPopover(ToolbarDragButton* trigger,
                         const toolbar_layout::Descriptor& descriptor) {
        auto* popover = new adqt::widgets::AdPopover(trigger);
        popover->setSourceWidget(trigger);
        popover->setTriggers(adqt::widgets::AdPopover::Trigger::Hover);
        popover->setPlacement(adqt::widgets::AdPopover::Placement::Top);
        popover->setPopupLayerMode(adqt::widgets::AdPopover::PopupLayerMode::QtTool);
        popover->setArrowVisible(true);
        popover->setObjectName(
            QStringLiteral("settings-drawing-toolbar-popover-%1").arg(descriptor.id));

        auto* content = new QWidget();
        content->setObjectName(
            QStringLiteral("settings-drawing-toolbar-popover-content-%1").arg(descriptor.id));
        auto* contentLayout = new QHBoxLayout(content);
        contentLayout->setContentsMargins(4, 4, 4, 4);
        contentLayout->setSpacing(4);
        for (const toolbar_layout::ChildDescriptor& child : descriptor.children) {
            auto* option = new adqt::widgets::AdButton(content);
            option->setButtonStyle(adqt::widgets::AdButton::ButtonStyle::Text);
            option->setShape(adqt::widgets::AdButton::Shape::Circle);
            option->setFixedSize(36, 36);
            option->setIconSize(QSize(20, 20));
            option->setIconRef(toolbar_layout::icon(child.icon));
            option->setToolTip(translatedToolbarText(child.label));
            option->setAccessibleName(translatedToolbarText(child.label));
            option->setProperty("toolbarChildSourceText", QString::fromLatin1(child.label));
            contentLayout->addWidget(option);
        }
        popover->setContentWidget(content);
    }

    void rebuild() {
        clearLayoutItems(visibleZone->contentLayout());
        clearLayoutItems(hiddenZone->contentLayout());
        const QSet<QString> hidden(layout.hidden.cbegin(), layout.hidden.cend());
        int visibleCount = 0;
        int hiddenCount = 0;
        for (const QString& itemId : layout.order) {
            ToolbarDragButton* button = buttons.value(itemId);
            if (button == nullptr) {
                continue;
            }
            button->show();
            if (hidden.contains(itemId)) {
                hiddenZone->contentLayout()->addWidget(button);
                ++hiddenCount;
            } else {
                visibleZone->contentLayout()->addWidget(button);
                ++visibleCount;
            }
        }
        visibleEmptyLabel->setVisible(visibleCount == 0);
        hiddenEmptyLabel->setVisible(hiddenCount == 0);
        if (visibleCount == 0) {
            visibleZone->contentLayout()->addWidget(visibleEmptyLabel);
        }
        if (hiddenCount == 0) {
            hiddenZone->contentLayout()->addWidget(hiddenEmptyLabel);
        }
        visibleZone->contentLayout()->invalidate();
        hiddenZone->contentLayout()->invalidate();
    }

    void syncFromRuntime() {
        const storage::ScreenshotToolbarLayout synchronized =
            toolbar_layout::normalizedLayout(runtimeBindings.toolbarLayout());
        if (synchronized != layout) {
            layout = synchronized;
            rebuild();
        }
    }

    void applyDrop(const QString& itemId, ToolbarDropTarget target, int insertionIndex) {
        if (toolbar_layout::descriptor(itemId) == nullptr) {
            return;
        }
        const storage::ScreenshotToolbarLayout previous = layout;
        storage::ScreenshotToolbarLayout candidate = previous;
        if (target == ToolbarDropTarget::Hidden) {
            if (!candidate.hidden.contains(itemId)) {
                candidate.hidden.push_back(itemId);
            }
            candidate = toolbar_layout::normalizedLayout(candidate);
        } else {
            const bool wasVisible = !candidate.hidden.contains(itemId);
            QStringList visibleItems;
            const QSet<QString> hiddenBefore(candidate.hidden.cbegin(), candidate.hidden.cend());
            for (const QString& orderedId : candidate.order) {
                if (!hiddenBefore.contains(orderedId)) {
                    visibleItems.push_back(orderedId);
                }
            }
            const int previousVisibleIndex = visibleItems.indexOf(itemId);
            visibleItems.removeAll(itemId);
            if (wasVisible && previousVisibleIndex >= 0 && insertionIndex > previousVisibleIndex) {
                --insertionIndex;
            }
            insertionIndex =
                std::clamp(insertionIndex, 0, static_cast<int>(visibleItems.size()));
            visibleItems.insert(insertionIndex, itemId);
            candidate.hidden.removeAll(itemId);

            const QSet<QString> hiddenAfter(candidate.hidden.cbegin(), candidate.hidden.cend());
            int visibleIndex = 0;
            for (QString& orderedId : candidate.order) {
                if (!hiddenAfter.contains(orderedId) && visibleIndex < visibleItems.size()) {
                    orderedId = visibleItems.at(visibleIndex++);
                }
            }
            candidate = toolbar_layout::normalizedLayout(candidate);
        }

        if (candidate == previous) {
            return;
        }
        if (!runtimeBindings.applyToolbarLayout(candidate)) {
            layout = previous;
            rebuild();
            return;
        }
        layout = candidate;
        rebuild();
    }

    void applyTheme(const snow_shot::presentation::styles::ThemeColorScheme& scheme) {
        colorScheme = scheme;
        const QColor border = scheme.map.colorBorder.isValid()
                                  ? scheme.map.colorBorder
                                  : QColor(QStringLiteral("#D9D9D9"));
        const QColor surface = scheme.map.colorBgContainer.isValid()
                                   ? scheme.map.colorBgContainer
                                   : QColor(Qt::white);
        const QColor activeBorder = scheme.map.colorPrimary.isValid()
                                        ? scheme.map.colorPrimary
                                        : QColor(QStringLiteral("#1677FF"));
        const QString zoneStyle =
            QStringLiteral(
                "QFrame { background: %1; border: 1px solid %2; border-radius: 8px; } "
                "QFrame[dragActive=\"true\"] { border: 2px solid %3; }")
                .arg(cssColor(surface), cssColor(border), cssColor(activeBorder));
        visibleZone->setStyleSheet(zoneStyle);
        hiddenZone->setStyleSheet(zoneStyle);

        const QVector<QLabel*> primaryLabels{visibleLabel, hiddenLabel};
        const QVector<QLabel*> secondaryLabels{instructionLabel, visibleEmptyLabel,
                                                hiddenEmptyLabel};
        for (QLabel* label : primaryLabels) {
            QPalette palette = label->palette();
            palette.setColor(QPalette::WindowText, scheme.map.colorText);
            label->setPalette(palette);
        }
        for (QLabel* label : secondaryLabels) {
            QPalette palette = label->palette();
            palette.setColor(QPalette::WindowText, scheme.map.colorTextSecondary);
            label->setPalette(palette);
        }
        owner.update();
    }

    void retranslateUi() {
        visibleLabel->setText(translatedToolbarText("Visible toolbar"));
        hiddenLabel->setText(translatedToolbarText("Hidden tools"));
        instructionLabel->setText(translatedToolbarText(
            "Drag tools to change their order or move them between the toolbar and hidden area."));
        visibleEmptyLabel->setText(translatedToolbarText("Drop tools here to show them"));
        hiddenEmptyLabel->setText(translatedToolbarText("Drag tools here to hide them"));
        for (const toolbar_layout::Descriptor& descriptor : toolbar_layout::descriptors()) {
            ToolbarDragButton* button = buttons.value(QString::fromLatin1(descriptor.id));
            if (button != nullptr) {
                const QString label = translatedToolbarText(descriptor.label);
                button->setToolTip(label);
                button->setAccessibleName(label);
            }
        }
        for (adqt::widgets::AdButton* option :
             owner.findChildren<adqt::widgets::AdButton*>()) {
            const QString sourceText = option->property("toolbarChildSourceText").toString();
            if (!sourceText.isEmpty()) {
                const QString label = translatedToolbarText(sourceText.toUtf8().constData());
                option->setToolTip(label);
                option->setAccessibleName(label);
            }
        }
    }

    DrawingToolbarEditorSettingsWidget& owner;
    snow_shot::presentation::settings::SettingsRuntimeBindings& runtimeBindings;
    snow_shot::presentation::styles::ThemeColorScheme colorScheme;
    storage::ScreenshotToolbarLayout layout;
    QLabel* visibleLabel = nullptr;
    QLabel* hiddenLabel = nullptr;
    QLabel* instructionLabel = nullptr;
    QLabel* visibleEmptyLabel = nullptr;
    QLabel* hiddenEmptyLabel = nullptr;
    ToolbarDropZone* visibleZone = nullptr;
    ToolbarDropZone* hiddenZone = nullptr;
    QHash<QString, ToolbarDragButton*> buttons;
};

DrawingToolbarEditorSettingsWidget::DrawingToolbarEditorSettingsWidget(
    snow_shot::presentation::settings::SettingsRuntimeBindings& runtimeBindings,
    QWidget* parent)
    : SettingsCustomWidget(parent), m_private(std::make_unique<Private>(*this, runtimeBindings)) {
    m_private->initialize();
}

DrawingToolbarEditorSettingsWidget::~DrawingToolbarEditorSettingsWidget() = default;

void DrawingToolbarEditorSettingsWidget::applyTheme(
    const snow_shot::presentation::styles::ThemeColorScheme& scheme) {
    m_private->applyTheme(scheme);
}

void DrawingToolbarEditorSettingsWidget::retranslateUi() {
    m_private->retranslateUi();
}

void DrawingToolbarEditorSettingsWidget::changeEvent(QEvent* event) {
    SettingsCustomWidget::changeEvent(event);
    if (event != nullptr && event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
}

SettingsCustomWidget* createSettingsCustomWidget(
    snow_shot::presentation::settings::SettingsCustomRenderer renderer,
    snow_shot::presentation::settings::SettingsRuntimeBindings& runtimeBindings,
    QWidget* parent) {
    using snow_shot::presentation::settings::SettingsCustomRenderer;
    switch (renderer) {
    case SettingsCustomRenderer::StorageStatus:
        return new StorageStatusSettingsWidget(runtimeBindings, parent);
    case SettingsCustomRenderer::DrawingToolbarEditor:
        return new DrawingToolbarEditorSettingsWidget(runtimeBindings, parent);
    }
    return nullptr;
}
