// Copyright (c) 2016-2026 The Karbowanec developers
// Distributed under the MIT/X11 software license

#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>
#include <QEvent>
#include <QLocale>

#include "Settings.h"
#include "TranslatorManager.h"

using namespace WalletGui;

TranslatorManager* TranslatorManager::m_Instance = nullptr;

TranslatorManager::TranslatorManager()
{
    QString lang = Settings::instance().getLanguage();
    if (lang.isEmpty()) {
        lang = QLocale::system().name().section('_', 0, 0);
    }

#if defined(_MSC_VER)
    m_langPath = QApplication::applicationDirPath() + "/languages";

#elif defined(Q_OS_MAC)
    m_langPath = QApplication::applicationDirPath() + "/../Resources/languages/";

#else
    QString basePath = QApplication::applicationDirPath();
    QString appDir = qEnvironmentVariable("APPDIR");
    bool isAppImage = !appDir.isEmpty();

    QString appImagePath = basePath + "/../share/karbo/languages";
    QString localPath    = basePath + "/languages";
    QString systemPath   = "/usr/share/karbo/languages";

    if (isAppImage) {
        m_langPath = QDir(appImagePath).absolutePath();
        m_langPath = appDir + "/usr/share/karbo/languages";
        qDebug() << "AppImage detected! Using internal path:" << m_langPath;
    } else if (QDir(localPath).exists()) {
        m_langPath = QDir(localPath).absolutePath();
    } else {
        m_langPath = systemPath;
    }
#endif

    qDebug() << "Translation path:" << m_langPath;

    loadLanguageInternal(lang);
}

TranslatorManager::~TranslatorManager()
{
    clearTranslators();
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

void TranslatorManager::clearTranslators()
{
    for (auto it = m_translators.begin(); it != m_translators.end(); ++it)
    {
        qApp->removeTranslator(it.value());
        delete it.value();
    }
    m_translators.clear();
    m_keyLang.clear();
}

void TranslatorManager::loadLanguageInternal(const QString& lang)
{
    // --- App translations ---
    QDir dir(m_langPath);
    QStringList resources = dir.entryList(QStringList("??.qm"));

    for (const QString& file : resources)
    {
        QString locale = file.section('_', -1).section('.', 0, 0);

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
            delete translator;
        }
    }

    // --- Qt translations ---
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
        QString locale = file.section('_', -1).section('.', 0, 0);

        if (locale == lang)
        {
            QTranslator* translator = new QTranslator;

            if (translator->load(file, langPathSys))
            {
                qApp->installTranslator(translator);
                m_translators.insert(file, translator);
            } else {
                delete translator;
            }
        }
    }
}

void TranslatorManager::switchLanguage(const QString& lang)
{
    if (lang == m_keyLang)
        return;

    qDebug() << "Switching language to:" << lang;

    clearTranslators();
    loadLanguageInternal(lang);

    Settings::instance().setLanguage(lang);

    // Notify UI
    QEvent event(QEvent::LanguageChange);
    QCoreApplication::sendEvent(qApp, &event);

    emit languageChanged();
}
