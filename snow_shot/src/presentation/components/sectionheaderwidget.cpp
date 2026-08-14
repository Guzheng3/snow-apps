#include "snow_shot/presentation/components/sectionheaderwidget.h"

#include "snow_shot/presentation/components/themedheadericonbutton.h"
#include "snow_shot/presentation/styles/thememanager.h"
#include "snow_shot/presentation/styles/themecolorscheme.h"

#include "antd_icons.h"
#include "widgets/button.h"
#include "widgets/popconfirm.h"

#include <QFont>
#include <QHBoxLayout>
#include <QEvent>
#include <QLabel>
#include <QPalette>

namespace {
namespace outlined_icons = adqt::icons::antd::outlined;
} // namespace

SectionHeaderWidget::SectionHeaderWidget(
    const QString& title, const snow_shot::presentation::styles::ThemeAliasMetricToken& metric,
    QWidget* parent)
    : QFrame(parent), m_title(title), m_titleLabel(new QLabel(title, this)),
      m_resetButton(new ThemedHeaderIconButton(metric, outlined_icons::Reload(), this)),
      m_resetPopconfirm(new adqt::widgets::AdPopconfirm(this)),
      m_sectionVerticalMargin(metric.marginMD), m_formBottomMargin(metric.margin) {
    setAutoFillBackground(false);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    m_headerLayout = new QHBoxLayout(this);
    m_headerLayout->setContentsMargins(0, m_sectionVerticalMargin, 0,
                                       m_sectionVerticalMargin);
    m_headerLayout->setSpacing(metric.marginXS);

    m_headerLayout->addWidget(m_titleLabel);
    m_headerLayout->addStretch();

    m_resetButton->setObjectName(QStringLiteral("sectionResetButton"));
    m_resetButton->setToolTip(tr("Reset"));
    m_resetButton->setAccessibleName(tr("Reset"));
    m_headerLayout->addWidget(m_resetButton);

    m_resetPopconfirm->setObjectName(QStringLiteral("sectionResetPopconfirm"));
    m_resetPopconfirm->setSourceWidget(m_resetButton);
    connect(m_resetPopconfirm, &adqt::widgets::AdPopconfirm::accepted, this,
            &SectionHeaderWidget::resetRequested);

    const auto& themeManager = snow_shot::presentation::styles::ThemeManager::instance();
    connect(&themeManager, &snow_shot::presentation::styles::ThemeManager::themeChanged, this,
            &SectionHeaderWidget::applyTheme);
    retranslateUi();
    applyTheme(themeManager.themeColorScheme());
}

QString SectionHeaderWidget::title() const {
    return m_title;
}

void SectionHeaderWidget::setTitle(const QString& title) {
    m_title = title;
    if (m_titleLabel != nullptr) {
        m_titleLabel->setText(m_title);
    }
    updateResetConfirmationText();
}

void SectionHeaderWidget::setPresentation(Presentation presentation) {
    if (m_presentation == presentation) {
        return;
    }

    m_presentation = presentation;
    if (m_headerLayout != nullptr) {
        if (m_presentation == Presentation::FormGroup) {
            m_headerLayout->setContentsMargins(0, 0, 0, m_formBottomMargin);
        } else {
            m_headerLayout->setContentsMargins(0, m_sectionVerticalMargin, 0,
                                               m_sectionVerticalMargin);
        }
    }
    applyTheme(snow_shot::presentation::styles::ThemeManager::instance().themeColorScheme());
    updateGeometry();
}

void SectionHeaderWidget::setResetVisible(bool visible) {
    m_resetVisible = visible;
    m_resetButton->setVisible(visible);
    m_resetPopconfirm->setEnabled(visible && m_resetButton->isEnabled());
}

void SectionHeaderWidget::setResetEnabled(bool enabled) {
    m_resetButton->setEnabled(enabled);
    m_resetPopconfirm->setEnabled(m_resetVisible && enabled);
}

void SectionHeaderWidget::changeEvent(QEvent* event) {
    QFrame::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
}

void SectionHeaderWidget::retranslateUi() {
    m_resetButton->setToolTip(tr("Reset"));
    m_resetButton->setAccessibleName(tr("Reset"));
    updateResetConfirmationText();
}

void SectionHeaderWidget::updateResetConfirmationText() {
    m_resetPopconfirm->setText(tr("Reset \"%1\" to default settings?").arg(m_title));
}

void SectionHeaderWidget::applyTheme(
    const snow_shot::presentation::styles::ThemeColorScheme& scheme) {
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, Qt::transparent);
    setPalette(palette);

    QPalette labelPalette = m_titleLabel->palette();
    labelPalette.setColor(QPalette::WindowText, scheme.map.colorText);
    m_titleLabel->setPalette(labelPalette);

    QFont titleFont = m_titleLabel->font();
    titleFont.setPixelSize(scheme.metricAlias.fontSizeXL);
    titleFont.setWeight(m_presentation == Presentation::FormGroup ? QFont::DemiBold
                                                                  : QFont::Bold);
    m_titleLabel->setFont(titleFont);

    update();
}
