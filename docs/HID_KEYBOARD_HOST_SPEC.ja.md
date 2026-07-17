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

Report Mapの判定は最小のHID report descriptor parser（`EspBleHidReportMap.h`）で行います。Keyboard Application collection内に「modifier usages `0xE0..0xE7`の8×1-bit variable入力」と「Keyboard usage pageの8-bit × 6個以上のarray入力」を宣言するInput Reportがあれば、項目の宣言順序やpadding、他のcollection（Consumer Control等の併載）に依存せず受理します。特定したReport IDに一致するInput/Output Report characteristicを購読・保持の対象とし、Report IDを宣言しないkeyboard（Report Reference descriptorなし）も受理します。Report ReferenceにReport IDがある場合も、Report characteristicのpayload自体にはReport IDが含まれないものとして扱います。

NKRO bitmap、複合HIDの同時利用、複数Keyboard Input Report、Boot Protocol切替、Consumer Controlの解釈は未対応です。keyboard Input Reportを特定できないReport MapはDiscovery失敗として明示し、8-byteという長さだけで推測しません。

受信したInput Reportのkey slotにusage `0x01`〜`0x03`（ErrorRollOver等のphantom state）が含まれる場合は、一般的なHost実装と同様にreport全体を無視し、直前のkey状態を維持します。7キー以上の同時押しでも全releaseと誤解釈しません。

内部eventキュー（容量8）が満杯のとき、Discovery結果と切断時の全releaseイベントは最古のkey stateイベントを追い出して保持します。落ちたkey stateイベントの数は`droppedEventCount()`で確認できます。

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

`unicode`は選択したlayoutが生成する文字のUnicodeコードポイント（生成なしは0）、`ascii`はそのISO-8859-1部分集合（256未満のときの下位byte、それ以外は0）です。UTF-8文字列への変換やIME処理は行いません。変換不能なusageは両方0を返し、正確な入力を必要とする用途では常にraw `usage`も参照します。

EspUsbHost互換の変換仕様として、次の挙動を採用します（EspUsbHostのUnicode 4-plane再設計に追随）。

- 変換tableはusageごとに無shift、Shift、AltGr、AltGr+Shiftの4列のUnicodeコードポイントを持ちます。AltGr（Right Alt）押下時はAltGr列を選択し、layoutがその位置に文字を持たない場合は無shift/Shift列へfallbackします（例: de-DEのAltGr+Q=`@`、AltGr+E=`€`(U+20AC、`ascii`は0)）。
- CapsLockは「Shift列が無shift列のUnicode大文字と一致する文字key」だけにShift反転として適用します。アクセント文字（ü→Ü、å→Å）やAZERTYの`m`にも正しく効き、数字・記号keyやde-DEの`ß`（Shiftは`?`）には効きません。
- dead key（アクセント合成キー）は0を返します。
- `ascii`のエンコーディングはISO-8859-1（Latin-1）です。UTF-8端末へ直接`Serial.write()`すると文字化けするため、必要なら利用者側で変換します。

これらはEspUsbHostの変換結果とコードポイント単位で一致させるための互換仕様です。tableはEspUsbHost / EspUsbDevice / EspBleの3プロジェクトで同一内容を共有します。

layoutを指定しない利用者やESP32KeyBridgeはraw usageをそのまま使用できます。現在のlayout設定はHost profile全体に適用します。異なるlayoutの複数keyboardを同時接続する場合のConnection単位設定は複数接続APIと合わせて追加します。

## ESP32KeyBridgeとの接続

ESP32KeyBridge input adapterは`onKeyboardState()`の`keys` bitmapをKeyboard Usageの`KeySet`へ写し、`connected()`はHID Discovery成功から切断までを表します。文字変換は介さないため、KeyBridgeのremapやlayout変換は物理usageを入力として一貫して動作します。

adapterの`update()`から`EspBle::update()`を呼ぶ構成をPeer検証済みで、sketchは`bridge.update()`だけを定期実行できます。切断時にEspBleが生成する全release snapshotと`ready(connectionId)==false`の両方を使って保持keyを除去します。KeyBridgeから渡されるLockStateは、Discovery済みの各Connectionへ`setKeyboardLeds()`で転送します。

再接続ではConnection IDが変わるため、Security完了後に新しいIDで`discover()`を実行します。自動scan/connectはadapterへ隠さず、現時点ではsketchがBLE接続方針を決めます。正式adapterの配置先はESP32KeyBridgeを想定し、EspBleのcallback登録方式を確定してから移します。

adapterは`addDiscoveredListener()`と`addKeyboardStateListener()`で購読し、sketchが設定する`onDiscovered()` / `onKeyboardState()`を上書きしません。追加listenerはevent種別ごとに最大4件で、登録成功時は0以外の`EspBleListenerId`を返します。不要になったlistenerは`removeListener(id)`で解除します。0の`EspBleInvalidListenerId`、空callback、容量超過、存在しないIDは明示的に失敗します。

`on*()`の単一callbackと追加listenerは同じ`update()` contextで、単一callback、listener登録順に呼びます。配送開始時にcallbackをsnapshotするため、callback内での追加・解除は現在配送中のeventではなく次のeventから反映します。

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
- ESP32KeyBridgeでのraw usage remap、modifier、切断release、LED転送、Bond再接続後の再Discovery、sketch callbackとの共存
