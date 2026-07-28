# BLE通信の入門ガイド

BLEを初めて使う人が、**何が起きているのか**を理解するための資料です。用語はすべてこの文書内で説明します。

実際のコードは各exampleにあります。この文書は概念に集中し、対応するexampleへのリンクを示します。

---

## 1. BLEとは

Bluetooth Low Energy（BLE）は、**小さなデータを低消費電力でやり取りする**ための無線規格です。

名前は似ていますが、イヤホンやSPP（Serial Port Profile）で使われてきた**Bluetooth Classicとは別物**で、互換性はありません。

| | Bluetooth Classic | BLE |
|---|---|---|
| 通信の形 | 常時接続のストリーム | 必要なときだけ短くやり取りするイベント指向 |
| 向いているもの | 音声（A2DP/HFP）、シリアル（SPP） | センサー値、キー入力、設定値 |
| 消費電力 | 大きい | ボタン電池で年単位を狙える |

EspBleはBLE専用です。ESP32-S3などの対象チップはBluetooth Classicを搭載しておらず、A2DP・HFP・SPPは使えません。「BLEでシリアル通信のようなことをしたい」場合は、後述するGATTの上に構築します。

### 1.1 GAPとGATT — 2つの層

BLEを理解する最初の鍵は、**GAPとGATTという2つの層がまったく別の仕事をしている**ことです。

| | GAP（Generic Access Profile） | GATT（Generic Attribute Profile） |
|---|---|---|
| 担当 | **探す・つながる** | **やり取りする** |
| 扱うもの | アドバタイズ、スキャン、接続、アドレス | Service、Characteristic、値の読み書き |
| いつ使うか | 接続が成立するまで | 接続が成立した後 |

一言でいえば、**探して繋ぐまでがGAP、繋がった後の会話がGATT**です。この文書は2章でGAP、3章でGATTを扱います。

### 1.2 4つの役割 — 2つの独立した軸

BLEには役割を表す言葉が4つ出てきます。混乱しやすいのは、これが**独立した2つの軸**だからです。

**軸1: リンクの役割（GAPの話）**

- **Peripheral（周辺機器）** — アドバタイズして待つ側。接続を**受け入れる**
- **Central（親機）** — スキャンして探す側。接続を**開始する**

**軸2: データの役割（GATTの話）**

- **GATT Server** — 値を**持っている**側。読み書きに応え、変化を通知する
- **GATT Client** — 値を**使う**側。読み書きを要求し、通知を購読する

典型的には「Peripheral = GATT Server」「Central = GATT Client」ですが、**これは決まりではありません**。接続が確立した後は、どちらの側もServerにもClientにもなれます。たとえばキーボード（Peripheral）がホストの時刻を読みに行けば、それはPeripheralかつGATT Clientです。

ESP32は1台で**CentralとPeripheralを同時に**こなせます。たとえばキーボードから入力を受け取りつつ（Central）、PCへキーボードとして見せる（Peripheral）といった構成が可能です。

### 1.3 大原則 — 要求とイベントは別のタイミング

EspBleのAPIを読む前に、必ず知っておくべき約束事です。

BLEの操作はほぼすべて**非同期**です。「接続して」と頼んでもその場では接続しません。無線のやり取りが終わるのは、早くて数十ミリ秒後、遅ければ数十秒後です。

そこでEspBleは操作を2段階に分けています。

1. **要求API** — 「お願いを受け付けたか」だけをその場で `bool` で返します。まだ何も完了していません
2. **イベント** — 実際の完了・失敗は、後から登録済みのコールバックへ届きます

そして**すべてのイベントは `loop()` の中で呼ぶ `ble.update()` から配送されます**。

```cpp
void loop() {
  ble.update();  // ここで初めて、溜まっていたイベントがコールバックへ配送される
  delay(1);
}
```

これは意図的な設計です。BLEスタックは専用のタスクで動いており、そこから直接コールバックを呼ぶと、アプリケーションのコードが別スレッドで動くことになります。EspBleはイベントを一度キューに溜め、`update()` を呼んだタスク（通常は `loop()`）でのみ配送します。**コールバックの中で共有変数を触っても排他制御が要らない**のはこのためです。

裏を返せば、**`update()` を呼び忘れると何も起きません**。スキャン結果も接続完了も届かず、原因の分かりにくい「動かない」状態になります。

この非同期の性質から、EspBleのコードは自然と**連鎖（チェーン）**の形になります。「操作を頼む → その完了イベントの中で次を頼む」の繰り返しです。

