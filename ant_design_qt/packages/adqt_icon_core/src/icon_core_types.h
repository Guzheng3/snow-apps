#ifndef ADQT_ICON_CORE_TYPES_H
#define ADQT_ICON_CORE_TYPES_H

#include <QByteArray>
#include <QColor>
#include <QHash>
#include <QIcon>
#include <QList>
#include <QMetaType>
#include <QSize>
#include <QString>
#include <Qt>
#include <QtGlobal>

#include <cstddef>
#include <functional>
#include <optional>

#include "adqt_icon_core_global.h"

namespace adqt::icons {

namespace detail {
struct IconRefAccess;
}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
using IconHashValue = uint;
#else
using IconHashValue = size_t;
#endif

inline IconHashValue iconHashCombine(IconHashValue seed, IconHashValue value) {
  return seed ^ (value + static_cast<IconHashValue>(0x9e3779b9u) + (seed << 6) + (seed >> 2));
}

enum class IconColorModel { Monochrome, TwoTone, ThreeTone, FullColor };
enum class IconFit { Contain, Stretch };

class ADQT_ICON_CORE_EXPORT IconColors final {
 public:
  IconColors() = default;

  static IconColors primary(const QColor& color);
  static IconColors twoTone(const QColor& primary, const QColor& secondary);
  static IconColors threeTone(const QColor& primary, const QColor& secondary,
                              const QColor& tertiary);

  IconColors withPrimary(const QColor& color) const;
  IconColors withSecondary(const QColor& color) const;
  IconColors withTertiary(const QColor& color) const;

  const std::optional<QColor>& primarySlot() const { return primary_; }
  const std::optional<QColor>& secondarySlot() const { return secondary_; }
  const std::optional<QColor>& tertiarySlot() const { return tertiary_; }
  bool isEmpty() const { return !primary_ && !secondary_ && !tertiary_; }

  friend bool operator==(const IconColors& lhs, const IconColors& rhs) {
    return lhs.primary_ == rhs.primary_ && lhs.secondary_ == rhs.secondary_ &&
           lhs.tertiary_ == rhs.tertiary_;
  }
  friend bool operator!=(const IconColors& lhs, const IconColors& rhs) { return !(lhs == rhs); }

 private:
  std::optional<QColor> primary_;
  std::optional<QColor> secondary_;
  std::optional<QColor> tertiary_;
};

ADQT_ICON_CORE_EXPORT IconHashValue qHash(const IconColors& value, IconHashValue seed = 0);

struct IconKey final {
  QString pack;
  QString variant;
  QString name;

  bool isValid() const { return !pack.isEmpty() && !variant.isEmpty() && !name.isEmpty(); }
};

inline bool operator==(const IconKey& lhs, const IconKey& rhs) {
  return lhs.pack == rhs.pack && lhs.variant == rhs.variant && lhs.name == rhs.name;
}
inline bool operator!=(const IconKey& lhs, const IconKey& rhs) { return !(lhs == rhs); }
ADQT_ICON_CORE_EXPORT IconHashValue qHash(const IconKey& value, IconHashValue seed = 0);

class ExternalIconPack;

class ADQT_ICON_CORE_EXPORT IconRef final {
 public:
  IconRef() = default;
  bool isValid() const { return valid_ && key_.isValid(); }
  const IconColors& colors() const { return colors_; }
  IconRef withColors(const IconColors& colors) const {
    return isValid() ? IconRef(key_, colors) : IconRef();
  }

  friend bool operator==(const IconRef& lhs, const IconRef& rhs) {
    return lhs.valid_ == rhs.valid_ && lhs.key_ == rhs.key_ && lhs.colors_ == rhs.colors_;
  }
  friend bool operator!=(const IconRef& lhs, const IconRef& rhs) { return !(lhs == rhs); }

 private:
  IconRef(IconKey key, IconColors colors)
      : key_(std::move(key)), colors_(std::move(colors)), valid_(true) {}

  IconKey key_;
  IconColors colors_;
  bool valid_ = false;

