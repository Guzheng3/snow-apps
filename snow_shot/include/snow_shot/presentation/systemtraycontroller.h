#ifndef SNOW_SHOT_PRESENTATION_SYSTEMTRAYCONTROLLER_H
#define SNOW_SHOT_PRESENTATION_SYSTEMTRAYCONTROLLER_H

#include <QObject>

#include <memory>

namespace snow_shot::presentation {
class SystemTrayController final : public QObject {
    Q_OBJECT

  public:
    explicit SystemTrayController(QObject* parent = nullptr);
    ~SystemTrayController() override;

    void show();
    void hide();

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
