#include "control_scale.h"

#include <QCoreApplication>
#include <QEvent>
#include <QLayout>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <limits>

namespace adqt::widgets {
namespace {

constexpr auto kScaleContextProperty = "_adqt_control_scale_context";

QVariant contextVariant(const AdControlScaleContext& context) {
  return QVariant::fromValue(context);
}

}  // namespace

qreal AdControlScaleContext::normalizeScale(qreal value) {
  if (!std::isfinite(value) || value <= 0.0) {
    return 1.0;
  }
  return std::clamp<qreal>(value, 0.25, 4.0);
}

qreal AdControlScaleContext::normalizeDpr(qreal value) {
  if (!std::isfinite(value) || value <= 0.0) {
    return 1.0;
  }
  return value;
}

AdControlScaleContext AdControlScaleContext::fromDprs(qreal reference, qreal current,
                                                      quint64 revision) {
  AdControlScaleContext result;
  result.referenceDpr = normalizeDpr(reference);
  result.currentDpr = normalizeDpr(current);
  result.logicalScale = normalizeScale(result.referenceDpr / result.currentDpr);
  result.revision = revision;
  return result;
}

bool AdControlScaleContext::equivalentTo(const AdControlScaleContext& other) const {
  return qFuzzyCompare(referenceDpr + 1.0, other.referenceDpr + 1.0) &&
         qFuzzyCompare(currentDpr + 1.0, other.currentDpr + 1.0) &&
         qFuzzyCompare(logicalScale + 1.0, other.logicalScale + 1.0);
}

AdControlScaleScope::AdControlScaleScope(QWidget* root, QObject* parent)
    : QObject(parent ? parent : root), root_(root) {
  qRegisterMetaType<AdControlScaleContext>();
  if (root_) {
    root_->setProperty(kScaleContextProperty, contextVariant(context_));
  }
}

QWidget* AdControlScaleScope::rootWidget() const { return root_; }

AdControlScaleContext AdControlScaleScope::context() const { return context_; }

QSize AdControlScaleScope::logicalClientExtent() const { return logicalClientExtent_; }

bool AdControlScaleScope::publishScale(qreal referenceDpr, qreal currentDpr,
                                       const QSize& logicalClientExtent) {
  return publishScale(
      AdControlScaleContext::fromDprs(referenceDpr, currentDpr, context_.revision + 1),
      logicalClientExtent);
}

bool AdControlScaleScope::publishScale(const AdControlScaleContext& requested,
                                       const QSize& logicalClientExtent) {
  if (!root_) {
    return false;
  }

  AdControlScaleContext next = AdControlScaleContext::fromDprs(
      requested.referenceDpr, requested.currentDpr, context_.revision + 1);
  if (context_.equivalentTo(next) && logicalClientExtent_ == logicalClientExtent) {
    return false;
  }

  QList<AdControlScaleParticipant*> participants;
  const auto appendParticipant = [&participants](QWidget* widget) {
    if (auto* participant = dynamic_cast<AdControlScaleParticipant*>(widget)) {
      participants.append(participant);
    }
  };
  appendParticipant(root_);
  const QList<QWidget*> descendants = root_->findChildren<QWidget*>();
  for (QWidget* widget : descendants) {
    appendParticipant(widget);
  }

  const bool updatesWereEnabled = root_->updatesEnabled();
  const QWidget* topLevel = root_->window();
  const bool submitRootUpdate = updatesWereEnabled && (topLevel == root_ || topLevel == nullptr ||
                                                       topLevel->updatesEnabled());
  if (updatesWereEnabled) {
    root_->setUpdatesEnabled(false);
  }

  context_ = next;
  logicalClientExtent_ = logicalClientExtent;
  root_->setProperty(kScaleContextProperty, contextVariant(context_));
  for (AdControlScaleParticipant* participant : participants) {
    participant->prepareControlScale(context_);
  }
  for (AdControlScaleParticipant* participant : participants) {
    participant->commitControlScale(context_);
  }

  if (logicalClientExtent_.isValid() && !logicalClientExtent_.isEmpty()) {
    root_->resize(logicalClientExtent_);
  }

  for (QWidget* widget : descendants) {
    QCoreApplication::removePostedEvents(widget, QEvent::LayoutRequest);
  }
  QCoreApplication::removePostedEvents(root_, QEvent::LayoutRequest);

  if (QLayout* layout = root_->layout()) {
    layout->invalidate();
    layout->activate();
  }
  QCoreApplication::removePostedEvents(root_, QEvent::LayoutRequest);

  if (updatesWereEnabled) {
    root_->setUpdatesEnabled(true);
    if (submitRootUpdate) {
      root_->update();
    }
  }
  emit scaleCommitted(context_, logicalClientExtent_);
  return true;
}

QVector<int> scaleCumulativeEdges(const QVector<qreal>& referenceEdges, qreal logicalScale,
                                  int targetExtent) {
  QVector<int> result;
  result.reserve(referenceEdges.size());
  const qreal scale = AdControlScaleContext::normalizeScale(logicalScale);
  int previous = std::numeric_limits<int>::min();
  for (qreal edge : referenceEdges) {
    int scaled = qRound(edge * scale);
    if (previous != std::numeric_limits<int>::min()) {
      scaled = std::max(previous, scaled);
    }
    result.append(scaled);
    previous = scaled;
  }
  if (!result.isEmpty() && targetExtent >= 0) {
    result.last() = targetExtent;
    for (qsizetype index = result.size() - 1; index > 0; --index) {
      if (result.at(index - 1) > result.at(index)) {
        result[index - 1] = result.at(index);
      }
    }
  }
  return result;
}

QVector<int> scaleCumulativeWidths(const QVector<int>& referenceWidths, qreal logicalScale,
                                   int targetExtent) {
  QVector<qreal> edges;
  edges.reserve(referenceWidths.size() + 1);
  qreal edge = 0.0;
  edges.append(edge);
  for (int width : referenceWidths) {
    edge += std::max(0, width);
    edges.append(edge);
  }
  return scaleCumulativeEdges(edges, logicalScale, targetExtent);
}

AdControlScaleContext controlScaleContextFor(const QWidget* widget) {
  const QWidget* current = widget;
  while (current) {
    const QVariant value = current->property(kScaleContextProperty);
    if (value.isValid() && value.canConvert<AdControlScaleContext>()) {
      return value.value<AdControlScaleContext>();
    }
    current = current->parentWidget();
  }
  return AdControlScaleContext();
}

}  // namespace adqt::widgets
