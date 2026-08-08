#include "icon_registry.h"

#include <QCache>
#include <QCryptographicHash>
#include <QIconEngine>
#include <QMutex>
#include <QMutexLocker>
#include <QPaintDevice>
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <QSvgRenderer>
#include <QWaitCondition>
#include <QtMath>

#include <algorithm>
#include <utility>

namespace adqt::icons::detail {

struct IconRefAccess final {
  static const IconKey& key(const IconRef& ref) { return ref.key_; }
  static const IconColors& colors(const IconRef& ref) { return ref.colors_; }
};

struct StoredIcon final {
  IconMetadata metadata;
  QByteArray svgTemplate;
};

struct IconPixmapCacheKey final {
  IconKey key;
  QSize physicalSize;
  QIcon::Mode mode = QIcon::Normal;
  QIcon::State state = QIcon::Off;
  IconFit fit = IconFit::Contain;
  int alignment = Qt::AlignCenter;
  quint64 paletteRevision = 1;
  quint64 statePaletteRevision = 0;
  QRgb primary = 0;
  QRgb secondary = 0;
  QRgb tertiary = 0;
};

inline bool operator==(const IconPixmapCacheKey& lhs, const IconPixmapCacheKey& rhs) {
  return lhs.key == rhs.key && lhs.physicalSize == rhs.physicalSize && lhs.mode == rhs.mode &&
         lhs.state == rhs.state && lhs.fit == rhs.fit && lhs.alignment == rhs.alignment &&
         lhs.paletteRevision == rhs.paletteRevision &&
         lhs.statePaletteRevision == rhs.statePaletteRevision && lhs.primary == rhs.primary &&
         lhs.secondary == rhs.secondary && lhs.tertiary == rhs.tertiary;
}

inline IconHashValue qHash(const IconPixmapCacheKey& value, IconHashValue seed = 0) {
  seed = iconHashCombine(seed, qHash(value.key, 0));
  seed = iconHashCombine(seed, ::qHash(value.physicalSize, 0));
  seed = iconHashCombine(seed, ::qHash(static_cast<int>(value.mode), 0));
  seed = iconHashCombine(seed, ::qHash(static_cast<int>(value.state), 0));
  seed = iconHashCombine(seed, ::qHash(static_cast<int>(value.fit), 0));
  seed = iconHashCombine(seed, ::qHash(value.alignment, 0));
  seed = iconHashCombine(seed, ::qHash(value.paletteRevision, 0));
  seed = iconHashCombine(seed, ::qHash(value.statePaletteRevision, 0));
  seed = iconHashCombine(seed, ::qHash(value.primary, 0));
  seed = iconHashCombine(seed, ::qHash(value.secondary, 0));
  return iconHashCombine(seed, ::qHash(value.tertiary, 0));
}

struct IconRegistryImpl final {
  static constexpr int kDefaultCacheLimitKB = 8 * 1024;
  IconRegistryImpl() : pixmapCache(kDefaultCacheLimitKB) {}

  QMutex mutex;
  QHash<IconKey, StoredIcon> icons;
  IconPaletteResolver resolver;
  QCache<IconPixmapCacheKey, QPixmap> pixmapCache;
  QSet<IconPixmapCacheKey> inFlight;
  QWaitCondition cacheReady;
  int cacheLimitKB = kDefaultCacheLimitKB;
  quint64 hitCount = 0;
  quint64 missCount = 0;
  quint64 rasterizationCount = 0;
};

}  // namespace adqt::icons::detail

