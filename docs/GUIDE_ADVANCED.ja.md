# EspBleを深く使う

> English: [GUIDE_ADVANCED.md](GUIDE_ADVANCED.md)

入門ガイド（[BLE](GUIDE_BLE_BASICS.ja.md) / [Classic](GUIDE_CLASSIC_BASICS.ja.md)）は
無線が何をするのか、どのAPIを呼ぶのかを説明します。この文書はその次の問い
——**負荷がかかったとき、上限に達したとき、うまく動かないときにlibraryがどう振る舞うか**
——に答えます。動くsketchがあり、購読12件や送信queue満杯、操作中に消える相手を扱う段階を
想定しています。

ここに出てくる数値はすべてソース由来で、定数名を併記しています。信じる代わりに確かめて
ください。

## 1. callbackはどこで動くか

EspBleの実行contextは3種類です。callbackがどこで動くかが、その中で何をしてよいかを決めます。

| context | そこで動くもの | 寿命 |
|---|---|---|
| 自分のtask（`update()`を呼ぶtask。通常は`loop()`） | **applicationのcallbackのほぼ全部**——`onConnected()`、`onWritten()`、`onInputReport()`、`onData()`など | 自分 |
| host task | NimBLE（またはClassic Bluedroid）host自身の処理と、後述する「遅らせられないcallback」だけ | backendが`begin()`で生成 |
| 一時的なworker task | blockingするbackend操作1件だけを実行して終了する | 操作ごと |

**例外はhost taskで動き、それぞれ理由があります。**

| callback | `update()`を待てない理由 |
|---|---|
| `gattServer().onRead()` | ATTのread transactionが完了する前に値が存在しなければならない。ここで`setValue()`した値がそのまま相手へ返る。値を埋められる所有者は1つなので、listener listではなく単一observerである |
| `a2dpSink().onMedia()`、HFPの`onAudio()` | payloadはbackendのbufferへの読み取り専用viewで、callbackが返るまでしか有効でない。必要な分をその場でcopyする。backendは直後に解放する |
| passkey / numeric comparisonの要求 | pairing手続きが待っている。EspBleはyieldしながら`loop()`が`providePasskey()` / `confirmNumericComparison()`で答えるのを待ち、timeoutすればpairingを拒否する |
| BLE MIDIのparser callback | 解析中のpacketを指すpointerがcallbackの間だけ有効である |

この4つの中では、表示もblockも`begin()` / `end()`の呼び戻しも行わないでください。それ以外は
次の`update()`まで待てます。

queueが流れるのは`EspBle::update()`と`EspBleClassic::update()`です。`update()`は有限時間の
advertisingとscanの期限処理、停滞したGATT操作のtimeout、GATT操作queueとserver送信queueの
汲み出し、保留notificationの解放、scan結果と接続eventの配送、auto-reconnectの駆動、HID
device / host eventの配送を行います。**上の例外を除き、これらの外で自分のcodeへ何かが届くことは
ありません。**

したがって3つの帰結があります。

- **callbackの中でblockするとすべてが止まります。**その`update()`の残りも進みません。
  代わりに配送するdispatcherは居ません。
- **自分のsketchのstateにlockは要りません。**`update()`が配送するcallbackと`loop()`から触る
  だけなら同じtaskです。上の4つのhost task callbackと共有するstateだけは注意が必要です。
- **`update()`を呼ばない`loop()`は無線が死んだように見えます。**host taskは接続してbufferし
  続けているだけです。`update()`をtimerで間引くなら、溢れるのは次章のqueueです。

一時的なworkerが在るのは、backendの一部呼び出しがblockするためです。操作ごとに固定stackで
priority 1で生成され、完了すると終了します——client GATT操作の`espble-gatt`（6144 byte）、
serverのnotify / indicateの`espble-gatt-send`（4096）、HID host discoveryの
`espble-hid-host`（16384。descriptor解析に必要）です。task生成に失敗した場合も、操作は通常の
callbackへ`ResourceExhausted`として結果を返します。memoryが薄い瞬間があっても、来ない
callbackを待つ呼び出し側は生まれません。

