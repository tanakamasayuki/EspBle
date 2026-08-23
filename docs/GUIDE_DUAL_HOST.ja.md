# BLE / Bluetooth Classic Dual Host技術ガイド

> English: [GUIDE_DUAL_HOST.md](GUIDE_DUAL_HOST.md)

このガイドは、無印ESP32の1つのBluetooth controllerを、BLE用のNimBLE hostと
Bluetooth Classic用のBluedroid hostで共有する仕組みを説明します。対象は、Bluetoothの内部を
初めて見る人から、Dual Hostの不具合を追う人までです。

最初にControllerとHostの役割を分け、通常の1 Host構成を理解してから、2 Hostで新しく発生する
問題とEspBleの解決方法を1つずつ積み上げます。単に「パケットをBLEとClassicへ振り分ける」だけでは
足りない理由が、このガイドの中心です。

> **先に知っておくこと:** Dual Hostが使えるのはBluetooth Classicを搭載する**無印ESP32だけ**です。
> `EspBle`と`EspBleClassic`の両方を`begin()`すると自動的に有効になります。build flagはありません。
> 現在は実験扱いです。不安定な場合の退避手段は、一方を`end()`して単一Hostに戻すことです。

## 1. まず5つの層を分ける

Bluetoothの説明では「Host」という語が複数の意味で使われます。ここを曖昧にすると、その後の
設計が分からなくなります。

| 層 | 役割 | EspBleでの例 |
|---|---|---|
| Application | 製品固有の動作を決める | Arduino sketch、`setup()`、`loop()` |
| Profile / protocol | データの意味と手順を決める | GATT、SMP、HID、SPP、A2DP、HFP |
| Bluetooth Host | 接続、security、profile、論理的なBluetooth状態を管理するsoftware | BLE側のNimBLE、Classic側のBluedroid |
| HCI transport | HostとControllerの間でcommand、event、dataを運ぶ | EspBle HCI brokerとESP32 VHCI |
| Bluetooth Controller | Link Layer/Baseband、packet timing、暗号処理、radioを制御する | 無印ESP32内蔵BTDM controller |

全体は次のように重なります。

```text
Arduino sketch
  │
  ├─ EspBle API ───────── GAP / GATT / SMP / BLE HID / BLE MIDI
  │                         │
  │                       NimBLE Host
  │                         │ logical HCI
  │
  └─ EspBleClassic API ─── Inquiry / SPP / HID / A2DP / AVRCP / HFP
                            │
                     Classic-only Bluedroid Host
                            │ logical HCI
                            ▼
                    EspBle HCI Broker
                            │ physical VHCI (H4 packets)
                            ▼
                 ESP32 BTDM Controller + Radio
```

このガイドでいう**Host**はNimBLEまたはBluedroidです。次の用語とは別物です。

- **HID Host**: keyboardやmouseからreportを受け取るprofile上の役割です。Bluetooth Hostの中で動きます。
- **ESP-Hosted Host**: P4＋C6構成でC6を操作する側のchipです。このガイドのDual Hostとは関係ありません。
- **Central / Peripheral**: BLE接続を開始する側／受ける側です。どちらも同じNimBLE Hostで動けます。
- **GATT Client / Server**: attributeを操作する側／提供する側です。Central / Peripheralとも独立した役割です。

## 2. 通常の1 Host構成

1つのHostだけなら経路は単純です。例としてBLE接続を開始するとき、NimBLEはHCI commandをControllerへ
送り、Controllerはradioで接続処理を行い、結果をHCI eventでNimBLEへ返します。接続後のGATT dataは
ACL data packetとして往復します。

HCIを通る主なpacketは次の4種類です。

| H4 packet | 向き | 内容 |
|---|---|---|
| Command (`0x01`) | Host → Controller | scan開始、接続、暗号化、切断などの要求 |
| ACL Data (`0x02`) | 双方向 | BLEのL2CAP/ATT/SMP、ClassicのL2CAP/profile data |
| SCO Data (`0x03`) | 双方向 | Classic HFPの音声data |
| Event (`0x04`) | Controller → Host | command応答、接続・切断、暗号状態、受信通知など |