namespace adqt::icons {
namespace {

using detail::IconPixmapCacheKey;
using detail::IconRegistryImpl;
using detail::StoredIcon;

constexpr auto kPrimaryPlaceholder = "__ADQT_SLOT_PRIMARY__";
constexpr auto kSecondaryPlaceholder = "__ADQT_SLOT_SECONDARY__";
constexpr auto kTertiaryPlaceholder = "__ADQT_SLOT_TERTIARY__";
constexpr qint64 kMaxCacheablePixmapBytes = 256 * 1024;

QString placeholderForSlot(const QString& slot) {
  if (slot.compare(QStringLiteral("secondary"), Qt::CaseInsensitive) == 0) {
    return QString::fromLatin1(kSecondaryPlaceholder);
  }
  if (slot.compare(QStringLiteral("tertiary"), Qt::CaseInsensitive) == 0) {
    return QString::fromLatin1(kTertiaryPlaceholder);
  }
  return QString::fromLatin1(kPrimaryPlaceholder);
}

QString svgColor(const QColor& value) {
  return (value.isValid() ? value : QColor(Qt::black)).name(QColor::HexRgb);
}

bool hasSlot(const IconColors& colors, int slot) {
  if (slot == 0) return colors.primarySlot().has_value();
  if (slot == 1) return colors.secondarySlot().has_value();
  return colors.tertiarySlot().has_value();
}

QColor slot(const IconColors& colors, int index) {
  if (index == 0 && colors.primarySlot()) return *colors.primarySlot();
  if (index == 1 && colors.secondarySlot()) return *colors.secondarySlot();
  if (index == 2 && colors.tertiarySlot()) return *colors.tertiarySlot();
  return QColor();
}

bool colorsAllowed(IconColorModel model, const IconColors& colors) {
  if (model == IconColorModel::FullColor) return colors.isEmpty();
  if (model == IconColorModel::Monochrome) {
    return !colors.secondarySlot() && !colors.tertiarySlot();
  }
  if (model == IconColorModel::TwoTone) return !colors.tertiarySlot();
  return true;
}

void addDiagnostic(IconPackRegistrationResult& result, IconRegistrationError error,
                   const IconKey& key, const QString& message) {
  result.diagnostics.append(IconRegistrationDiagnostic{error, key, message});
}

bool replaceColorAttribute(QString& tag, const QString& name, const QString& placeholder) {
  const QRegularExpression expression(QStringLiteral(R"(\b%1\s*=\s*(['"])([^'"]*)\1)").arg(name),
                                      QRegularExpression::CaseInsensitiveOption);
  const QRegularExpressionMatch match = expression.match(tag);
  if (!match.hasMatch() ||
      match.captured(2).compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0) {
    return false;
  }
  tag.replace(match.capturedStart(), match.capturedLength(),
              QStringLiteral("%1=\"%2\"").arg(name, placeholder));
  return true;
}

QByteArray normalizeSvg(const IconDefinition& definition, QString* error) {
  QString svg = QString::fromUtf8(definition.svg);
  if (!svg.contains(QRegularExpression(QStringLiteral(R"(<svg\b)"),
                                       QRegularExpression::CaseInsensitiveOption))) {
    if (error) *error = QStringLiteral("source does not contain an svg root");
    return {};
  }
  if (svg.contains(QRegularExpression(QStringLiteral(R"((?:href|src)\s*=\s*['"](?:https?:)?//)"),
                                      QRegularExpression::CaseInsensitiveOption))) {
    if (error) *error = QStringLiteral("external network references are forbidden");
    return {};
  }
  if (!definition.allowEmbeddedDataImages &&
      svg.contains(QRegularExpression(QStringLiteral(R"(<image\b)"),
                                      QRegularExpression::CaseInsensitiveOption))) {
    if (error) *error = QStringLiteral("image elements require explicit embedded-data permission");
    return {};
  }

  if (definition.colorModel == IconColorModel::FullColor) {
    if (svg.contains(QStringLiteral("data-adqt-slot"), Qt::CaseInsensitive) ||
        svg.contains(QStringLiteral("currentColor"), Qt::CaseInsensitive)) {
      if (error) *error = QStringLiteral("full-color icons cannot contain theme slots");
      return {};
    }
    return svg.toUtf8();
  }

  const QRegularExpression slotTag(
      QStringLiteral(
          R"(<([A-Za-z_:][A-Za-z0-9:._-]*)([^>]*)\bdata-adqt-slot\s*=\s*(['"])(primary|secondary|tertiary)\3([^>]*)>)"),
      QRegularExpression::CaseInsensitiveOption);
  const QRegularExpression slotAttribute(
      QStringLiteral(R"(\s*data-adqt-slot\s*=\s*(['"])(primary|secondary|tertiary)\1)"),
      QRegularExpression::CaseInsensitiveOption);
  const QRegularExpression currentColor(QStringLiteral("currentColor"),
                                        QRegularExpression::CaseInsensitiveOption);

  QString normalized;
  qsizetype cursor = 0;
  auto matches = slotTag.globalMatch(svg);
  while (matches.hasNext()) {
    const auto match = matches.next();
    normalized += svg.mid(cursor, match.capturedStart() - cursor);
    QString tag = match.captured();
    const QString placeholder = placeholderForSlot(match.captured(4));
    tag.remove(slotAttribute);
    bool changed = replaceColorAttribute(tag, QStringLiteral("fill"), placeholder);
    changed = replaceColorAttribute(tag, QStringLiteral("stroke"), placeholder) || changed;
    if (tag.contains(currentColor)) {
      tag.replace(currentColor, placeholder);
      changed = true;
    }
    if (!changed) {
      qsizetype position = tag.lastIndexOf(QLatin1Char('>'));
      if (position > 0 && tag.at(position - 1) == QLatin1Char('/')) --position;
      tag.insert(position, QStringLiteral(" fill=\"") + placeholder + QStringLiteral("\""));
    }
    normalized += tag;
    cursor = match.capturedEnd();
  }
  normalized += svg.mid(cursor);
  normalized.replace(currentColor, QString::fromLatin1(kPrimaryPlaceholder));

  const bool primary = normalized.contains(QString::fromLatin1(kPrimaryPlaceholder));
  const bool secondary = normalized.contains(QString::fromLatin1(kSecondaryPlaceholder));
  const bool tertiary = normalized.contains(QString::fromLatin1(kTertiaryPlaceholder));
  const bool valid =
      definition.colorModel == IconColorModel::Monochrome ? primary && !secondary && !tertiary
      : definition.colorModel == IconColorModel::TwoTone  ? primary && secondary && !tertiary
                                                          : primary && secondary && tertiary;
  if (!valid) {
    if (error) *error = QStringLiteral("SVG slots do not match the declared color model");
    return {};
  }
  return normalized.toUtf8();
}

