# 無印ESP32 NimBLE / Classic共存 技術検証

検証日: 2026-08-11

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
bond再接続、暗号化必須GATT readも成立した。数時間級負荷を完走し、実機で観測したcommandを明示policyへ
分類した。dual-hostは未観測commandをfail-closedにする。HID接続失敗、pairing失敗からの復旧と
callback参照寿命監査も完了した。公開範囲とACL credit一元管理が未確定なので、opt-inでない通常buildは
引き続き二つ目のhost登録を`ESP_ERR_NOT_SUPPORTED`とする。

現在の引き継ぎ状態と残作業は[HANDOFF_ESP32_CLASSIC.ja.md](HANDOFF_ESP32_CLASSIC.ja.md)、
独自Classic host archiveの再生成方法は[CLASSIC_HOST_BUILD.ja.md](CLASSIC_HOST_BUILD.ja.md)を参照する。

## 実機・ビルド検証結果

| 検証 | 確認できたこと |
|---|---|
| 変更前 ESP32 × ESP32-S3 `gatt_read_write` | 変更前の基準 |
| broker経由 ESP32 × ESP32-S3 `gatt_read_write` | HCI TX/RXをbroker経由にしてもGATT read/writeが成立 |
| broker経由 ESP32 × ESP32-S3 `lifecycle_stress` | `begin()`/`end()`、再接続、event floodを含む反復 |
| broker経由 ESP32 × ESP32-S3 `security_bond` | SMP、暗号化、bond保存経路 |
| EspBleBluedroid ESP32 dual-mode `dual_mode_scan_spp` | active SPPとBLE scan/GATT/notificationの同時動作 |
| ESP32 / ESP32-S3 compile smoke | target guardが両構成で成立 |
| 独自Classic HID + NimBLE GATT同時Peer | 同じBTDM controllerでBR/EDR ACLとLE ACL、単発GATT readが共存 |
| dual-host ACL反復 | GATT read反復後もClassic HID双方向が継続し、unknown handleなし |
| dual-host security / bond再接続 | Classic HID接続中に両側pairing・bond保存、再接続後の暗号化必須read、`encrypted=1 bonded=1 key=16`を確認 |
| dual-host controller command policy | 接続中・切断後の観測opcodeを分類。未知／別host commandは物理送信前に拒否 |
| dual-host FIFO backpressure | FIFO満杯と超過拒否を発生させ、未送信分破棄後にGATT/HID/lifecycle復帰 |
| dual-host同時切断 | LE / BR-EDR Disconnectの完了を正しいhostへ配送後、停止・再起動・destructor成功 |
| dual-host異常report / peer消失 | null・上限超過reportを接続維持のまま拒否し、peer突然再起動後にbond済みBLEとClassic HIDを復旧 |
| dual-host接続 / pairing失敗 | 誤passkey後にbondなしで再pairing。HID非同期接続失敗後も暗号化LEを維持してClassic再接続 |
| Classic callback参照寿命 | SPP/HID targetの登録mutex・参照数barrierを追加し、全停止順とdestructor順をclean実機回帰 |
| Classic archive clean再現 | cleanなIDF v5.5.5 / GCC 14.2.0から生成し、格納済み`.a`とbyte単位・SHA-256一致 |
| HCI router / controller policy unit | opcode応答、handle所有、ACL、切断、mixed completed event、command scopeと許可host |
| ESP-IDF Classic-only host spike | controllerなし、BLEなし、SPPありのBluedroid hostを外部HCIへattachしてbuild/link成功 |
| EspBle unit test | 既存host非依存ロジックの回帰なし |

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

dual-host parserとcommand scheduler stateはcritical sectionで保護する。callback登録解除時はreceive gateと
host session世代を使い、停止済みhostへの遅延配送と旧sessionのcommand送信を防ぐ。commandは`send()`で16 packetのbroker所有FIFOへcopyし、専用taskが
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
commandを再登録後のcontrollerへ送らない。この再登録経路は後述の数時間級soakでも検証した。