  friend class ExternalIconPack;
  friend class IconRegistry;
  friend struct detail::IconRefAccess;
  friend IconHashValue qHash(const IconRef&, IconHashValue);
};

ADQT_ICON_CORE_EXPORT IconHashValue qHash(const IconRef& value, IconHashValue seed = 0);
inline bool isValid(const IconRef& ref) { return ref.isValid(); }

struct IconMetadata final {
  IconKey key;
  IconColorModel colorModel = IconColorModel::Monochrome;
  IconFit fit = IconFit::Contain;
  IconColors defaultColors;
  QByteArray sourceHash;

  bool isValid() const { return key.isValid() && !sourceHash.isEmpty(); }
};

inline bool operator==(const IconMetadata& lhs, const IconMetadata& rhs) {
  return lhs.key == rhs.key && lhs.colorModel == rhs.colorModel && lhs.fit == rhs.fit &&
         lhs.defaultColors == rhs.defaultColors && lhs.sourceHash == rhs.sourceHash;
}
inline bool operator!=(const IconMetadata& lhs, const IconMetadata& rhs) { return !(lhs == rhs); }

struct IconDefinition final {
  IconKey key;
  IconColorModel colorModel = IconColorModel::Monochrome;
  IconFit fit = IconFit::Contain;
  IconColors defaultColors;
  QByteArray svg;
  QByteArray sourceHash;
  bool allowEmbeddedDataImages = false;

  bool isValid() const { return key.isValid() && !svg.isEmpty(); }
};

enum class IconRegistrationError {
  None,
  InvalidPack,
  InvalidEntry,
  DuplicateKey,
  InvalidSvg,
  ColorModelMismatch,
  HashMismatch,
  ConflictingRegistration,
};

struct IconRegistrationDiagnostic final {
  IconRegistrationError error = IconRegistrationError::None;
  IconKey key;
  QString message;
};

struct IconPackRegistrationResult final {
  int registeredCount = 0;
  int existingCount = 0;
  QList<IconRegistrationDiagnostic> diagnostics;
  bool ok() const { return diagnostics.isEmpty(); }
};

struct IconRenderRequest final {
  QSize logicalSize = QSize(16, 16);
  // A non-positive value lets direct painting derive the target device DPR.
  qreal devicePixelRatio = 0.0;
  QIcon::Mode mode = QIcon::Normal;
  QIcon::State state = QIcon::Off;
  std::optional<IconFit> fit;
  Qt::Alignment alignment = Qt::AlignCenter;
};

class ADQT_ICON_CORE_EXPORT IconStatePalette final {
 public:
  IconStatePalette& set(QIcon::Mode mode, QIcon::State state, const IconColors& colors);
  IconStatePalette with(QIcon::Mode mode, QIcon::State state, const IconColors& colors) const;
  std::optional<IconColors> exact(QIcon::Mode mode, QIcon::State state) const;
  std::optional<IconColors> resolve(QIcon::Mode mode, QIcon::State state) const;
  bool isEmpty() const { return entries_.isEmpty(); }
  quint64 revision() const;

 private:
  static int key(QIcon::Mode mode, QIcon::State state);
  QHash<int, IconColors> entries_;
};

struct IconPalette final {
  QColor text = QColor(QStringLiteral("#1F1F1F"));
  QColor textDisabled = QColor(QStringLiteral("#BFBFBF"));
  QColor primary = QColor(QStringLiteral("#1677FF"));
  QColor twoToneSecondary = QColor(QStringLiteral("#E6F4FF"));
  QColor tertiary = QColor(QStringLiteral("#BAE0FF"));
  quint64 revision = 1;
};

using IconPaletteResolver = std::function<IconPalette()>;

struct IconPixmapRequest final {
  IconRef ref;
  IconRenderRequest render;
  IconStatePalette palette;
};

struct IconCacheStatistics final {
  int entryCount = 0;
  int costKB = 0;
  int limitKB = 0;
  quint64 hitCount = 0;
  quint64 missCount = 0;
  quint64 rasterizationCount = 0;
};

}  // namespace adqt::icons

Q_DECLARE_METATYPE(adqt::icons::IconColorModel)
Q_DECLARE_METATYPE(adqt::icons::IconFit)
Q_DECLARE_METATYPE(adqt::icons::IconColors)
Q_DECLARE_METATYPE(adqt::icons::IconKey)
Q_DECLARE_METATYPE(adqt::icons::IconRef)
Q_DECLARE_METATYPE(adqt::icons::IconMetadata)

#endif  // ADQT_ICON_CORE_TYPES_H