QColor deriveSecondary(const QColor& primary) {
  QColor hsl = (primary.isValid() ? primary : QColor(QStringLiteral("#1677FF"))).toHsl();
  hsl.setHsl(hsl.hslHue(), qMax(8, qRound(hsl.hslSaturation() * 0.22)),
             qMin(245, qRound(hsl.lightness() + (255 - hsl.lightness()) * 0.82)), hsl.alpha());
  return hsl.toRgb();
}

IconPalette resolvedApplicationPalette(const IconPaletteResolver& resolver) {
  IconPalette result = resolver ? resolver() : IconPalette();
  if (!result.text.isValid()) result.text = QColor(QStringLiteral("#1F1F1F"));
  if (!result.textDisabled.isValid()) result.textDisabled = QColor(QStringLiteral("#BFBFBF"));
  if (!result.primary.isValid()) result.primary = QColor(QStringLiteral("#1677FF"));
  if (!result.twoToneSecondary.isValid()) result.twoToneSecondary = deriveSecondary(result.primary);
  if (!result.tertiary.isValid()) result.tertiary = result.twoToneSecondary;
  if (result.revision == 0) result.revision = 1;
  return result;
}

struct ResolvedColors final {
  QColor primary;
  QColor secondary;
  QColor tertiary;
  quint64 applicationRevision = 1;
  quint64 stateRevision = 0;
};

ResolvedColors resolveColors(const StoredIcon& stored, const IconRef& ref,
                             const IconStatePalette& statePalette, const IconPalette& app,
                             QIcon::Mode mode, QIcon::State state) {
  ResolvedColors result;
  result.applicationRevision = app.revision;
  result.stateRevision = statePalette.revision();
  if (stored.metadata.colorModel == IconColorModel::FullColor) return result;

  const QColor appPrimary = stored.metadata.colorModel == IconColorModel::Monochrome
                                ? (mode == QIcon::Disabled ? app.textDisabled : app.text)
                                : (mode == QIcon::Disabled ? app.textDisabled : app.primary);
  result.primary = appPrimary;
  result.secondary =
      mode == QIcon::Disabled ? deriveSecondary(app.textDisabled) : app.twoToneSecondary;
  result.tertiary = mode == QIcon::Disabled ? result.secondary : app.tertiary;

  const std::optional<IconColors> stateColors = statePalette.resolve(mode, state);
  for (int index = 0; index < 3; ++index) {
    QColor selected;
    if (stateColors && hasSlot(*stateColors, index))
      selected = slot(*stateColors, index);
    else if (hasSlot(detail::IconRefAccess::colors(ref), index))
      selected = slot(detail::IconRefAccess::colors(ref), index);
    else if (hasSlot(stored.metadata.defaultColors, index))
      selected = slot(stored.metadata.defaultColors, index);
    if (!selected.isValid()) continue;
    if (index == 0)
      result.primary = selected;
    else if (index == 1)
      result.secondary = selected;
    else
      result.tertiary = selected;
  }
  const bool hasPrimaryOverride = (stateColors && stateColors->primarySlot()) ||
                                  detail::IconRefAccess::colors(ref).primarySlot();
  const bool hasSecondaryColor = (stateColors && stateColors->secondarySlot()) ||
                                 detail::IconRefAccess::colors(ref).secondarySlot() ||
                                 stored.metadata.defaultColors.secondarySlot();
  if (stored.metadata.colorModel != IconColorModel::Monochrome && hasPrimaryOverride &&
      !hasSecondaryColor) {
    result.secondary = deriveSecondary(result.primary);
  }
  return result;
}