commandを両hostから意図的に同時発行する実機試験も追加した。Arduino main taskからNimBLEの
`Read RSSI`を20回発行する間、別FreeRTOS taskからClassic-only Bluedroidのscan modeを20回
切り替える。Classic APIがBTC taskへ非同期postした処理の収束を待ち、最後の同期`Read RSSI`で
broker FIFOがdrainしたことを確認する。10サイクル連続試験では各基板でNimBLE約210件、Classic
約500件のHCI commandを競合区間だけで物理送信し、全件でFIFO投入数と物理送信数が一致した。
最大queue深度はDUT 4、Peer 5で、queue overflow、応答opcode不一致、host解除busy、未知ACLは
すべて0だった。各サイクル直後の暗号化GATT readとClassic HID Input / Output Report、続くbond
再接続、Classic再attach、任意順停止、再起動、destructorも成功し、3点のheap測定値は両基板とも
完全に同一だった。

正常なNimBLE / Bluedroidはhost内部でもHCI transactionを直列化するため、実APIの同時呼び出しだけで
16 packet FIFOを満杯にすることはできない。そこでdual-host実機test sketchだけに
`ESPBLE_HCI_BACKPRESSURE_TEST`を定義し、schedulerがidleであることを確認して物理dispatchを一時停止する
検証フックを追加した。二つのFreeRTOS taskから通常の`espble_hci_broker_send()`経路へ各12件を同時投入し、
両基板とも16件を受理、残り8件を`ESP_ERR_NO_MEM`で拒否し、queue high-water 16を記録した。試験commandは
controllerへ送らず、logical hostへ偽のCommand Completeも返さずに破棄し、元のschedulerとdiagnosticsを復元する。
直後の暗号化GATT readとClassic HID双方向通信、bond再接続、Classic再attach、任意順停止、再起動、
destructorが成功したため、満杯・超過拒否後のlive session復帰も確認できた。検証フックは通常buildには
コンパイルされない。

同じ試験を20 run連続実行する数時間級soakを2026-08-09 14:29:26〜16:11:10 JSTに行い、全runが
成功した。総経過時間は1時間41分44秒で、各runはcommand競合100サイクルと停止・再登録100サイクルを
含む。全runの最終diagnosticsでcommand enqueue数と物理send数が一致し、
`qfull=0 mismatch=0 busy=0 unknown=0`、通常queue深度の最大値は5だった。各run内でfree heapと
largest blockの低下はなく、1 runだけ開始後にfree heapが8 byte増えて以後一定だった。panic、watchdog、
backtrace、再接続失敗は観測していない。logは`tests/.soak/dual-host-20260809T052926Z/`へ保存した。

当初はClassic側の負荷に`Write Local Name`を使ったが、5〜10サイクルでcontrollerが
`ASSERT_ERR(0), in nvds.c at line 400`となった。ELFでdecodeしたbacktraceは
`hci_wr_local_name_cmd_handler` → `ke_task_schedule` → `btdm_controller_task`であり、その直前まで
brokerの投入数と送信数は一致し、overflow、opcode不一致、未知eventも0だった。これはNVDSへ触れる
永続設定commandをsoak刺激として反復したcontroller側の制約であり、brokerの競合不良とは扱わない。
以降は永続領域へ書かないscan mode切替を刺激に使い、10サイクルでassertが再現しないことを確認した。

同じ実機試験で、停止後に同一の`EspBle` / `EspBleClassic` instanceとGATT/HID定義を使って
Classic→NimBLEを再登録し、両hostの初期化成功と正常停止を通常3サイクル、拡張実行20サイクルで
確認した。20サイクル実行ではbaseline、各サイクル、destructor後の計22点でheapを計測し、DUTは
free 187692 byte、peerはfree 188192 byteで全測定値が同一だった（観測分解能上の漏れ0 byte）。
DUTのminimum free / largest blockは84144 / 73716 byte、peerは71124 / 59380 byteだった。
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
| NimBLE専有・LE procedure / handle | `200d` LE Create Connection、`2016` LE Remote Features、`2019` LE Start Encryption、`201a` LTK Reply、`2022` Set Data Length、`2030` Read PHY | 接続前procedureはNimBLE専有、接続後はLE handle所有を検証して物理送信 |
| transport共通・handle procedure | `0406` Disconnect、`041d` Remote Version、`1405` Read RSSI | LE / BR-EDRのhandle所有hostだけが物理送信 |
| Classic専有・local BR/EDR設定 | `0c13` Local Name、`0c14` Read Local Name、`0c18` Page Timeout、`0c1a` Scan Enable、`0c1e` Authentication Enable、`0c24` Class of Device、`0c3a` Current IAC LAP、`0c43` Inquiry Scan Type、`0c45` Inquiry Mode、`0c47` Page Scan Type、`0c52` Extended Inquiry Response、`0c56` Simple Pairing Mode、`080f` Default Link Policy | LE状態を変更しないClassic専有設定として物理送信 |
| Classic専有・address / handle procedure | `0405` Create Connection、`0409` Accept Connection、`040b` Link Key Reply、`040f` PIN Reply、`0411` Authentication Requested、`0413` Set Connection Encryption、`0419` Remote Name、`041b` Remote Features、`041c` Remote Extended Features、`041f` IO Capability Reply、`0803` Sniff Mode、`0804` Exit Sniff Mode、`080d` Link Policy、`0c37` Link Supervision Timeout | BD_ADDR段階はClassic専有、接続後はBR/EDR handle所有を検証して物理送信 |