---

## 2. GAP編 — 探してつながる

この章はBLE通信の時系列に沿って進みます。**アドバタイズ（Peripheral）→ スキャン（Central）→ 接続（両者）**の順です。

### 2.1 アドバタイズ — Peripheralが存在を知らせる

すべての始まりはPeripheral側の**アドバタイズ**（advertising、広告）です。

アドバタイズとは、**「ここにいます、こういう機器です」という短いデータを周囲へ一定間隔で放送し続ける**ことです。宛先はありません。電波の届く範囲にいる全員が受信できます。

#### 何を載せられるか

アドバタイズのデータは**AD構造**（AD structure）の並びです。それぞれが「長さ・種別・値」の3要素を持ちます。主な種別は次のとおりです。

| 載せるもの | 用途 |
|---|---|
| **Flags** | 「接続可能か」「Classic非対応か」などの基本属性。EspBleが自動で付けます |
| **Local Name** | 人間が読む名前。`EspBle Keyboard` など |
| **Service UUID** | 提供する機能の種別。受信側が絞り込みに使う最も重要な情報 |
| **Service Data** | Service UUIDと組にした値そのもの。センサーが値を放送するときに使う |
| **Manufacturer Data** | ベンダー独自のデータ。iBeaconもこの形式 |
| **Appearance** | 機器の見た目の種別（キーボード、体温計など）。スマホがアイコン表示に使う |
| **Tx Power Level** | 送信電力。受信側がRSSIと組み合わせて距離を推定できる |

#### 31バイトの壁

ここが最初の関門です。**アドバタイズのデータは31バイトしか入りません。**

しかも各AD構造は値のほかに2バイト（長さ＋種別）を消費します。128ビットのService UUIDを1つ載せるだけで、16 + 2 = 18バイト。Flagsの3バイトと合わせると、残りは10バイトしかありません。

この制限を緩める仕組みが**Scan Response**です。受信側が「もっと教えて」と要求すると、Peripheralは**もう1つの31バイト**を返せます。合計62バイトです。

- **アドバタイズ本体** — 近くの全員に届く。相手を判別するための最小限を置く
- **Scan Response** — 要求してきた相手にだけ届く。名前などかさばる情報を置く

EspBleは、Scan Responseに何も指定しなければ**デバイス名を自動的にそちらへ置きます**。31バイトを名前で消費しないためです。どちらの面に何を載せるかを自分で決めるときは、`advertising().data()` と `advertising().scanResponse()` がそれぞれの面のビルダーを返します。

> **なぜ31バイトなのか、増やせないのか**
> これはBLE 4.0からある**Legacy Advertising**の仕様上の上限です。BLE 5.0の**Extended Advertising**を使えば255バイトまで拡張できますが、EspBleでは利用できません。Arduino-ESP32に同梱されているNimBLEが `CONFIG_BT_NIMBLE_EXT_ADV` を無効にしてビルドされており、Arduinoライブラリ側からこの設定を変更する手段がないためです。

#### 接続できるアドバタイズ、できないアドバタイズ

アドバタイズには2種類あります。

- **Connectable** — 「接続していいですよ」という放送。通常のPeripheral（既定）
- **Non-connectable** — 放送するだけで接続は受け付けない。**ビーコン**と呼ばれる形態。`advertising().setConnectable(false)` で切り替える

ビーコンは、値そのものをアドバタイズに載せてしまい、接続という手続きを省きます。温度センサーが5秒ごとに温度を放送する、店舗の棚が識別子を放送する、といった用途です。受信側は接続しないので、**1つのビーコンを何台でも同時に受信できる**という利点もあります。

#### アドバタイズ間隔

放送の間隔は `advertising().setInterval(minMs, maxMs)` で20ミリ秒から10.24秒まで設定できます。トレードオフは明快です。

- **短い** — すぐ見つけてもらえるが、電力を消費する
- **長い** — 電池は持つが、相手が見つけるまで時間がかかる

なお仕様上、Non-connectableなアドバタイズは**100ミリ秒以上**にする必要があります。

#### 関連するexample

| example | 内容 |
|---|---|
| [Gap/Advertise](../examples/Gap/Advertise/) | 名前とService UUIDを載せた最小のアドバタイズ |
| [Gap/ScanResponse](../examples/Gap/ScanResponse/) | 2面に分けて31バイト制限を回避する |
| [Gap/Beacon](../examples/Gap/Beacon/) | Manufacturer Dataを載せた接続不可のビーコン |
| [Gap/IBeacon](../examples/Gap/IBeacon/) | Appleが定めたiBeaconレイアウト |
| [Gap/ServiceData](../examples/Gap/ServiceData/) | Service Dataとしてセンサー値を放送する |

