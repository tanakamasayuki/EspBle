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
| `hidKeyboard()` | 1 | 6KRO 8 bytes / NKRO 29 bytes | `enableNkro`、`sendReport`（6キー版とNKRO全状態版）、`pressUsage`、`releaseUsage`、`tapUsage`、`pressKey`、`tapKey`、`write`、`releaseAll`、`setLayout` |
| `hidMouse()` | 2 | buttons、X、Y、wheel（4 bytes） | `move`、`wheel`、`press`、`release`、`click`、`releaseAll`。`EspBleHidMouseConfig::buttons`で1〜5 buttons |
| `hidGamepad()` | 3 | 6 axis、hat、32 buttons（11 bytes） | `send`、`sendReport`、`releaseAll` |
| `hidConsumerControl()` | 4 | 16-bit usage | `press`、`release`、`click`、`sendUsage` |
| `hidSystemControl()` | 5 | 8-bit usage | `press`、`release`、`click`、`sendUsage` |
| `hidVendor()` | 6 | 1〜64 bytes | `sendInput`、`onOutputReport`、`onFeatureReport`。`EspBleHidVendorConfig::reportSize`で長さを構成 |
| `hidCustom()` | 任意 | 任意（Report Descriptorが決める） | `setReportMap`、`addInputReport`、`addOutputReport`、`addFeatureReport`、`sendInput`、`onOutputReport`、`onFeatureReport` |

すべてのprofileは`configure()`、`configured()`、`sendReport()`相当、`ready()`、`releaseAll()`を共通骨格に持ち、送信結果は`bool`と`ble.lastError*()`で返します。keyboardの文字変換はEspUsbDeviceと同じkeymapを逆引きし、19 layoutに対応します。

`ready()`は「購読済みHostが居て今notifyできるか」を返します。真になる条件は送信ゲートそのもの——Peripheral接続があり、security有効時は暗号化済みで、そのReport IDのInput Report CCCDを購読済み（Boot Protocol Mode時はBoot Keyboard Input CCCD）——で、`configure()` / `begin()`前、Host未接続、接続済みだが未購読のいずれでもfalseです。falseのまま`sendReport()`すると`InvalidState`で失敗するので、送信結果から接続状態を推測せずこちらを見ます。**Host未接続は正常状態なので`ready()`は`lastError()`を書き換えません。** `hidCustom()`はReport ID単位の`ready(uint8_t reportId)`です。

Host側`EspBleHidHost::ready(connectionId)`が接続を取るのに対しDevice側が引数を取らないのは、Deviceが購読済みの全Hostへ同報するため「どれか1つへ送れるか」が答えだからです。

`hidCustom()`は固定profileと**同じHID Serviceへ合成**され、併用できます。Report Descriptorのバイト列は`setReportMap()`でそのまま渡し、Reportは`addInputReport()` / `addOutputReport()` / `addFeatureReport()`で宣言します（1 deviceあたり最大`EspBleHidCustom::MaxReports`=4）。Report IDは一意で、固定profileも使う場合はその予約ID（1〜6）を避けます。Descriptorの内容はライブラリが検証しないため、間違いは例外ではなく「Hostが認識しない」という形で出ます。

## GATT構成

