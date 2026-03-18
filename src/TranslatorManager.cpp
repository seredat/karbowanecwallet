// Copyright (c) 2016-2021 The Karbowanec developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include "Settings.h"
#include "TranslatorManager.h"

using namespace WalletGui;

TranslatorManager* TranslatorManager::m_Instance = 0;

TranslatorManager::TranslatorManager()
{
    QString lang = Settings::instance().getLanguage();
    if (lang.isEmpty()) {
        lang = QLocale::system().name();
        lang = lang.section('_', 0, 0); // cleaner truncate
    }

#if defined(_MSC_VER)
    m_langPath = QApplication::applicationDirPath() + "/languages";

#elif defined(Q_OS_MAC)
    m_langPath = QApplication::applicationDirPath() + "/../Resources/languages/";

#else
    // Default: try relative to executable (works for AppImage + local builds)
    QString basePath = QApplication::applicationDirPath();

    QString appImagePath = basePath + "/../share/karbo/languages";
    QString localPath    = basePath + "/languages";
    QString systemPath   = "/usr/share/karbo/languages";

    if (QDir(appImagePath).exists()) {
        m_langPath = QDir(appImagePath).absolutePath();
    } else if (QDir(localPath).exists()) {
        m_langPath = QDir(localPath).absolutePath();
    } else {
        m_langPath = systemPath;
    }
#endif

    qDebug() << "Translation path:" << m_langPath;

    QDir dir(m_langPath);

    QStringList resources = dir.entryList(QStringList("??.qm"));
    for (const QString& file : resources)
    {
        QString locale = file;
        locale.truncate(locale.lastIndexOf('.'));

        if (locale == lang)
        {
            QTranslator* translator = new QTranslator;
            if (translator->load(file, m_langPath))
            {
                qApp->installTranslator(translator);
                m_keyLang = locale;
                m_translators.insert(locale, translator);
                break;
            }
        }
    }

    // Qt translations
    QString langPathSys;

#if defined(_MSC_VER) || defined(Q_OS_MAC)
    langPathSys = m_langPath;
#else
    QString qtAppImagePath = QApplication::applicationDirPath() + "/../translations";
    QString qtSystemPath   = "/usr/share/qt6/translations";

    if (QDir(qtAppImagePath).exists()) {
        langPathSys = qtAppImagePath;
    } else {
        langPathSys = qtSystemPath;
    }
#endif

    QDir dirSys(langPathSys);

    QStringList filters;
    filters << "qt_??.qm" << "qtbase_??.qm" << "qtmultimedia_??.qm";

    QStringList resourcesQt = dirSys.entryList(filters);

    for (const QString& file : resourcesQt)
    {
        QString locale = file;
        locale.truncate(locale.lastIndexOf('.'));

        QString shortLang = locale.right(2);

        if (shortLang == lang)
        {
            QTranslator* translator = new QTranslator;
            if (translator->load(file, langPathSys))
            {
                qApp->installTranslator(translator);
                m_translators.insert(locale, translator);
            }
        }
    }
}

TranslatorManager::~TranslatorManager()
{
    TranslatorMap::const_iterator i = m_translators.begin();
    while (i != m_translators.end())
    {
        QTranslator* pTranslator = i.value();
        delete pTranslator;
        ++i;
    }

    m_translators.clear();
}

TranslatorManager* TranslatorManager::instance()
{
    static QMutex mutex;
    if (!m_Instance)
    {
        mutex.lock();

        if (!m_Instance)
            m_Instance = new TranslatorManager;

        mutex.unlock();
    }

    return m_Instance;
}

void TranslatorManager::switchTranslator(QTranslator& translator, const QString& filename)
{
    // remove the old translator
    qApp->removeTranslator(&translator);

    // load the new translator
    if(translator.load(filename))
       qApp->installTranslator(&translator);
}
