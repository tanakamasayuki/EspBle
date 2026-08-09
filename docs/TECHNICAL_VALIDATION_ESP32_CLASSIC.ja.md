# 無印ESP32 NimBLE / Classic共存 技術検証

検証日: 2026-08-09

## 結論

無印ESP32のBTDM controllerはBLEとClassicを同時に動かせる。Classic-onlyのBluedroid hostも
controllerから分離してビルドできる。そのため、EspBleへClassicを追加する際の第一候補は、
NimBLEとClassic-only BluedroidをHCI brokerへ接続する構成とする。

ただし、VHCIのH4 byte streamをパケット種別だけで二分することはできない。command credit、
command応答、connection handle、ACL credit、controller初期化をbrokerが一元管理する必要がある。
single-host pass-throughを基準にした後、opt-inの
`ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL`としてH4 routerまで実装した。Classic HIDの双方向通信を
維持したままLE接続、GATT read反復、負荷後のClassic HID双方向通信が成立している。command schedulerは
broker所有FIFOとcontroller credit管理、最後のhostがcontrollerを停止するlifecycle、event maskのunion、
Classic再attach時のflow-control command仮想化まで実装した。Classic HID接続中のBLE pairing、bond保存、
bond再接続、暗号化必須GATT readも成立した。ただし未分類のcontroller-wide commandと長時間負荷は残るため、
通常buildは引き続き二つ目のhost登録を`ESP_ERR_NOT_SUPPORTED`とする。

## 実機・ビルド検証結果

| 検証 | 結果 | 確認できたこと |
|---|---:|---|
| 変更前 ESP32 × ESP32-S3 `gatt_read_write` | 2 passed / 78.55秒 | 変更前の基準 |
| broker経由 ESP32 × ESP32-S3 `gatt_read_write` | 2 passed / 67.13秒 | HCI TX/RXをbroker経由にしてもGATT read/writeが成立 |
| broker経由 ESP32 × ESP32-S3 `lifecycle_stress` | 8 passed / 106.98秒 | `begin()`/`end()`、再接続、event floodを含む反復 |
| broker経由 ESP32 × ESP32-S3 `security_bond` | 1 passed / 68.66秒 | SMP、暗号化、bond保存経路 |
| EspBleBluedroid ESP32 dual-mode `dual_mode_scan_spp` | 1 passed / 73.26秒 | active SPPとBLE scan/GATT/notificationの同時動作 |
| ESP32 / ESP32-S3 compile smoke | 成功 | target guardが両構成で成立 |
| 独自Classic HID + NimBLE GATT同時Peer | 1 passed | 同じBTDM controllerでBR/EDR ACLとLE ACL、単発GATT readが共存 |
| dual-host ACL反復 | 1 passed | GATT read 25回後もClassic HID双方向が継続。両側LE ACL tx/rx/completed=36/36/36、unknown handle 0 |
| dual-host security / bond再接続 | 1 passed / 66.93秒 | Classic HID接続中に両側pairing・bond保存、再接続後の暗号化必須read、`encrypted=1 bonded=1 key=16`を確認 |
| dual-host controller command inventory | 1 passed | NimBLE 19種、Classic 32〜34種を実機記録。再attach時Reset / flow-control設定を仮想化し、overflow・opcode不一致0 |
| dual-host同時切断 | 1 passed / 106.05秒（clean） | LE / BR-EDR Disconnectを連続発行し、両handleの完了を正しいhostへ配送後、停止・再起動・destructor成功 |
| HCI router unit | 1 passed | opcode応答、handle所有、ACL、切断、mixed completed eventの分割 |
| ESP-IDF Classic-only host spike | build/link成功 | controllerなし、BLEなし、SPPありのBluedroid hostを外部HCIへattach可能 |
| EspBle unit test | 7 passed / 1.92秒 | 既存host非依存ロジックの回帰なし |

Classic-only host spikeはESP-IDF `v6.1-dev-6931-g08e0d30a74a`で、次の設定を用いた。

```text
CONFIG_BT_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED=y
CONFIG_BT_CONTROLLER_DISABLED=y
CONFIG_BT_CLASSIC_ENABLED=y
CONFIG_BT_SPP_ENABLED=y
# CONFIG_BT_BLE_ENABLED is not set
```