NimBLE hostはcontrollerの準備完了を通知し、それ以前にGAPへ触れてはいけません。`begin()`は
それを待つので、即座には返りません。

## 2. 上限と、埋まったときの挙動

EspBleのqueueとtableはすべて固定長です。これは意図した設計で、無制限に確保するlibraryは
忙しい瞬間をsketchの別の場所でのheap不足に変えてしまいます。代わりに、上限ごとの挙動を
先に知っておく必要があります。

### BLE

| 上限 | 値 | 定数 | 上限での挙動 |
|---|---|---|---|
| 同時接続数 | slotは4、実際はcontrollerが決める | `ConnectionCapacity`、`CONFIG_BT_NIMBLE_MAX_CONNECTIONS`（同梱buildでは3） | slotが空いていてもcontroller上限を超える接続はbackendで失敗する |
| GATT client操作 | 実行中1件 + queue 8件 | `GattQueueCapacity` | `ResourceExhausted`。電波に出る前に拒否 |
| serverのnotify / indicate | queue 8件 | `SendQueueCapacity` | 送信結果として`ResourceExhausted` |
| 有効なclient購読 | 16 | `ClientSubscriptionCapacity` | `subscribe()`が失敗し、CCCDも書かない |
| persistent subscription record | 16 | `PersistentSubscriptionCapacity` | 購読自体は成功し、recordだけが失われる。`droppedPersistentSubscriptionCount()`が数える |
| 保留notification | 4 | `DeferredNotificationCapacity` | GATT完了との順序を保つためだけに保持する |
| 配送待ちscan結果 | 16 | `ScanQueueCapacity` | 新しい結果を落として数える。溜めてくれることを期待せず`update()`を頻繁に呼ぶ |
| 配送待ち接続event | 8 | `ConnectionEventQueueCapacity` | lifecycleや完了のeventは自分が落ちる代わりに最古のnotificationを追い出す。追い出せるnotificationが無ければ新しいeventを落とす。どちらも`droppedEventCount()`が数える |
| eventごとのlistener | 4 + primaryの`on*()` | `EspBleCallbackList<Callback, 4>` | `addXListener()`がidを返さない。既存listenerを追い出さない |
| bond | `CONFIG_BT_NIMBLE_MAX_BONDS`（未定義なら16） | `BondCapacity` | backendの置換policyに従う |
| discovery結果 | service 16、characteristic 48、descriptor 48 | `MaxDiscoveredGatt*` | 記録を止め、discovery自体が`ResourceExhausted`で**失敗**する。部分的なdatabaseを完全なものとして報告しない |
| advertising payload | advertisingとscan responseで各31 byte | `EspBleAdvertising::Payload::Capacity` | 収まらない項目を理由付きで拒否 |
| accept list | 8 | `MaxAcceptListEntries` | 以降は拒否 |
| custom HID report | 4 | `EspBleHidCustom::MaxReports` | 拒否 |
| 自動再discovery対象のpeer記憶 | 4 | `MaxRediscoverPeers` | 古いものを忘れる |

このうち2つは、動いているsketchが最初にぶつかるので強調しておきます。

**GATT操作は実行中1件だけです。**characteristic 12件を`for`で購読しても通りません——1件が
電波に出て8件がqueueに入り、残りは電波に出る前に`ResourceExhausted`で返ります。前の操作の
完了callbackから次を出してください。persistent subscription registryを埋めるPeer testが
直列に購読しているのも同じ理由です。並行して投げるとregistryではなくqueueを試すことに
なります。

**registryが満杯のときは黙っています。**これは設計です。`subscribe()`は成功し、データも流れ、
再接続してはじめて復元されないことが分かります。`droppedPersistentSubscriptionCount()`だけが
手がかりで、そのために存在します。数台以上のpeerへ購読するsketchでは必ず見てください。

### Classic

