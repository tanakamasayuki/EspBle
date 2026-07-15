# HID Keyboard Host仕様

この文書は、最初のHID Keyboard Host vertical sliceで実装・Peer検証した仕様を記録します。公開APIは初期リリースまで変更される可能性があります。

## 接続フロー

HID Keyboard HostはBLE Central / GATT Clientとして動作します。

1. `ble.scanner()`でHID Service `0x1812`をadvertiseする接続先を選ぶ。
2. `ble.connect(scanResult)`で接続する。
3. 必要なPairing/Bonding完了後に`ble.hidKeyboardHost().discover(connectionId)`を呼ぶ。
4. `onDiscovered()`で利用可能になったことを受け取る。
5. `onKeyboardState()`でHID usage snapshotを受け取る。

Scan/ConnectとHID Discoveryを分け、device nameだけで接続先を決めません。Service UUIDは`"1812"`とBluetooth base UUIDへ展開した表記のどちらでも比較できます。

## Discovery

初期実装は次を行います。

- HID Service `0x1812`のDiscovery
- HID Information `0x2A4A`のcountry code取得
- Report Map `0x2A4B`のReadと対応形式判定
- 同一UUIDを含む全Report `0x2A4D` characteristicの列挙
- Report Reference `0x2908`によるInput/OutputとReport IDの識別
- Input Report Notificationの購読
- 対応するOutput Reportの保持
- Battery Service `0x180F` / Battery Level `0x2A19`の任意Read

Battery ServiceとOutput ReportがなくてもInputを利用できます。Discovery結果にはそれぞれの有無を含めます。

## 初期対応Report Map

最初のparserは次の8-byte 6KRO keyboard Input Reportだけを受理します。

| Offset | Size | 内容 |
|---:|---:|---|
| 0 | 1 | modifier bitmap |
| 1 | 1 | reserved |
| 2 | 6 | Keyboard/Keypad usage array |

Report MapがKeyboard Application、modifier usages `0xE0..0xE7`、8-bit × 6個のarrayを宣言することを確認します。Report ReferenceにReport IDがある場合も、Report characteristicのpayload自体にはReport IDが含まれないものとして扱います。

NKRO bitmap、複合HID、複数Keyboard Input Report、Boot Protocol切替、Consumer Controlは未対応です。未知の形式はDiscovery失敗として明示し、8-byteという長さだけで推測しません。

## Keyboard layoutと一般向けevent

HID Keyboard/Keypad usageは基本的に物理的なkey位置・機能を表し、入力文字そのものではありません。BLE HOGPもUSB HIDのReport descriptorとUsage Tablesを使用するため、通常の文字変換はHost OSまたはEspBle利用者が選んだkeyboard layoutで行います。

HID Informationのcountry codeはDeviceから得られるhintですが、接続時にHostとlayoutを交渉する仕組みではなく、OSが自動的に同じlayoutへ切り替える保証もありません。EspBleはcountry codeを取得できるようにしても、それだけでlayoutを決定しません。

`onKeyboardState()`はlayout非依存のraw usage境界です。その上に一般的なsketch向けの`onKeyboard()` convenience layerを提供します。

- usage単位のpress/release event
- Connection ID、Report ID、usage、modifier、lock状態
- 選択したlayoutに基づくASCII変換（変換不能は0）
- modifier単独変化をusage `0xE0..0xE7`として通知
- 同一report内ではpressをusage昇順、その後releaseをusage昇順で配送

```cpp
keyboard.setKeyboardLayout(EspBleKeyboardLayout::JaJp);
keyboard.onKeyboard([](const EspBleHidKeyboardEvent &event) {
  if (event.pressed && event.ascii != 0) {
    Serial.write(event.ascii);
  }
});
```

layout識別子と変換tableはEspUsbHostと揃え、`ZhTw`、`DaDk`、`DeDe`、`EnUs`、`FiFi`、`FrFr`、`HuHu`、`ItIt`、`JaJp`、`KoKr`、`NlNl`、`NbNo`、`PtBr`、`SvSe`、`ZhCn`、`EnGb`、`PtPt`、`EsEs`、`FrCh`を実装します。KoKr/ZhCn/ZhTwはEspUsbHostと同様にASCII範囲をEN-US tableで扱います。

`ascii`はEspUsbHost互換の1 byte変換結果です。ASCII外の文字を含むlayoutではtableの8-bit値を返しますが、UTF-8文字列への変換やIME処理は行いません。変換不能なusageは0を返し、正確な入力を必要とする用途では常にraw `usage`も参照します。

layoutを指定しない利用者やESP32KeyBridgeはraw usageをそのまま使用できます。現在のlayout設定はHost profile全体に適用します。異なるlayoutの複数keyboardを同時接続する場合のConnection単位設定は複数接続APIと合わせて追加します。

## Keyboard state

受信reportは`EspBleHidKeyboardState`へ正規化します。

- Connection ID
- Report ID
- modifier bitmap
- Num Lock、Caps Lock、Scroll Lock、Compose、Kana状態
- Keyboard/Keypad usage page全体の256-bit現在値
- 直前reportから変化した256-bit bitmap

modifierは`modifiers`に加えてusage `0xE0..0xE7`として現在値bitmapにも設定します。`isDown()`、`wasPressed()`、`wasReleased()`でusageを照会できます。同じ状態の重複Notificationは配送しません。

切断時に保持中のusageがある場合は、すべてreleaseされたstateを`ble.update()` contextで配送します。これはbridge用途でstuck keyを防ぐための必須動作です。

## LED Output

`setKeyboardLeds(connectionId, numLock, capsLock, scrollLock, compose, kana)`は対応するOutput Reportへ1 byteを書き込みます。bit配置はDevice側と同じくNum Lock、Caps Lock、Scroll Lock、Compose、Kanaです。

初期実装のwriteは完了を`bool`で返します。汎用GATT APIと同じ非同期Resultへ揃えるか、短いprofile helperとして維持するかはKeyBridge adapter実装時に再検討します。

## Event contextと同時操作

DiscoveryとNotification値は内部でcopyし、`onDiscovered()`と`onKeyboardState()`を`ble.update()`から呼びます。Discovery中は汎用Central GATT操作と排他し、初期実装では同時に1つのDiscoveryだけを実行します。

## Peer検証

`tests/peer/hid_keyboard_host`では親側をEspBle HID Keyboard Host、`peer_device/`側をEspBle HID Keyboard Deviceに固定し、次をradio経由で確認します。Device側のwire形式は別の`hid_keyboard_device`テストでArduino-ESP32同梱BLE API直接実装のHostから独立に検証しています。

- HID Serviceを使ったScanと接続
- Just Works Pairing、暗号化、Bond保存
- Report Mapと2個の同一UUID Report characteristicのDiscovery
- Input/Output Report ReferenceとReport ID 1
- Battery Level 73%のRead
- Shift+A pressとreleaseのusage bitmap/changed bitmap
- EN-US/JA-JP/en-GBのShift+2、de-DEのY/Z、fr-FRのAZERTY変換とmodifier単独event
- LED `0x03`のOutput Write
- callbackがloop contextであること
- 切断と両側Bond削除
