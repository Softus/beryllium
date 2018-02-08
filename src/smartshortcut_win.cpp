/*
 * Copyright (C) 2013-2018 Softus Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "smartshortcut.h"
#include <QAbstractNativeEventFilter>
#include <QApplication>
#include <QDebug>
#include <QKeyEvent>
#include <qt_windows.h>

class WinShortcut
    : public QAbstractNativeEventFilter
{
public:
    WinShortcut()
    {
         qApp->installNativeEventFilter(this);
    }

    ~WinShortcut()
    {
         qApp->removeNativeEventFilter(this);
    }

    bool grabKey(int qtKey);

protected:
    virtual bool nativeEventFilter
        ( const QByteArray& eventType
        , void* message
        , long* result
        )
    {
        Q_UNUSED(eventType);
        Q_UNUSED(result);

        auto msg = static_cast<const MSG*>(message);
        if (msg->message == WM_HOTKEY)
        {
            auto keycode = toQtKey(HIWORD(msg->lParam));
            auto modifiers = toQtModifiers(LOWORD(msg->lParam));

            QCoreApplication::postEvent(qApp,
                new QKeyEvent(QEvent::KeyPress, keycode, modifiers));
            QCoreApplication::postEvent(qApp,
                new QKeyEvent(QEvent::KeyRelease, keycode, modifiers));
        }
        return false;
    }
private:
    quint32 toNativeKey(int qtKey);
    quint32 toNativeModifiers(int qtModifiers);
    Qt::Key toQtKey(quint32 nativeKey);
    Qt::KeyboardModifiers toQtModifiers(quint32 nativeModifiers);
};

static WinShortcut* s_pNativeHandler = nullptr;

bool ungrabKey(int key)
{
    if (SmartShortcut::isMouse(key))
    {
        return false;
    }

    auto status = UnregisterHotKey(nullptr, key);
    qDebug() << "release key" << SmartShortcut::toString(key) << status;
    return status;
}

bool grabKey(int key)
{
    if (SmartShortcut::isMouse(key))
    {
        qWarning() << "Grabbing mouse buttons is not supported in windows";
        return false;
    }

    if (s_pNativeHandler == nullptr)
    {
        s_pNativeHandler = new WinShortcut();
    }

    auto status = s_pNativeHandler->grabKey(key);
    qDebug() << "grab key" << SmartShortcut::toString(key) << status;
    return status;
}

bool WinShortcut::grabKey(int qtKey)
{
    auto nativeKey = toNativeKey(qtKey & 0x01ffffff);
    auto nativeModifiers = toNativeModifiers(qtKey);
    return RegisterHotKey(nullptr, qtKey, nativeModifiers, nativeKey);
}

quint32 WinShortcut::toNativeKey(int qtKey)
{
    if ((qtKey >= Qt::Key_0 && qtKey <= Qt::Key_Z))
    {
        return qtKey;
    }

    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24)
    {
        return qtKey - Qt::Key_F1 + VK_F1;
    }

    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
    {
        return qtKey - Qt::Key_0 + VK_NUMPAD0;
    }

    switch (qtKey)
    {
    case Qt::Key_Cancel:         return VK_CANCEL;
    case Qt::Key_Backspace:      return VK_BACK;
    case Qt::Key_Comma:          return VK_OEM_COMMA;
    case Qt::Key_Semicolon:      return VK_OEM_1;
    case Qt::Key_Slash:          return VK_OEM_2;
    case Qt::Key_QuoteLeft:      return VK_OEM_3;
    case Qt::Key_ParenLeft:      return VK_OEM_4;
    case Qt::Key_Backslash:      return VK_OEM_5;
    case Qt::Key_ParenRight:     return VK_OEM_6;
    case Qt::Key_Apostrophe:     return VK_OEM_7;
    case Qt::Key_Tab:            return VK_TAB;
    case Qt::Key_Clear:          return VK_CLEAR;
    case Qt::Key_Return:         return VK_RETURN;
    case Qt::Key_Pause:          return VK_PAUSE;
    case Qt::Key_Escape:         return VK_ESCAPE;
    case Qt::Key_Mode_switch:    return VK_MODECHANGE;
    case Qt::Key_Space:          return VK_SPACE;
    case Qt::Key_PageUp:         return VK_PRIOR;
    case Qt::Key_PageDown:       return VK_NEXT;
    case Qt::Key_End:            return VK_END;
    case Qt::Key_Home:           return VK_HOME;
    case Qt::Key_Left:           return VK_LEFT;
    case Qt::Key_Up:             return VK_UP;
    case Qt::Key_Right:          return VK_RIGHT;
    case Qt::Key_Down:           return VK_DOWN;
    case Qt::Key_Select:         return VK_SELECT;
    case Qt::Key_Printer:        return VK_PRINT;
    case Qt::Key_Execute:        return VK_EXECUTE;
    case Qt::Key_Print:          return VK_SNAPSHOT;
    case Qt::Key_Insert:         return VK_INSERT;
    case Qt::Key_Delete:         return VK_DELETE;
    case Qt::Key_Help:           return VK_HELP;
    case Qt::Key_Menu:           return VK_APPS;
    case Qt::Key_Sleep:          return VK_SLEEP;
    case Qt::Key_Asterisk:       return VK_MULTIPLY;
    case Qt::Key_Plus:           return VK_ADD;
    case Qt::Key_Minus:          return VK_SUBTRACT;
    case Qt::Key_Period:         return VK_DECIMAL;
    case Qt::Key_Massyo:         return VK_OEM_FJ_MASSHOU;
    case Qt::Key_Touroku:        return VK_OEM_FJ_TOUROKU;
    case Qt::Key_Back:           return VK_BROWSER_BACK;
    case Qt::Key_Forward:        return VK_BROWSER_FORWARD;
    case Qt::Key_Refresh:        return VK_BROWSER_REFRESH;
    case Qt::Key_Stop:           return VK_BROWSER_STOP;
    case Qt::Key_Search:         return VK_BROWSER_SEARCH;
    case Qt::Key_Favorites:      return VK_BROWSER_FAVORITES;
    case Qt::Key_HomePage:       return VK_BROWSER_HOME;
    case Qt::Key_VolumeMute:     return VK_VOLUME_MUTE;
    case Qt::Key_VolumeDown:     return VK_VOLUME_DOWN;
    case Qt::Key_VolumeUp:       return VK_VOLUME_UP;
    case Qt::Key_MediaNext:      return VK_MEDIA_NEXT_TRACK;
    case Qt::Key_MediaPrevious:  return VK_MEDIA_PREV_TRACK;
    case Qt::Key_MediaStop:      return VK_MEDIA_STOP;
    case Qt::Key_MediaPlay:      return VK_MEDIA_PLAY_PAUSE;
    case Qt::Key_LaunchMail:     return VK_LAUNCH_MAIL;
    case Qt::Key_LaunchMedia:    return VK_LAUNCH_MEDIA_SELECT;
    case Qt::Key_Launch0:        return VK_LAUNCH_APP1;
    case Qt::Key_Launch1:        return VK_LAUNCH_APP2;
    case Qt::Key_Play:           return VK_PLAY;
    case Qt::Key_Zoom:           return VK_ZOOM;

    default:
        qWarning() << "Key" << SmartShortcut::toString(qtKey)
                   << QString::number((int)qtKey, 16) << "is not supported";
        break;
    }

    return 0;
}

quint32 WinShortcut::toNativeModifiers(int qtModifiers)
{
    quint32 native = 0;

    if (qtModifiers & Qt::ShiftModifier)
        native |= MOD_SHIFT;
    if (qtModifiers & Qt::ControlModifier)
        native |= MOD_CONTROL;
    if (qtModifiers & Qt::AltModifier)
        native |= MOD_ALT;
    if (qtModifiers & Qt::MetaModifier)
        native |= MOD_WIN;

    return native;
}

Qt::Key WinShortcut::toQtKey(quint32 nativeKey)
{
    if ((nativeKey >= Qt::Key_0 && nativeKey <= Qt::Key_Z))
    {
        return (Qt::Key)nativeKey;
    }

    if (nativeKey >= VK_F1 && nativeKey <= VK_F24)
    {
        return (Qt::Key)(nativeKey - VK_F1 + Qt::Key_F1);
    }

    if (nativeKey >= VK_NUMPAD0 && nativeKey <= VK_NUMPAD9)
    {
        return (Qt::Key)(nativeKey - VK_NUMPAD0 + Qt::Key_0);
    }

    switch (nativeKey)
    {
    case VK_CANCEL:              return Qt::Key_Cancel;
    case VK_BACK:                return Qt::Key_Backspace;
    case VK_OEM_COMMA:           return Qt::Key_Comma;
    case VK_OEM_1:               return Qt::Key_Semicolon;
    case VK_OEM_2:               return Qt::Key_Slash;
    case VK_OEM_3:               return Qt::Key_QuoteLeft;
    case VK_OEM_4:               return Qt::Key_ParenLeft;
    case VK_OEM_5:               return Qt::Key_Backslash;
    case VK_OEM_6:               return Qt::Key_ParenRight;
    case VK_OEM_7:               return Qt::Key_Apostrophe;
    case VK_TAB:                 return Qt::Key_Tab;
    case VK_CLEAR:               return Qt::Key_Clear;
    case VK_RETURN:              return Qt::Key_Return;
    case VK_PAUSE:               return Qt::Key_Pause;
    case VK_ESCAPE:              return Qt::Key_Escape;
    case VK_MODECHANGE:          return Qt::Key_Mode_switch;
    case VK_SPACE:               return Qt::Key_Space;
    case VK_PRIOR:               return Qt::Key_PageUp;
    case VK_NEXT:                return Qt::Key_PageDown;
    case VK_END:                 return Qt::Key_End;
    case VK_HOME:                return Qt::Key_Home;
    case VK_LEFT:                return Qt::Key_Left;
    case VK_UP:                  return Qt::Key_Up;
    case VK_RIGHT:               return Qt::Key_Right;
    case VK_DOWN:                return Qt::Key_Down;
    case VK_SELECT:              return Qt::Key_Select;
    case VK_PRINT:               return Qt::Key_Printer;
    case VK_EXECUTE:             return Qt::Key_Execute;
    case VK_SNAPSHOT:            return Qt::Key_Print;
    case VK_INSERT:              return Qt::Key_Insert;
    case VK_DELETE:              return Qt::Key_Delete;
    case VK_HELP:                return Qt::Key_Help;
    case VK_APPS:                return Qt::Key_Menu;
    case VK_SLEEP:               return Qt::Key_Sleep;
    case VK_MULTIPLY:            return Qt::Key_Asterisk;
    case VK_ADD:                 return Qt::Key_Plus;
    case VK_SUBTRACT:            return Qt::Key_Minus;
    case VK_DECIMAL:             return Qt::Key_Period;
    case VK_DIVIDE:              return Qt::Key_Slash;
    case VK_OEM_FJ_MASSHOU:      return Qt::Key_Massyo;
    case VK_OEM_FJ_TOUROKU:      return Qt::Key_Touroku;
    case VK_BROWSER_BACK:        return Qt::Key_Back;
    case VK_BROWSER_FORWARD:     return Qt::Key_Forward;
    case VK_BROWSER_REFRESH:     return Qt::Key_Refresh;
    case VK_BROWSER_STOP:        return Qt::Key_Stop;
    case VK_BROWSER_SEARCH:      return Qt::Key_Search;
    case VK_BROWSER_FAVORITES:   return Qt::Key_Favorites;
    case VK_BROWSER_HOME:        return Qt::Key_HomePage;
    case VK_VOLUME_MUTE:         return Qt::Key_VolumeMute;
    case VK_VOLUME_DOWN:         return Qt::Key_VolumeDown;
    case VK_VOLUME_UP:           return Qt::Key_VolumeUp;
    case VK_MEDIA_NEXT_TRACK:    return Qt::Key_MediaNext;
    case VK_MEDIA_PREV_TRACK:    return Qt::Key_MediaPrevious;
    case VK_MEDIA_STOP:          return Qt::Key_MediaStop;
    case VK_MEDIA_PLAY_PAUSE:    return Qt::Key_MediaPlay;
    case VK_LAUNCH_MAIL:         return Qt::Key_LaunchMail;
    case VK_LAUNCH_MEDIA_SELECT: return Qt::Key_LaunchMedia;
    case VK_LAUNCH_APP1:         return Qt::Key_Launch0;
    case VK_LAUNCH_APP2:         return Qt::Key_Launch1;
    case VK_PLAY:                return Qt::Key_Play;
    case VK_ZOOM:                return Qt::Key_Zoom;
    case VK_OEM_CLEAR:           return Qt::Key_Clear;

    default:
        qWarning() << "Native key" << nativeKey << "is not supported";
        break;
    }

    return Qt::Key_unknown;
}

Qt::KeyboardModifiers WinShortcut::toQtModifiers(quint32 nativeModifiers)
{
    Qt::KeyboardModifiers qtModifiers = 0;

    if (nativeModifiers & MOD_SHIFT)
        qtModifiers |= Qt::ShiftModifier;
    if (nativeModifiers & MOD_CONTROL)
        qtModifiers |= Qt::ControlModifier;
    if (nativeModifiers & MOD_ALT)
        qtModifiers |= Qt::AltModifier;
    if (nativeModifiers & MOD_WIN)
        qtModifiers |= Qt::MetaModifier;

    return qtModifiers;
}
