#ifndef SNOW_SHOT_PRESENTATION_SYSTEMTRAYCONTROLLER_H
#define SNOW_SHOT_PRESENTATION_SYSTEMTRAYCONTROLLER_H

#include <QObject>
#include <QString>

#include <memory>

namespace snow_shot::presentation {
class SystemTrayController final : public QObject {
    Q_OBJECT

  public:
    explicit SystemTrayController(QObject* parent = nullptr);
    ~SystemTrayController() override;

    void show();
    void hide();
    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const;
    void setIconSelection(const QString& selection);
    [[nodiscard]] QString iconSelection() const;
    void setCustomIconPath(const QString& path);
    [[nodiscard]] QString customIconPath() const;

  signals:
    void screenshotRequested();
    void showMainWindowRequested();
    void exitRequested();

  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
} // namespace snow_shot::presentation

#endif // SNOW_SHOT_PRESENTATION_SYSTEMTRAYCONTROLLER_H