QByteArray coloredSvg(const StoredIcon& stored, const ResolvedColors& colors) {
  if (stored.metadata.colorModel == IconColorModel::FullColor) return stored.svgTemplate;
  QString svg = QString::fromUtf8(stored.svgTemplate);
  svg.replace(QString::fromLatin1(kPrimaryPlaceholder), svgColor(colors.primary));
  svg.replace(QString::fromLatin1(kSecondaryPlaceholder), svgColor(colors.secondary));
  svg.replace(QString::fromLatin1(kTertiaryPlaceholder), svgColor(colors.tertiary));
  return svg.toUtf8();
}

QRectF alignedContainedRect(const QSizeF& source, const QRectF& bounds, Qt::Alignment alignment) {
  if (source.isEmpty() || bounds.isEmpty()) return bounds;
  const qreal scale = qMin(bounds.width() / source.width(), bounds.height() / source.height());
  const QSizeF size(source.width() * scale, source.height() * scale);
  qreal x = bounds.left();
  qreal y = bounds.top();
  if (alignment.testFlag(Qt::AlignHCenter))
    x += (bounds.width() - size.width()) / 2.0;
  else if (alignment.testFlag(Qt::AlignRight))
    x += bounds.width() - size.width();
  if (alignment.testFlag(Qt::AlignVCenter))
    y += (bounds.height() - size.height()) / 2.0;
  else if (alignment.testFlag(Qt::AlignBottom))
    y += bounds.height() - size.height();
  return QRectF(QPointF(x, y), size);
}

bool lookup(const std::shared_ptr<IconRegistryImpl>& impl, const IconRef& ref, StoredIcon* stored,
            IconPaletteResolver* resolver) {
  if (!ref.isValid()) return false;
  QMutexLocker lock(&impl->mutex);
  const auto found = impl->icons.constFind(detail::IconRefAccess::key(ref));
  if (found == impl->icons.constEnd()) return false;
  if (stored) *stored = found.value();
  if (resolver) *resolver = impl->resolver;
  return true;
}

int pixmapCost(const QPixmap& pixmap) {
  return qMax(1, static_cast<int>(
                     (static_cast<qint64>(pixmap.width()) * pixmap.height() * 4 + 1023) / 1024));
}

