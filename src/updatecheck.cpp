#include "updatecheck.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

UpdateCheck::UpdateCheck(QObject *parent)
    : QObject(parent),
      m_nam(new QNetworkAccessManager(this))
{
}

void UpdateCheck::check()
{
    QNetworkRequest req(QUrl(QStringLiteral(
        "https://api.github.com/repos/mekineer-com/nterminal/releases?per_page=1")));
    req.setRawHeader("Accept", "application/vnd.github+json");

    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;

        const QJsonArray releases = QJsonDocument::fromJson(reply->readAll()).array();
        if (releases.isEmpty())
            return;

        const QJsonObject rel = releases.first().toObject();
        const QString tag = rel.value(QStringLiteral("tag_name")).toString();
        const QString url = rel.value(QStringLiteral("html_url")).toString();
        if (tag.isEmpty())
            return;

        const QString current = QStringLiteral(NTERMINAL_VERSION);
        // Strip leading 'v' for comparison.
        const QString remoteVer = tag.startsWith(QLatin1Char('v')) ? tag.mid(1) : tag;
        if (remoteVer != current)
            emit updateAvailable(current, tag, url);
    });
}