現時点で追加virtualizationが必要と判明したのはbootstrapの3 commandとhost flow-control creditである。
観測済みcommandは純粋Cのcontroller policyでも同じ分類へ固定した。dual-host時は未知opcodeと別host専用
opcodeを物理controllerへ送らず`ESP_ERR_NOT_SUPPORTED`で拒否する。connection scopeはpacket先頭parameterの
handleが要求host所有であることも検査し、別hostまたは解放済みhandleを`ESP_ERR_NOT_FOUND`で拒否する。
single-host pass-throughは変更しない。
今後profileを増やしてinventoryに新opcodeが現れた場合は、この表とpolicyへ分類してからdual-host対応済みに
する。特にHardware Error、controller test mode、vendor command、共有data path設定はhost専有にできないため、
明示policyなしでは拒否する。

policy実装後のclean実機回帰では、最初のconnected-state inventoryより後の切断時にClassicが条件付きで
`0804` Exit Sniff Modeと`0406` Disconnectを発行することを新たに検出した。前者をClassic接続所有、後者を
transport共通のhandle所有へ分類し、切断完了後にも両基板のinventoryを再取得するようPeer testを補強した。
純粋C unit testは観測済み全opcodeのscopeと許可host、未知opcode、別host、壊れたH4長を検証する。
最終実機回帰では接続中・切断後とも全opcodeが許可集合内で、未知command拒否はなかった。

## 異常reportとpeer突然消失

Classic HID Device / Hostの公開送信APIに共通上限`MaximumReportLength = 1024`を設け、受信側の保護上限と
一致させた。null pointerと1025 byte reportはBluedroidへ渡す前に`InvalidArgument`で拒否する。両基板で
Device Input / Host Outputの両方向を試し、拒否後もClassic HIDとBLE接続が維持され、暗号化GATT readと
HID双方向通信が直ちに成功することを確認した。

さらにpeerをgraceful shutdownせず`ESP.restart()`し、生存側がLEとBR/EDR双方の切断を検出する経路を追加した。
software resetしたpeerは保存済みLTKを維持し、両側のbondが残っていることを明示確認してからClassic HIDと
BLEを再接続する。再接続したBLEは`encrypted=1 bonded=1 key=16`となり、暗号化必須GATT readとHID双方向通信が
成立した。生存側はcontrollerもlogical hostも再起動していない。この試験の初期化では通常のupload / power-on時だけ
bondを削除し、意図したsoftware reset時は削除しないようreset reasonを区別している。

## 接続失敗とpairing失敗からの復旧

public-address版`dual_host_smoke`へ、両transport稼働中の失敗経路を追加した。BLEは両側をMITM必須の
DisplayOnly / KeyboardOnlyにし、最初の接続では表示値と異なるpasskeyを入力する。client/server双方で
`success=0 encrypted=0 bonded=0 key=0`、bond数0、暗号化必須GATT read失敗を確認し、その間もClassic HIDは
接続したまま通信できた。LEだけを切断して再接続し、新しく生成された正しいpasskeyを入力すると、双方が
`success=1 encrypted=1 bonded=1 key=16`となり、暗号化必須GATT readが成功した。失敗したSMP状態やbondが
次の試行を汚染しないことを確認している。