QPixmap renderPixmap(const std::shared_ptr<IconRegistryImpl>& impl, const IconRef& ref,
                     IconRenderRequest request, const IconStatePalette& statePalette) {
  StoredIcon stored;
  IconPaletteResolver resolver;
  if (!lookup(impl, ref, &stored, &resolver)) return {};
  if (stored.metadata.colorModel == IconColorModel::FullColor &&
      (!detail::IconRefAccess::colors(ref).isEmpty() || !statePalette.isEmpty()))
    return {};

  if (!request.logicalSize.isValid() || request.logicalSize.isEmpty())
    request.logicalSize = QSize(16, 16);
  const qreal dpr =
      request.devicePixelRatio > 0.0 ? qBound(0.25, request.devicePixelRatio, 8.0) : 1.0;
  const QSize physical(qMax(1, qRound(request.logicalSize.width() * dpr)),
                       qMax(1, qRound(request.logicalSize.height() * dpr)));
  const IconFit fit = request.fit.value_or(stored.metadata.fit);
  const IconPalette app = resolvedApplicationPalette(resolver);
  const ResolvedColors colors =
      resolveColors(stored, ref, statePalette, app, request.mode, request.state);

  IconPixmapCacheKey key;
  key.key = stored.metadata.key;
  key.physicalSize = physical;
  key.mode = request.mode;
  key.state = request.state;
  key.fit = fit;
  key.alignment = static_cast<int>(request.alignment);
  key.paletteRevision = colors.applicationRevision;
  key.statePaletteRevision = colors.stateRevision;
  key.primary = colors.primary.rgba();
  key.secondary = colors.secondary.rgba();
  key.tertiary = colors.tertiary.rgba();
  const bool cacheable =
      static_cast<qint64>(physical.width()) * physical.height() * 4 <= kMaxCacheablePixmapBytes;

  if (cacheable) {
    QMutexLocker lock(&impl->mutex);
    for (;;) {
      if (QPixmap* cached = impl->pixmapCache.object(key)) {
        ++impl->hitCount;
        QPixmap copy = *cached;
        copy.setDevicePixelRatio(dpr);
        return copy;
      }
      if (!impl->inFlight.contains(key)) {
        impl->inFlight.insert(key);
        ++impl->missCount;
        break;
      }
      impl->cacheReady.wait(&impl->mutex);
    }
  }

  QSvgRenderer renderer(coloredSvg(stored, colors));
  QPixmap pixmap;
  if (renderer.isValid()) {
    pixmap = QPixmap(physical);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF bounds(QPointF(0, 0), QSizeF(physical));
    const QRectF target = fit == IconFit::Stretch ? bounds
                                                  : alignedContainedRect(renderer.viewBoxF().size(),
                                                                         bounds, request.alignment);
    renderer.render(&painter, target);
    if (stored.metadata.colorModel == IconColorModel::Monochrome && colors.primary.alpha() < 255) {
      painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
      painter.fillRect(bounds, QColor(0, 0, 0, colors.primary.alpha()));
    }
  }

  if (cacheable) {
    QMutexLocker lock(&impl->mutex);
    if (!pixmap.isNull()) {
      ++impl->rasterizationCount;
      impl->pixmapCache.insert(key, new QPixmap(pixmap), pixmapCost(pixmap));
    }
    impl->inFlight.remove(key);
    impl->cacheReady.wakeAll();
  }
  pixmap.setDevicePixelRatio(dpr);
  return pixmap;
}

class RegistryIconEngine final : public QIconEngine {
 public:
  RegistryIconEngine(std::shared_ptr<IconRegistryImpl> impl, IconRef ref, IconStatePalette palette)
      : impl_(std::move(impl)), ref_(std::move(ref)), palette_(std::move(palette)) {}
  QIconEngine* clone() const override { return new RegistryIconEngine(impl_, ref_, palette_); }
  QString key() const override { return QStringLiteral("adqt.icon.engine.v2"); }
  QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override {
    IconRenderRequest request;
    request.logicalSize = size;
    request.mode = mode;
    request.state = state;
    return renderPixmap(impl_, ref_, request, palette_);
  }
  QPixmap scaledPixmap(const QSize& size, QIcon::Mode mode, QIcon::State state,
                       qreal scale) override {
    IconRenderRequest request;
    request.logicalSize = size;
    request.devicePixelRatio = scale;
    request.mode = mode;
    request.state = state;
    return renderPixmap(impl_, ref_, request, palette_);
  }
  void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override {
    if (!painter || rect.isEmpty()) return;
    IconRenderRequest request;
    request.logicalSize = rect.size();
    request.devicePixelRatio = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
    request.mode = mode;
    request.state = state;
    const QPixmap pixmap = renderPixmap(impl_, ref_, request, palette_);
    if (!pixmap.isNull()) painter->drawPixmap(rect, pixmap);
  }

 private:
  std::shared_ptr<IconRegistryImpl> impl_;
  IconRef ref_;
  IconStatePalette palette_;
};

}  // namespace