### 2.2 スキャン — Centralが相手を探す

Central側は**スキャン**（scanning）で周囲のアドバタイズを受信します。

#### passiveとactive

スキャンには2種類あり、`scanner().start()` へ渡す `EspBleScanConfig` の `active` で選びます。

| | `active` | 動作 | 受け取れるもの |
|---|---|---|---|
| **Active Scan** | `true`（既定） | アドバタイズを受信したら**Scan Request**を送り返す | アドバタイズ本体＋**Scan Response** |
| **Passive Scan** | `false` | ただ聞くだけ。こちらは何も送信しない | アドバタイズ本体のみ |

前節のとおり名前はScan Responseに置かれることが多いため、**名前で相手を探すならActive Scanが必要**です。既定が `true` なのはこのためです。

Passive Scanの利点は、こちらが電波を出さないことです。消費電力が下がり、周囲に自分の存在を知られません。相手を判別するのにService UUIDだけで足りるなら、Passiveで十分です。

#### intervalとwindow

同じく `EspBleScanConfig` に、スキャンの時間を決める設定が2つあります。

- **interval（間隔）** — スキャンを開始する周期。`intervalMilliseconds`
- **window（窓）** — そのうち実際に受信している時間。`windowMilliseconds`

たとえばinterval 100ミリ秒・window 50ミリ秒なら、**受信しているのは半分の時間**です。残り半分は他の処理に使えます。window = intervalにすれば常時受信になりますが、消費電力は最大になります。

スキャンを続ける時間は `durationSeconds` で指定し、`0` なら止めるまで続きます。

#### 見落としの問題

ここが実務で最も引っかかる点です。

アドバタイズは一瞬の放送であり、スキャン側のwindowが閉じている間に飛んできたものは**受信できません**。さらにBLEは3つのチャネルを順に使うため、タイミングによってはさらに取りこぼします。

したがって、**1回スキャンして見つからなくても「その機器が存在しない」とは言えません**。実用的には次のようにします。

- 通常は**3〜5秒**のスキャンを行う
- 周囲にBLE機器が多い環境では、さらに長く取る
- 特定の相手を待つなら、時間無制限で見つかるまでスキャンし続ける

#### 受信結果に何が入るか

1件のスキャン結果（`EspBleScanResult`）には、アドバタイズ（とActive Scanなら Scan Response）から取り出した情報が入ります。`address` / `addressType` / `rssi`（受信強度）/ `connectable` / `name` / `serviceUuids` / `serviceData` / `manufacturerData` です。

目的のService UUIDを持つ相手かどうかは `advertisesService(uuid)` で判定します。UUIDを値として比較するため、短縮形とフル形のどちらで書いても一致します。

RSSIはdBmで、0に近いほど近くにあります。目安として-40は至近、-90はかなり遠い、という感覚です。

#### 重複除外という落とし穴

同じ機器のアドバタイズは繰り返し飛んできます。EspBleは既定で**重複を除外**し、1つの機器につき1回だけ通知します。周囲の機器を一覧するだけならこのほうが扱いやすいためです。

ここに落とし穴があります。**payloadが変化し続ける機器では、最初の値しか届きません。** 温度を5秒ごとに更新して放送するセンサービーコンでも、受け取れるのは1回目だけで、以降の更新は「送られてこない」ように見えます。送信側は正常に放送しているので、原因に気づきにくい部類の問題です。

ビーコンの値を追いたいときは、`scanner().start()` へ渡す `EspBleScanConfig` の `wantDuplicates` を `true` にします。この設定はスキャン開始時に反映されるため、動作中に変える場合はスキャンを止めて開始し直してください。

代償は通知の量です。周囲の全機器の全アドバタイズが届くようになるため、処理が追いつかないと取りこぼしが発生します。目的の機器が決まっているなら、絞り込みを先に行ってください。

AppearanceとTx Power Levelも、載っていれば `appearance` と `txPowerLevel`（`hasTxPowerLevel()` で有無を判定）で取り出せます。Tx Powerは特に有用で、**申告された送信電力とRSSIの差が経路損失**になり、距離推定の基礎になります。RSSIだけでは「もともと弱く送っている近くの機器」と「強く送っている遠くの機器」を区別できません。

