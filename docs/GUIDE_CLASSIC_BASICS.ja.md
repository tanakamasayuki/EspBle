# Bluetooth Classic通信の入門ガイド

> English: [GUIDE_CLASSIC_BASICS.md](GUIDE_CLASSIC_BASICS.md)

このガイドはBluetooth Classic（BR/EDR）の概念とAPI境界を説明します。
具体的なコードは[Classic examples](../examples/README.ja.md)にあります。
BLEについては[BLE通信の入門ガイド](GUIDE_BLE_BASICS.ja.md)、どちらを使うかの判断は
[BLEとClassicの選び方](CLASSIC_VS_BLE.ja.md)を参照してください。

**Classicは無印ESP32でしか使えません。** ESP32-S3/C3/C6/H2/P4はBR/EDRの無線を持ちません。
機能ごとの検証状態（実機検証済み / 未検証 / 未実装）は
[Classic機能の棚卸し](CLASSIC_FEATURE_INVENTORY.ja.md)が正本です。
precompiled Classic hostはESP-IDF 5.5.5 / xtensa-esp32 GCC 14.2.0のABIに固定しています。
buildに使ったBluedroid headerを同梱しているため、対応Coreは実測で3.2.0〜3.3.11です
（HFP audio（SCO）のみ3.3.8以上）。

## 1. BLEとは別の通信モデル

同じcontrollerを共有できますが、次の概念は統合しません。

```text
BLEのscan                  != Classicのinquiry
BLEのconnection            != ClassicのACL link
BLE GATTのclient接続       != SPP session
LE pairing / bond          != Classicのpairing / link key
```

BLEは`EspBle`、Classicは`EspBleClassic`という別のclassで、どちらのcallbackも
そのobjectの`update()`から配送されます。両方を`begin()`すればdual-hostになります
（build flagはありません）。

Classicは「inquiryで機器を見つけ、必要ならACL linkを確立し、その上で用途ごとの
profileを動かす」という順序で動きます。SPP、A2DP、HIDは同じClassic transportを
使いますが、データ形式も接続手順も別のprofileです。

| | Bluetooth Classic | BLE |
|---|---|---|
| 周辺探索 | inquiry | advertising / scan |
| 接続後の機能 | SPP、A2DP、HIDなどのprofile | GATTのService / Characteristic |
| Serial相当 | SPPのRFCOMM byte stream | GATT上に独自protocolを構築 |
| 保存する鍵 | Classicのlink key | LE bond key / IRK |

ClassicではBLEのCentral / Peripheralという役割名でprofile接続を説明しません。
SPPでは待受側をServer、接続を開始する側をClientと呼びます。このServer / Clientも
GATT Server / Clientとは別の概念です。

## 2. 起動と可視性

`EspBleClassic::begin()`がcontrollerとhostを起動します。profileは個別にstackを
初期化しません。

```cpp
EspBleClassicConfig config;
config.deviceName = "EspBle Classic";
bluetooth.begin(config);
```

**HIDのようにprofileを合成するものは`begin()`より前に設定します。** 合成した
Report Descriptorは、Hostがpairing時に読むdevice recordの一部になるためです。
後から追加しても、既に読み終えたrecordには入りません。

### 2.1 接続可能と発見可能

Classicには2つの状態があり、意味が違います。

| 状態 | 意味 |
|---|---|
| connectable | addressを知っている相手からの接続を受け付ける |
| discoverable | inquiryに応答して見つけられる |

`EspBleClassicConfig::visibility`と実行時の`setVisibility()`で
`Hidden` / `ConnectableOnly` / `ConnectableDiscoverable`を選びます。所有者は
`EspBleClassic`で、profileは値を決めず再適用だけを行います——以前はprofileが各自
設定していたため、最後にstartしたprofileが可視性を決めてしまっていました。

**接続できる機器が必ずinquiryで見つかるとは限りません。** 相手が`ConnectableOnly`で
あれば、addressを知っている側からは繋がるのに一覧には出ません。

### 2.2 Class of Device

Hostがiconを選び、機種によっては接続を提案するかどうかを決める値です。既定のままだと
「未分類」として扱われ、HIDとして提示されないことがあります。

```cpp
config.classOfDevice.majorDeviceClass = 0x05;  // Peripheral
config.classOfDevice.minorDeviceClass = 0x10;  // keyboard
```

`minorDeviceClass`は6 bitのfield値で、電波上はこれを2 bit左シフトした値になります。
`setClassOfDevice()`は**要求が受理されたこと**を返し、反映は非同期です。直後の
`classOfDevice()`はまだ前の値を返すため、反映を確認するなら一致するまで読み直します。