IconColors IconColors::primary(const QColor& color) { return IconColors().withPrimary(color); }
IconColors IconColors::twoTone(const QColor& primary, const QColor& secondary) {
  return IconColors().withPrimary(primary).withSecondary(secondary);
}
IconColors IconColors::threeTone(const QColor& primary, const QColor& secondary,
                                 const QColor& tertiary) {
  return twoTone(primary, secondary).withTertiary(tertiary);
}
IconColors IconColors::withPrimary(const QColor& color) const {
  IconColors copy = *this;
  copy.primary_ = color;
  return copy;
}
IconColors IconColors::withSecondary(const QColor& color) const {
  IconColors copy = *this;
  copy.secondary_ = color;
  return copy;
}
IconColors IconColors::withTertiary(const QColor& color) const {
  IconColors copy = *this;
  copy.tertiary_ = color;
  return copy;
}

IconHashValue qHash(const IconColors& value, IconHashValue seed) {
  for (const auto* optional :
       {&value.primarySlot(), &value.secondarySlot(), &value.tertiarySlot()}) {
    seed = iconHashCombine(seed, ::qHash(optional->has_value(), 0));
    if (*optional) seed = iconHashCombine(seed, ::qHash((*optional)->rgba(), 0));
  }
  return seed;
}

IconHashValue qHash(const IconKey& value, IconHashValue seed) {
  seed = iconHashCombine(seed, ::qHash(value.pack, 0));
  seed = iconHashCombine(seed, ::qHash(value.variant, 0));
  return iconHashCombine(seed, ::qHash(value.name, 0));
}

IconHashValue qHash(const IconRef& value, IconHashValue seed) {
  seed = iconHashCombine(seed, qHash(value.key_, 0));
  return iconHashCombine(seed, qHash(value.colors_, 0));
}

int IconStatePalette::key(QIcon::Mode mode, QIcon::State state) {
  return static_cast<int>(mode) * 2 + static_cast<int>(state);
}
IconStatePalette& IconStatePalette::set(QIcon::Mode mode, QIcon::State state,
                                        const IconColors& colors) {
  entries_.insert(key(mode, state), colors);
  return *this;
}
IconStatePalette IconStatePalette::with(QIcon::Mode mode, QIcon::State state,
                                        const IconColors& colors) const {
  IconStatePalette copy = *this;
  copy.set(mode, state, colors);
  return copy;
}
std::optional<IconColors> IconStatePalette::exact(QIcon::Mode mode, QIcon::State state) const {
  const auto found = entries_.constFind(key(mode, state));
  return found == entries_.constEnd() ? std::optional<IconColors>() : found.value();
}
std::optional<IconColors> IconStatePalette::resolve(QIcon::Mode mode, QIcon::State state) const {
  const int candidates[] = {key(mode, state), key(mode, QIcon::Off), key(QIcon::Normal, state),
                            key(QIcon::Normal, QIcon::Off)};
  for (int candidate : candidates) {
    const auto found = entries_.constFind(candidate);
    if (found != entries_.constEnd()) return found.value();
  }
  return {};
}
quint64 IconStatePalette::revision() const {
  QList<int> keys = entries_.keys();
  std::sort(keys.begin(), keys.end());
  IconHashValue seed = 0;
  for (int item : keys) {
    seed = iconHashCombine(seed, ::qHash(item, 0));
    seed = iconHashCombine(seed, qHash(entries_.value(item), 0));
  }
  return static_cast<quint64>(seed);
}

IconRegistry::IconRegistry() : impl_(std::make_shared<IconRegistryImpl>()) {}
IconRegistry::~IconRegistry() = default;