1件のアドバタイズにService Dataが複数載っていることもあります。その場合は順序に頼らず、`serviceDataFor(uuid, data)` でUUIDを指定して目的のブロックを取り出してください。UUIDは値として比較されるため、16ビットの短縮形で書いても一致します。

#### 関連するexample

| example | 内容 |
|---|---|
| [Gap/Scan](../examples/Gap/Scan/) | アドレス・RSSI・名前を表示する最小のスキャン |
| [Info/ScanDump](../examples/Info/ScanDump/) | 取り出せる全フィールドの表示とiBeaconのデコード |

接続の必要がない用途——ビーコンの受信——は、ここで完結します。

### 2.3 接続 — 1対1の関係を作る

目的の相手が見つかったら**接続**します。接続を開始できるのはCentral側だけです。

#### 接続する前に判断する

見つかった端末すべてに接続してはいけません。BLEの同時接続数には上限があり（EspBleが使う同梱NimBLEのビルドではESP32-S3で3接続）、無駄な接続は資源を奪います。

スキャン結果には判断材料が揃っています。

- **Service UUID** — 目的の機能を持っているか。最も確実な判定基準
- **名前** — 人間が識別しやすい。ただし同名の機器がありうる
- **接続可能フラグ** — ビーコンには接続できない
- **アドレス** — 特定の1台だけを狙う場合
- **RSSI** — 「十分近いものだけ」という条件を付けたい場合

複数を組み合わせるのが実用的です。「このService UUIDを持ち、かつRSSIが-70より強いもの」といった具合です。

#### Peripheral側は接続を拒否できるか

**アプリケーションのコードでは拒否できません。**

BLEには「接続要求が来ました、承認しますか？」という問い合わせの仕組みがありません。接続の可否はコントローラ（無線チップ側）が判断し、アプリケーションが知るのは接続が成立した後です。

制限したい場合の手段は3つあります。

| 手段 | 効果 | 使うAPI |
|---|---|---|
| **Filter Accept List** | 許可リストに載っていない相手の接続要求をコントローラが黙って捨てる。最も確実 | `addToAcceptList()` ＋ `advertising().setFilterPolicy()` |
| **接続後に切断する** | 相手を見て切断する。一度は接続が成立してしまう | `onConnected()` の中で `disconnect()` |
| **属性を暗号化で守る** | 接続は許すが、値の読み書きにペアリングを要求する | Characteristicの `encryptedRead` / `encryptedWrite` |

なお拒否された相手に「拒否された」とは伝わりません。Link Layerに拒否を返すPDUが存在せず、要求が無視されるだけだからです。相手側からは応答のないタイムアウトに見えます。

#### 接続が成立したら

接続すると、以降のやり取りは**その1対1のリンクの中だけ**で行われます。アドバタイズのように周囲へ漏れることはありません。

接続には次のパラメータがあり、通信の応答性と消費電力を決めます。

- **Connection Interval** — 通信機会の周期。短いほど応答が速く、電力を食う
- **Peripheral Latency** — Peripheralが応答をスキップしてよい回数。送るものがないときに電力を節約する
- **Supervision Timeout** — この時間だけ通信が途絶えたら切断とみなす

現在の値は接続情報（`EspBleConnection`）の `connectionInterval` / `peripheralLatency` / `supervisionTimeout` で読めます。変更については2.7節を参照してください。

もう1つ重要なのが**MTU**（Maximum Transmission Unit）です。1回のやり取りで運べるバイト数の上限で、接続時に両者が希望値を交換し、**小さい方**が採用されます。

MTUの仕様上の最小値は23バイトです。このうち3バイトはプロトコルのヘッダが使うため、実際に運べるのは**20バイト**しかありません。EspBleの既定値は247で、244バイトまで1回で送れます。これを超えるデータは分割が必要になります。

交渉結果は `EspBleConnection::mtu`、1回で送れる実バイト数は `maximumNotificationPayload()` で確認できます。

接続が切れると切断イベントが届き、その中で切断理由のコードが分かります。自分から切ったのか、相手が切ったのか、電波が届かなくなった（Supervision Timeout）のかを区別できます。自分から切断するときに相手へ伝える理由コードを `disconnect(id, reason)` の第2引数で指定することもでき、その値がそのまま相手の `disconnectReason` に現れます。

#### 関連するexample