### 2.3 無線・linkの設定

profileの動作ではなく無線の振る舞いを変える設定が3つあります。

| 設定 | 呼び出し | trade-off |
|---|---|---|
| 送信電力 | `setTxPower(dBm)` / `setTxPower(min, max)` | 距離と消費電流 |
| page timeout | `setPageTimeout(milliseconds)` | 接続失敗までの速さと、応答が遅い相手を諦めること |
| 暗号鍵の最小長 | `setMinimumEncryptionKeySize(bytes)` | 弱いlinkを断ることと、一部の相手を断ること |

BR/EDRの電力制御は範囲の中からpacketごとにlevelを選ぶため、範囲を渡す形があります。
1つだけ渡す形は上下限を同じ値に固定します。levelは-12〜+9 dBmの3 dB刻みで、間の値は
丸められるため、`txPower()`は無線が適用した値を返します。BR/EDRの送信電力は
`EspBle::setTxPower()`のLE側とは独立です。

page timeoutは「何も応答しない相手をpageし続ける時間」で、相手の電源が入っていない、
あるいは圏外のときに`connect()`が失敗するまでの時間です。既定は5120 msです。次のpageから
効くため接続の前に設定します。`setPageTimeout()`の`true`は要求が受理されたという意味で、
`pageTimeout()`はbackendが確定させた値——確定するまでは0——を返します。

関連example: [RadioSettings](../examples/Classic/RadioSettings/)

## 3. Inquiry

inquiryはClassic機器の探索で、BLEのscanとは別です。結果にはaddress、remote name、
Class of Device、RSSIが含まれる場合があります。

`start()`の`true`は探索完了を意味しません。個々の結果は`onResult()`、終了は
`onComplete()`へ届き、`stop()`した場合も完了eventが届いて`cancelled`で区別できます。
callbackを処理できずqueueが満杯になった分は`droppedResultCount()`で観測します。

nameはinquiry応答に含まれないこともあります。addressだけ分かっている相手には個別に
問い合わせられます。

```cpp
bluetooth.inquiry().requestName(address);      // onRemoteName()へ
bluetooth.inquiry().requestServices(address);  // onRemoteServices()へ
```

`requestServices()`は相手が公開しているservice UUIDを返します。**scan中は応答が
来ません**——inquiryとSDPは両方が無線を使うためで、`onComplete()`を待ってから
照会します。

関連example: [Inquiry](../examples/Classic/Inquiry/)

## 4. SPP

SPPはClassic上のbinary-safeなbyte streamです。内部でRFCOMMを使います。RFCOMMは
Serial cableを模した信頼性のあるbyte streamで、GATTのCharacteristicやMTU、Notifyとは
関係ありません。

### 4.1 ServerとClient

Serverはservice nameとchannelを構成して待ち受けます。channelを0にするとbackendが
空きchannelを選びます。**`startServer()`は繰り返し呼べ、最大4 serviceを公開できます。**
どのchannelになったかは`onServerStarted()`が渡します。停止は`stopServer()`で全停止のみです。

Clientの`connect()`はSDPとRFCOMM接続を非同期に開始します。要求が受理されても、
相手が見つからない、SPPを公開していない、pairingに失敗するなどの理由で後から失敗し、
それは`onConnectionFailed()`へ届きます。**`connect()`の`true`は「試行を開始した」
という意味に過ぎません。**

相手が複数serviceを公開している場合、discoveryは全channelを返すだけでどれを使うかを
示せません。`connectToChannel(address, channel)`でchannelを指定します。送信側の
接続は同時1本です。

### 4.2 送受信

受信はpacket eventの`onData()`と、byte streamとして読む`available()` / `read()`の
両方があります。値はbinary-safeで、途中の`0x00`で終端しません。送信要求はqueueへ入り、
`true`はqueueへ入ったことを示します。peerへの送信完了は`onWriteCompleted()`です。
捨てた分は`droppedWriteCount()` / `droppedReceiveByteCount()`で観測します。

`EspBleClassicSppStream`は1 sessionをArduinoの`Stream`として扱うadapterで、`Serial`向けに
書かれたcode——`print()`、`readStringUntil()`、`parseInt()`——がそのまま動きます。session
自体は所有せず借りるだけなので、sessionが開いたら`attach()`し、閉じたら`detach()`します。
`Serial`と違う点は2つです。write 1回が1 SPP packetになるため、文字単位ではなく行単位で
書きます。もう1つは送信queueが有限なことで、空きが無いwriteは`setWriteTimeout()`の時間
（既定1000 ms、0なら待たない）まで待ち、書けた分を返します。

