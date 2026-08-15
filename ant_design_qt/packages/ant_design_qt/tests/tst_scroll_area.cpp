#include "widgets/scroll_area.h"

#include <QEnterEvent>
#include <QScrollBar>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

namespace {

class TallerMinimumHintContent final : public QWidget {
 public:
  TallerMinimumHintContent() {
    auto* rows = new QVBoxLayout(this);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(12);

    firstRow = new QWidget(this);
    firstRow->setFixedHeight(24);
    rows->addWidget(firstRow);

    secondRow = new QWidget(this);
    secondRow->setFixedHeight(24);
    rows->addWidget(secondRow);
  }

  QSize sizeHint() const override { return QSize(240, 60); }
  QSize minimumSizeHint() const override { return QSize(80, 180); }

  QWidget* firstRow = nullptr;
  QWidget* secondRow = nullptr;
};

}  // namespace

class ScrollAreaTest final : public QObject {
  Q_OBJECT

 private slots:
  void horizontalOverlayMirrorsTheNativeScrollRange();
  void embeddedHorizontalScrollBarKeepsAStableExtent();
  void compactScrollBarBoundsItsMinimumSliderLength();
  void fitToWidthDoesNotStretchToTheMinimumHintHeight();
};

void ScrollAreaTest::horizontalOverlayMirrorsTheNativeScrollRange() {
  adqt::widgets::AdScrollArea area;
  area.resize(180, 120);
  area.setFitToWidth(false);

  auto* content = new QWidget;
  content->setMinimumSize(560, 360);
  content->resize(content->minimumSize());
  area.setContentWidget(content);
  area.show();
  QCoreApplication::processEvents();

  auto* overlay = area.overlayHorizontalScrollBar();
  QScrollBar* source = area.horizontalScrollBar();
  QVERIFY(overlay != nullptr);
  QVERIFY(source != nullptr);
  QVERIFY(source->maximum() > source->minimum());
  QVERIFY(overlay->isVisible());
  QCOMPARE(overlay->minimum(), source->minimum());
  QCOMPARE(overlay->maximum(), source->maximum());
  QCOMPARE(overlay->pageStep(), source->pageStep());
  QCOMPARE(overlay->geometry().bottom(), area.viewport()->rect().bottom() - 2);
  QCOMPARE(overlay->geometry().left(), area.viewport()->rect().left() + 2);

  overlay->setValue(overlay->maximum());
  QCOMPARE(source->value(), source->maximum());
  source->setValue(source->minimum());
  QCOMPARE(overlay->value(), overlay->minimum());
}

void ScrollAreaTest::embeddedHorizontalScrollBarKeepsAStableExtent() {
  QWidget host;
  host.resize(240, 80);
  adqt::widgets::AdScrollBar bar(Qt::Horizontal, &host);
  bar.setScrollBarThickness(10);
  bar.setEmbedded(true);
  bar.setRange(0, 100);
  bar.setPageStep(20);
  bar.setOverlayMargins({0, 0, 0, 0});
  bar.setOverlayBounds(host.rect());
  bar.show();
  host.show();
  QCoreApplication::processEvents();

  const QRect geometry = bar.geometry();
  QCOMPARE(geometry.height(), 10);
  QCOMPARE(geometry.width(), host.width());
  QCOMPARE(geometry.bottom(), host.rect().bottom());
  QVERIFY(bar.isEmbedded());
  QVERIFY(!bar.isExpanded());

  QEnterEvent enter(QPointF(1.0, 1.0), QPointF(1.0, 1.0), QPointF(1.0, 1.0));
  QCoreApplication::sendEvent(&bar, &enter);
  QCOMPARE(bar.geometry(), geometry);
  QVERIFY(!bar.isExpanded());
}

void ScrollAreaTest::compactScrollBarBoundsItsMinimumSliderLength() {
  QWidget host;
  host.resize(24, 20);
  adqt::widgets::AdScrollBar bar(Qt::Vertical, &host);
  bar.setGeometry(14, 2, 8, 16);
  bar.setEmbedded(true);
  bar.setRange(0, 100);
  bar.setPageStep(10);
  bar.show();
  host.show();
  QCoreApplication::processEvents();

  QCOMPARE(bar.height(), 16);
  QVERIFY(bar.isVisible());
}

void ScrollAreaTest::fitToWidthDoesNotStretchToTheMinimumHintHeight() {
  adqt::widgets::AdScrollArea area;
  area.resize(320, 100);
  area.setFitToWidth(true);

  auto* content = new TallerMinimumHintContent;
  area.setContentWidget(content);
  area.show();
  QCoreApplication::processEvents();

  QCOMPARE(content->width(), area.viewport()->width());
  QCOMPARE(content->height(), content->sizeHint().height());
  QVERIFY(content->height() < content->minimumSizeHint().height());
  QCOMPARE(content->secondRow->geometry().top() - content->firstRow->geometry().bottom() - 1,
           content->layout()->spacing());

  area.setFitToWidth(false);
  QCoreApplication::processEvents();
  QCOMPARE(content->height(), content->minimumSizeHint().height());
}

QTEST_MAIN(ScrollAreaTest)

#include "tst_scroll_area.moc"