IconPackRegistrationResult IconRegistry::registerPack(const QString& pack,
                                                      const QList<IconDefinition>& definitions) {
  IconPackRegistrationResult result;
  if (pack.isEmpty() || definitions.isEmpty()) {
    addDiagnostic(result, IconRegistrationError::InvalidPack, {},
                  QStringLiteral("pack name and entries are required"));
    return result;
  }

  QHash<IconKey, StoredIcon> pending;
  for (const IconDefinition& definition : definitions) {
    if (!definition.isValid() || definition.key.pack != pack) {
      addDiagnostic(result, IconRegistrationError::InvalidEntry, definition.key,
                    QStringLiteral("invalid entry or pack mismatch"));
      continue;
    }
    if (pending.contains(definition.key)) {
      addDiagnostic(result, IconRegistrationError::DuplicateKey, definition.key,
                    QStringLiteral("duplicate key in pack definition"));
      continue;
    }
    if (!colorsAllowed(definition.colorModel, definition.defaultColors)) {
      addDiagnostic(result, IconRegistrationError::ColorModelMismatch, definition.key,
                    QStringLiteral("default colors do not match the color model"));
      continue;
    }
    QString error;
    const QByteArray normalized = normalizeSvg(definition, &error);
    if (normalized.isEmpty()) {
      addDiagnostic(result, IconRegistrationError::InvalidSvg, definition.key, error);
      continue;
    }
    const QByteArray hash =
        QCryptographicHash::hash(normalized, QCryptographicHash::Sha256).toHex();
    if (!definition.sourceHash.isEmpty() && definition.sourceHash.toLower() != hash) {
      addDiagnostic(result, IconRegistrationError::HashMismatch, definition.key,
                    QStringLiteral("declared source hash does not match normalized SVG"));
      continue;
    }
    StoredIcon stored;
    stored.metadata = IconMetadata{definition.key, definition.colorModel, definition.fit,
                                   definition.defaultColors, hash};
    stored.svgTemplate = normalized;
    pending.insert(definition.key, stored);
  }
  if (!result.ok()) return result;

  QMutexLocker lock(&impl_->mutex);
  for (auto it = pending.constBegin(); it != pending.constEnd(); ++it) {
    const auto existing = impl_->icons.constFind(it.key());
    if (existing != impl_->icons.constEnd() &&
        (existing->metadata != it->metadata || existing->svgTemplate != it->svgTemplate)) {
      addDiagnostic(result, IconRegistrationError::ConflictingRegistration, it.key(),
                    QStringLiteral("key is already registered with different content"));
    }
  }
  if (!result.ok()) return result;
  for (auto it = pending.constBegin(); it != pending.constEnd(); ++it) {
    if (impl_->icons.contains(it.key()))
      ++result.existingCount;
    else {
      impl_->icons.insert(it.key(), it.value());
      ++result.registeredCount;
    }
  }
  return result;
}

