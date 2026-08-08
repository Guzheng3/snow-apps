#ifndef SNOW_SHOT_NETWORK_SNOWSHOTAPICLIENT_H
#define SNOW_SHOT_NETWORK_SNOWSHOTAPICLIENT_H

#include <QHash>
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

struct SnowShotTableResult {
    QString html;
    QString error;
    QString code;
    int httpStatus = 0;

    [[nodiscard]] bool succeeded() const {
        return !html.trimmed().isEmpty() && error.isEmpty();
    }
};

class SnowShotApiClient final : public QObject {
    Q_OBJECT

  public:
    using RequestToken = quint64;
    using Completion = std::function<void(SnowShotTableResult)>;

    explicit SnowShotApiClient(QString baseUrl, QObject* parent = nullptr);
    ~SnowShotApiClient() override;

    [[nodiscard]] const QString& baseUrl() const;
    [[nodiscard]] RequestToken extractTable(const QImage& image, QObject* receiver,
                                             Completion completion);
    void cancel(RequestToken token);

    [[nodiscard]] static QImage prepareImage(const QImage& image);
    [[nodiscard]] static QByteArray encodeWebp(const QImage& image);
    [[nodiscard]] static QString formatFailure(int httpStatus, const QString& failureCode,
                                               const QString& description);

  private:
    struct Request;
    void finish(RequestToken token, SnowShotTableResult result);

    QString m_baseUrl;
    RequestToken m_nextToken = 0;
    QHash<RequestToken, Request*> m_requests;
};

#endif // SNOW_SHOT_NETWORK_SNOWSHOTAPICLIENT_H