| example | 内容 |
|---|---|
| [Gap/Connect](../examples/Gap/Connect/) | Service UUIDで絞り込んで接続し、接続・切断・失敗を受け取る |
| [Gap/AcceptList](../examples/Gap/AcceptList/) | Filter Accept Listで接続できる相手を制限する |
| [Gap/Mtu](../examples/Gap/Mtu/) | MTUの交換と、1回で送れるサイズの確認 |
| [Info/ConnectionInspector](../examples/Info/ConnectionInspector/) | 接続パラメータやPHYの観察 |

### 2.4 アドレスとプライバシー

アドバタイズには必ず送信元の**アドレス**（6バイト）が載ります。ここに問題があります。

工場出荷時のアドレス（**Public Address**）をそのまま使うと、**その値が変わらないため、周囲の誰でもあなたの機器を追跡できます**。持ち歩く機器では現実的な問題です。

BLEはこれに対して3種類のアドレスを用意しています。

| 種別 | 性質 | 追跡耐性 |
|---|---|---|
| **Public** | 工場出荷の固定値 | なし |
| **Random Static** | 起動時に生成する固定のランダム値 | 出荷アドレスは隠せるが、値自体で追跡できる |
| **Resolvable Private Address（RPA）** | コントローラが定期的に変える | 高い |

RPAは一定時間ごとにアドレスを変えるので、外から見ると別の機器になります。しかしそれでは**正規の相手も見失ってしまいます**。

これを解決するのが**ボンディング**（bonding）です。ペアリング時に**IRK**（Identity Resolving Key）という鍵を交換しておくと、相手はその鍵でRPAを計算し、「これはあのときの機器だ」と復元できます。鍵を持たない第三者には、ただの変化するアドレスにしか見えません。

つまり**RPAはボンディングとセットでのみ意味を持ちます**。ボンディングなしでRPAを使うと、相手は再接続できなくなります。

ボンディング済みの相手を指す不変のアドレスを**Identity Address**と呼びます。Filter Accept Listがアドレスで照合する以上、RPAを使う相手を許可リストに載せられるのは、ボンディングしてIdentity Addressが効くようになってからです。

> **RPAの変更周期は変えられません**
> 同梱NimBLEのビルド設定（`CONFIG_BT_NIMBLE_RPA_TIMEOUT`、900秒）で固定されており、アプリケーションから変更する手段がありません。

自分が今どのアドレスを使っているかは `localAddress()` で取得できます（種別は `localAddressType()`）。相手のFilter Accept Listへ登録してもらう際に必要になる値で、RPAを使っている場合はコントローラが回転させるたびに変わります。

関連するexample: [Gap/PrivateAddress](../examples/Gap/PrivateAddress/)

### 2.5 初期化時に決めること

GAPの締めくくりとして、通信を始める前に決めておく設定をまとめます。これらは初期化時に指定し、以降の通信全体に影響します。

いずれも `EspBleConfig` に指定して `begin()` へ渡します。

| 設定 | 内容 | フィールド |
|---|---|---|
| **デバイス名** | アドバタイズや接続後に相手へ見せる名前 | `deviceName` |
| **希望MTU** | 1回で運べるサイズ。既定247。大きいほど効率的だが、接続ごとにメモリを使う | `preferredMtu` |
| **自分のアドレス種別** | Public / Random Static / RPA（2.4節） | `ownAddressType` |
| **セキュリティ** | ペアリング・ボンディングの有効化と、認証方式 | `security` |

MTUを下げる理由があるとすれば、多数の同時接続でメモリを節約したい場合です。逆に既定の247で困ることは通常ありません。

**送信電力**は `setTxPower(dBm)` で変更でき、実際に適用された値は `txPower()` で読めます。上げれば距離が伸び、下げれば消費電流が減ります。無線が対応するのは飛び飛びの値（同梱ビルドでは-12〜+9 dBmの3 dB刻み）で、指定した値に最も近いものが適用されます。これは初期化時に限らずいつでも変更でき、アドバタイズ・スキャン・接続のすべてに効きます。アドバタイズにTx Power Levelを載せている場合、その値も追従します。

セキュリティには、確認なしでリンクを暗号化する**Just Works**と、6桁の数字で相手を確認する**Passkey認証**があります。目的が「盗聴を防ぐ」だけならJust Worksで足り、「意図しない相手との接続を防ぐ」までを求めるならPasskey認証が必要です。

関連するexample: [Gap/Mtu](../examples/Gap/Mtu/)、[Security/JustWorksServer](../examples/Security/JustWorksServer/)、[Security/StaticPasskeyServer](../examples/Security/StaticPasskeyServer/)

### 2.6 時系列で見る全体像（GAP）