Classic側には`EspBleClassicHidHost::onConnectionFailed()`を追加した。接続開始直後のbackend APIエラーだけでなく、
Bluedroidの最終`ESP_HIDH_OPEN_EVT`がconnected以外で終わる場合も、peer address、`BackendFailure`、backendの
status/stateをevent queue経由で非同期通知し、内部のconnecting状態とpeer情報を消去する。試験では暗号化LEを
維持したままClassicだけを切断し、自身のClassic addressへの接続を要求して最終OPEN失敗を発生させた。失敗通知後も
GATT readは`classic=0`で成功し、直ちに正しいpeerへClassic HIDを再接続して双方向reportを復旧できた。

Bluedroidの公開HID Host APIにはpage中の接続試行を取り消すAPIがない。到達不能peerへ独自timeoutを設け、
`esp_bt_hid_host_disconnect()`で取り消す案も実機で試したが、公開APIは未接続として拒否する一方、host内部は
connectingのまま残り次の接続を拒否した。このため任意timeoutは公開せず、backendが返す最終OPEN結果を失敗境界とする。

2026-08-11のclean実機回帰は、上記に加えて暗号化GATT反復、HID双方向、command競合、FIFO backpressure、
peer突然再起動、停止・再登録、両destructor順を含めて成功した。ESP32-S3代表compileとhost unit testも成功した。

## backend callbackの参照寿命

Classic profileのlifecycleをコード監査した結果、SPP、HID Device、HID Hostがいずれもglobalなatomic生ポインタから
callback targetを取得していた。atomic load/storeはポインタ値のdata raceを防ぐだけで、callbackがloadした直後に
別coreの`end()` / destructorがtargetを解除・解放するuse-after-free窓は閉じない。

各profileへcallback target登録mutexとtarget内のatomic参照数を追加した。callbackは登録mutex下でactive targetを
取得して参照数を増やし、処理終了時にRAIIで減らす。`end()`はbackend deinit callbackを受け取った後、同じ登録mutex下で
active targetを外して新規取得を止め、既に取得済みの参照数が0になるまで待ってからstateを初期化する。destructorは
`end()`後にだけstateをdeleteするため、callbackが取得したtargetは処理終了まで生存する。callback内から利用者callbackを
直接呼ばず固定長event queueへ積む既存設計なので、barrier待ちが利用者コードの実行時間へ依存することもない。

修正後のclean実機回帰では、Classic先行／BLE先行の停止、Classic再attach、停止・再登録、両destructor順を通過し、
panic、watchdog、heap低下は発生しなかった。callback登録を持たないClassic GAP callbackは所有stateを参照しないため
同じbarrierの対象外である。公開API自体の`begin()` / `end()`同時呼び出しはthread-safe契約には含めない。

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
host unit testも成功した。生成script再実行前後の同梱NimBLE全ファイルSHA-256集約値は一致した。

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
scan側の試験ではPeripheralを停止したままCentralの2秒timerを発火させ、発火後に初めて開始した
advertisingを受信できることで、preempt前の受信ではなくscanが毎回実際に再開したことを確認した。
既定回帰は3周期、追加soakは10周期連続で行い、advertising側では毎回異なるRPAを観測した。各周期に
Classic HID input/output reportを往復させ、scan側でも10回のpreemption中に同じ双方向通信を継続した。
新RPAから保存bondによる暗号化GATTも再確立した。

有限durationも別に検査した。8秒のadvertising/scanを2秒ごとに3回preemptし、3秒時点ではactive、
元deadlineを越えた9秒時点ではinactiveだった。再開時に元の8秒を再設定して期限を延長する実装では
成立しない条件である。両基板のbroker opcode履歴にNimBLE hostの`0x2005`（LE Set Random Address）が
残り、10周期soak後も`unknown=0 qfull=0 mismatch=0 busy=0`だった。

続いて両端を同時再起動し、NVS上のbond数1を確認してClassic HIDを再接続後、再起動で変化した双方のRPAを
IRKで解決し、保存LTKによる暗号化とGATT readを復元した。最後に`classic=1 ble=1`を確認し、rotationと
再起動復元までを含む既定3周期試験は128.16秒、10周期soakは160.56秒で成功した。
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