| 上限 | 値 | 定数 | 上限での挙動 |
|---|---|---|---|
| SPPの1 write | 990 byte | `EspBleClassicSpp::MaximumWriteSize` | `EspBleClassicSppStream`は分割し、生APIは拒否する |
| sessionごとの送信queue | 8 | `WriteQueueCapacity` | writeは書けた分を返す。`Stream::write()`は`setWriteTimeout()`まで待つ |
| sessionごとの受信buffer | 2048 byte | `ReceiveBufferCapacity` | 未読byteを捨てて数える（`droppedReceiveByteCount()`） |
| 1台のSPP service数 | 4 | `EspBleClassicSpp::MaximumServers` | `startServer()`が拒否 |
| HID reportの長さ | 1024 byte | `MaximumReportLength` | 送信前に`InvalidArgument` |
| HID descriptor + profile文字列 | 214 byte | `MaximumSdpRecordPayload` | `begin()`が`ResourceExhausted`で拒否 |
| 1回の照会で返るremote service | 12 | `MaximumServices` | 超過分は報告しない |
| 宣言できるAVRCP notification | 8 | `MaximumNotifications` | 拒否 |
| page timeout | 14〜40959 ms（既定5120） | `setPageTimeout()`内で検査 | `InvalidArgument`。送信前に拒否 |
| 送信電力 | -12〜+9 dBm、3 dB刻み | `ClassicTxPowerLevels` | 対応levelへ丸める |

214 byteのSDP予算だけは「どれだけ速く動くか」ではなく「何が作れるか」を変える上限です。
合成したReport Descriptorと`name` / `description` / `provider`は、recordの標準属性（86 byte）と
共有する300 byteのpad（`CONFIG_BT_SDP_PAD_LEN`）に収まる必要があります。既定の文字列では
keyboard + mouse + consumerが201 byteで収まり、gamepadを加えた215 byteは収まりません
——推定ではなく実機で測った境界です。BLEにこの制限は無く、Report Mapは通常の値と同じく
characteristicとして読まれます。

## 3. 「受理」は「反映」ではない

電波に届くEspBleのAPIはたいてい、**受理されたか**——引数が妥当でstateが適法、backendへ
要求を渡せた——を返し、**実際に何が起きたか**はcallbackで報告します。戻り値を結果として
扱うのが、机上では動いて現場で動かないsketchの最も多い書き方です。

往復である（＝それぞれのcallbackを持つ）API: GATTのread / write / subscribe / discovery、
serverのnotify / indicate、MTU negotiation（`preferredMtu`の既定は247。実効値は23から始まり
`onMtuChanged()`で届く）、security要求、HID hostのreport要求・protocol mode・idle rate変更・
virtual cable unplug、Classicのname / service照会、A2DP delayのsetとget、AVRCPの全command、
HFPの全command。

この区別は検証にも効きます。`setPageTimeout()`がtrueを返しても、controllerが値を取り込んだ
証拠にはなりません。だからPeer testは**応答しないaddressへの接続試行の所要時間**で測ります
——1000 msなら3秒未満、既定の5120 msならそれより1秒以上長い。自分で設定APIを足すときも、
この形の確認を選んでください。

backendが完了を返さない経路では、EspBleは完了をでっち上げずにそう書きます。backendが成功を
返すのに実際には起きていない経路——padを溢れたSDP record、終端のOKが無いAT交換——では
libraryが自分で穴を閉じます。黙って失敗したbackendは、Hostが相手にしてくれるまで正常な
deviceと区別できないためです。

## 4. errorの意味

`EspBleError`は6値で、EspBleは一貫して使い分けています。値を見ればどこを見ればよいか分かります。

| 値 | 意味 | 直す場所 |
|---|---|---|
| `InvalidState` | 呼び出し自体は正しいが今ではない——未起動、未接続、role違い、競合する操作が進行中 | sketch側の順序 |
| `InvalidArgument` | 引数から妥当な要求を作れない。送信前に拒否 | 呼び出し箇所 |
| `ResourceExhausted` | 2章の固定容量が満杯、またはworker taskを生成できなかった | 出す速度、同時要求数 |
| `NotFound` | 指したものが無い——未知のhandle、UUID、connection / session id | 識別子、または変わってしまった相手 |
| `Timeout` | 受理され送られたが、期限内に返ってこなかった | 相手、または電波環境 |
| `BackendFailure` | host stackが拒否または失敗した。backendのstatusはdetail文字列に入る | まずdetail文字列、次にこの表の他の行 |

