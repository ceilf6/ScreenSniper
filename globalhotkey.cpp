#include "globalhotkey.h"
#include <QDebug>
#include <QApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#ifdef Q_OS_MAC
#include <Carbon/Carbon.h>
#endif

#ifdef Q_OS_LINUX
#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#endif

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
#ifdef Q_OS_MAC
    , m_eventHandler(nullptr)
#endif
{
    // 安装原生事件过滤器
    qApp->installNativeEventFilter(this);
}

GlobalHotkey::~GlobalHotkey()
{
    unregisterAll();
    qApp->removeNativeEventFilter(this);
}

bool GlobalHotkey::registerHotkey(int id, Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    // 如果已经注册过，先注销
    if (m_hotkeys.contains(id)) {
        unregisterHotkey(id);
    }

    // 先创建数据结构
    HotkeyData data;
    data.key = key;
    data.modifiers = modifiers;
#ifdef Q_OS_MAC
    data.hotkeyRef = nullptr;
#endif
    m_hotkeys[id] = data;
    
    // 注册平台相关的快捷键
    if (registerNativeHotkey(id, key, modifiers)) {
        qDebug() << "Successfully registered hotkey:" << id << "Key:" << key << "Modifiers:" << modifiers;
        return true;
    }
    
    // 注册失败，移除
    m_hotkeys.remove(id);

    qDebug() << "Failed to register hotkey:" << id;
    return false;
}

void GlobalHotkey::unregisterHotkey(int id)
{
    if (m_hotkeys.contains(id)) {
        unregisterNativeHotkey(id);
        m_hotkeys.remove(id);
    }
}

void GlobalHotkey::unregisterAll()
{
    QList<int> ids = m_hotkeys.keys();
    for (int id : ids) {
        unregisterHotkey(id);
    }
}

#ifdef Q_OS_WIN
// Windows 实现
bool GlobalHotkey::registerNativeHotkey(int id, Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    UINT nativeModifiers = 0;
    if (modifiers & Qt::ShiftModifier)
        nativeModifiers |= MOD_SHIFT;
    if (modifiers & Qt::ControlModifier)
        nativeModifiers |= MOD_CONTROL;
    if (modifiers & Qt::AltModifier)
        nativeModifiers |= MOD_ALT;
    if (modifiers & Qt::MetaModifier)
        nativeModifiers |= MOD_WIN;

    // Qt键码转换为Windows虚拟键码
    UINT vk = 0;
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        vk = 'A' + (key - Qt::Key_A);
    } else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        vk = '0' + (key - Qt::Key_0);
    } else if (key >= Qt::Key_F1 && key <= Qt::Key_F24) {
        vk = VK_F1 + (key - Qt::Key_F1);
    } else {
        qDebug() << "Unsupported key:" << key;
        return false;
    }

    bool result = RegisterHotKey(nullptr, id, nativeModifiers | MOD_NOREPEAT, vk);
    if (!result) {
        qDebug() << "RegisterHotKey failed with error:" << GetLastError();
    }
    return result;
}

void GlobalHotkey::unregisterNativeHotkey(int id)
{
    UnregisterHotKey(nullptr, id);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
#else
bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
#endif
{
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY) {
            int id = msg->wParam;
            if (m_hotkeys.contains(id)) {
                emit activated(id);
                return true;
            }
        }
    }
    return false;
}

#elif defined(Q_OS_MAC)
// macOS 实现
static QHash<int, GlobalHotkey*> g_hotkeyMap;

OSStatus hotkeyHandler(EventHandlerCallRef nextHandler, EventRef event, void *userData)
{
    EventHotKeyID hotkeyId;
    GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, NULL, sizeof(hotkeyId), NULL, &hotkeyId);
    
    int id = hotkeyId.id;
    if (g_hotkeyMap.contains(id)) {
        emit g_hotkeyMap[id]->activated(id);
        return noErr;
    }
    
    return eventNotHandledErr;
}