関連example: [SppServer](../examples/Classic/SppServer/)、
[SppClient](../examples/Classic/SppClient/)、
[SppStream](../examples/Classic/SppStream/)

## 5. Securityとbond

Classic securityはBLE securityと分離します。SSPのnumeric comparison、passkey entry、
bondの列挙・削除が使えます。

| IO capability | applicationに届く操作 |
|---|---|
| `None` | 確認なし（Just Works） |
| `DisplayOnly` | 6桁passkeyを表示 |
| `KeyboardOnly` | 相手が表示した6桁passkeyを入力 |
| `DisplayYesNo` | 両端に同じ値を表示して一致を確認 |

**`EspBleClassicSecurityConfig::enabled`を有効にしないとJust Worksになり、
IO capabilityは効きません。** SSPはserviceが要求したときだけapplicationを介するため、
有効にした場合はservice側がMITMを要求します。

legacy PIN pairingは**拒否します**。応答経路が無いため、固定PINで自動承諾するよりも
断る方が安全という判断です。

Classicのbondはlink key、BLEのbondはLE keyを管理します。片方の削除で他方は消えません。

関連example: [SppPairing](../examples/Classic/SppPairing/)

## 6. HID

Report Descriptorとreport packingはBLE側と同じmoduleを共有します。**電波に出る
reportの中身は同じで、API名もsignatureも揃えてあります。**

```cpp
bluetooth.hidKeyboard().configure(hidConfig);  // begin()より前
bluetooth.hidGamepad().configure(hidConfig);
// ...
bluetooth.hidKeyboard().write("hi");
bluetooth.hidGamepad().send(0, 0, 0, 0, 0, 0, ESP_BLE_HID_GAMEPAD_HAT_UP, 1);
```

Classicはdevice recordを1つ登録するため、configureした全profileが1つの合成
Report Descriptorに入り、profileごとにreport IDが分かれます。

**合成できるprofile数には上限があります。**descriptorと`name` / `description` /
`provider`の3つは1つのSDP recordを共有し、合計214 byteまでです。既定の文字列
（57 byte）ならkeyboard + mouse + consumer（144 byte）は入り、gamepadを加えた
212 byteは入りません。backendはこの失敗を報告せず、recordが無いまま「起動した」
deviceになるため、`begin()`が登録前に検査して`ResourceExhausted`で拒否します。
BLEにこの制限はありません。

### 6.1 Host側

`hidHost().connect(address)`で接続します。Classicには絞り込むadvertisementが無いため、
addressが必要です。SDPで受け取ったReport Descriptorを解析し、keyboard stateとusage単位の
event、mouse eventを配送します（BLEと同じ順序で、stateが先）。分類できないreportは
`onInputReport()`へrawで届きます。

**Host側で復号する範囲はBLEより狭く、keyboardとmouseだけです。**
consumer / system / gamepadはrawで受けます。

### 6.2 制御チャネル

Get_ReportとSet_Reportは制御チャネル上の**応答が必須の経路**です。返さないとHostは
待ち続け、機種によってはその後問い合わせをやめます。

- Get_Report: `onReportRequested()`で受け、`respondToReportRequest(request, ...)`で
  答えるか`refuseReportRequest(error)`で断る。requestをそのまま渡すのは、Hostが
  typeとreport IDを照合するため
- Set_Report: 拒否しなければlibraryがHID handshakeを自動で返す
- protocol mode（Boot / Report）はHostが決めるもので、deviceは`protocolMode()`と
  `onProtocolMode()`で観測するだけ

report IDの位置はchannelで異なります。device側では`reportId`が正で、`value`はpayloadのみ
です。host側では相手が送ったままの形で届くため、report IDを宣言するdeviceならpayloadの
前に付きます。

関連example: [HidKeyboard](../examples/Classic/HidKeyboard/)、
[HidGamepad](../examples/Classic/HidGamepad/)、
[HidComposite](../examples/Classic/HidComposite/)、
[HidKeyboardHost](../examples/Classic/HidKeyboardHost/)

## 7. A2DPとAVRCP

A2DPは音楽向けのaudio profileです。受信側がSink、送信側がSourceです。
**EspBleが扱うのはencode済みのpayloadで、codecとPCMのI/Oは別libraryの担当です。**
SBCへのencode/decodeはEspBleに入れません。

`send()`は`Accepted`、`WouldBlock`、失敗を返します。`WouldBlock`は正常なbackpressureで
errorではありません。frameは捨てずに保持して再送します——捨てるとstreamに欠落が出ます。

