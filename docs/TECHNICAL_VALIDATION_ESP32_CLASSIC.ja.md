# 無印ESP32 NimBLE / Classic共存 技術検証

検証日: 2026-08-08

## 結論

無印ESP32のBTDM controllerはBLEとClassicを同時に動かせる。Classic-onlyのBluedroid hostも
controllerから分離してビルドできる。そのため、EspBleへClassicを追加する際の第一候補は、
NimBLEとClassic-only BluedroidをHCI brokerへ接続する構成とする。

ただし、VHCIのH4 byte streamをパケット種別だけで二分することはできない。command credit、
command応答、connection handle、ACL credit、controller初期化をbrokerが一元管理する必要がある。
今回追加したbrokerは、この境界を先に導入したsingle-host pass-throughであり、二つ目のhost登録は
意図的に`ESP_ERR_NOT_SUPPORTED`とする。dual-host対応済みではない。

## 実機・ビルド検証結果

| 検証 | 結果 | 確認できたこと |
|---|---:|---|
| 変更前 ESP32 × ESP32-S3 `gatt_read_write` | 2 passed / 78.55秒 | 変更前の基準 |
| broker経由 ESP32 × ESP32-S3 `gatt_read_write` | 2 passed / 67.13秒 | HCI TX/RXをbroker経由にしてもGATT read/writeが成立 |
| broker経由 ESP32 × ESP32-S3 `lifecycle_stress` | 8 passed / 106.98秒 | `begin()`/`end()`、再接続、event floodを含む反復 |
| broker経由 ESP32 × ESP32-S3 `security_bond` | 1 passed / 68.66秒 | SMP、暗号化、bond保存経路 |
| EspBleBluedroid ESP32 dual-mode `dual_mode_scan_spp` | 1 passed / 73.26秒 | active SPPとBLE scan/GATT/notificationの同時動作 |
| ESP32 / ESP32-S3 compile smoke | 成功 | target guardが両構成で成立 |
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

現在はcallback pointerの切替をcontroller停止中に行う前提である。dual-host化の前に、登録解除と
RX callbackの並行実行を保護し、broker自身のtask/queueへ受信を移す。

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
2. broker管理command、command transaction、event maskのunionを実装する。
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
