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

全Report `0x2A4D`をReport Reference `0x2908`と照合し、対応Inputをすべて購読します。Report IDを持たない単一report keyboard/mouseも受理します。Batteryとkeyboard LED Outputは任意です。

| callback | eventの主な内容 |
|---|---|
| `onKeyboardState` | 256-bit usage snapshot、changed bitmap、modifier、lock state |
| `onKeyboard` | usage単位のpress/release、Unicode/Latin-1、19 layout |
| `onMouse` | X/Y/wheel、buttons、previousButtons、moved/buttonsChanged |
| `onConsumerControl` | 16-bit usage、pressed/released |
| `onSystemControl` | 8-bit usage、pressed/released |
| `onGamepad` | raw reportと`EspBleHidFieldValue`配列 |

全イベントは`EspBleHidReport`を継承し、`connectionId`、`reportId`、callback中だけ有効なraw bytesを持ちます。Notificationは内部queueへcopyし、ユーザーcallbackは`ble.update()` contextで実行します。

各種別には単一`on*()`と最大4件の`add*Listener()`があり、IDはHost instance内で種別横断に一意です。callbackは`shared_ptr` ownershipを配送開始時にsnapshotし、registry mutexを解放してから単一callback→listener登録順に実行します。callback内の追加・解除は次イベントから反映されます。

keyboard固有APIとして`setKeyboardLeds()`、`setKeyboardLayout()`、`keyboardLayout()`を提供します。`ready(connectionId)`はDiscovery済み接続を示します。切断時に保持keyがあれば全release snapshotを合成し、stuck keyを防ぎます。

## 検証

`tests/peer/hid_keyboard_host`は全5 profileの複合EspBle Deviceに接続し、横断Discovery、全Input購読、keyboard layout、mouse、consumer、system、gamepad field、LED Output、Battery、切断releaseを実機検証します。boot keyboard、rollover、queue overflow、KeyBridge listener共存は各専用Peer suiteで回帰します。