単一Hostは「Controllerのcommand creditもconnection handleも、すべて自分のもの」と考えて構いません。
EspBleのbrokerも、登録Hostが1つならこの前提を変えないpass-throughとして動きます。

## 3. なぜ2つのHostを直接つなげないのか

無印ESP32のBTDM ControllerはLEとBR/EDR（Classic）の両方を同時に実行できます。しかし、
Controllerが2つの独立したHostを理解してくれるわけではありません。VHCIには物理的な入口と出口が
1つずつしかなく、Controllerから見ると相手は1 Hostです。

NimBLEとBluedroidをそのまま同じVHCIへつなぐと、少なくとも次が衝突します。

1. **HCI commandを同時に送れる。** Controllerのcommand creditは両Hostで共有です。
2. **command応答の宛先が分からない。** `Command Complete`にはopcodeはありますが、NimBLE/Bluedroidの印はありません。
3. **共通eventがある。** 切断、暗号変更、ACL送信完了はLEにもClassicにも現れます。
4. **Controller全体の設定を上書きする。** Reset、event mask、flow controlは一方だけの設定ではありません。
5. **同じbuffer poolを消費する。** 片方だけがcreditを返しても、もう片方のtrafficで共有bufferが枯れます。
6. **停止責任が競合する。** 先に終了したHostがControllerを止めると、もう一方の接続も切れます。

つまり必要なのはpacket typeだけを見るdemultiplexerではなく、**共有Controllerの所有者として状態を
管理するbroker**です。

## 4. EspBleが2つのHostを用意する方法

無印ESP32のArduino Coreは通常Bluedroidを同梱しますが、EspBleのBLE側はNimBLEを使います。
さらにClassic用にも独立したHostが必要です。EspBleは次の2つを持ち込みます。

| 用途 | Host | 配布形態 |
|---|---|---|
| BLE | EspBle同梱NimBLE | `src/nimble_esp32/`のsource |
| Classic | Classic-only Bluedroid | `src/esp32/libespble_bluedroid_classic.a` |

Classic archiveはControllerを含まない**Host-only / Classic-only**構成です。公開symbolを
`espble_bd_`へ名前空間化しているため、Arduino Core側のsymbolと衝突しません。Bluedroidは
`esp_bluedroid_attach_hci_driver()`に相当する境界から、物理VHCIではなくbrokerへ接続されます。
NimBLEのESP32 transportも同じようにbrokerをlogical Hostとして利用します。

これにより、2つのHostは互いの内部実装を知らず、brokerだけが共有Controllerを知る構成になります。

### 4.1 どこまでがsourceで、どこだけが`.a`か

配布物は意図的に混在しています。

- **EspBle本体**（公開API、profile、HCI broker、router、scheduler、policy、ACL credit処理）はsourceです。
  Arduino build時にsketchと一緒にcompileされます。
- **BLE HostのNimBLE**も`src/nimble_esp32/`にsourceで同梱します。broker transport、Controllerを
  起動せず既存Controllerへattachする経路、無印ESP32固有の修正をsourceと生成scriptで追跡します。
- **Classic HostのBluedroidだけ**は、事前build済みの
  `src/esp32/libespble_bluedroid_classic.a`です。これはControllerを含まないClassic-only Hostです。
- **BTDM Controller**はEspBleのsourceでも`.a`でもなく、Arduino-ESP32 Coreが提供する無印ESP32向けの
  prebuilt Controllerを使います。brokerはCoreのVHCI APIを介して接続します。