bool IconRegistry::containsIcon(const IconKey& key) const {
  QMutexLocker lock(&impl_->mutex);
  return key.isValid() && impl_->icons.contains(key);
}
IconMetadata IconRegistry::describeIcon(const IconRef& ref) const {
  return ref.isValid() ? describeIcon(detail::IconRefAccess::key(ref)) : IconMetadata();
}
IconMetadata IconRegistry::describeIcon(const IconKey& key) const {
  QMutexLocker lock(&impl_->mutex);
  const auto found = impl_->icons.constFind(key);
  return found == impl_->icons.constEnd() ? IconMetadata() : found->metadata;
}
QList<IconMetadata> IconRegistry::listIcons(const QString& pack, const QString& variant) const {
  QList<IconMetadata> result;
  {
    QMutexLocker lock(&impl_->mutex);
    for (auto it = impl_->icons.constBegin(); it != impl_->icons.constEnd(); ++it) {
      if ((!pack.isEmpty() && it.key().pack != pack) ||
          (!variant.isEmpty() && it.key().variant != variant))
        continue;
      result.append(it->metadata);
    }
  }
  std::sort(result.begin(), result.end(), [](const IconMetadata& a, const IconMetadata& b) {
    if (a.key.pack != b.key.pack) return a.key.pack < b.key.pack;
    if (a.key.variant != b.key.variant) return a.key.variant < b.key.variant;
    return a.key.name < b.key.name;
  });
  return result;
}
IconRef IconRegistry::reference(const IconKey& key, const IconColors& colors) const {
  if (!containsIcon(key)) return {};
  const IconMetadata metadata = describeIcon(key);
  return colorsAllowed(metadata.colorModel, colors) ? IconRef(key, colors) : IconRef();
}
QIcon IconRegistry::makeIcon(const IconRef& ref, const IconStatePalette& palette) const {
  return describeIcon(ref).isValid() ? QIcon(new RegistryIconEngine(impl_, ref, palette)) : QIcon();
}
QPixmap IconRegistry::renderIconPixmap(const IconRef& ref, const IconRenderRequest& request,
                                       const IconStatePalette& palette) const {
  return renderPixmap(impl_, ref, request, palette);
}
void IconRegistry::paintIcon(QPainter* painter, const IconRef& ref, const QRectF& rect,
                             const IconRenderRequest& request,
                             const IconStatePalette& palette) const {
  if (!painter || rect.isEmpty()) return;
  IconRenderRequest actual = request;
  actual.logicalSize = rect.size().toSize();
  if (actual.devicePixelRatio <= 0.0)
    actual.devicePixelRatio = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
  const QPixmap pixmap = renderPixmap(impl_, ref, actual, palette);
  if (!pixmap.isNull()) {
    painter->drawPixmap(rect, pixmap, QRectF(pixmap.rect()));
  }
}
QCursor IconRegistry::makeCursor(const IconRef& ref, const QSize& logicalSize,
                                 const QPoint& hotSpot, qreal devicePixelRatio) const {
  const IconMetadata metadata = describeIcon(ref);
  if (!metadata.isValid() || metadata.colorModel != IconColorModel::FullColor) return {};
  IconRenderRequest request;
  request.logicalSize = logicalSize;
  request.devicePixelRatio = devicePixelRatio;
  const QPixmap pixmap = renderPixmap(impl_, ref, request, {});
  return pixmap.isNull() ? QCursor() : QCursor(pixmap, hotSpot.x(), hotSpot.y());
}
void IconRegistry::setPaletteResolver(IconPaletteResolver resolver) {
  QMutexLocker lock(&impl_->mutex);
  impl_->resolver = std::move(resolver);
  impl_->pixmapCache.clear();
}
void IconRegistry::clearPaletteResolver() { setPaletteResolver({}); }
void IconRegistry::setCacheLimitKB(int kb) {
  QMutexLocker lock(&impl_->mutex);
  impl_->cacheLimitKB = qMax(1024, kb);
  impl_->pixmapCache.setMaxCost(impl_->cacheLimitKB);
  impl_->pixmapCache.clear();
}
void IconRegistry::clearCache() {
  QMutexLocker lock(&impl_->mutex);
  impl_->pixmapCache.clear();
  impl_->hitCount = impl_->missCount = impl_->rasterizationCount = 0;
}
void IconRegistry::prewarm(const QList<IconPixmapRequest>& requests) const {
  for (const auto& request : requests)
    renderPixmap(impl_, request.ref, request.render, request.palette);
}
IconCacheStatistics IconRegistry::cacheStatistics() const {
  QMutexLocker lock(&impl_->mutex);
  return {static_cast<int>(impl_->pixmapCache.size()),
          static_cast<int>(impl_->pixmapCache.totalCost()),
          impl_->cacheLimitKB,
          impl_->hitCount,
          impl_->missCount,
          impl_->rasterizationCount};
}

IconRegistry& defaultRegistry() {
  static IconRegistry instance;
  return instance;
}
IconMetadata describeIcon(const IconRef& ref) { return defaultRegistry().describeIcon(ref); }
QList<IconMetadata> listIcons(const QString& pack, const QString& variant) {
  return defaultRegistry().listIcons(pack, variant);
}
QIcon makeIcon(const IconRef& ref, const IconStatePalette& palette) {
  return defaultRegistry().makeIcon(ref, palette);
}
QPixmap renderIconPixmap(const IconRef& ref, const IconRenderRequest& request,
                         const IconStatePalette& palette) {
  return defaultRegistry().renderIconPixmap(ref, request, palette);
}
void paintIcon(QPainter* painter, const IconRef& ref, const QRectF& rect,
               const IconRenderRequest& request, const IconStatePalette& palette) {
  defaultRegistry().paintIcon(painter, ref, rect, request, palette);
}
QCursor makeCursor(const IconRef& ref, const QSize& logicalSize, const QPoint& hotSpot,
                   qreal devicePixelRatio) {
  return defaultRegistry().makeCursor(ref, logicalSize, hotSpot, devicePixelRatio);
}
void setPaletteResolver(IconPaletteResolver resolver) {
  defaultRegistry().setPaletteResolver(std::move(resolver));
}
void clearPaletteResolver() { defaultRegistry().clearPaletteResolver(); }
void setCacheLimitKB(int kb) { defaultRegistry().setCacheLimitKB(kb); }
void clearCache() { defaultRegistry().clearCache(); }
void prewarm(const QList<IconPixmapRequest>& requests) { defaultRegistry().prewarm(requests); }
IconCacheStatistics cacheStatistics() { return defaultRegistry().cacheStatistics(); }

}  // namespace adqt::icons
