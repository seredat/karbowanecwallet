// Copyright (c) 2016-2020 The Karbowanec developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "Update.h"
#include "Settings.h"
#include <QApplication>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QMessageBox>
#include <QtNetwork>
#include <QVersionNumber>

using namespace WalletGui;

Updater::Updater(QObject *parent) :
    QObject(parent)
{
}

void Updater::checkForUpdate()
{
    manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished,
            this, &Updater::replyFinished);

    // Proceed with the update check
    QNetworkRequest request((QUrl(KARBO_UPDATE_URL)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    manager->get(request);
}

void Updater::replyFinished(QNetworkReply *reply)
{
    if(reply->error())
    {
        QString error = QString(tr("Error: %1")).arg(reply->errorString());
        qDebug() << tr("Unable to check for update") << ": " << error;
    }
    else
    {
        QString ourVersionStr = Settings::instance().getVersion().split("-")[0];
        if (ourVersionStr.startsWith(QStringLiteral("v.")))
            ourVersionStr.remove(0, 2);
        else if (ourVersionStr.startsWith('v'))
            ourVersionStr.remove(0, 1);

        QJsonArray jsonArray = QJsonDocument::fromJson(reply->readAll()).array();
        QString remoteVersionStr = jsonArray[0].toObject()["name"].toString();
        if (remoteVersionStr.startsWith(QStringLiteral("v.")))
            remoteVersionStr.remove(0, 2);
        else if (remoteVersionStr.startsWith('v'))
            remoteVersionStr.remove(0, 1);

        int suffixIndex;
        QVersionNumber ourVersion = QVersionNumber::fromString(ourVersionStr, &suffixIndex);
        QVersionNumber remoteVersion = QVersionNumber::fromString(remoteVersionStr, &suffixIndex);

        bool r = QVersionNumber::compare(ourVersion, remoteVersion) < 0;
        if (r) {
            QString message = QObject::tr(
                                  "There is an update available.\n"
                                  "Your version: %1\n"
                                  "New version: %2\n"
                                  "Do you want to go to the download page?"
                                  ).arg(ourVersion.toString(), remoteVersion.toString());

            if (QMessageBox::warning(nullptr, QObject::tr("New version available"),
                                     message,
                                     QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Ok) {
                QDesktopServices::openUrl(QUrl(KARBO_DOWNLOAD_URL));
            }
        }
    }
    reply->deleteLater();
}
