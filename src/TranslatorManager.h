// Copyright (c) 2016-2026 The Karbowanec developers
// Distributed under the MIT/X11 software license

#ifndef TRANSLATORMANAGER_H
#define TRANSLATORMANAGER_H

#include <QObject>
#include <QMap>
#include <QTranslator>
#include <QMutex>

typedef QMap<QString, QTranslator*> TranslatorMap;

class TranslatorManager : public QObject
{
    Q_OBJECT

public:
    static TranslatorManager* instance();
    ~TranslatorManager();

    void switchLanguage(const QString& lang);
    QString getCurrentLang() const { return m_keyLang; }
    QString getLangPath() const { return m_langPath; }

signals:
    void languageChanged();

private:
    TranslatorManager();

    // Disable copy
    TranslatorManager(const TranslatorManager &) = delete;
    TranslatorManager& operator=(const TranslatorManager &) = delete;

    void loadLanguageInternal(const QString& lang);
    void clearTranslators();

    static TranslatorManager* m_Instance;

    TranslatorMap   m_translators;
    QString         m_keyLang;
    QString         m_langPath;
};

#endif // TRANSLATORMANAGER_H
