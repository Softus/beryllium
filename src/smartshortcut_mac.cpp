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
#include <QApplication>
#include <QDebug>
#include <QKeyEvent>

#include <Carbon/Carbon.h>

#define HOTKEY_SIGNATURE 'GKEY'

static QMap<quint32, EventHotKeyRef> keyRefs;
static EventHandlerRef s_eventHandlerRef = nullptr;

static OSStatus macEventHandler(EventHandlerCallRef nextHandler, EventRef event, void* data)
{
    Q_UNUSED(nextHandler);
    Q_UNUSED(data);

    if (GetEventClass(event) == kEventClassKeyboard)
    {
        EventHotKeyID keyID{};
        GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr, sizeof(keyID), nullptr, &keyID);
        if (keyID.signature == HOTKEY_SIGNATURE)
        {
            Qt::KeyboardModifiers modifiers = Qt::KeyboardModifiers(keyID.id & Qt::KeyboardModifierMask);
            Qt::Key keycode = Qt::Key(keyID.id & ~Qt::KeyboardModifierMask);

            if (GetEventKind(event) == kEventHotKeyPressed)
            {
                QApplication::sendEvent(qApp, new QKeyEvent(QEvent::KeyPress, keycode, modifiers));
            }
            else if (GetEventKind(event) == kEventHotKeyReleased)
            {
                QApplication::sendEvent(qApp, new QKeyEvent(QEvent::KeyRelease, keycode, modifiers));
            }
        }
    }
    return noErr;
}

static quint32 toNativeKey(Qt::Key key)
{
    switch (key)
    {
    case Qt::Key_Return:     return kVK_Return;
    case Qt::Key_Enter:      return kVK_ANSI_KeypadEnter;
    case Qt::Key_Tab:        return kVK_Tab;
    case Qt::Key_Space:      return kVK_Space;
    case Qt::Key_Backspace:  return kVK_Delete;
    case Qt::Key_Control:    return kVK_Command;
    case Qt::Key_Shift:      return kVK_Shift;
    case Qt::Key_CapsLock:   return kVK_CapsLock;
    case Qt::Key_Option:     return kVK_Option;
    case Qt::Key_Meta:       return kVK_Control;
    case Qt::Key_F17:        return kVK_F17;
    case Qt::Key_VolumeUp:   return kVK_VolumeUp;
    case Qt::Key_VolumeDown: return kVK_VolumeDown;
    case Qt::Key_F18:        return kVK_F18;
    case Qt::Key_F19:        return kVK_F19;
    case Qt::Key_F20:        return kVK_F20;
    case Qt::Key_F5:         return kVK_F5;
    case Qt::Key_F6:         return kVK_F6;
    case Qt::Key_F7:         return kVK_F7;
    case Qt::Key_F3:         return kVK_F3;
    case Qt::Key_F8:         return kVK_F8;
    case Qt::Key_F9:         return kVK_F9;
    case Qt::Key_F11:        return kVK_F11;
    case Qt::Key_F13:        return kVK_F13;
    case Qt::Key_F16:        return kVK_F16;
    case Qt::Key_F14:        return kVK_F14;
    case Qt::Key_F10:        return kVK_F10;
    case Qt::Key_F12:        return kVK_F12;
    case Qt::Key_F15:        return kVK_F15;
    case Qt::Key_Help:       return kVK_Help;
    case Qt::Key_Home:       return kVK_Home;
    case Qt::Key_PageUp:     return kVK_PageUp;
    case Qt::Key_Delete:     return kVK_ForwardDelete;
    case Qt::Key_F4:         return kVK_F4;
    case Qt::Key_End:        return kVK_End;
    case Qt::Key_F2:         return kVK_F2;
    case Qt::Key_PageDown:   return kVK_PageDown;
    case Qt::Key_F1:         return kVK_F1;
    case Qt::Key_Left:       return kVK_LeftArrow;
    case Qt::Key_Right:      return kVK_RightArrow;
    case Qt::Key_Down:       return kVK_DownArrow;
    case Qt::Key_Up:         return kVK_UpArrow;

    default:
        break;
    }

    UTF16Char ch = (key == Qt::Key_Escape) ? 27 : key;

    CFDataRef currentLayoutData;
    TISInputSourceRef currentKeyboard = TISCopyCurrentKeyboardInputSource();
    if (currentKeyboard == nullptr)
        return 0;

    currentLayoutData = (CFDataRef)TISGetInputSourceProperty(currentKeyboard, kTISPropertyUnicodeKeyLayoutData);
    CFRelease(currentKeyboard);

    if (currentLayoutData == nullptr)
        return 0;

    UCKeyboardLayout* header = (UCKeyboardLayout*)CFDataGetBytePtr(currentLayoutData);
    UCKeyboardTypeHeader* table = header->keyboardTypeList;

    uint8_t *data = (uint8_t*)header;
    // God, would a little documentation for this shit kill you...
    for (quint32 i = 0; i < header->keyboardTypeCount; i++)
    {
        UCKeyStateRecordsIndex* stateRec = 0;
        if (table[i].keyStateRecordsIndexOffset != 0)
        {
            stateRec = reinterpret_cast<UCKeyStateRecordsIndex*>(data + table[i].keyStateRecordsIndexOffset);
            if (stateRec->keyStateRecordsIndexFormat != kUCKeyStateRecordsIndexFormat) stateRec = 0;
        }

        UCKeyToCharTableIndex* charTable = reinterpret_cast<UCKeyToCharTableIndex*>(data + table[i].keyToCharTableIndexOffset);
        if (charTable->keyToCharTableIndexFormat != kUCKeyToCharTableIndexFormat) continue;

        for (quint32 j = 0; j < charTable->keyToCharTableCount; j++)
        {
            UCKeyOutput* keyToChar = reinterpret_cast<UCKeyOutput*>(data + charTable->keyToCharTableOffsets[j]);
            for (quint32 k = 0; k < charTable->keyToCharTableSize; k++)
            {
                if (keyToChar[k] & kUCKeyOutputTestForIndexMask)
                {
                    long idx = keyToChar[k] & kUCKeyOutputGetIndexMask;
                    if (stateRec && idx < stateRec->keyStateRecordCount)
                    {
                        UCKeyStateRecord* rec = reinterpret_cast<UCKeyStateRecord*>(data + stateRec->keyStateRecordOffsets[idx]);
                        if (rec->stateZeroCharData == ch) return k;
                    }
                }
                else if (!(keyToChar[k] & kUCKeyOutputSequenceIndexMask) && keyToChar[k] < 0xFFFE)
                {
                    if (keyToChar[k] == ch) return k;
                }
            } // for k
        } // for j
    } // for i
    return 0;
}

