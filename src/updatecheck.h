#ifndef UPDATECHECK_H
#define UPDATECHECK_H

#include <QObject>

class QNetworkAccessManager;

class UpdateCheck : public QObject
{
    Q_OBJECT

public:
    explicit UpdateCheck(QObject *parent = nullptr);
    void check();

signals:
    void updateAvailable(const QString &current, const QString &latest, const QString &url);

private:
    QNetworkAccessManager *m_nam;
};

#endif