- HID Service `0x1812`、Device Information `0x180A`、Battery `0x180F`は内部共通マネージャが一度だけ登録します。
- 構成済みprofileのReport Descriptorだけを固定ID付きでReport Map `0x2A4B`へ連結します。
- profileごとにInput Report `0x2A4D`とReport Reference `{id, 1}`を作ります。keyboardはLED Output Report `{1, 2}`、VendorはOutput `{6, 2}`とFeature `{6, 3}`も持ちます。
- keyboardは既定で8-byte 6KROです。`configure()`より前に`enableNkro()`を呼ぶと、EspUsbDeviceと同じmodifier 1 byte + usage `0x00`〜`0xDF`の28-byte bitmapへ切り替わります。29-byte通知のため`preferredMtu`は32以上（29＋ATTヘッダ3）が必要で、**満たさない場合は`begin()`が`InvalidArgument`で拒否します**（無言でMTUを引き上げず、通知が毎回失敗する状態も作りません）。exampleとPeerテストは64を設定しています。
- NKROの全状態は`EspBleHidKeyboardNkroReport`を取る`sendReport()`のoverloadで**1 notificationとして**送ります。6キー版overloadは`keys[6]`を内部でbitmapへ展開するだけなので、NKRO有効でも1送信で表現できるのは6キーのままです。7キー以上を1 reportで出す、あるいは状態全体を毎周期書く用途はこちらを使います（`pressUsage()` / `releaseUsage()`の増分APIだと、1キーの変化ごとに1 notifyとなりconnection intervalに律速されます）。`enableNkro()`していない場合は`InvalidState`で失敗します。このoverloadは内部のNKRO状態を置き換えるため、`pressUsage()`との併用時も状態が一貫します。
- `EspBleHidKeyboardNkroReport`は`modifiers`とusage `0x00`〜`MaxBitmapUsage`（`0xDF`）の28-byte bitmapを持ち、`clear()` / `press()` / `release()` / `isDown()`で操作します。modifier usage `0xE0`〜`0xE7`はbitmapの範囲外なので`press()` / `release()`が`modifiers`側へ振り分け、呼び出し側にusageの区別を意識させません。`press()` / `release()`は`MaxBitmapUsage`超のmodifier以外のusage（このreportで表現できないもの）に対してだけfalseを返します。
- アクセサ名`isDown()`はHost側`EspBleHidKeyboardState`と揃えてあります。**bitmapのサイズは非対称**で、Hostがusage `0x00`〜`0xFF`の32 byte、Deviceがusage `0x00`〜`0xDF`の28 byteです。Report Descriptorが宣言する範囲がそれぞれ異なるためで、「Hostで受けてDeviceで出す」経路では`0xE0`以上のusageの扱いが変わる点に注意します（modifierは`modifiers`へ入り、それ以外の`0xE0`超は表現できません）。
- keyboardはBoot Protocol（Protocol Mode `0x2A4E`、Boot Keyboard Input/Output Report `0x2A22`/`0x2A32`）にも対応しますが、`EspBleHidKeyboardConfig::bootProtocol`によるopt-inで既定offです。多くのHOGP HostはReport Protocol Modeで足り、characteristicが増えると全HostのDiscoveryが膨らむためです。有効時はBoot Protocol Modeで8-byte Boot Keyboard Input Reportへ自動的に切り替わり、モードは`protocolMode()` / `onProtocolMode()`で観測できます。対象はkeyboardのみで、mouseのboot report（`0x2A33`）は非対応です。
- `hidCustom()`のReportもReport Reference `{id, type}`（1=Input / 2=Output / 3=Feature）付きの`0x2A4D`として同じServiceへ並びます。すべて同一UUIDなので、Client側から特定のReport Referenceを読むには属性ハンドル指定（`readDescriptor(connectionId, descriptorHandle)`）が必要です。
- Report payloadのNotificationにはReport IDを含めません。
- CCCD購読状態は接続・Report IDごとに追跡し、購読済みpeerだけへ通知します。
- security有効時はHID attributeへHOGP Security Mode 1 Level 2（暗号化必須）を適用し、未暗号化linkへHID入力を送りません。
- 切断・再初期化時はhandle、購読状態、保持reportを破棄します。

Output / Feature Report callbackはstack taskではなく`ble.update()`から配送します。Battery Levelはprofile共通のDevice情報として、最初に構成したprofileのconfigを採用します。

## 検証

`tests/peer/hid_keyboard_device`はArduino-ESP32 BLE APIを直接使うCentralから6KROを検証します。`tests/peer/hid_keyboard_nkro`はEspBle Host / Device間で8キー同時押し、高usage、個別release、LED Outputに加え、`EspBleHidKeyboardNkroReport`による全状態の1 report送信（最初のstate eventが全キーを持つこと、modifierが`modifiers`へ振り分けられること）を実機検証します。`tests/peer/hid_keyboard_host`では全6 profileを同時構成し、Vendor Input / Output / Featureを含む相互運用を検証します。`tests/peer/hid_convenience`は便利入力API（`pressKey` / `tapKey` / `write` / `tapUsage` / `setLayout` / `wheel` / `click` / `sendUsage` / gamepad `send`）が実際に電波へ出すReportを検証します。`tests/peer/hid_custom`は任意Report DescriptorのCustom HIDを、Report Reference（Input / Output / Feature）を属性ハンドル指定で読んで検証します。`tests/peer/hid_boot_protocol`と`tests/peer/hid_boot_keyboard`はBoot Protocol、`tests/peer/hid_security`は未暗号化linkの拒否、`tests/peer/hid_robustness`は購読gateとqueue満杯、および`ready()`が購読gateどおりに遷移すること（未接続false → 接続済み未購読false → 購読後true → 切断後false）と`lastError()`を書き換えないことを検証します。
