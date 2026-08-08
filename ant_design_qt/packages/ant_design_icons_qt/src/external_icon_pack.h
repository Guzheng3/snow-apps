#ifndef ADQT_EXTERNAL_ICON_PACK_H
#define ADQT_EXTERNAL_ICON_PACK_H

#include "ant_design_icons_qt_global.h"
#include "icon_core.h"

#include <QList>
#include <QString>

#include <memory>

namespace adqt::icons {

struct ADQT_ICONS_EXPORT ExternalIconPackEntry final {
  QString variant;
  QString name;
  IconColorModel colorModel = IconColorModel::Monochrome;
  IconFit fit = IconFit::Contain;
  IconColors defaultColors;
  QByteArray svg;
  QByteArray sourceHash;
  bool allowEmbeddedDataImages = false;
};

struct ADQT_ICONS_EXPORT ExternalIconPackDefinition final {
  QString pack;
  QString source;
  QByteArray contentHash;
  QList<ExternalIconPackEntry> entries;
};

class ADQT_ICONS_EXPORT ExternalIconPack final {
 public:
  explicit ExternalIconPack(ExternalIconPackDefinition definition);
  ~ExternalIconPack();
  ExternalIconPack(const ExternalIconPack&) = delete;
  ExternalIconPack& operator=(const ExternalIconPack&) = delete;

  const ExternalIconPackDefinition& definition() const;
  IconPackRegistrationResult registerWith(IconRegistry& registry) const;
  IconPackRegistrationResult ensureRegistered() const;
  IconRef icon(const QString& variant, const QString& name, const IconColors& colors = {}) const;
  IconRef icon(IconRegistry& registry, const QString& variant, const QString& name,
               const IconColors& colors = {}) const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace adqt::icons

#endif  // ADQT_EXTERNAL_ICON_PACK_H