`esp_bluedroid_hci_driver_operations_t`へdummy transportを与え、
`esp_bluedroid_attach_hci_driver()`から`esp_bluedroid_init()`までを最終ELFへリンクした。
ELFには`esp_bluedroid_attach_hci_driver`、`esp_bluedroid_init`が存在し、
`esp_bt_controller_init`は存在しない。生成binは428,448 bytesだった。このhost-only APIは
EspBleが現在基準にするESP-IDF v5.5.5にも存在するが、今回の完全なビルド検証はmasterで行った。

## 今回導入した境界

`EspBleHciBroker`だけが物理VHCI callbackを登録する。vendored NimBLE transportは、VHCIを
直接呼ばず、logical hostとしてbrokerの次の最小APIを使う。

- host callbackの登録・解除
- controllerが送信可能かの確認
- H4 packetの送信
- H4 packetとsend-available通知の受信

このファイルは`CONFIG_IDF_TARGET_ESP32 && !CONFIG_NIMBLE_ENABLED`でのみ実体を持つ。
ESP32-S3などcore内蔵NimBLEを使うtargetでは既存transportを変更せず、brokerもリンクされない。
vendored NimBLEの再生成で変更を失わないよう、書き換えは`tools/vendor_nimble_esp32.py`へ記録した。

dual-host parserとcommand scheduler stateはcritical sectionで保護する。callback登録解除とcallback実行の完全な
lifecycle同期は今後の作業である。commandは`send()`で16 packetのbroker所有FIFOへcopyし、専用taskが
controller creditを見て1 transactionずつ送る。`can_send()`時点のslot予約は
Bluedroidの先読み確認と両立しないことを実機で確認したため、送信queueは`send()`でpacketを
broker所有メモリへcopyする境界として実装する。FIFOだけでは反復負荷時のClassic ACL停滞を
解消できなかった。HCI traceとbroker counterにより、Classic Bluedroidが有効化した
controller→host flow controlに対し、NimBLEへroutingしたLE ACLのhost creditが返らず、20 packetで
共有bufferが枯渇することを特定した。現在のdual-host実験buildは
`Set Controller To Host Flow Control`を無効へ正規化する。一般対応ではbroker自身が両hostの
incoming ACL処理完了を数え、`Host Number Of Completed Packets`を一元生成する。

command scheduler導入後の完全再ビルド試験では、DUTがNimBLE / Classic commandを20 / 44件、
Peerが16 / 41件送信した。全件でFIFO投入数と物理送信数が一致し、最大queue深度は3 / 1、
queue overflowと応答opcode不一致は両側0だった。同じ試験中のACLは両側とも
LE tx/rx/completed=36/36/36、Classic tx/rx/completed=25/25/25で、unknown handleは0だった。
負荷後は両側ともNimBLE hostを先に、Classic host/controllerを後に停止でき、両hostの
`initialized()`がfalse、host解除時のin-flight commandは0だった。queueはhost解除時にowner単位で
破棄し、専用送信taskはsession世代とFIFO先頭を物理送信直前に再照合するため、前sessionからcopyした
commandを再登録後のcontrollerへ送らない。再登録の長時間soakは未検証である。
同じ実機試験で、停止後に同一の`EspBle` / `EspBleClassic` instanceとGATT/HID定義を使って
Classic→NimBLEを再登録し、両hostの初期化成功と正常停止を通常3サイクル、拡張実行20サイクルで
確認した。20サイクル実行ではbaseline、各サイクル、destructor後の計22点でheapを計測し、DUTは
free 187692 byte、peerはfree 188192 byteで全測定値が同一だった（観測分解能上の漏れ0 byte）。
DUTのminimum free / largest blockは84144 / 73716 byte、peerは71124 / 59380 byteだった。数時間級の長時間soakは
引き続き未検証である。
その後、heap減少許容値を0 byteにしてテスト上限の100サイクルも実行した。baseline、100サイクル、
destructor後の計102点で、DUTのfree / largestは187788 / 73716 byte、peerは188288 / 69620 byteと
すべて同一だった。再登録・停止は全回`started=1`、`ble=0 classic=0 busy=0`で完了し、246.57秒で合格した。
Classicは起動直後にcontroller停止callbackをbrokerへ委譲する。Classic先行`end()`ではClassic
profile/hostだけが停止し、NimBLEとcontrollerは継続する。追加のLE GATT readが成功した後、最後の
NimBLE解除でbrokerがcontrollerを停止し、その後のClassic→NimBLE再起動と3サイクル停止も成功した。
さらに一時objectを使って実際のdestructorをClassic先行／NimBLE先行の両順序で実行し、残ったhostが
初期化済みであることと、両object破棄後に長寿命instanceを再起動できることを両側で確認した。
高速反復でNimBLE hostがOFFの間に遅延eventが届く窓は、`ble_hs_start()` / stop完了に連動する
broker receive gateで閉じた。修正後の完全再ビルド試験では`Host not enabled`出力は発生していない。