アドバタイズから接続確立までを1本の流れにすると次のようになります。Scan Requestは**Active Scanのときだけ**送られます。

```mermaid
sequenceDiagram
    participant C as Central
    participant P as Peripheral
    Note over P: advertising().start()<br/>以後、聞き手の有無に関わらず<br/>一定間隔で放送し続ける
    Note over C: scanner().start()
    loop 目的の相手が見つかるまで
        P-->>C: アドバタイズ（誰宛でもない放送）
        C->>P: Scan Request（Active Scanのみ）
        P-->>C: Scan Response（残り31バイト）
        Note over C: onResult<br/>UUID・名前・RSSIで<br/>目的の相手か判定
    end
    Note over C: scanner().stop()
    C->>P: 接続要求
    Note over C,P: 接続確立（以後は1対1）
    Note over C,P: 接続パラメータとMTUの交渉
    Note over C: onConnected
    Note over P: onConnected
    Note over C,P: 以降はGATT（3章）
```

接続の必要がないビーコン用途では、`onResult` までで完結します。

### 2.7 GAPで対応していないこと

BLEの仕様にはあるが、EspBleでは使えない機能です。理由もあわせて挙げます。

#### Directed Advertising（送信できない）

**Directed Advertising**は、相手を1台に限定したアドバタイズです。通常のアドバタイズが「誰でもどうぞ」と放送するのに対し、これは**送信先のアドレスを指定**し、その相手だけが接続できます。ボンディング済みの相手へ素早く再接続する用途で使われ、特に**High Duty Cycle Directed Advertising**は3.75ミリ秒間隔で最大1.28秒間送出し、極めて短時間で再接続を成立させます。

EspBleでは**送信できません**。同梱backendのアドバタイズ開始処理が、送信先アドレスの指定箇所を常に「指定なし」で呼ぶ実装になっており、ライブラリ側から送信先を渡す経路がないためです。

代替手段としては、通常のアドバタイズにFilter Accept Listを組み合わせることで「特定の相手だけが接続できる」状態は作れます（2.3節）。再接続の速さは得られませんが、接続先を限定する目的は満たせます。

**受信側の挙動**は次のとおりです。

- 自分宛のDirected Advertisingだけがスキャン結果に届きます。他人宛のものはコントローラが破棄します
- 届いた結果は**アドレス・アドレス種別・RSSIだけ**を持ちます。Directed Advertisingは仕様上ADデータを一切載せられないため、名前もService UUIDもありません
- 接続可能フラグは立ち、スキャン応答可能フラグは立ちません
- そのまま通常どおり接続できます
- ただし**「これはDirected Advertisingだ」と判別する手段はありません**。EspBleがアドバタイズ種別を公開していないためです。「接続可能・スキャン応答不可・データが空」という組み合わせから推測することになります

#### Extended Advertising / Periodic Advertising

BLE 5.0で追加された、255バイトまでのペイロードを扱う仕組み（Extended）と、接続せずに定期的なデータ配信を受ける仕組み（Periodic）です。

**使えません。** 同梱NimBLEが `CONFIG_BT_NIMBLE_EXT_ADV` を無効にしてビルドされており、Arduinoライブラリ側からこの設定を変更する手段がないためです。Periodic AdvertisingはExtended Advertisingの上に成り立つ仕組みなので、同じ理由で使えません。

結果として、アドバタイズは31バイト × 2面（本体とScan Response）が上限になります。

#### スキャン側のFilter Accept List

Filter Accept Listは、アドバタイズ側（誰の接続を受けるか）だけでなく、**スキャン側**（誰のアドバタイズを受け取るか）にも適用できる仕組みが仕様上あります。

EspBleでは**スキャン側には適用できません**。同梱backendのスキャンAPIがフィルタポリシーの設定を公開していないためです。スキャン結果の絞り込みは、受け取った後にアプリケーション側で判定する形になります。

#### 接続時のパラメータ指定

接続を開始する時点でConnection IntervalやPHYを指定することは**できません**。同梱backendの接続APIが指定を受け付けないためです。

ただしこれは実用上の障害にはなりません。**接続はコントローラが決めた値で成立し、そのあとで変更を要求できます**。パラメータは `updateConnectionParameters()`、PHYは `updatePhy()` で要求し、結果はそれぞれ `onConnectionParametersUpdated()` と `onPhyUpdated()` に届きます。要求も結果の受け取りも、どちらの役割からでも行えます。手順は[Gap/ConnectionParameters](../examples/Gap/ConnectionParameters/)を参照してください。