Sinkは自分の再生遅延を`setDelay()`でSourceへ伝え、Sourceは`onSinkDelay()`で受け取ります
（単位は1/10 ms）。映像を出すSourceはこの分だけ絵を遅らせます。自分の遅延を知っているのは
Sink側だけで、libraryは計測できません。

AVRCPは操作とvolumeを扱い、audioは流しません。**backendの要件によりAVRCPをA2DPより先に
開始します。** ControllerとTargetは1つの`avrcp()`が持ちます。

Target側のnotificationには**同梱hostの制限**があります。宣言できるのはvolumeだけで、
play statusやtrack changeはこのbuildでは到達できません。許可集合は
`supportedNotifications()`が返し、許可外の宣言は理由付きで拒否されます。
metadata / play statusの応答送信は公開backend APIに手段が無く未対応です。

関連example: [A2dpSinkRaw](../examples/Classic/A2dpSinkRaw/)、
[A2dpSource](../examples/Classic/A2dpSource/)、
[A2dpSinkAvrcp](../examples/Classic/A2dpSinkAvrcp/)、
[AvrcpController](../examples/Classic/AvrcpController/)

## 8. HFP

HFPは通話向けです。Headset側が`hfpClient()`、電話側が`hfpAudioGateway()`で、
**2つのroleはprocess単位で排他**です。A2DPと違いmono音声で、CVSDは8 kHz、
mSBCは16 kHzです。

SCO payloadもencode済みのraw viewで受け渡します。実機ではmSBCの57 byte送信が受信側で
58/60 byteのpadding付きになり、bad frameも60 byteで届きます。長さとbad frame情報を
失わずにdecoder側へ渡します。

通話以外に、電話機へ問い合わせたり自分のことを伝えたりできます。
`queryOperatorName()`と`requestSubscriberNumber()`は要求で、答えは
`onOperatorName()`と`onSubscriberNumber()`へ届きます。空文字も正当な応答です。
`disableNoiseReduction()`は電話機側のnoise reductionを止めるよう頼むもので、自前で
DSPを持つ機器のためにあります——2段重ねは1段より悪くなります。電池残量を電話機へ
伝えるには`enableAppleExtensions()`の後に`reportBatteryLevel()`を呼びます。Appleが
定めた拡張ですがAndroidやWindowsも受け付け、levelは0〜9です。

`dialMemory(location)`は電話機内の位置を指定して発信します。Audio Gateway側では
`Dial`ではなく`DialMemory`として届きます——位置は番号ではないため、桁として掛けると
別の相手に繋がります。Apple拡張はAG側では`UnknownAt`のtextとして届きます。backendに
解釈するAPIが無いためです。

Audio Gateway側の`setInBandRingTone()`は「呼出音を鳴らすのはどちらか」を機器へ伝え、
機器側は`onInBandRingTone()`で受け取ります。誤って伝えると、二重に鳴るか、来ない呼出音を
待ち続けます。

通話待ち・三者通話（CHLD、BTRH）は**未実装**です。このlibraryのAudio Gatewayが単一call
modelのため相手側で検証できず、外部phoneでしか動かせないAPIを未検証のまま出さない判断です。

関連example: [HfpClientRaw](../examples/Classic/HfpClientRaw/)、
[HfpAudioGatewayRaw](../examples/Classic/HfpAudioGatewayRaw/)

## 9. BLEとの同時利用

`EspBle`と`EspBleClassic`の両方を`begin()`するとdual-hostになり、brokerがHCIを
routingします。片方だけなら単一hostのpass-throughです。

同時に使えることは、無制限に並行できるという意味ではありません。radio、heap、
callback queueは共有資源です。dropカウンタとqueueを監視し、application側にも
上限のあるqueueを設けます。

**Classicをlinkしたsketchは、どちらの順で起動してもcontrollerがBTDMで起動します。**
そのためBLE側のcontroller memoryを解放できず、Classicを使わない場合よりheapが減ります。

dual-hostの検証はEspBle同士とcore内蔵host相手までで、**外部機器との相互運用は未検証**です。
不安定な場合は一方を`end()`して単一hostで使います。

同梱hostの構成、archiveの生成手順、検証済み範囲は
[引き継ぎ](HANDOFF_ESP32_CLASSIC.ja.md)と[Classic host archive再生成](CLASSIC_HOST_BUILD.ja.md)にあります。

## 次に読むもの

SDP recordの予算、SPP queueの構成、dual-hostの内部、既知の不具合の見取り図は
[EspBleを深く使う](GUIDE_ADVANCED.ja.md)にあります。