20サイクル化前の高速停止では、3回目に`npl_freertos_eventq_remove()`が別coreのhost taskによる
dequeueと競合し、FreeRTOS queue receive assertionを1回再現した。backtraceは
`nimble_port_stop` → `ble_hs_stop` → `ble_hs_timer_resched` → `callout_stop`を示した。
停止開始をNimBLE event queueへ要求し、host task自身が`ble_hs_stop()`を実行するよう変更した後は、
完全再ビルドの通常3サイクルと拡張20サイクルの両方が成功した。

## controller-wide event mask

brokerの独立policy層はGeneral Event Mask（`0x0c01`）、Page 2（`0x0c63`）、LE Event Mask
（`0x2001`）を別々にhost単位でcacheする。物理commandのopcodeと応答ownerは変えず、parameterだけを
登録host要求のORへ置換するため、command schedulerとCommand Complete routingに特別な応答生成は要らない。
Classic-only BluedroidのGeneral要求と通常のNimBLE要求から、従来hard-codedしていた
`ff ff ff ff ff ff bf 3d`が生成される。実機では両側ともmask command 4件、union書換え1件を記録し、
Classic HID、LE GATT 25回、停止・再起動まで成立した。

## 実機command inventoryの分類

両側brokerはlogical hostごとに初出opcodeを64件まで記録する。pairing、bond再接続、GATT read、
Classic HID接続、Classic再attachまでの通常試験で、NimBLEは各側19種、Classicは32〜34種だった。
ESP-IDF / esp-nimbleのHCI定義と照合した分類は次の通り。ここで「専有」は該当radioまたは
connectionのownerだけが発行するため物理送信できるもの、「共有read」は状態を変えないためschedulerで
直列化すればよいものを表す。

| 分類 | opcode（command） | 現在の扱い / 次の条件 |
|---|---|---|
| 共有read | `1001` Local Version、`1002` Supported Commands、`1003` Local Features、`1004` Extended Features、`1005` Buffer Size、`1009` BD_ADDR | 物理送信し、opcode transaction ownerへ応答 |
| broker統合mask | `0c01` Event Mask、`0c63` Event Mask Page 2、`2001` LE Event Mask | host別cacheのORを物理送信 |
| 再attach仮想完了 | `0c03` Reset、`0c31` Controller→Host Flow Control、`0c33` Host Buffer Size | dual-host時は物理状態を変えず要求hostだけへ成功応答 |
| broker消費 | `0c35` Host Number Of Completed Packets | 現在のflow-control無効構成では応答せず消費 |
| NimBLE専有・LE controller設定 | `2002` LE Buffer Size、`2003` LE Features、`2006` Advertising Parameters、`2008` Advertising Data、`2009` Scan Response、`200a` Advertising Enable、`200b` Scan Parameters、`200c` Scan Enable、`2018` LE Rand | Classic hostは発行しない。scheduler経由で物理送信 |
| NimBLE専有・LE procedure / handle | `200d` LE Create Connection、`2016` LE Remote Features、`2019` LE Start Encryption、`201a` LTK Reply、`2022` Set Data Length、`2030` Read PHY、`0406` Disconnect、`041d` Remote Version | 接続前procedureはNimBLE専有、接続後はLE handle所有を検証して物理送信 |
| Classic専有・local BR/EDR設定 | `0c13` Local Name、`0c14` Read Local Name、`0c18` Page Timeout、`0c1a` Scan Enable、`0c1e` Authentication Enable、`0c24` Class of Device、`0c3a` Current IAC LAP、`0c43` Inquiry Scan Type、`0c45` Inquiry Mode、`0c47` Page Scan Type、`0c52` Extended Inquiry Response、`0c56` Simple Pairing Mode、`080f` Default Link Policy | LE状態を変更しないClassic専有設定として物理送信 |
| Classic専有・address / handle procedure | `0405` Create Connection、`0409` Accept Connection、`040b` Link Key Reply、`040f` PIN Reply、`0411` Authentication Requested、`0413` Set Connection Encryption、`0419` Remote Name、`041b` Remote Features、`041c` Remote Extended Features、`041d` Remote Version、`041f` IO Capability Reply、`0803` Sniff Mode、`080d` Link Policy、`0c37` Link Supervision Timeout | BD_ADDR段階はClassic専有、接続後はBR/EDR handle所有を検証して物理送信 |

