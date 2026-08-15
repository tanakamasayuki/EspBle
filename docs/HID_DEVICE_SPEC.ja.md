# HID Device仕様

EspBleはkeyboard、mouse、gamepad、consumer control、system controlを1つのHID Serviceへ複合できるHOGP Deviceを提供します。`begin()`前に必要なprofileだけを構成します。

この文書はBLE（HOGP）側の仕様です。無印ESP32のBluetooth Classic HIDは`EspBleClassic`の`hidDevice()` / `hidKeyboard()`などで同名・同signatureのAPIを公開し、Report Descriptorとreport構造を`src/EspBleHidProfile.h`で共有します。無線ごとの差（report IDの位置、SDP recordによる合成上限）は[Classic通信の入門](GUIDE_CLASSIC_BASICS.ja.md)と[HID Report Descriptorを書く](GUIDE_HID_DESCRIPTORS.ja.md)にあります。

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
- `EspBleHidKeyboardNkroReport`は`modifiers`とusage `0x00`〜`MaxBitmapUsage`（`0xDF`）の28-byte `bitmap`を持ち、`clear()` / `press()` / `release()` / `isDown()`で操作します。modifier usage `0xE0`〜`0xE7`はbitmapの範囲外なので`press()` / `release()`が`modifiers`側へ振り分け、呼び出し側にusageの区別を意識させません。`press()` / `release()`は`MaxBitmapUsage`超のmodifier以外のusage（このreportで表現できないもの）に対してだけfalseを返します。
- アクセサ名`isDown()`はHost側`EspBleHidKeyboardState`と揃えてあります。**bitmapを持つメンバは`bitmap`、usageの配列を持つメンバは`keys`**という規則で、Host側`EspBleHidKeyboardState`の`bitmap` / `changedBitmap`、Device NKROの`bitmap`、6KRO `EspBleHidKeyboardInputReport`の`keys[6]`が対応します。`keys[0] = 0x04`が型によって「usage 0x04が押されている」と「usage 3と5が押されている」の別の意味になり、取り違えてもコンパイルが通ってしまうためです。姉妹ライブラリ`EspUsbHost` / `EspUsbDevice`も同じ規則です。**bitmapのサイズは非対称**で、Hostがusage `0x00`〜`0xFF`の32 byte、Deviceがusage `0x00`〜`0xDF`の28 byteです。Report Descriptorが宣言する範囲がそれぞれ異なるためで、「Hostで受けてDeviceで出す」経路では`0xE0`以上のusageの扱いが変わる点に注意します（modifierは`modifiers`へ入り、それ以外の`0xE0`超は表現できません）。
- **`enableNkro()`していないNKRO送信は失敗させ、Boot Protocol Modeでは畳んで送ります。** 同じ「NKROの形では送れない状況」に対して挙動が2通りあるのは意図的で、基準は責任の所在です。Boot Protocol Modeの選択は**Host主導の実行時条件**でスケッチに責任がないため、送れる形（8-byte Boot Keyboard Input Report）へ畳んで送ります。`enableNkro()`忘れは**構成の誤り**で、畳んで成功させると7キー目以降が恒久的に無言で消えるため、`InvalidState`で即座に気付ける失敗にします。呼び出し側は`nkroEnabled()`で事前に判定できます。
- **同じ状態の再送をライブラリ側で抑制しません。** 状態ベースの呼び出し側は毎周期`sendReport()`を呼ぶため抑制したくなりますが、`releaseAll()`やProtocol Mode切替を挟んだあとの再同期が読めなくなります。抑制は呼び出し側の責務とし、比較対象として`heldState()`を提供します。
- `heldState()`はHostへ最後に伝えたNKRO状態を返します。重複送信の抑制と、状態を見失ったときの`releaseAll()`によらない再同期に使えます。NKRO専用で、無効時はクリアされたままです（6KROの保持状態は8-byteのwire値として別に持つため）。Boot Protocol Mode中は**要求した状態**であって電波に出たバイト列ではありません（上記のとおり8 byteへ畳まれるため）。
- keyboardはBoot Protocol（Protocol Mode `0x2A4E`、Boot Keyboard Input/Output Report `0x2A22`/`0x2A32`）にも対応しますが、`EspBleHidKeyboardConfig::bootProtocol`によるopt-inで既定offです。多くのHOGP HostはReport Protocol Modeで足り、characteristicが増えると全HostのDiscoveryが膨らむためです。有効時はBoot Protocol Modeで8-byte Boot Keyboard Input Reportへ自動的に切り替わり、モードは`protocolMode()` / `onProtocolMode()`で観測できます。対象はkeyboardのみで、mouseのboot report（`0x2A33`）は非対応です。
- `hidCustom()`のReportもReport Reference `{id, type}`（1=Input / 2=Output / 3=Feature）付きの`0x2A4D`として同じServiceへ並びます。すべて同一UUIDなので、Client側から特定のReport Referenceを読むには属性ハンドル指定（`readDescriptor(connectionId, descriptorHandle)`）が必要です。
- Report payloadのNotificationにはReport IDを含めません。
- CCCD購読状態は接続・Report IDごとに追跡し、購読済みpeerだけへ通知します。
- security有効時はHID attributeへHOGP Security Mode 1 Level 2（暗号化必須）を適用し、未暗号化linkへHID入力を送りません。
- 切断・再初期化時はhandle、購読状態、保持reportを破棄します。