```text
sketchと一緒にcompile                       事前に固定環境でbuild
┌──────────────────────────────┐          ┌─────────────────────────┐
│ EspBle / EspBleClassic API   │          │ Classic-only Bluedroid  │
│ HCI broker一式               │          │ namespaced static .a    │
│ bundled NimBLE source        │          └────────────┬────────────┘
└──────────────┬───────────────┘                       │ link
               └──────────────────┬────────────────────┘
                                  ▼
                         Arduino sketchのELF
                                  │ VHCI
                                  ▼
                     Core同梱prebuilt BTDM Controller
```

### 4.2 なぜ両方を同じ形式にしないのか

NimBLEはlocal patchとtarget別条件を追いやすく、必要な修正を再生成scriptへ記録できるためsource配布が
適しています。一方BluedroidはESP-IDFのKconfig、生成header、複数componentへの依存が強く、そのまま
Arduino buildへ持ち込むと構成の再現とbuild時間の負担が大きくなります。そのため、ESP-IDF v5.5.5 / 
GCC 14.2.0の固定環境でHost-only / Classic-onlyとしてbuildし、static archiveにしています。

archive内のdefined symbolは`espble_bd_`名前空間へ変換します。これにより、Arduino Coreが持つ
Bluedroidと名前が衝突せず、EspBleClassicの呼び出しが誤ってCore側Hostへ解決されることも防ぎます。
FreeRTOS、NVS、timer、loggingなど安定した外部依存は変換せず、最終link時にArduino Coreから解決します。

`.a`全体がそのままflashへ入るわけではありません。linkerは使用profileに必要なarchive memberだけを
取り込みます。ただしarchiveは固定したESP-IDF / compiler ABIを前提とするため、対応Core範囲の実測、
必須symbolのlink check、clean環境からの再生成とSHA-256一致をrelease gateにしています。再生成方法は
[Classic-only Bluedroid host archiveの再生成](CLASSIC_HOST_BUILD.ja.md)を参照してください。

## 5. `begin()`からDual Hostになるまで

起動順はBLE→ClassicでもClassic→BLEでも構いません。内部では次の順序で進みます。

### Step 1: 将来Classicを使う可能性を起動前に知らせる

Classic Hostがsketchへlinkされていることを統合層がbrokerへ知らせます。これは、Controller memoryの
解放が取り消せないためです。BLEを先に開始してClassic用memoryを解放してしまうと、後からClassicを
開始できません。

### Step 2: 最初のHostがControllerを起動する

最初のHostは必要なmodeでControllerを初期化し、停止責任をbrokerへ移します。

- BLEだけのsketch: BLE mode
- Classicだけであると確定できるsketch: Classic mode
- 両方がlinkされるsketch: BTDM mode（BLE＋Classic）

Classicをlinkしただけでまだ`begin()`していなくても、後から開始できるようBTDM memoryを保持します。
その分、BLE専用sketchよりheap使用量が増えます。

### Step 3: 1つ目のHostをpass-through登録する

brokerは物理VHCI callbackを1つだけ登録します。最初のlogical Hostはsingle-host pass-throughとして
登録されます。この時点でも、2つ目のHostが非同期初期化中に入ってきてもよいよう、commandとhandleの
状態は記録します。

### Step 4: 2つ目のHostをattachする

2つ目は既に動いているControllerを再初期化せず、Host部分だけをattachします。brokerはcommand配送taskを
用意し、routed modeへ移ります。同時にController→Host ACL flow controlの所有権もbrokerへ移ります。

### Step 5: 両方が独立してprofileを動かす

以後、NimBLEはBLEだけ、BluedroidはClassicだけを管理します。sketchから見ると通常どおり両objectを
使い、両方の`update()`を呼ぶだけです。

```cpp
#include <EspBle.h>
#include <EspBleClassic.h>

EspBle ble;
EspBleClassic classic;

void setup() {
  EspBleConfig bleConfig;
  bleConfig.deviceName = "Dual Device";

  EspBleClassicConfig classicConfig;
  classicConfig.deviceName = "Dual Device";

  if (!ble.begin(bleConfig)) {
    Serial.println(ble.lastErrorDetail());
    return;
  }
  if (!classic.begin(classicConfig)) {
    Serial.println(classic.lastErrorDetail());
    ble.end();
    return;
  }

  // ここでBLE advertising / GATTとClassic profileを通常どおり開始する。
}

void loop() {
  ble.update();
  classic.update();
}
```

