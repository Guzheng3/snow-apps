#include "external_icon_pack.h"

#include <QByteArrayView>
#include <QCryptographicHash>
#include <QMutex>
#include <QMutexLocker>

namespace adqt::icons {

struct ExternalIconPack::Impl final {
  explicit Impl(ExternalIconPackDefinition value) : definition(std::move(value)) {}
  ExternalIconPackDefinition definition;
  mutable QMutex mutex;
  mutable QHash<IconRegistry*, IconPackRegistrationResult> registrations;
};

ExternalIconPack::ExternalIconPack(ExternalIconPackDefinition definition)
    : impl_(std::make_unique<Impl>(std::move(definition))) {}

ExternalIconPack::~ExternalIconPack() = default;

const ExternalIconPackDefinition& ExternalIconPack::definition() const { return impl_->definition; }

IconPackRegistrationResult ExternalIconPack::registerWith(IconRegistry& registry) const {
  QMutexLocker lock(&impl_->mutex);
  const auto previous = impl_->registrations.constFind(&registry);
  if (previous != impl_->registrations.constEnd() && previous->ok()) return previous.value();

  QList<IconDefinition> definitions;
  definitions.reserve(impl_->definition.entries.size());
  QCryptographicHash packHash(QCryptographicHash::Sha256);
  for (const ExternalIconPackEntry& entry : impl_->definition.entries) {
    IconDefinition definition;
    definition.key = {impl_->definition.pack, entry.variant, entry.name};
    definition.colorModel = entry.colorModel;
    definition.fit = entry.fit;
    definition.defaultColors = entry.defaultColors;
    definition.svg = entry.svg;
    definition.sourceHash = entry.sourceHash;
    definition.allowEmbeddedDataImages = entry.allowEmbeddedDataImages;
    definitions.append(definition);
    const QByteArray variantUtf8 = entry.variant.toUtf8();
    const QByteArray nameUtf8 = entry.name.toUtf8();
    packHash.addData(QByteArrayView{variantUtf8});
    packHash.addData(QByteArrayView{"\0", 1});
    packHash.addData(QByteArrayView{nameUtf8});
    packHash.addData(QByteArrayView{"\0", 1});
    packHash.addData(QByteArrayView{entry.sourceHash});
    packHash.addData(QByteArrayView{"\n", 1});
  }

  IconPackRegistrationResult result;
  const QByteArray actualPackHash = packHash.result().toHex();
  if (!impl_->definition.contentHash.isEmpty() &&
      impl_->definition.contentHash.toLower() != actualPackHash) {
    result.diagnostics.append({IconRegistrationError::HashMismatch,
                               {},
                               QStringLiteral("external pack content hash mismatch")});
  } else {
    result = registry.registerPack(impl_->definition.pack, definitions);
  }
  impl_->registrations.insert(&registry, result);
  return result;
}

IconPackRegistrationResult ExternalIconPack::ensureRegistered() const {
  return registerWith(defaultRegistry());
}

IconRef ExternalIconPack::icon(const QString& variant, const QString& name,
                               const IconColors& colors) const {
  return icon(defaultRegistry(), variant, name, colors);
}

IconRef ExternalIconPack::icon(IconRegistry& registry, const QString& variant, const QString& name,
                               const IconColors& colors) const {
  const IconPackRegistrationResult result = registerWith(registry);
  if (!result.ok()) return {};
  return registry.reference({impl_->definition.pack, variant, name}, colors);
}

}  // namespace adqt::icons