#### アドバタイズチャネルの選択

アドバタイズは3つのチャネル（37・38・39）を使いますが、そのうち一部だけを使う設定は**できません**。同梱backendがNimBLE経路でチャネルマップの設定を公開していないためです。常に3チャネルすべてを使います。

---

## 3. GATT編 — データをやり取りする

接続が成立したら、ここからはGATTの領域です。

### 3.1 GATTの構造

GATTでは、データが3階層で表現されます。

- **Service** — 機能のまとまり。「電池」「心拍計」など
- **Characteristic** — 個々の値。「電池残量」「心拍数」など。Serviceの中に複数入る
- **Descriptor** — Characteristicに付随する補足情報。単位や説明、通知の有効・無効の設定など

それぞれがUUIDという識別子を持ちます（4章で詳しく説明します）。

ただし**UUIDは「型」であって「どれか」ではありません。** 仕様上、同じUUIDのServiceやCharacteristicを1台が複数持てます。HIDキーボードが複数のReport Characteristicを同じUUIDで並べるのが典型例です。

そのためEspBleでは、**登録時に返るハンドルで対象を指定します**。`addService()` がServiceのハンドルを返し、それを `addCharacteristic()` へ渡すとCharacteristicのハンドルが返り、以降の値設定やNotifyはそのハンドルで行います。イベント（書き込み通知や購読状態の変化）にも対象のハンドルが入るので、UUIDが同じでもどれのことか分かります。

Client側は、相手のCharacteristicを**属性ハンドル**で指定できます。同じUUIDのCharacteristicが並ぶHIDのReportを撃ち分けるのはこの方法です。

#### 同一UUIDの重複はどこまで扱えるか

仕様が認めている重複は、**どちらの役割でも扱えます**。

| | 同じUUIDの**Service**を複数持つ | 同じServiceの中に同じUUIDの**Characteristic**を複数持つ |
|---|---|---|
| **Peripheral（公開する側）** | できる | できる |
| **Central（読む側）** | 区別できる（属性ハンドルで指定） | 区別できる（属性ハンドルで指定） |

Peripheral側は、`addService()` / `addCharacteristic()` が返すハンドルが対象を表します。EspBleはBLEスタックのAPI（`ble_gatts_add_svcs()`）で属性テーブルを直接組み立て、読み書きの通知もスタックが渡す「どの定義か」の情報で判別するので、UUIDが同じでも取り違えません。

Central側は、相手がどう重複させていても属性ハンドルで撃ち分けられます。discoveryを`ble_gattc_disc_all_svcs()`などのAPIで自前に行い、read / write / 購読（CCCDへの書き込み）もすべて属性ハンドルに対して直接発行するためです。Notificationも、どのハンドルから来たかで対応付けます。

ひとつだけ制限があります。**再接続時の購読自動復元は、UUIDが一意なCharacteristicに限られます。** 復元は相手のアドレスとUUIDを手掛かりに行うため、同じUUIDが複数あると「どれを購読していたか」を言えないからです。該当する場合は、再接続後に自分でハンドルを指定して購読し直してください。

値のやり取りには次の方法があります。

| 操作 | 向き | 説明 |
|---|---|---|
| **Read** | Client → Server | 値を読む |
| **Write** | Client → Server | 値を書く。応答ありと応答なしがある |
| **Notify** | Server → Client | 値の変化を送りつける。確認応答なし |
| **Indicate** | Server → Client | 同上だが、Clientの確認応答を待つ |

NotifyとIndicateは、Clientが事前に**購読**（subscribe）したものだけが届きます。購読の状態はDescriptorに記録されます。

使い分けの基準は**取りこぼしが許されるか**です。秒間何度も更新されるセンサー値ならNotify（1つ落ちても次が来る）、確実に届けたい設定変更の結果ならIndicateです。

### 3.2 Client側の手順

Clientは相手のデータ構造を知りません。そこでまず**Discovery**（探索）を行い、目的のServiceとCharacteristicがどこにあるかを調べます。その後にRead・Write・購読を行います。

1.3節で説明したとおり、これらはすべて非同期です。「Discoveryを頼む → 完了イベントの中でReadを頼む → 完了イベントの中でWriteを頼む」という連鎖で書きます。

### 3.3 時系列で見る全体像

