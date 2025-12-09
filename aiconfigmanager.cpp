#include "aiconfigmanager.h"

AIConfigManager::AIConfigManager()
{
    // 尝试多个可能的配置文件路径
    QStringList configPaths;

    // 1. 当前工作目录下的config.ini（最优先）
    QString currentDirPath = QDir::currentPath() + "/config.ini";
    configPaths << currentDirPath;

    // 2. 项目根目录下的config.ini
    QString projectRootPath = QCoreApplication::applicationDirPath() + "/../../../../config.ini";
    configPaths << projectRootPath;

    // 3. 可执行文件同级目录下的config.ini
    QString exeDirPath = QCoreApplication::applicationDirPath() + "/config.ini";
    configPaths << exeDirPath;

    // 选择第一个存在的配置文件
    m_configPath = exeDirPath;
    for (const QString &path : configPaths)
    {
        if (QFile::exists(path))
        {
            m_configPath = path;
            break;
        }
    }

    // 如果文件不存在，创建一个默认的
    if (!QFile::exists(m_configPath))
    {
        // 创建默认配置
        QMap<QString, QMap<QString, QString>> defaultConfig;
        
        // 通用配置
        defaultConfig["General"]["ServiceType"] = "Aliyun";
        
        // 阿里云配置
        defaultConfig["Aliyun"]["ApiKey"] = "sk-c30504bf26dd4dfebb3342e7f2a9af4d";
        defaultConfig["Aliyun"]["Model"] = "qwen-vl-max";
        
        // OpenAI配置
        defaultConfig["OpenAI"]["ApiKey"] = "";
        defaultConfig["OpenAI"]["Model"] = "gpt-4-vision-preview";
        defaultConfig["OpenAI"]["Endpoint"] = "https://api.openai.com/v1/chat/completions";
        
        // 保存默认配置
        saveIniFile(m_configPath, defaultConfig);
        m_configCache = defaultConfig;
    }
    else
    {
        // 手动解析配置文件
        m_configCache = parseIniFile(m_configPath);
        
        // 兼容旧版本配置文件
        if (m_configCache.contains("QianWen"))
        {
            // 将旧版本配置迁移到新版本格式
            m_configCache["Aliyun"]["ApiKey"] = m_configCache["QianWen"]["ApiKey"];
            m_configCache["Aliyun"]["Model"] = m_configCache["QianWen"].value("Model", "qwen-vl-max");
            m_configCache.remove("QianWen");
            
            // 保存迁移后的配置
            saveIniFile(m_configPath, m_configCache);
        }
    }
}

// 手动解析INI文件
QMap<QString, QMap<QString, QString>> AIConfigManager::parseIniFile(const QString &filePath)
{
    QMap<QString, QMap<QString, QString>> config;
    QString currentSection = "";
    
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);
        while (!in.atEnd())
        {
            QString line = in.readLine().trimmed();
            
            // 跳过空行和注释
            if (line.isEmpty() || line.startsWith(';') || line.startsWith('#'))
            {
                continue;
            }
            
            // 处理节
            if (line.startsWith('[') && line.endsWith(']'))
            {
                currentSection = line.mid(1, line.length() - 2).trimmed();
                continue;
            }
            
            // 处理键值对
            int equalsIndex = line.indexOf('=');
            if (equalsIndex != -1)
            {
                QString key = line.left(equalsIndex).trimmed();
                QString value = line.mid(equalsIndex + 1).trimmed();
                
                // 移除行尾注释
                int commentIndex = value.indexOf(';');
                if (commentIndex != -1)
                {
                    value = value.left(commentIndex).trimmed();
                }
                commentIndex = value.indexOf('#');
                if (commentIndex != -1)
                {
                    value = value.left(commentIndex).trimmed();
                }
                
                // 确保节存在
                if (!config.contains(currentSection))
                {
                    config[currentSection] = QMap<QString, QString>();
                }
                
                config[currentSection][key] = value;
            }
        }
        
        file.close();
    }
    
    return config;
}

// 保存INI文件
void AIConfigManager::saveIniFile(const QString &filePath, const QMap<QString, QMap<QString, QString>> &config)
{
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream out(&file);
        
        // 遍历所有节
        for (auto sectionIter = config.constBegin(); sectionIter != config.constEnd(); ++sectionIter)
        {
            const QString &section = sectionIter.key();
            const QMap<QString, QString> &sectionConfig = sectionIter.value();
            
            // 写入节
            out << "[" << section << "]" << Qt::endl;
            
            // 写入节内的键值对
            for (auto keyIter = sectionConfig.constBegin(); keyIter != sectionConfig.constEnd(); ++keyIter)
            {
                const QString &key = keyIter.key();
                const QString &value = keyIter.value();
                
                out << key << "=" << value << Qt::endl;
            }
            
            // 节之间空一行
            out << Qt::endl;
        }
        
        file.close();
    }
}

// 从配置中获取值
QString AIConfigManager::getConfigValue(const QString &section, const QString &key, const QString &defaultValue)
{
    if (m_configCache.contains(section) && m_configCache[section].contains(key))
    {
        return m_configCache[section][key];
    }
    return defaultValue;
}

// 设置配置值
void AIConfigManager::setConfigValue(const QString &section, const QString &key, const QString &value)
{
    m_configCache[section][key] = value;
    saveIniFile(m_configPath, m_configCache);
}

QString AIConfigManager::serviceTypeToString(ServiceType type) const
{
    switch (type)
    {
    case Aliyun:
        return "Aliyun";
    case OpenAI:
        return "OpenAI";
    default:
        return "Aliyun";
    }
}

AIConfigManager::ServiceType AIConfigManager::stringToServiceType(const QString &typeStr) const
{
    if (typeStr.compare("OpenAI", Qt::CaseInsensitive) == 0)
    {
        return OpenAI;
    }
    return Aliyun;
}

AIConfigManager::ServiceType AIConfigManager::getServiceType()
{
    QString typeStr = getConfigValue("General", "ServiceType", "Aliyun");
    
    if (typeStr.isEmpty())
    {
        typeStr = "Aliyun";
    }
    
    return stringToServiceType(typeStr);
}

void AIConfigManager::setServiceType(ServiceType type)
{
    setConfigValue("General", "ServiceType", serviceTypeToString(type));
}

QString AIConfigManager::getApiKey()
{
    ServiceType type = getServiceType();
    QString section = (type == OpenAI ? "OpenAI" : "Aliyun");
    
    return getConfigValue(section, "ApiKey", "");
}

void AIConfigManager::setApiKey(const QString &key)
{
    ServiceType type = getServiceType();
    QString section = (type == OpenAI ? "OpenAI" : "Aliyun");
    
    setConfigValue(section, "ApiKey", key);
}

QString AIConfigManager::getModelName()
{
    ServiceType type = getServiceType();
    QString section = (type == OpenAI ? "OpenAI" : "Aliyun");
    QString defaultValue = (type == OpenAI ? "gpt-4-vision-preview" : "qwen-vl-max");
    
    return getConfigValue(section, "Model", defaultValue);
}

QString AIConfigManager::getEndpoint()
{
    return getConfigValue("OpenAI", "Endpoint", "https://api.openai.com/v1/chat/completions");
}