`BackendFailure`には必ずmessageが付きます。logへ出してください。「flow controlが既に有効
なのでcontrollerが拒否した」と「相手が切断した」の違いはenumではなくその文字列にあります。

Classicの音声は独自の結果型を持ちます。media送信はhot pathで、「少し待って再試行」がerror
ではなく普通の答えだからです。`EspBleClassicAudioSendResult`は`Accepted`、`WouldBlock`、
`InvalidState`、`InvalidArgument`、`TooLarge`、`BackendFailure`です。

## 5. backpressureとthroughput

streamするものには必ず有限のqueueがあり、満杯への正しい対応はいつも同じ形です——
**生産を止め、`update()`を回し続け、再試行する。**

- **GATT client操作**: 実行中1件。完了callbackから次を出す。
- **serverのnotify / indicate**: queue 8件、`update()`が汲み出す。それを超えるburstは
  `ResourceExhausted`で返る。queueは伸びない。
- **SPP**: 1 packet 990 byte、queue 8 packet。`write()` 1回が1 packetなので、1文字ずつより
  1行ずつが圧倒的に安い。`Stream::write()`は`setWriteTimeout()`（既定1000 ms、0なら待たない）
  まで待って書けた分を返す。bufferが尽きた`Serial`と同じ挙動である。`flush()`は
  `pendingWriteCount()`が0になったら返る。
- **A2DP Source**: `WouldBlock`はtransportが忙しいという普通の合図。20,000 packetの転送は
  `WouldBlock`が0でない状態で欠損なく完走する——retryはerror経路ではなく通常動作の一部である。
- **HFPのSCO**: frameはcodecの周期で届き、その周期で送る必要がある（mSBCなら57 byte frame）。
  遅れた生産者を隠すqueueは無く、落ちたframeは音の欠けになる。

BLEのthroughputのもう半分はMTUです。`preferredMtu`（既定247）は要求で、`onMtuChanged()`が
来るまでは23 byte——notificationのpayloadは20 byteです。244 byteのpacketを用意して接続直後に
送るのは、よくある自作の失敗です。

## 6. 素性・bond・戻ってくること

再接続はBLEの識別modelが意外に働く場所なので、要素を分けて書きます。

- **connection idは接続が生きている間だけ安定で、生きている間は再利用されません。**
  controllerのhandleではなくlibraryの識別子です。保存しないでください。
- **接続ごとのcache**がdiscovery結果と購読stateを接続間で分離するので、同じUUIDを出す
  2台の相手を取り違えることがありません。
- **persistent subscription**はpeerが何を購読していたかを記録し、再接続後に再適用します
  ——record 16件、落ちた分は数えます（2章）。
- **auto-reconnect**（`setAutoReconnect()`、既定off）は予期しない切断のあと2000 ms間隔
  （`ReconnectIntervalMilliseconds`）で再試行し、`update()`から駆動されます。
- **HID hostの再discovery**は別機構です（`setAutoRediscover()`、既定off、peerを4件記憶）。
  HID hostは汎用の購読registryを使わないためです。sketchが自分で`discover()`を呼んでいれば
  自動実行はskipします。
- **address privacy**は再接続が成立するかどうかを決めます。`ResolvablePrivate`では相手の
  addressが定期的に変わり（無印ESP32の同梱hostでは900秒ごと）、IRKを持つbond済みのpeerだけが
  解決できます。bondしないRPAは誰も追えないaddressです。
- **BLEのbondとClassicのbondは別物です。**片方を削除しても他方は残ります。無印ESP32では
  どちらもNVSにあり、bondを消すsketchはどちらの無線の話かを明示するべきです。

## 7. BLEとClassicを同時に使う

無印ESP32では両hostが1つのcontrollerを共有し、その間にHCI brokerが入ります。build flagは
無く、登録hostが1つならpass-through、`EspBle`と`EspBleClassic`の両方を開始するとbrokerが
routingします。このmodeは**実験扱い**で、退避手段は一方を`end()`することです。使うのなら、
理解しておく価値があるのは次の点です。

brokerが所有するもの——つまりどちらのhostも制御しなくなるもの:

