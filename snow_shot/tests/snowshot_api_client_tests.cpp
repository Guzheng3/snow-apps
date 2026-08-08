#include "snow_shot/network/snowshotapiclient.h"

#include <QCoreApplication>
#include <QImage>

#include <cstdlib>
#include <iostream>

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
} // namespace

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);

    QImage wide(4000, 2000, QImage::Format_RGBA8888);
    wide.fill(Qt::white);
    const QImage preparedWide = SnowShotApiClient::prepareImage(wide);
    require(preparedWide.size() == QSize(2880, 1440),
            "wide images should scale proportionally to a 2880px longest side");

    QImage small(1280, 720, QImage::Format_RGBA8888);
    small.fill(Qt::black);
    require(SnowShotApiClient::prepareImage(small).size() == small.size(),
            "small images should not be upscaled");

    const QByteArray webp = SnowShotApiClient::encodeWebp(small);
    require(webp.size() > 12 && webp.left(4) == QByteArrayLiteral("RIFF") &&
                webp.mid(8, 4) == QByteArrayLiteral("WEBP"),
            "table requests should be encoded as WebP");

    require(SnowShotApiClient::formatFailure(503, QStringLiteral("SERVICE_BUSY"),
                                             QStringLiteral("  Service unavailable  ")) ==
                QStringLiteral("503: Service unavailable"),
            "HTTP failures should show only the HTTP status and concise description");
    require(SnowShotApiClient::formatFailure(200, QStringLiteral("TABLE_NOT_FOUND"),
                                             QStringLiteral("No table was detected")) ==
                QStringLiteral("TABLE_NOT_FOUND: No table was detected"),
            "API failures should show the failure code and description");
    require(SnowShotApiClient::formatFailure(0, {}, QStringLiteral("  Connection\nfailed ")) ==
                QStringLiteral("Connection failed"),
            "transport failures without a code should remain concise");
    return 0;
}