順序を逆にしても構いません。build flagやDual Host専用の`begin()`はありません。

## 6. HostからControllerへ: commandを安全に直列化する

### Step 1: commandの権限を確認する

Dual Host中のHCI commandは、Controllerへ送る前にscopeで分類されます。

| Scope | 例 | 許可される送信元 |
|---|---|---|
| shared read | local version、supported features、BD_ADDR | 両方 |
| NimBLE radio / connection | LE scan、LE advertising、LE encryption | NimBLE |
| Classic radio / connection | inquiry、scan mode、pairing、SCO設定 | Bluedroid |
| shared connection | disconnect、remote version、RSSI | handleの所有Host |
| controller merged | General / Page 2 / LE event mask | 両方の要求を統合 |
| controller virtual | Reset、Host Buffer Size、flow-control設定 | brokerが仮想応答または代理実行 |
| host credit | Host Number Of Completed Packets | brokerが所有 |

観測・分類されていないopcode、別Host専用のopcode、他方が所有するhandleへのcommandは、物理送信前に
拒否されます。このpolicyは**fail closed**です。新しいprofileを追加した直後にDual Hostだけ失敗する
場合、必要なopcodeが未分類である可能性があります。単一Host pass-throughの動作は変えません。

### Step 2: Controller全体のcommandを統合または仮想化する

両Hostが`Set Event Mask`を別々に送ると、後から送ったmaskが前のHostに必要なeventを消してしまいます。
brokerはGeneral、Page 2、LEの3種類をHost別にcacheし、bitwise ORしたunionをControllerへ送ります。

生きているLE linkがある状態で、再attachしたBluedroidが`HCI Reset`を送ると全接続が失われます。
そのためResetはControllerへ送らず、要求したHostだけへ成功`Command Complete`を返します。Host Buffer
SizeとController→Host flow-control設定も同様にbroker管理へ置き換えます。

### Step 3: FIFOへcopyする

commandはbroker所有の16 entry FIFOへpacket全体をcopyします。呼び出し元のbuffer寿命には依存しません。
FIFOが満杯なら`ESP_ERR_NO_MEM`で拒否し、診断counterを増やします。

### Step 4: 1 transactionずつ物理送信する

専用taskはControllerの`Num_HCI_Command_Packets` creditとVHCIの送信可能状態を確認し、保守的に
1 commandだけをin-flightにします。送信時に`opcode + owner`を記録します。

### Step 5: 応答を要求元だけへ返す

Controllerから`Command Complete`または`Command Status`が戻ると、opcodeをin-flight transactionと
照合し、要求したHostだけへ配送します。opcodeが一致しなければ
`command_response_mismatch`へ記録します。

## 7. Data packetはconnection handleで守る

HCI ACL headerには12-bitのconnection handleがあります。Controllerが接続成功を通知した時点で、brokerは
handleと所有Hostを表へ登録します。

| Controller event | 登録する所有者 |
|---|---|
| LE Connection Complete / LE Enhanced Connection Complete | NimBLE |
| BR/EDR Connection Complete | Classic Bluedroid |
| Synchronous Connection Complete（SCO/eSCO） | Classic Bluedroid |

Host→ControllerのACLは、送信Hostがそのhandleを所有するときだけ通します。Controller→HostのACLも
handle表から1つのHostだけへ配送します。Disconnection Completeを配送した後、そのhandleを表から削除します。

SCO packetはClassicへ、ISO packetはNimBLEへ限定します。現在のhandle表の上限は16接続です
（`ESPBLE_HCI_ROUTER_MAX_CONNECTIONS`）。

## 8. ControllerからHostへ: eventを分類する