- **command FIFOとその配車**。16 entry（`ESPBLE_HCI_COMMAND_SCHEDULER_CAPACITY`）、実行中は
  1 transaction、解放はcontroller自身の`Num_HCI_Command_Packets` credit。high-water markと
  満杯counterを持つ。
- **command応答のrouting**（opcodeを出したhostへ返す）と、connection handle所有権による
  ACLのrouting。
- **event mask**。両hostの要求のunionを取るので、片方のmaskが他方のeventを黙らせることは
  ない。
- **controllerのlifecycle**。controllerを起動したhostが停止責任をbrokerへ渡すので、
  どちらのhostから停止・破棄してもよい。
- **再attachするClassic hostのHCI Resetとhost flow control設定への仮想完了**。生きているLE
  linkが依存するcontrollerをresetすればlinkが落ちるため。
- **controller-to-host ACL flow control**。共有controllerではどちらのhostも実行できない
  ——Bluedroidは自分宛ての分しかcreditせず、同梱NimBLEは一切返さないので、controllerの
  bufferが枯渇して双方の通信が止まる。

policyは**fail closed**です。実機で観測したHCI opcodeをtransportとscopeで分類し、未分類の
opcodeや所有していないhostからのopcodeはcontrollerへ届く前に拒否します——dual-host時だけ
なので、単一hostの挙動は変わりません。libraryを拡張して「両host起動時だけcommandが失敗する」
なら、まず未分類opcodeを疑ってください。

診断には[`EspBleHciBroker.h`](../src/EspBleHciBroker.h)の
`espble_hci_broker_get_diagnostics()`があります。host別の投入数と送信数、queueのhigh-water
mark、`command_queue_full`、`command_response_mismatch`、`command_unregister_busy`、返した／
落としたACL credit、brokerがflow controlを所有しているか、host別のsecurity event要約、
host別の実際に観測したopcode一覧が入ります。Peer testはこのcounterで判定しており、sketchでも
同じように出力できます。

brokerが**やらないこと**が1つあります。送信ACL bufferを2つのhost間で按分しません。各hostは
自分だけが居るものとしてcontrollerに対する送信量を見積もるので、相手の送信を勘定できません。

## 8. size——何が効くか、どう測るか

EspBleのコストは、呼ぶ機能の数よりも**どのhostをlinkするか**で決まります。

- **controller内蔵target**（S3 / C3 / C6 / H2）ではNimBLE hostがcoreから来るので、EspBleは
  自分のcodeだけを足します。
- **無印ESP32**ではcoreのprebuiltがBluedroidなので、EspBleがこのchip向けにNimBLE hostを
  同梱します（`src/nimble_esp32/`）。このtargetで最も大きい追加です。
- **Classic**は独自build・名前空間化したClassic-only Bluedroid archive
  （`src/esp32/libespble_bluedroid_classic.a`）をlinkします。archiveはdisk上では大きい
  （約4.6 MB）ものの、flashへ入るのはlinkerが必要としたmemberだけなので、実際の支払いは
  どのprofileを起動するかで変わります。
- **名前がsizeに効きます。**libraryのsource中のClassic呼び出しはすべて`espble_bd_`接頭辞の
  macroを通ります。これがcore側Bluedroidをlinkから排除しています。接頭辞の無い呼び出しが
  1つあると2つ目のBluedroidが入り、およそ0.5 MBかかります。今はunit testが機械的に検査します。

前提を置かずに測ってください。

- `arduino-cli compile`はsketchごとのprogram / global変数のsizeを表示します。同じsketchの
  2 profileを比べれば機能の値段が出ます。
- 実行時は`ESP.getFreeHeap()` / `ESP.getMinFreeHeap()`を`begin()`の前後と最初の接続の前後で
  取ると、定常コストとpeakコストを分けられます。A2DPのPeer testはまさにこれ（baseline /
  current / minimum / largest block）を報告しており、真似する価値があります。
- 1章の一時的なworkerを忘れないでください。peak heapにはHID host discovery中の16 KB stackが
  含まれます。

## 9. 不具合の見取り図

実機で見つかった実際の不具合と、それを特定する症状です。これらが出ているなら原因は既知です。

