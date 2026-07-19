# HID Device仕様

EspBleはkeyboard、mouse、gamepad、consumer control、system controlを1つのHID Serviceへ複合できるHOGP Deviceを提供します。`begin()`前に必要なprofileだけを構成します。

```cpp
ble.hidKeyboard().configure();
ble.hidMouse().configure();
ble.begin(config);
```

## APIとReport ID

| 入口 | 固定ID | payload | 主なAPI |
|---|---:|---|---|
| `hidKeyboard()` | 1 | modifier + reserved + 6 usage（8 bytes） | `sendReport`、`pressUsage`、`tapUsage`、`pressKey`、`tapKey`、`write`、`releaseAll`、`setLayout` |
| `hidMouse()` | 2 | buttons、X、Y、wheel（4 bytes） | `move`、`wheel`、`press`、`release`、`click`、`releaseAll`。`EspBleHidMouseConfig::buttons`で1〜5 buttons |
| `hidGamepad()` | 3 | 6 axis、hat、32 buttons（11 bytes） | `send`、`sendReport`、`releaseAll` |
| `hidConsumerControl()` | 4 | 16-bit usage | `press`、`release`、`click`、`sendUsage` |
| `hidSystemControl()` | 5 | 8-bit usage | `press`、`release`、`click`、`sendUsage` |
| `hidVendor()` | 6 | 1〜64 bytes | `sendInput`、`onOutputReport`、`onFeatureReport`。`EspBleHidVendorConfig::reportSize`で長さを構成 |

すべてのprofileは`configure()`、`configured()`、`sendReport()`相当、`releaseAll()`を共通骨格に持ち、送信結果は`bool`と`ble.lastError*()`で返します。keyboardの文字変換はEspUsbDeviceと同じkeymapを逆引きし、19 layoutに対応します。

## GATT構成

- HID Service `0x1812`、Device Information `0x180A`、Battery `0x180F`は内部共通マネージャが一度だけ登録します。
- 構成済みprofileのReport Descriptorだけを固定ID付きでReport Map `0x2A4B`へ連結します。
- profileごとにInput Report `0x2A4D`とReport Reference `{id, 1}`を作ります。keyboardはLED Output Report `{1, 2}`、VendorはOutput `{6, 2}`とFeature `{6, 3}`も持ちます。
- Report payloadのNotificationにはReport IDを含めません。
- CCCD購読状態は接続・Report IDごとに追跡し、購読済みpeerだけへ通知します。
- security有効時はHID attributeへHOGP Security Mode 1 Level 2（暗号化必須）を適用し、未暗号化linkへHID入力を送りません。
- 切断・再初期化時はhandle、購読状態、保持reportを破棄します。

Output / Feature Report callbackはstack taskではなく`ble.update()`から配送します。Battery Levelはprofile共通のDevice情報として、最初に構成したprofileのconfigを採用します。

## 検証

`tests/peer/hid_keyboard_device`はArduino-ESP32 BLE APIを直接使うCentralから、keyboard+mouse複合Report Map、複数Report Reference、個別CCCD、8-byte keyboard通知、4-byte mouse通知、LED Output、Battery、暗号化を実機検証します。`tests/peer/hid_keyboard_host`では全6 profileを同時構成し、Vendor Input / Output / Featureを含むEspBle Hostとの相互運用を検証します。