bool GlobalHotkey::registerNativeHotkey(int id, Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    // 转换修饰键
    UInt32 carbonModifiers = 0;
    if (modifiers & Qt::ShiftModifier)
        carbonModifiers |= shiftKey;
    if (modifiers & Qt::ControlModifier)
        carbonModifiers |= cmdKey;  // macOS上Ctrl映射为Cmd
    if (modifiers & Qt::AltModifier)
        carbonModifiers |= optionKey;
    if (modifiers & Qt::MetaModifier)
        carbonModifiers |= controlKey;  // macOS上Meta映射为Control

    // Qt键码转换为Carbon键码
    UInt32 carbonKeyCode = 0;
    bool found = false;
    
    // 字母键
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        carbonKeyCode = kVK_ANSI_A + (key - Qt::Key_A);
        found = true;
        qDebug() << "Mapping letter key:" << key << "to Carbon keycode:" << carbonKeyCode;
    }
    // 数字键
    else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        carbonKeyCode = kVK_ANSI_0 + (key - Qt::Key_0);
        found = true;
    }
    // F键
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        carbonKeyCode = kVK_F1 + (key - Qt::Key_F1);
        found = true;
    }

    if (!found) {
        qDebug() << "Unsupported key on macOS:" << key;
        return false;
    }
    
    qDebug() << "Registering hotkey - Key:" << key << "Carbon keycode:" << carbonKeyCode 
             << "Modifiers:" << carbonModifiers;

    // 安装事件处理器（只安装一次）
    if (m_eventHandler == nullptr) {
        EventTypeSpec eventType;
        eventType.eventClass = kEventClassKeyboard;
        eventType.eventKind = kEventHotKeyPressed;
        
        EventHandlerUPP handlerUPP = NewEventHandlerUPP(hotkeyHandler);
        EventHandlerRef handlerRef;
        
        OSStatus status = InstallApplicationEventHandler(handlerUPP, 1, &eventType, NULL, &handlerRef);
        if (status != noErr) {
            qDebug() << "Failed to install event handler:" << status;
            return false;
        }
        
        m_eventHandler = handlerRef;
    }

    // 注册热键
    EventHotKeyID hotkeyId;
    hotkeyId.signature = 'htk1';
    hotkeyId.id = id;
    
    EventHotKeyRef hotkeyRef;
    OSStatus status = RegisterEventHotKey(carbonKeyCode, carbonModifiers, hotkeyId,
                                          GetApplicationEventTarget(), 0, &hotkeyRef);
    
    if (status != noErr) {
        qDebug() << "RegisterEventHotKey failed with status:" << status;
        return false;
    }

    // 保存 EventHotKeyRef
    HotkeyData &data = m_hotkeys[id];
    data.hotkeyRef = hotkeyRef;
    
    g_hotkeyMap[id] = this;
    return true;
}

