#include "i18nmanager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QSettings>
#include <QDebug>

I18nManager::I18nManager(QObject *parent)
    : QObject(parent),
      m_currentLanguage("zh") // 默认语言为中文
{
    // 初始化支持的语言列表
    m_supportedLanguages << "zh" << "en" << "zhHK";

    // 尝试加载保存的语言设置
    loadLanguageSetting();

    // 强制加载翻译文件，不检查语言是否变化
    // 因为loadLanguageSetting可能已经修改了m_currentLanguage
    QString languageToLoad = m_currentLanguage;
    m_currentLanguage = "";
    loadLanguage(languageToLoad);
}

I18nManager::~I18nManager()
{
    // 保存当前语言设置
    saveLanguageSetting();
}

I18nManager *I18nManager::instance()
{
    static I18nManager instance;
    return &instance;
}

bool I18nManager::loadLanguage(const QString &language)
{
    // 检查语言是否支持
    if (!m_supportedLanguages.contains(language))
    {
        qWarning() << "Unsupported language:" << language;
        return false;
    }

    // 如果语言没有变化，直接返回
    if (m_currentLanguage == language)
    {
        return true;
    }

    // 尝试从缓存加载
    if (m_languageCache.contains(language))
    {
        m_translations = m_languageCache.value(language);
        m_currentLanguage = language;
        // 保存语言设置
        saveLanguageSetting();
        emit languageChanged(language);
        return true;
    }

    // 构建翻译文件路径
    QString filePath = QDir::currentPath() + "/locales/" + language + ".json";

    // 如果当前目录没有，尝试从资源文件加载
    if (!QFile::exists(filePath))
    {
        filePath = ":/locales/" + language + ".json";
    }

    // 加载翻译文件
    if (loadTranslationFile(filePath))
    {
        m_currentLanguage = language;
        // 缓存翻译数据
        m_languageCache[language] = m_translations;
        // 保存语言设置
        saveLanguageSetting();
        // 发送语言变化信号
        emit languageChanged(language);
        return true;
    }

    return false;
}

QString I18nManager::currentLanguage() const
{
    return m_currentLanguage;
}

QString I18nManager::getText(const QString &key, const QString &defaultValue) const
{
    // 检查键是否存在
    if (m_translations.contains(key))
    {
        return m_translations[key].toString();
    }

    // 找不到对应翻译，返回默认值
    qWarning() << "Translation not found for key:" << key;
    return defaultValue;
}

QStringList I18nManager::supportedLanguages() const
{
    return m_supportedLanguages;
}

bool I18nManager::loadTranslationFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qWarning() << "Failed to open translation file:" << filePath;
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData, &parseError);

    if (parseError.error != QJsonParseError::NoError)
    {
        qWarning() << "Failed to parse translation file:" << filePath;
        qWarning() << "Parse error:" << parseError.errorString();
        return false;
    }

    if (!jsonDoc.isObject())
    {
        qWarning() << "Translation file is not a valid JSON object:" << filePath;
        return false;
    }

    m_translations = jsonDoc.object();
    return true;
}

void I18nManager::saveLanguageSetting()
{
    // 使用 QSettings 保存语言设置
    QSettings settings("ScreenSniper", "ScreenSniper");
    settings.setValue("language", m_currentLanguage);
    settings.sync();
}

void I18nManager::loadLanguageSetting()
{
    // 使用 QSettings 加载语言设置
    // 兼容旧版本的设置，使用空的组织名和应用名
    QSettings settings_old;
    QSettings settings_new("ScreenSniper", "ScreenSniper");

    // 先尝试从新的设置中加载
    QString savedLanguage = settings_new.value("language").toString();

    // 如果新设置中没有，尝试从旧设置中加载
    if (savedLanguage.isEmpty())
    {
        savedLanguage = settings_old.value("language").toString();
    }

    // 如果仍然没有，使用默认语言
    if (savedLanguage.isEmpty())
    {
        savedLanguage = "zh";
    }

    // 检查保存的语言是否支持
    if (m_supportedLanguages.contains(savedLanguage))
    {
        m_currentLanguage = savedLanguage;
    }
    else
    {
        // 如果不支持，使用默认语言
        m_currentLanguage = "zh";
    }

    // 将设置保存到新的位置
    settings_new.setValue("language", m_currentLanguage);
    settings_new.sync();
}
