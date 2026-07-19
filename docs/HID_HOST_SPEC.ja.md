# HID Host仕様

`ble.hidHost()`はBLE Central接続上のHID Serviceを横断Discoveryし、対応する全Input Reportを購読します。

```cpp
ble.hidHost().onMouse([](const EspBleHidMouseEvent &event) { /* ... */ });
ble.hidHost().discover(connectionId);
```

security有効時は`onSecurityChanged()`成功後に`discover()`を呼びます。再接続時は新しいConnection IDで再Discoveryします。

## Discoveryと配送

Report Map parserはApplication collectionとReport IDから次を識別します。

- Generic Desktop / Keyboard
- Generic Desktop / Mouse
- Consumer Page / Consumer Control
- Generic Desktop / System Control
- Generic Desktop / GamepadまたはJoystick
- Vendor-defined Application Collection

全Report `0x2A4D`をReport Reference `0x2908`と照合し、対応Inputをすべて購読します。Report IDを持たない単一report keyboard/mouseも受理します。Batteryとkeyboard LED Outputは任意です。

| callback | eventの主な内容 |
|---|---|
| `onKeyboardState` | 256-bit usage snapshot、changed bitmap、modifier、lock state |
| `onKeyboard` | usage単位のpress/release、Unicode/Latin-1、19 layout |
| `onMouse` | X/Y/wheel、buttons、previousButtons、moved/buttonsChanged |
| `onConsumerControl` | 16-bit usage、pressed/released |
| `onSystemControl` | 8-bit usage、pressed/released |
| `onGamepad` | raw reportと、Report Mapのvariable inputを分解した`EspBleHidFieldValue`配列（最大40 field） |
| `onVendorInput` | Report ID 6とraw Input bytes |

全イベントは`EspBleHidReport`を継承し、`connectionId`、`reportId`、callback中だけ有効なraw bytesを持ちます。Notificationは内部queueへcopyし、ユーザーcallbackは`ble.update()` contextで実行します。

Report Mapから算出したInput Report長と一致しないNotificationは配送せず、`invalidInputReportCount()`へ加算します。Gamepad fieldはusage page / usage、logical range、bit offset / size、flags、符号拡張済みvalueを持ち、callback中だけ有効です。初期parserはshort item、最大8 report、最大64解析fieldを対象とし、Host eventへは先頭40 fieldを配送します。array inputやvendor固有表現の意味解釈は行わず、raw bytesは常に参照できます。

各種別には単一`on*()`と最大4件の`add*Listener()`があり、IDはHost instance内で種別横断に一意です。callbackは`shared_ptr` ownershipを配送開始時にsnapshotし、registry mutexを解放してから単一callback→listener登録順に実行します。callback内の追加・解除は次イベントから反映されます。

keyboard固有APIとして`setKeyboardLeds()`、`setKeyboardLayout()`、`keyboardLayout()`を提供します。Vendor HIDは`sendVendorOutput()` / `sendVendorFeature()`でReport ID 6へ書き込みます。`ready(connectionId)`はDiscovery済み接続を示します。切断時に保持keyがあれば全release snapshotを合成し、stuck keyを防ぎます。

## 検証

`tests/peer/hid_keyboard_host`は全6 profileの複合EspBle Deviceに接続し、横断Discovery、全Input購読、keyboard layout、mouse、consumer、system、gamepad field、Vendor Input / Output / Feature、LED Output、Battery、切断releaseを実機検証します。boot keyboard、rollover、queue overflow、KeyBridge listener共存は各専用Peer suiteで回帰します。