void GlobalHotkey::unregisterNativeHotkey(int id)
{
    if (m_hotkeys.contains(id)) {
        HotkeyData &data = m_hotkeys[id];
        if (data.hotkeyRef) {
            UnregisterEventHotKey((EventHotKeyRef)data.hotkeyRef);
            data.hotkeyRef = nullptr;
        }
    }
    g_hotkeyMap.remove(id);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
#else
bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
#endif
{
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
    // macOS使用Carbon事件处理，不需要在这里处理
    return false;
}

#elif defined(Q_OS_LINUX)
// Linux (X11) 实现
bool GlobalHotkey::registerNativeHotkey(int id, Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    Display *display = QX11Info::display();
    if (!display) {
        qDebug() << "Cannot get X11 display";
        return false;
    }

    // 转换修饰键
    unsigned int nativeModifiers = 0;
    if (modifiers & Qt::ShiftModifier)
        nativeModifiers |= ShiftMask;
    if (modifiers & Qt::ControlModifier)
        nativeModifiers |= ControlMask;
    if (modifiers & Qt::AltModifier)
        nativeModifiers |= Mod1Mask;
    if (modifiers & Qt::MetaModifier)
        nativeModifiers |= Mod4Mask;

    // Qt键码转换为X11键码
    KeySym keysym = 0;
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        keysym = XK_a + (key - Qt::Key_A);
    } else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        keysym = XK_0 + (key - Qt::Key_0);
    } else if (key >= Qt::Key_F1 && key <= Qt::Key_F35) {
        keysym = XK_F1 + (key - Qt::Key_F1);
    } else {
        qDebug() << "Unsupported key on Linux:" << key;
        return false;
    }

    KeyCode keycode = XKeysymToKeycode(display, keysym);
    if (keycode == 0) {
        qDebug() << "Invalid keycode for keysym:" << keysym;
        return false;
    }

    // 抓取所有屏幕的根窗口
    Window root = DefaultRootWindow(display);
    
    // 注册快捷键（需要考虑NumLock和CapsLock的状态）
    XGrabKey(display, keycode, nativeModifiers, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, keycode, nativeModifiers | Mod2Mask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, keycode, nativeModifiers | LockMask, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, keycode, nativeModifiers | Mod2Mask | LockMask, root, True, GrabModeAsync, GrabModeAsync);
    
    XSync(display, False);
    
    return true;
}

void GlobalHotkey::unregisterNativeHotkey(int id)
{
    if (!m_hotkeys.contains(id))
        return;

    Display *display = QX11Info::display();
    if (!display)
        return;

    HotkeyData data = m_hotkeys[id];
    
    // 转换修饰键
    unsigned int nativeModifiers = 0;
    if (data.modifiers & Qt::ShiftModifier)
        nativeModifiers |= ShiftMask;
    if (data.modifiers & Qt::ControlModifier)
        nativeModifiers |= ControlMask;
    if (data.modifiers & Qt::AltModifier)
        nativeModifiers |= Mod1Mask;
    if (data.modifiers & Qt::MetaModifier)
        nativeModifiers |= Mod4Mask;

    KeySym keysym = 0;
    if (data.key >= Qt::Key_A && data.key <= Qt::Key_Z) {
        keysym = XK_a + (data.key - Qt::Key_A);
    } else if (data.key >= Qt::Key_0 && data.key <= Qt::Key_9) {
        keysym = XK_0 + (data.key - Qt::Key_0);
    }

    KeyCode keycode = XKeysymToKeycode(display, keysym);
    Window root = DefaultRootWindow(display);

    XUngrabKey(display, keycode, nativeModifiers, root);
    XUngrabKey(display, keycode, nativeModifiers | Mod2Mask, root);
    XUngrabKey(display, keycode, nativeModifiers | LockMask, root);
    XUngrabKey(display, keycode, nativeModifiers | Mod2Mask | LockMask, root);
    
    XSync(display, False);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
#else
bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
#endif
{
    if (eventType == "xcb_generic_event_t") {
        xcb_generic_event_t *event = static_cast<xcb_generic_event_t *>(message);
        if ((event->response_type & ~0x80) == XCB_KEY_PRESS) {
            xcb_key_press_event_t *keyEvent = static_cast<xcb_key_press_event_t *>(message);
            
            // 检查是否匹配已注册的快捷键
            for (auto it = m_hotkeys.begin(); it != m_hotkeys.end(); ++it) {
                // 这里需要匹配键码和修饰键
                // 简化实现，实际应该更精确地匹配
                emit activated(it.key());
                return true;
            }
        }
    }
    return false;
}

#else
// 不支持的平台
bool GlobalHotkey::registerNativeHotkey(int id, Qt::Key key, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(id)
    Q_UNUSED(key)
    Q_UNUSED(modifiers)
    qWarning() << "Global hotkeys are not supported on this platform";
    return false;
}

void GlobalHotkey::unregisterNativeHotkey(int id)
{
    Q_UNUSED(id)
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
#else
bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, long *result)
#endif
{
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
    return false;
}

#endif