現時点で追加virtualizationが必要と判明したのはbootstrapの3 commandとhost flow-control creditである。
未観測commandを無条件に安全とは扱わない。今後profileを増やしてinventoryに新opcodeが現れた場合は、
この表へ分類してからdual-host対応済みにする。特にHardware Error、controller test mode、vendor command、
共有data path設定はhost専有にできないため、明示policyなしでは拒否する設計が必要になる。

## controller継続中のClassic再attach

Classic先行`end()`後もNimBLEが登録中なら、BTDM controllerはbroker所有のまま動作している。
この状態では`EspBleClassic::begin()`は`btStartMode()`を再実行せず、custom Bluedroid hostだけをattachする。
Bluedroidのcontroller bootstrapは毎回HCI Reset（`0x0c03`）を要求するため、二つのhostが登録済みなら
brokerのcommand taskがResetを物理送信せず、Classicへ成功Command Completeを非同期配送する。
同期callbackでBluedroid送信処理へ再入しないこと、物理controller command creditを消費しないことを
この境界の条件とした。

両側でClassicを停止し、既存LE接続のGATT read成功後にClassicを再attachした。各brokerはvirtual Reset
を1件記録し、再attach後も同じLE接続でGATT readが成功した。その後Classic HIDを再接続し、
Input / Output Reportの双方向通信も成功した。この経路を含む20回のcontroller停止・再起動も成功した。

再attachするBluedroidはResetに続いて`Set Controller To Host Flow Control`（`0x0c31`）と
`Host Buffer Size`（`0x0c33`）も再発行する。既存NimBLE linkのcontroller状態を変えないよう、dual-host時は
これらをClassicだけへ非同期Command Completeとして返す。`Host Number Of Completed Packets`
（`0x0c35`）も、物理flow controlを無効化した構成ではcontrollerへ送らず消費する。実機では両側の
Classic再attachが`resets=1 flow=2`で成功し、従来発生した`0x0c31 Command Disallowed`は消えた。

## security / bondingとNimBLE host修正

両ESP32でClassic HID ACLを接続したままNimBLE同士をpairingし、bond数1を確認してからBLEだけを切断した。
再広告・scan・接続後、両側のEncryption Change（event `0x08`、status 0、enabled 1）、
`encrypted=1 bonded=1 key_size=16`、暗号化必須characteristicのread成功を確認した。

この試験で、中央側のbond再接続だけNimBLE内部状態とGAP callbackが更新されない問題を検出した。
controller eventはbrokerから正しくNimBLEへ届いていたが、`ble_sm_process_result()`が対応するSM procedureなしで
早期終了し、`ble_sm_enc_event_rx()`が要求した暗号化callbackを捨てていた。Apache NimBLEとEspressif
esp-nimbleの現行masterにも同じ制御構造がある。同梱生成patchでは、procedureなしのEncryption Changeも
applicationへ通知し、成功した暗号化が既存bondに対応する場合はpeer security storeからauthenticated、
bonded、key sizeを復元する。変更は`tools/vendor_nimble_esp32.py`に固定し、二回の再生成で同一生成物になることを
確認した。この修正後に短縮clean試験（102.88秒）と通常25 read試験（66.93秒）がともに成功した。

さらに、起動時に復元したIRKを`deleteAllBonds()`で削除して再pairingすると、両側で
`ble_hs_resolv_list_add rc=3`が発生することを検出した。診断では、永続storeとpeer-device recordは削除済みだが、
host-based resolving listにはlocalとpeerの2 entryが残っていた。Espressif esp-nimbleの現行実装は
peer-device recordが見つかる場合だけlistから削除する。同梱生成patchではrecordが無い復元IRKも
identity addressで削除する。修正後は旧bond復元→削除→再pairingで`rc=3`は発生せず、bond数1、
暗号化必須read、bond再接続が成功した（89.97秒）。

