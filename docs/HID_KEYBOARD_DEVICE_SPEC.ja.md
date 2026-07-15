# HID Keyboard Device仕様

この文書は、最初のHID Keyboard Device vertical sliceで実装・Peer検証した仕様を記録します。公開APIは初期リリースまで変更される可能性があります。

## 対象

- BLE Peripheral / GATT ServerとしてHID Keyboard Deviceを提供する。
- HID over GATTのReport Protocolを使用する。
- modifier 8 bit + reserved 1 byte + key usage 6個の6KRO Input Reportを提供する。
- Keyboard LEDのOutput Reportを受信する。
- Device Information ServiceとBattery Serviceを同じServerへ登録する。
- Arduino-ESP32に同梱されたNimBLE backendだけを使用する。

Boot Protocol、NKRO、Consumer Control、Mouse、複合HID、HID Host、keyboard layoutと文字列変換はこのsliceの対象外です。

## GATT構成

| Service | Characteristic | Property | 内容 |
|---|---|---|---|
| HID `0x1812` | HID Information `0x2A4A` | Read | HID 1.11、normally connectable |
| HID `0x1812` | Report Map `0x2A4B` | Read | 固定6KRO keyboard descriptor |
| HID `0x1812` | HID Control Point `0x2A4C` | Write Without Response | suspend/control writeを受理 |
| HID `0x1812` | Report `0x2A4D` | Read, Notify | Input Report |
| HID `0x1812` | Report `0x2A4D` | Read, Write, Write Without Response | LED Output Report |
| Device Information `0x180A` | Manufacturer Name `0x2A29` | Read | configのmanufacturer |
| Device Information `0x180A` | PnP ID `0x2A50` | Read | vendor/product/version |
| Battery `0x180F` | Battery Level `0x2A19` | Read, Notify | 0〜100% |

2つのReport characteristicは同じUUIDを持ち、Report Reference descriptor `0x2908`で区別します。Inputは`{reportId, 1}`、Outputは`{reportId, 2}`です。AdvertisingにはHID Service UUIDとKeyboard appearance `0x03C1`を自動追加します。

## Report wire format

Report Mapにはconfigurableな非0のReport IDを含めます。GATT Report characteristicのvalue/notificationにはReport IDを含めません。

Input Reportは8 bytesです。

| Offset | Size | 内容 |
|---:|---:|---|
| 0 | 1 | modifier bitmap |
| 1 | 1 | reserved、常に0 |
| 2 | 6 | HID Keyboard/Keypad usage、未使用slotは0 |

Output Reportは1 byteです。bit 0から順にNum Lock、Caps Lock、Scroll Lock、Compose、Kanaを表し、上位3 bitは未使用です。

`sendInputReport()`は接続中のPeripheral-role connection全体へ現在値を通知します。接続がない場合は失敗します。現時点では送信queueやConnection指定送信を提供しません。利用者はkey press後に必要なタイミングで`releaseAll()`を呼びます。

## Security

Profile構成だけではPairingを強制しません。利用者が`EspBleConfig::security`でJust Works、Bonding、passkeyなどを選択します。Keyboard exampleはSecurityとBondingを有効にします。Pairing方針とCharacteristic permissionをProfile側でどこまで既定化するかは相互運用試験後に再検討します。

## Callback context

Output Reportは受信時に値とConnection IDを内部queueへcopyし、登録した`onOutputReport()` callbackを`ble.update()`から呼びます。stack callback内でユーザーコードは実行しません。queue容量は現在8件で、overflow公開方法は未確定です。

## Backend実装上の制約

Arduino-ESP32 3.3.10同梱の`BLEService` wrapperはCharacteristicをUUIDで管理するため、同一Serviceへ同じ`0x2A4D` UUIDを2つ追加できません。同梱`BLEHIDDevice`をそのまま使うとOutput ReportがGATT tableへ登録されないことをPeer実機で確認しました。

EspBle内部では同梱NimBLEのGATT定義APIを使い、Input/Output Reportを別attributeとして登録します。これは外部NimBLE-Arduinoへの依存追加ではなく、公開APIへnative handleを露出しないbackend workaroundです。

## 検証

`tests/peer/hid_keyboard_device`では親側ESP32-S3のArduino-ESP32同梱BLE API直接実装をHID Host、`peer_device/`側のEspBleをHID Keyboard Deviceとして使用します。profile名にはBLE roleの意味を持たせず、既存Peer配置どおり親側をCentral、2台目をPeripheralに固定します。次をradio経由で確認します。

- Advertising、接続、LE Secure Connections Pairing、Bond保存
- Report MapとInput/Output Report ReferenceのDiscovery
- Battery Level 87%のRead
- Input CCCD購読、Shift+Aの8-byte Notification、全key release
- LED値`0x03`のOutput Writeとloop context callback
- 切断と両側Bond削除