```mermaid
sequenceDiagram
    participant C as Central（GATT Client）
    participant P as Peripheral（GATT Server）
    Note over C,P: 2章の手順で接続が確立している
    C->>P: Discovery要求
    P-->>C: Service / Characteristic の構成
    Note over C: onCharacteristicDiscovered
    C->>P: Read要求
    P-->>C: 値
    Note over C: onCharacteristicRead
    C->>P: Write要求
    P-->>C: 応答（Write with Responseの場合）
    Note over C: onCharacteristicWritten
    C->>P: 購読の登録
    loop 値が変わるたび
        P-->>C: Notify（確認応答なし）
        Note over C: onNotification
    end
    P-->>C: Indicate（確認応答あり）
    C->>P: 確認応答
```

すべてのイベントは `loop()` の `ble.update()` から配送されます。要求を出した直後ではなく、**次に `update()` を呼んだとき**にコールバックが呼ばれます。

### 3.4 関連するexample

| example | 内容 |
|---|---|
| [Gatt/Basics/Server](../examples/Gatt/Basics/Server/) | 独自ServiceとCharacteristicを公開するServer |
| [Gatt/Basics/Client](../examples/Gatt/Basics/Client/) | Discovery → Read → Write の連鎖 |
| [Gatt/Basics/NotifyServer](../examples/Gatt/Basics/NotifyServer/) / [SubscribeClient](../examples/Gatt/Basics/SubscribeClient/) | Notifyの送出と購読 |
| [Gatt/Basics/IndicateServer](../examples/Gatt/Basics/IndicateServer/) / [IndicateClient](../examples/Gatt/Basics/IndicateClient/) | 確認応答つきのIndicate |
| [Gatt/Basics/NusServer](../examples/Gatt/Basics/NusServer/) / [NusClient](../examples/Gatt/Basics/NusClient/) | シリアル通信に相当するやり取り |

---

## 4. UUIDを理解する

### 4.1 UUIDは「機能の型」を表す名札

ServiceやCharacteristicが何であるかは、**UUID**（Universally Unique IDentifier）で表されます。128ビット（16バイト）の、世界で一意な識別子です。

```
5266f727-49d7-4eaf-a6f1-636f6e6e6563   （8-4-4-4-12桁の16進数）
```

たとえば「電池残量」というCharacteristicには決まったUUIDが割り当てられており、どのメーカーの機器でも同じ値を使います。だから相手の機種を知らなくても「このUUIDを読めば電池残量が分かる」と決め打ちできます。

UUIDは名前ではなく**型（種類）を表す名札**だと考えてください。

### 4.2 標準UUIDと独自UUID

- **標準UUID** — Bluetooth SIGが割り当てたもの。電池、心拍計、HIDなど、仕様で決まった機能に対応する
- **独自UUID** — 自分のアプリ専用。ランダムに128ビットを生成して使う

### 4.3 フル形と短縮形

標準UUIDには**16ビットの短縮形**があります。たとえば電池サービスは `180F` です。

これは次の**Base UUID**の中に短縮形を差し込んだ128ビットUUIDの、省略表記にすぎません。

```
Base UUID:  0000____-0000-1000-8000-00805F9B34FB
                ↑ここに16ビット短縮形が入る
180F の実体: 0000180F-0000-1000-8000-00805F9B34FB
```

つまり**短縮形とフル形は同じUUIDを指す別表記**です。EspBleはUUIDを**値として**比較する（内部で短縮形をBase UUIDへ展開する）ので、`180F` と `0000180f-0000-1000-8000-00805f9b34fb` はどちらで書いても同じ相手に一致します。大文字小文字も区別しません。

ただし**文字列としては別物**です。スキャン結果に入っているUUIDを自分で文字列比較するのではなく、値として比較する仕組みを使ってください。

### 4.4 気をつける点

1. **独自サービスは必ず128ビットのフル形で書く。** 短縮形はSIGが割り当てた標準UUID専用の表記です。自作サービスに勝手に16ビット値を使ってはいけません。
2. **桁とハイフンの位置は正確に。** 大文字小文字は無視されますが、`8-4-4-4-12` の形が崩れると別のUUIDになります。
3. **アドバタイズに出ていない＝そのServiceが無い、ではない。** 31バイトの制限のため、Service UUIDを全部載せられるとは限りません。確実に知りたいときは接続後のDiscoveryで確認します。
4. **Service UUIDを一切載せない機器もある。** iBeaconのようにManufacturer Dataだけの機器は、UUIDでは絞り込めません。アドレスやManufacturer Dataの中身で判定します。
5. **短縮形が意味を持つのはSIG登録済みの値だけ。** 未登録の16ビット値に決まった意味はありません。