eventは次の規則でroutingされます。

| Eventの種類 | 配送先 |
|---|---|
| LE Meta Event | NimBLE |
| Classic固有の非同期event | Classic Bluedroid |
| Command Complete / Status | commandを送ったHost |
| 切断、認証、暗号変更、remote feature/version、mode change | handleの所有Host |
| Hardware Error / Data Buffer Overflow | 両方 |
| Number Of Completed Packets | handleごとに分割・再構成して各Host |

最後のeventは特に重要です。1つの物理eventにLE handleとClassic handleが混在することがあります。
brokerはそのpacketをそのままbroadcastせず、各Hostが所有するrecordだけを含む別eventへ組み直します。

「LEらしいeventはNimBLE、それ以外はClassic」という規則だけでは、切断や暗号化など共通eventを
正しく扱えません。connection handle表が必要になる理由はここにあります。

## 9. 2方向のACL flow control

flow controlは向きを分けて考える必要があります。

### Host → Controller

2 HostはController側のACL送信bufferを共有します。Controllerは`Number Of Completed Packets` eventで
空いたbufferを通知します。EspBleはeventをhandleごとに分割して各Hostへ返しますが、送信bufferそのものを
NimBLE用とClassic用に公平配分してはいません。片方が大量送信すると、もう片方が一時的に待つことがあります。

### Controller → Host

ControllerがHostへACLを渡す方向では、Hostが処理済みbufferを
`Host Number Of Completed Packets` commandで返却します。しかし共有時には次の不整合があります。

- Classic Bluedroidは、自分へroutingされたClassic ACL分しか返せない。
- 同梱NimBLEは、このController→Host creditを返さない。
- Controllerのbuffer poolは両方で共通なので、LE trafficだけでも最終的にpool全体が枯れる。

そこでbrokerがこのloopを所有します。Controllerの`Read Buffer Size`応答からbuffer構成を覚え、両Hostへの
ACL配送をhandle別に数え、broker自身がcredit commandを生成します。通常は4 packetをthresholdとして
まとめますが、queueが空になると残りもflushし、少数のcreditを置き去りにしません。切断時はController自身が
bufferを解放するため、そのhandleの未返却creditを破棄します。

これがないと、しばらくは動いても共有bufferが尽きた時点でBLEとClassicの両方が止まります。

## 10. 起動・停止・再attach

Controllerは両Hostより寿命が長い共有資源です。brokerが次の規則を守ります。

1. Controllerを起動したHostは停止責任をbrokerへ渡す。
2. 一方を`end()`しても、もう一方が登録中ならControllerを止めない。
3. 最後のHostが解除されたときだけControllerをdisable / deinitする。
4. 解除したHostの未送信commandをFIFOから除き、そのHostのevent mask要求も削除する。
5. session generationを更新し、旧sessionから遅れてきたcommandを再起動後のControllerへ送らない。
6. callbackのreceive gateを閉じ、停止済みHostへ遅延eventを配送しない。

NimBLEはHost taskの準備が完了するまでController eventを受け取れないため、登録直後のreceiveは明示的に
gateされています。Classicはprofile初期化が非同期なので、1 Hostから2 Hostへ切り替わる境界でも
既に発行済みのcommand状態を維持します。

Classicを一度停止し、BLE接続を維持したまま再開することもできます。このときBluedroidのbootstrapが
要求するResetとflow-control commandを仮想完了することで、生きているBLE linkを壊さずHostだけを
再attachします。

## 11. 共有されるもの、分かれているもの

| 共有されるもの | Hostごとに独立するもの |
|---|---|
| 1つのradioとBTDM Controller | BLEのGAP / GATT / SMP状態 |
| HCI command credit | ClassicのGAP / SPP / HID / Audio状態 |
| ControllerのACL/SCO buffer | BLE bondとClassic bond |
| event maskの物理設定 | connectionとprofileのapplication callback |
| heap、CPU時間、無線時間 | `EspBle::update()`と`EspBleClassic::update()`のevent queue |