RPA専用Peer testも追加し、両側を`ResolvablePrivate`にして初回pairing、NVSへbond保存、両端の
`ESP.restart()`、復元LTKでの再暗号化、暗号化必須GATT read/write、復元bond削除、再pairingを連続実行した。
この検証でhost-based privacyの4点を修正した。(1) 無印ESP32ではcontroller用
`BLE_OWN_ADDR_RPA_RANDOM_DEFAULT`ではなく`ble_hs_pvcy_rpa_config()`と`BLE_OWN_ADDR_RANDOM`を使う、
(2) scan結果はidentity置換後ではなく保存済みOTA RPAを公開する、(3) RPA照合時にResolving Listの
identity typeをOTA random typeで上書きしない、(4) HCI接続先へRPAを渡す場合だけ明示的にrandom型を使い、
接続完了時は置換前RPAを`peer_ota_addr`へ保存する、という分離である。補助peer recordの古い型ではなく
canonicalなResolving Listの型を使う修正も含む。修正後、全シナリオは49.18秒で成功し、通常の
public-address security/bond試験も62.56秒で成功した。ESP32-S3向けRPA Central/Peripheral compileと
host unit test 10件も成功した。生成script再実行前後の同梱NimBLE全ファイルSHA-256集約値は一致した。

このRPA経路をdual-hostへ組み合わせる専用Peer testも追加した。無印ESP32 2台の双方で独自Classic
hostとNimBLE hostを起動し、Classic HID ACLを接続したまま、BLEは双方host-based RPAで初回pairing、
暗号化必須GATT read、bond保存、LE切断、保存LTKによる再暗号化とGATT readを実行した。scan結果と
client/server双方のconnection結果はいずれもrandom型かつ上位bit `01`のOTA RPAを維持した。さらに
RPA timeoutを試験中だけ2秒へ短縮し、Classic HID ACLを維持した状態でhost privacy calloutを発火させた。
初回実験ではNimBLEの`ble_gap_preempt()`がadvertisingを停止し、`ADV_COMPLETE(EPreempted)`を通知した後、
EspBleが通知を無視するためadvertisingが停止したままになる問題を検出した。EspBleAdvertisingに
「アプリがadvertising継続を要求しているか」と有限durationの元deadlineを保持させ、privacy preempt時だけ
再開するよう修正した。同じ契約の`DISC_COMPLETE(EPreempted)`にもscan条件と有限durationの元deadlineを
保持する対称な処理を追加した。明示的な`stop()`/`end()`では要求を消すため意図しない再開はしない。
scan側の試験ではPeripheralを停止したままCentralの2秒timerを3周期発火させ、発火後に初めて開始した
advertisingを受信できることで、preempt前の受信ではなくscanが毎回実際に再開したことを確認する。
advertising側も3回連続で異なるRPAを観測し、各周期にClassic HID input/output reportを往復させた。
Centralの接続RPAは`79:05:43:5b:b8:4b`から`44:7d:08:48:b2:91`へ変化した。advertising側は
`66:34:47:7b:4f:45`から`57:30:23:ce:b6:61`、`70:dd:75:77:38:2b`、`52:27:50:6c:be:d7`と
3周期すべてで変化し、Classic接続を維持したまま新RPAから保存bondで
暗号化GATTを再確立した。

続いて両端を同時再起動し、NVS上のbond数1を確認してClassic HIDを再接続後、再起動で変化した双方のRPAを
IRKで解決し、保存LTKによる暗号化とGATT readを復元した。最後に`classic=1 ble=1`を確認し、rotationと
再起動復元までを含む一連の実機試験は113.03秒で成功した。
条件マクロを使わない通常public-addressのdual-host smokeも再実行し、暗号化GATT反復、bond再接続、
Classic再attach、両link切断、3回再起動、両destructor順、heap不変を118.29秒で完走した。

bond再接続とClassic再attach後、中央側からLEとBR/EDRのDisconnectを連続発行する試験も追加した。
controllerからの完了順に依存せず、NimBLE client/serverとClassic HID Host/Deviceの4切断callbackが
すべて対応する側へ届き、直後の両host停止（in-flight command 0）、3回再起動、両destructor順も成功した。

## dual-hostでbrokerが持つべき責務

### Hostからcontroller

HCI commandはcontroller全体で一つのcommand creditを共有する。二つのhostから来たcommandを
直列化し、opcodeと送信元hostをtransaction tableへ保存する。Command Complete / Command Statusは
その表を使って要求元だけへ返す。Reset、Set Event Mask、Host Buffer Sizeなどcontroller全体を
変更するcommandはhostへ直接発行させず、brokerの管理commandにする。

ACL packetはconnection handleから送信先linkを検証してcontrollerへ渡す。controllerのACL bufferと
Number Of Completed Packetsは共通資源なので、hostごとの使用数をbrokerで追跡してcreditを分配する。

### Controllerからhost

