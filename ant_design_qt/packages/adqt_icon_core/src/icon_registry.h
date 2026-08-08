#ifndef ADQT_ICON_REGISTRY_H
#define ADQT_ICON_REGISTRY_H

#include "adqt_icon_core_global.h"
#include "icon_core_types.h"

#include <QCursor>
#include <QIcon>
#include <QList>
#include <QPainter>
#include <QPixmap>
#include <QPoint>
#include <QRectF>

#include <memory>

namespace adqt::icons::detail {
struct IconRegistryImpl;
}

namespace adqt::icons {

class ADQT_ICON_CORE_EXPORT IconRegistry final {
 public:
  IconRegistry();
  ~IconRegistry();
  IconRegistry(const IconRegistry&) = delete;
  IconRegistry& operator=(const IconRegistry&) = delete;

  IconPackRegistrationResult registerPack(const QString& pack,
                                          const QList<IconDefinition>& definitions);
  bool containsIcon(const IconKey& key) const;
  IconMetadata describeIcon(const IconRef& ref) const;
  IconMetadata describeIcon(const IconKey& key) const;
  QList<IconMetadata> listIcons(const QString& pack = QString(),
                                const QString& variant = QString()) const;

  QIcon makeIcon(const IconRef& ref, const IconStatePalette& palette = {}) const;
  QPixmap renderIconPixmap(const IconRef& ref, const IconRenderRequest& request,
                           const IconStatePalette& palette = {}) const;
  void paintIcon(QPainter* painter, const IconRef& ref, const QRectF& rect,
                 const IconRenderRequest& request = {}, const IconStatePalette& palette = {}) const;
  QCursor makeCursor(const IconRef& ref, const QSize& logicalSize, const QPoint& hotSpot,
                     qreal devicePixelRatio = 1.0) const;

  void setPaletteResolver(IconPaletteResolver resolver);
  void clearPaletteResolver();
  void setCacheLimitKB(int kb);
  void clearCache();
  void prewarm(const QList<IconPixmapRequest>& requests) const;
  IconCacheStatistics cacheStatistics() const;

 private:
  IconRef reference(const IconKey& key, const IconColors& colors) const;
  std::shared_ptr<detail::IconRegistryImpl> impl_;
  friend class ExternalIconPack;
};

ADQT_ICON_CORE_EXPORT IconRegistry& defaultRegistry();
ADQT_ICON_CORE_EXPORT IconMetadata describeIcon(const IconRef& ref);
ADQT_ICON_CORE_EXPORT QList<IconMetadata> listIcons(const QString& pack = QString(),
                                                    const QString& variant = QString());
ADQT_ICON_CORE_EXPORT QIcon makeIcon(const IconRef& ref, const IconStatePalette& palette = {});
ADQT_ICON_CORE_EXPORT QPixmap renderIconPixmap(const IconRef& ref, const IconRenderRequest& request,
                                               const IconStatePalette& palette = {});
ADQT_ICON_CORE_EXPORT void paintIcon(QPainter* painter, const IconRef& ref, const QRectF& rect,
                                     const IconRenderRequest& request = {},
                                     const IconStatePalette& palette = {});
ADQT_ICON_CORE_EXPORT QCursor makeCursor(const IconRef& ref, const QSize& logicalSize,
                                         const QPoint& hotSpot, qreal devicePixelRatio = 1.0);
ADQT_ICON_CORE_EXPORT void setPaletteResolver(IconPaletteResolver resolver);
ADQT_ICON_CORE_EXPORT void clearPaletteResolver();
ADQT_ICON_CORE_EXPORT void setCacheLimitKB(int kb);
ADQT_ICON_CORE_EXPORT void clearCache();
ADQT_ICON_CORE_EXPORT void prewarm(const QList<IconPixmapRequest>& requests);
ADQT_ICON_CORE_EXPORT IconCacheStatistics cacheStatistics();

}  // namespace adqt::icons

#endif  // ADQT_ICON_REGISTRY_H