static quint32 toNativeModifiers(Qt::KeyboardModifiers modifiers)
{
     quint32 native = 0;
     if (modifiers & Qt::ShiftModifier)
         native |= shiftKey;
     if (modifiers & Qt::ControlModifier)
         native |= cmdKey;
     if (modifiers & Qt::AltModifier)
         native |= optionKey;
     if (modifiers & Qt::MetaModifier)
         native |= controlKey;
     if (modifiers & Qt::KeypadModifier)
         native |= kEventKeyModifierNumLockMask;
     return native;
}

bool ungrabKey(int key)
{
    if (!keyRefs.contains(key))
    {
        return false;
    }

    auto ret = !UnregisterEventHotKey(keyRefs.take(key));
    if (keyRefs.isEmpty())
    {
        RemoveEventHandler(s_eventHandlerRef);
        s_eventHandlerRef = nullptr;
    }
    return ret;
}

bool grabKey(int key)
{
    if (SmartShortcut::isMouse(key))
    {
        qWarning() << "Grabbing mouse buttons is not supported in macos";
        return false;
    }

    quint32 nativeKey = toNativeKey(Qt::Key(key & ~Qt::KeyboardModifierMask));
    quint32 nativeMods = toNativeModifiers(Qt::KeyboardModifiers(key & Qt::KeyboardModifierMask));

    if (!s_eventHandlerRef)
    {
        EventTypeSpec keyEvents[] =
        {
            { kEventClassKeyboard, kEventHotKeyPressed},
            { kEventClassKeyboard, kEventHotKeyReleased},
        };
        InstallApplicationEventHandler(&macEventHandler, GetEventTypeCount(keyEvents), keyEvents, NULL, &s_eventHandlerRef);
    }

    EventHotKeyID keyID;
    keyID.signature = HOTKEY_SIGNATURE;
    keyID.id = key;

    EventHotKeyRef ref = 0;
    bool ret = !RegisterEventHotKey(nativeKey, nativeMods, keyID, GetApplicationEventTarget(), 0, &ref);
    if (ret)
    {
       keyRefs.insert(key, ref);
    }
    return ret;
}