- LE Meta EventとLE connection handleはNimBLEへ送る。
- BR/EDR Connection Completeで得たhandle、SCO/eSCO、Classic固有eventはClassic hostへ送る。
- Disconnection Complete、Encryption Change、Number Of Completed Packetsなど共通eventはhandle tableで
  配送先を決める。複数hostのhandleを含む完了eventはhost別のeventへ再構成する。
- Hardware Errorなどcontroller全体のeventはbrokerが状態遷移を行ったうえで両hostへ通知する。

connection handleは12 bitなのでACL headerから抽出できる。handle所有権はLE/BR-EDRそれぞれの
Connection Complete成功時に登録し、Disconnection Complete後に破棄する。

### Lifecycle

controllerはbrokerだけが初期化・enable・disableする。排他モードではBLEなら`ESP_BT_MODE_BLE`、
Classicなら`ESP_BT_MODE_CLASSIC_BT`、共存モードでは`ESP_BT_MODE_BTDM`を使う。共存モードでは
BLE側またはClassic側のmemoryをreleaseしてはならない。両hostのready条件を満たした後にtrafficを
開放し、停止時は新規送信停止、接続切断、host停止、controller停止の順にする。

## ESP-IDF upstreamとの合わせ方

ESP-IDF masterの`components/bt/porting_btdm/transport`はすでにcontroller→host方向を
`HCI_DRIVER_DIR_LEC2H`と`HCI_DRIVER_DIR_BREDRC2H`で区別しており、NimBLE dual-host対応を示す
TODOもある。長期的には、このdirection metadataをhost routerへ渡す小さな汎用transportを
ESP-IDF側へ提案するのがよい。

一方、Arduino-ESP32の無印ESP32で利用できるprebuilt controllerのVHCI APIはH4 byte streamしか
公開しない。そのためEspBle内の実装ではH4 parserとhandle tableが必要になる。brokerのhost-facing
APIをESP-IDF側のdirection-aware transportと分離しておけば、後にupstream実装へ差し替えられる。

Classic hostはBlueZではなくClassic-only Bluedroidを第一候補とする。BlueZはLinux kernel socket、
D-Bus、glibなど組み込み用途以外の依存が大きい。BluedroidはESP32で実績があり、host-only HCI API、
Classic-only Kconfig、Apache-2.0という条件が揃っている。vendoringする場合はNimBLEと同様に取得元を
commit固定し、機械生成と局所patchで管理する。

## 段階的な実装案

### Phase A: 排他モード

1. `BLE_NIMBLE`と`CLASSIC_BLUEDROID`のbuild-time switchを追加する。
2. Classic-only Bluedroid hostと、`esp_bluedroid_attach_hci_driver()`をbrokerへ接続するadapterを同梱する。
3. brokerへcontroller lifecycleの所有権を移し、NimBLE側の直接初期化・memory releaseを除く。
4. Classic側はまずSPP、GAP discovery、pairing、bond、再接続を実機試験する。

この段階ではhostは一つしか登録しないので、現在のpass-through構造を保ったままClassicの同梱方法、
API、メモリ量、ライフサイクルを先に確定できる。

### Phase B: Classicを基準にBLEを追加

1. controllerをBTDMで起動し、Classic hostを先に初期化する。
2. event mask / HCI Reset以外のbroker管理commandとcommand transactionを分類・実装する。
3. NimBLEを登録し、LE eventとLE ACLだけを開放する。
4. handle routingとACL credit分配を実装する。
5. SPP traffic中にBLE scan、GATT接続、read/write、notification、SMPを順に追加検証する。

### Phase C: 品質とupstream化

parser、transaction、handle table、credit分配をcontrollerなしでfuzz/unit testする。実機pytestでは
command同時発行、BLE/Classic同時切断、bond再起動、ACL飽和、event flood、長時間反復を追加する。
ESP-IDF向けにはdirection-aware routerを独立componentとして小さく切り出し、EspBle固有APIや
Arduino依存を含めない。

## 次の判定条件

次の実装へ進む前に、Phase AのClassic-only vendoringで以下を測定する。

- Arduino-ESP32 3.3.11上でのcompile/link可否
- SPP connect/send/receive、discovery、pairing、bond復元、`begin()`/`end()`反復
- flash、static RAM、接続中heap、task数の増分
- ESP32-S3ほか既存targetの生成物が変わらないこと

これらが通ればBluedroid案を採用する。host-only BluedroidをArduino buildへ安全に同梱できない場合に
限り、別のClassic hostを再評価する。