| 症状 | 原因 |
|---|---|
| Classic HID deviceはpairingできるのに、Hostが使えるdeviceとして認識しない | SDP recordがpadを溢れた。今は`begin()`が`ResourceExhausted`で拒否する。古いcodeならdescriptor + 文字列を214 byteと突き合わせる |
| report IDを使うdeviceからHID hostが何も受け取らない | transportはreport IDをpayloadの前に付けるが、descriptorのoffsetはpayload起点。修正済み。自分でreportを解析するなら同じoffsetに注意 |
| HFPの相手がAT commandを1つ送ったあと黙る | unknown ATへの応答が終端のOKを欠くと交換が閉じず、相手の次のcommandがqueueに残り続ける。`respondToUnknownAt()`が閉じる |
| IO capabilityを設定してもClassic pairingが常にJust Works | Secure Simple Pairingはserviceが要求したときだけapplicationを介する。service側の要求水準を設定したsecurityから導くようにした |
| loopの途中から`subscribe()`が`ResourceExhausted`になる | GATT queue（1 + 8）。完了callbackから連鎖させる |
| 再接続後に購読が復元されない | registry満杯。`droppedPersistentSubscriptionCount()`を見る |
| 接続は生きているのにnotificationが止まる | `update()`の呼び出し間隔が長すぎる、またはその中のcallbackがblockしている |
| host 1つなら通るcommandが、両host起動だと失敗する | dual-hostのfail-closed policy。未分類または別host所有のopcode |
| sketchが起動時に1度出す行を待つtestがtimeoutする | serial monitorがreset後に接続した。commandで同じ行を要求する（`tests/conftest.py`の`probe` fixture） |

習慣として得なのは2つです。`BackendFailure`のdetail文字列を必ずlogへ出すこと。データが
消えたように見えるときはdrop counter（`droppedEventCount()`、
`droppedPersistentSubscriptionCount()`、`droppedReceiveByteCount()`）を出すこと。これらの
counterは、他に気づく手段が無い失敗のためにあります。

## 10. 拡張できるところと、意図して無いもの

公開APIとしての拡張点:

- **任意のHID Report Descriptor**。`EspBleHidCustom`（report 4件）またはvendor report経路で、
  BLEとClassicの両方で使えます。固定profileと同じHID serviceへ合成されます。report IDは
  一意で、固定profileの予約（1〜6）を避ける必要があります。
- **生の属性tableから組むGATT server**。だからEspBleは同じUUIDのserviceを2つ、あるいは1つの
  service内に同じUUIDのcharacteristicを2つ、それぞれ別handleで公開できます。
- **属性handle指定**。UUIDを受けるところではhandleでも指定できます。UUIDが曖昧な相手向けです。
- **生payload経路**。ClassicのA2DPはencode済みmedia、HFPは生SCO frameを運ぶので、codec
  libraryを上に載せられます。EspBleはPCMについて立場を取りません。

意図して無いもの（探さないために）:

- **生HCI API**。brokerの分類がdual-hostの安全性を作っているので、抜け道はそれを無効化します。
- **codec、PCM処理、device I/O**。この境界がBluetooth側を単体で試験可能にしています。
- **VFS経由のSPP**。`EspBleClassicSppStream`がfile descriptor経路を増やさずに同じ用途を
  満たします。
- **AVRCP Targetのmetadata / play status送信**。同梱hostの公開APIに手段がありません。Targetが
  宣言できるのはvolume changeだけで、`supportedNotifications()`がそれを返します。
- **通話待ち・三者通話（CHLD、BTRH）**。ここのAudio Gatewayは単一call modelなので、実装を
  検証する相手が居ません。
- **HID Hostの複数device同時接続**。対応すると既存signatureがdevice単位のidを取る形へ
  変わります。

Classicが何を公開し、何が実機検証済みで、何が未検証・未実装かは、理由込みで
[CLASSIC_FEATURE_INVENTORY.ja.md](CLASSIC_FEATURE_INVENTORY.ja.md)にあります。APIの形の
理由は[API_DESIGN.ja.md](API_DESIGN.ja.md)と[DECISIONS.ja.md](DECISIONS.ja.md)にあります。