BLEとClassicのbondは別の鍵・別の記録です。片方のbondを削除しても、もう片方は削除されません。
同時利用できることは、radio timeやheapが2倍になることでも、無制限に並行できることでもありません。

## 12. 診断の読み方

`EspBleHciBroker.h`の`espble_hci_broker_get_diagnostics()`で、現在のController sessionに対するsnapshotを
取得できます。このheaderは内部検証用で、Dual Host自体と同じく実験的な境界です。

```cpp
#include <EspBleHciBroker.h>

espble_hci_broker_diagnostics_t d = {};
espble_hci_broker_get_diagnostics(&d);
Serial.printf("qmax=%u qfull=%lu mismatch=%lu unknown=%lu credits=%lu/%lu\n",
  d.command_queue_high_water,
  static_cast<unsigned long>(d.command_queue_full),
  static_cast<unsigned long>(d.command_response_mismatch),
  static_cast<unsigned long>(d.unknown_acl),
  static_cast<unsigned long>(d.acl_credits_returned),
  static_cast<unsigned long>(d.acl_credits_dropped));
```

| 値 | 意味 | 正常時の見方 |
|---|---|---|
| `command_enqueued[]` / `command_sent[]` | Host別のFIFO投入数／物理送信数 | traffic収束後は一致する |
| `command_queue_high_water` | FIFOの最大使用数 | 16未満には余裕がある |
| `command_queue_full` | FIFO満杯による拒否回数 | 通常は0 |
| `command_response_mismatch` | 応答opcode不一致 | 0であるべき |
| `command_unregister_busy` | 応答待ちのままHostを解除した回数 | 通常は0 |
| `unknown_acl` | 所有者不明handleの受信ACL | 0であるべき |
| `tx_acl[]` / `rx_acl[]` / `completed_acl[]` | Host別ACL統計 | 停止や偏りの場所を探す |
| `event_mask_unions` | event maskをunionへ書き換えた回数 | Dual Hostでは発生してよい |
| `virtual_resets` | Resetを物理送信せず完了した回数 | Classic再attachで発生してよい |
| `acl_flow_control_owned` | brokerの受信credit管理が有効か | routed sessionでは1 |
| `acl_credits_returned` / `dropped` | 返した／handle消失等で破棄したcredit | `dropped`増加は切断と合わせて見る |

症状別には次の順で確認します。

1. **2 Hostとも止まる:** `command_queue_full`、`mismatch`、ACL credit、heapを確認する。
2. **一方だけdataが来ない:** `unknown_acl`とHost別`rx_acl`、接続handleの生成・切断eventを確認する。
3. **新profileがDual Hostだけ失敗:** logの`unclassified` / `wrong host` opcodeを確認する。
4. **一方の`end()`でもう一方が切れる:** 最後のHost判定とController ownershipのlogを確認する。
5. **長時間後に止まる:** credit返却数、application側queueのdrop、free heapを時間軸で記録する。

## 13. 現在の制限

- 無印ESP32専用です。他のESP32 SoCはBluetooth Classicを持ちません。
- Dual Hostは実験扱いです。外部機器との相互運用はEspBle同士・Core host相手ほど広く検証されていません。
- Host→Controller方向のACL bufferを2 Host間で按分しません。大量転送時の公平性は保証しません。
- HCI command policyはfail closedです。新しいopcodeは分類と検証を追加するまでDual Hostでは拒否されます。
- ClassicをlinkしたsketchはBTDM memoryを保持するため、BLE専用よりheapが減ります。
- radio、heap、CPU、各application callback queueは共有資源です。application側にも上限のあるqueueとdrop監視が必要です。
- precompiled Classic Hostの実測対応CoreはArduino-ESP32 3.2.0〜3.3.11です。HFP audioは3.3.8以上が必要です。