- **LED状態は`ledState()`でも取れます。** `onOutputReport()`が「変化したとき」の通知なのに対し、`ledState()`は「今どうなっているか」の問い合わせで、Lock状態を同期的に返す必要がある利用側（ESP32KeyBridgeの`OutputAdapter::getLockState()`など）が自前の写しを持たずに済みます。返すのは`onOutputReport()`と同じ`EspBleHidKeyboardOutputReport`で、Boot Protocol Modeの`0x2A32`書き込みも同じ値へ入ります。Lock状態は`numLock` / `capsLock` / `scrollLock` / `compose` / `kana`の**boolメンバ**で、raw byteの`leds`と併せて`setLeds()`が同時に決めるため食い違いません（Host側`EspBleHidKeyboardState`および姉妹ライブラリと同じ形）。
- `ledState()`は**Hostが書いた時点**で更新し、コールバック配送時ではありません。Output queue（容量8）が溢れてコールバックが落ちても最新状態を取り逃さないためで、代償として`onOutputReport()`より最大1回の`update()`ぶん先行しうる点はSPEC上の約束とします。LED Output Report characteristicへのGATT Readが書き込み直後の値を返すのと揃います。
- 複数Hostが接続している場合、`ledState()`は**最後に書いたHostの値**を`connectionId`付きで返します。GATT Readが単一の値をどのHostへも返しているのと同じ意味論です。Hostごとに分けたくなった時点で`ledState(connectionId)`を足す余地は残します。
- 最後のHostが切断したとき、`ledState()`と`heldState()`はクリアします。前のHostのLED状態や押下状態を次の接続へ持ち越さないためで、特に`heldState()`は再接続後の重複抑制の比較対象になるため、残すと「送ったつもりで送っていない」stuck keyを生みます。

Output / Feature Report callbackはstack taskではなく`ble.update()`から配送します。Battery Levelはprofile共通のDevice情報として、最初に構成したprofileのconfigを採用します。

## 検証

`tests/peer/hid_keyboard_device`はArduino-ESP32 BLE APIを直接使うCentralから6KROを検証します。`tests/peer/hid_keyboard_nkro`はEspBle Host / Device間で8キー同時押し、高usage、個別release、LED Outputに加え、`EspBleHidKeyboardNkroReport`による全状態の1 report送信（最初のstate eventが全キーを持つこと、modifierが`modifiers`へ振り分けられること）を実機検証します。`tests/peer/hid_keyboard_host`では全6 profileを同時構成し、Vendor Input / Output / Featureを含む相互運用を検証します。`tests/peer/hid_convenience`は便利入力API（`pressKey` / `tapKey` / `write` / `tapUsage` / `setLayout` / `wheel` / `click` / `sendUsage` / gamepad `send`）が実際に電波へ出すReportを検証します。`tests/peer/hid_custom`は任意Report DescriptorのCustom HIDを、Report Reference（Input / Output / Feature）を属性ハンドル指定で読んで検証します。`tests/peer/hid_boot_protocol`と`tests/peer/hid_boot_keyboard`はBoot Protocol、`tests/peer/hid_security`は未暗号化linkの拒否、`tests/peer/hid_robustness`は購読gateとqueue満杯、および`ready()`が購読gateどおりに遷移すること（未接続false → 接続済み未購読false → 購読後true → 切断後false）と`lastError()`を書き換えないことを検証します。
