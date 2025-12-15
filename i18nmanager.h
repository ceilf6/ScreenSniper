#ifndef I18NMANAGER_H
#define I18NMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QMap>
#include <QList>
#include <QMutex>

/**
 * @brief 国际化管理器，使用单例模式
 * 负责加载和管理翻译文件，提供翻译文本的获取接口
 * 支持发布订阅机制，当语言变化时通知订阅者
 */
class I18nManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     * @return I18nManager* 单例指针
     */
    static I18nManager *instance();

    /**
     * @brief 加载翻译文件
     * @param language 语言代码，如"zh"、"en"、"zhHK"
     * @return bool 是否加载成功
     */
    bool loadLanguage(const QString &language);

    /**
     * @brief 获取当前语言
     * @return QString 当前语言代码
     */
    QString currentLanguage() const;

    /**
     * @brief 获取翻译文本
     * @param key 翻译键
     * @param defaultValue 默认值，当找不到对应翻译时返回
     * @return QString 翻译后的文本
     */
    QString getText(const QString &key, const QString &defaultValue = "") const;

    /**
     * @brief 获取支持的语言列表
     * @return QStringList 支持的语言代码列表
     */
    QStringList supportedLanguages() const;

    /**
     * @brief 保存当前语言设置
     */
    void saveLanguageSetting();

    /**
     * @brief 加载语言设置
     */
    void loadLanguageSetting();

signals:
    /**
     * @brief 语言变化信号
     * @param newLanguage 新的语言代码
     */
    void languageChanged(const QString &newLanguage);

private:
    /**
     * @brief 构造函数，私有，防止外部实例化
     */
    explicit I18nManager(QObject *parent = nullptr);

    /**
     * @brief 析构函数，私有
     */
    ~I18nManager();

    /**
     * @brief 加载翻译文件
     * @param filePath 文件路径
     * @return bool 是否加载成功
     */
    bool loadTranslationFile(const QString &filePath);

    QString m_currentLanguage;                  // 当前语言代码
    QJsonObject m_translations;                 // 当前语言的翻译数据
    QMap<QString, QJsonObject> m_languageCache; // 语言缓存，避免重复加载
    QStringList m_supportedLanguages;           // 支持的语言列表
};

#endif // I18NMANAGER_H