不具合を切り分ける最初の操作は、一方を`end()`して単一Hostに戻すことです。単一Hostで再現しなければ、
broker policy、共有resource、起動停止境界を優先して調べます。

## 14. どこまで検証しているか

実機Peer testでは、次をBLE linkを維持した状態で確認しています。

- Classic HIDの双方向reportと反復GATT read
- BLE pairing、bond保存、暗号化必須GATT、RPA再接続
- 両Hostからのcommand競合、FIFO満杯後の復旧
- LE / BR-EDR同時切断、任意順の`end()`・destructor・再起動
- Classic Hostの再attachとheap不変の反復
- HFP SLC、mSBC SCO双方向dataとSCO中のGATT
- A2DP SBC media、AVRCP操作とstream中のGATT
- peer突然消失、誤passkey、接続失敗からの復旧

代表suiteは`tests/peer/dual_host_smoke/`、`dual_host_rpa/`、`dual_host_hfp/`、
`dual_host_a2dp/`です。長時間検証と判定値は
[技術検証記録](TECHNICAL_VALIDATION_ESP32_CLASSIC.ja.md)にあります。

## 15. 実装を読む順序

| 順序 | ファイル | 見るもの |
|---:|---|---|
| 1 | [`EspBleHciBroker.h`](../src/EspBleHciBroker.h) | logical Host APIとdiagnostics |
| 2 | [`EspBleHciRouter.c`](../src/EspBleHciRouter.c) | H4解析、command応答、handle/event routing |
| 3 | [`EspBleHciCommandScheduler.c`](../src/EspBleHciCommandScheduler.c) | 16 entry FIFO、command credit、in-flight transaction |
| 4 | [`EspBleHciControllerPolicy.c`](../src/EspBleHciControllerPolicy.c) | opcode分類、event mask union、仮想command |
| 5 | [`EspBleHciAclCredits.c`](../src/EspBleHciAclCredits.c) | Controller→Host ACL creditの集計とcommand生成 |
| 6 | [`EspBleHciBroker.c`](../src/EspBleHciBroker.c) | VHCI、FreeRTOS task、全componentの統合 |
| 7 | [`EspBleClassic.cpp`](../src/EspBleClassic.cpp) | namespaced BluedroidのattachとController起動順 |
| 8 | [`esp_nimble_hci.c`](../src/nimble_esp32/src/esp-idf/esp_nimble_hci.c) | NimBLE transportからbrokerへの接続 |

Router、scheduler、policy、ACL credit計算はESP-IDFに依存しない純粋Cへ分離されています。物理Controllerが
なくてもunit testでき、brokerだけがVHCI、FreeRTOS、ESP-IDFを知ります。Arduino固有のlink状態や
`begin()`は統合層に閉じ込めています。

## 16. 最後にもう一度、全体の流れ

Dual Hostを次の順で考えると整理できます。

1. ESP32にはradioを動かす**Controllerが1つ**ある。
2. BLEのNimBLEとClassicのBluedroidは、別々の**Host**である。
3. 2 HostはControllerのcommand、handle、buffer、設定、寿命を共有する。
4. brokerが唯一の物理VHCI所有者になり、共有状態を一元管理する。
5. commandは分類・直列化し、応答を要求元へ戻す。
6. dataと共通eventはconnection handleで所有Hostへ戻す。
7. flow controlとController lifecycleは、どちらのHostにも任せずbrokerが所有する。
8. applicationは両objectを通常どおり使い、両方の`update()`を呼ぶ。

関連する利用上の判断は[BLEとBluetooth Classicのどちらを使うか](CLASSIC_VS_BLE.ja.md)、
各profileの概念は[BLE通信の入門ガイド](GUIDE_BLE_BASICS.ja.md)と
[Bluetooth Classic通信の入門ガイド](GUIDE_CLASSIC_BASICS.ja.md)、負荷時の全般的な挙動は
[EspBleを深く使う](GUIDE_ADVANCED.ja.md)を参照してください。
