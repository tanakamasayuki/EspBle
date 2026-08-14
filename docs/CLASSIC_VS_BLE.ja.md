# BLEとBluetooth Classicのどちらを使うか

EspBleはBLE（`EspBle`）とBluetooth Classic（`EspBleClassic`）の両方を提供します。
HIDのように両方にある機能もあるため、この文書は「どちらを選ぶか」と
「同じ機能でも何が違うか」を扱います。Classicの概念とAPI境界は
[Classic通信の入門ガイド](GUIDE_CLASSIC_BASICS.ja.md)にあります。API別の対応状況は
[Feature Matrix](FEATURE_MATRIX.ja.md)、Classic側の細かい公開範囲は
[Classic機能の棚卸し](CLASSIC_FEATURE_INVENTORY.ja.md)を参照してください。

## 1. 結論から

**新しく作るなら既定はBLEです。** BLEはESP32全SoCで使え、消費電力が小さく、
2015年前後以降の携帯・タブレット・PCはBLE HID（HOGP）を受け付けます。

Classicを選ぶのは、次のどれかに当てはまるときだけです。

| Classicが必要な理由 | 具体例 |
|---|---|
| 相手がBLEを受け付けない | BR/EDRしか持たない旧世代ゲーム機、古いPC、産業機器 |
| Serial port profile（SPP）が要る | PC・Androidの仮想COM、既存のSerial protocolを持つ機器、計測器 |
| 音声を運ぶ | A2DP（音楽）、HFP（通話）。BLEにはこのlibraryが扱う標準audio pathが無い |
| 相手がClassicのHIDしか受けない | BR/EDR HIDのgamepad、keyboard、mouseとして認識させたい場合 |

**Classicは無印ESP32でしか使えません。** ESP32-S3/C3/C6/H2/P4はBluetooth Classicの
無線を持たないため、これらのSoCではBLEだけが選択肢です。Classicは実験扱いで、
公開範囲は未確定です。

逆に、**相手がBLEを受け付けるならClassicを選ぶ理由はほぼありません。**
Classicは無印ESP32限定で、消費電力が大きく、EspBleでは独自buildしたhostを同梱するため
FLASHとheapも余分に使います。

## 2. Classicの用途は実質3つ

実際に使われるのはSPP、音声、旧世代HIDです。この3つ以外でClassicを選ぶ場面は
ほとんどありません。EspBleが提供するClassic機能もこの範囲に沿っています。

- **SPP**: RFCOMMのbyte stream。BLEに標準のSerial profileは無く、BLEでは
  Nordic UART Service（NUS）のような独自GATT serviceを両端で決めて作ります。
  相手がPCやAndroidで「COMポートとして見えること」を期待するならSPPです。
  なお**iOSはアプリからSPPを使えません**（MFi機器を除く）。iOS相手ならBLEにします。
- **音声**: A2DP Sink / Source、AVRCP、HFP Client / Audio Gateway。
  EspBleが扱うのはencode済みpayloadとraw SCOで、codecとPCMのI/Oは別libraryの担当です。
- **旧世代HID**: gamepad、keyboard、mouse。相手がBLE HIDを受けないときの唯一の道です。
  gamepadは特にこの理由で必要になります。

## 3. HIDとSPPは同時に使えるか

**使えます。** Classicの1台でHID DeviceとSPP serverを同時に動かし、相手側でも
HID HostとSPP clientを同時に動かす構成を実機で検証しています
（Peer test `classic_hid_report`）。HIDとSPPは同じClassic transport上の別profileなので、
片方が他方を排除しません。

したがって「HIDとSerialの両方を1台で提供したい」場合、**Classicだけで完結できます。**
これはBLE HID + Classic SPPというdual-host構成を選ばなくてよい、という意味でもあります。

BLE側で同じことをするなら、HID ServiceとNUS相当の独自serviceを同じGATT serverに
並べます。これも可能で、相手がBLEを受けるならこちらが素直です。

同時利用について、EspBleには次の3つの構成があります。

| 構成 | 状態 | 注意 |
|---|---|---|
| Classicの複数profile同時（HID + SPPなど） | 検証済み | HFPのClient / Audio Gatewayは互いに排他。A2DPは1 role 1 session |
| BLEの複数service同時（HID + 独自service） | 対応 | 無印ESP32同梱hostの同時接続上限は3 |
| BLEとClassicの同時（dual-host） | 実験 | 両方`begin()`するとbrokerがHCIをroutingする。不安定なら片方を`end()`する |

## 4. 両方にある機能の差

### 4.1 HID

Report Descriptorとreport packingは両transportで同じmoduleを共有しているため、
**電波に出るreportの中身は同じです。** API名・signatureも揃えてあります。
差があるのは、その周辺です。

