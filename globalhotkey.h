#ifndef GLOBALHOTKEY_H
#define GLOBALHOTKEY_H

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QHash>

// 跨平台全局快捷键管理器
class GlobalHotkey : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT

public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey();

    // 注册全局快捷键
    bool registerHotkey(int id, Qt::Key key, Qt::KeyboardModifiers modifiers);

    // 注销全局快捷键
    void unregisterHotkey(int id);

    // 注销所有快捷键
    void unregisterAll();

signals:
    void activated(int id);

protected:
    // Qt原生事件过滤器
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;
#else
    bool nativeEventFilter(const QByteArray &eventType, void *message, long *result) override;
#endif

private:
    struct HotkeyData
    {
        Qt::Key key;
        Qt::KeyboardModifiers modifiers;
#ifdef Q_OS_WIN
        quint32 nativeKey;
        quint32 nativeModifiers;
#endif
#ifdef Q_OS_MAC
        void *hotkeyRef; // EventHotKeyRef
#endif
    };

    QHash<int, HotkeyData> m_hotkeys;

    // 将Qt的键和修饰符转换为平台相关的键码
    bool registerNativeHotkey(int id, Qt::Key key, Qt::KeyboardModifiers modifiers);
    void unregisterNativeHotkey(int id);

#ifdef Q_OS_MAC
    void *m_eventHandler; // macOS事件处理器
#endif
};

#endif // GLOBALHOTKEY_H