| | BLE（HOGP） | Classic（HID over BR/EDR） |
|---|---|---|
| 接続の始め方 | advertisingを見つけて接続。HID Service `0x1812`で絞り込める | addressを指定して接続。相手を探すのはinquiryで、advertisingのような絞り込みは無い |
| Report Descriptorの受け取り | GATTのReport Map characteristic | SDP（`ESP_HIDH_GET_DSCP_EVT`） |
| 合成できるprofile数 | 実質制限なし（Report Mapはcharacteristicとして読まれる） | **descriptor + profile文字列3つで214 byteまで**（SDP recordのpad 300 byteのうち固定属性が86 byte）。超える組み合わせは`begin()`が`ResourceExhausted`で拒否する |
| Report IDの位置 | characteristicが分かれるのでpayloadに含まれない | payloadの先頭に付く。`onInputReport()`のraw値も同じ |
| Host側で復号できる範囲 | keyboard、mouse、consumer、system、gamepad | keyboard、mouseのみ。それ以外は`onInputReport()`のrawで受ける |
| Battery level | HID Host側で取得できる | 取得しない |
| Boot Protocol | 対応（opt-in、既定off） | 未対応 |
| 同時接続 | 上限3（無印ESP32同梱host） | HID Hostは1接続 |
| 自動再接続 | `setAutoReconnect()` / persistent subscription / `setAutoRediscover()` | 相当APIは無い。再接続はsketchが`connect()`を呼ぶ |
| LED送信 | `setKeyboardLeds(connectionId, ...)` | `setKeyboardLeds(...)`。1接続なのでID引数が無い。report IDは相手のdescriptor由来 |
| 暗号化の要求 | 市販keyboardはHID属性へ暗号化を要求するのが普通 | pairingしてlink keyを持つことが前提 |

合成の上限は実機で確認しています。既定の文字列（合計57 byte）なら
keyboard + mouse + consumer（descriptor 144 byte）は登録でき、gamepadを加えた212 byteは
登録できません。gamepadはkeyboardと組めば133 byteで収まります。BLE側にこの制限は無く、
同じprofileの組み合わせをそのまま合成できます。

keyboard layout（`setLayout()` / `setKeyboardLayout()`）、NKRO、`pressKey()` / `tapKey()` /
`write()`、mouseの`wheel()` / `click()` / `press()`の加算、consumer / system / gamepadの
送信APIは、名前も挙動も同じです。

### 4.2 Security・bond

| | BLE | Classic |
|---|---|---|
| 方式 | LE Pairing（Just Works / passkey / numeric comparison） | SSP（Just Works / passkey / numeric comparison）。legacy PINは拒否する |
| 保存する鍵 | LE bond key、IRK | link key |
| 鍵の独立性 | 片方を削除しても他方は残る。BLEのbondとClassicのbondは別物 | 同左 |
| IO capabilityの効き方 | `EspBleConfig::security`で決まる | `EspBleClassicSecurityConfig`を有効にするとserviceがMITMを要求する。無効なままではJust Worksになり、IO capabilityは効かない |

### 4.3 探索

| | BLE | Classic |
|---|---|---|
| 探索 | advertisingのscan。service UUID、名前、製造者データで絞り込める | inquiry。得られるのはaddress、name、Class of Device、RSSI |
| 探索される条件 | advertisingしていること | connectableに加えdiscoverableであること。接続できる機器が必ずinquiryで見つかるとは限らない |
| 名前 | advertising / scan responseに含める | inquiry応答より遅れて取得される場合がある |

### 4.4 データ転送

| | BLE | Classic |
|---|---|---|
| 汎用の転送路 | GATT（Characteristic、notify / indicate、MTU） | SPPのRFCOMM byte stream |
| 境界 | Characteristic単位。MTUで上限が決まる | byte stream。`0x00`で終端しないbinary-safe |
| Serial互換のAPI | なし（GATT上に自分で作る） | `EspBleClassicSppStream`がArduinoの`Stream`としてsessionを包む。write 1回が1 packetになる点と送信queueが有限な点だけがSerialと違う |

### 4.5 無線・linkの設定

| | BLE | Classic |
|---|---|---|
| 送信電力 | `EspBle::setTxPower(dBm)` / `txPower()`。1つのlevelを設定する | `setTxPower(dBm)`と`setTxPower(min, max)` / `txPower()`。BR/EDRの電力制御は範囲の中からpacketごとに選ぶため範囲指定がある。どちらも-12〜+9 dBmの3 dB刻み |
| 電力の独立性 | LE側だけに効く | BR/EDR側だけに効く。dual-hostでは両方を別々に設定する |
| 接続失敗までの時間 | connect()のtimeout引数で決める | `setPageTimeout()`（14〜40959 ms、既定5120 ms）。pagingに応答しない相手を諦めるまでの時間で、次のpageから効く |
| 暗号鍵の最小長 | 指定するAPIは無い | `setMinimumEncryptionKeySize()`（7〜16 byte） |
| 相手の信号強度 | scan結果のRSSI、接続後も取得できる | inquiry結果のRSSIのみ。接続後のRSSIはbackendが差分値しか返さないため公開していない |

## 5. 制限の出どころ

Classic側の制限の多くはbackend（独自buildしたClassic-only Bluedroid）とcontrollerに
由来し、EspBleの設計判断ではありません。たとえばHID Hostの単一接続、HFPのrole排他、
A2DPの1 role 1 sessionはbackendの制約です。無印ESP32のcontrollerはBLE 4.2相当のため、
BLE側にもLE 2M / Coded PHYとExtended / Periodic Advertisingが使えない制限があります。

Classicとdual-hostの正式な対応範囲は未確定です。決定までは互換性を保証しません。
経緯と検証結果は[Classic実装計画](PLAN_ESP32_CLASSIC.ja.md)と
[引き継ぎ](HANDOFF_ESP32_CLASSIC.ja.md)にあります。
