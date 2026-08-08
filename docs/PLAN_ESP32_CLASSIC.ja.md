# 無印ESP32 Classic拡張 設計・検証記録

## 目標と段階

無印ESP32だけを対象に、EspBle同梱NimBLEとは独立したClassic hostを追加する。他のESP32
シリーズはClassic radioを持たないため、既存BLEコード・生成物へClassic実装を入れない。

段階は次の順とする。

1. BLE / Classicを起動時に排他選択し、HCI brokerを単一host pass-throughとして検証する。
2. core内蔵BluedroidのSPPで公開APIと実機試験を先に固める。
3. Classic-only Bluedroid hostを独自ビルドし、core hostと置き換える。
4. 独自hostでClassic HID Device / Hostを公開APIへ接続する。
5. HCI command、event、ACLの所有権とflow controlを実装し、NimBLEとClassicを同時利用する。

同時利用を始める前に排他構成を完成させる。これによりHCI routerの問題と各profileの問題を
分離できる。

## 独自Bluedroid host

Arduino-ESP32 3.3.11と同じESP-IDF v5.5.5およびGCC 14.2.0で、次の構成をビルドする。

- controller無効（host-only）
- BLE無効、Classic有効
- SPP有効
- Classic HID Device / HID Host有効
- SMP有効

生成された`libbt.a`が定義するglobal symbolをすべて`espble_bd_`名前空間へ変換する。
未定義のFreeRTOS、NVS、timer、logging等は変換せずArduino coreから解決する。この境界により、
core内蔵Bluedroidとリンク時に衝突せず、独自hostの公開APIが誤ってcore側へ解決されることもない。

再生成入口は`tools/build_classic_bluedroid_host.sh`、設定と必須APIのlink checkは
`tools/classic_bluedroid_host/`に置く。スクリプトはIDF tagがv5.5.5以外なら停止する。

## Arduino側の分離

- アーカイブは`src/esp32/`だけへ置き、他SoCではリンク対象にしない。
- `ESPBLE_CLASSIC_ONLY`は同梱NimBLE 83 sourceと`EspBle.cpp`を空にする。
- `ESPBLE_CLASSIC_CUSTOM_HOST`は名前空間化した独自host APIとHCI broker adapterを選ぶ。
- Classicを明示しない通常BLE buildでは`EspBleClassic.cpp`をstub backendとしてコンパイルし、
  Bluedroidへの未解決参照を一切生成しない。
- 共通の`EspBleError`だけを`EspBleTypes.h`へ置き、Classic公開headerはBLE公開headerへ依存しない。

現在のbrokerはhostを1つだけ受け付ける。独自BluedroidのHCI driver operationsをbrokerへ接続し、
brokerがArduino core controllerのVHCIへ転送する。2つ目のhost登録は意図的に
`ESP_ERR_NOT_SUPPORTED`とする。

## 2026-08-08 技術検証結果

| 検証 | 結果 |
|---|---|
| IDF v5.5.5 host-only / Classic-only / SPP / HID Device / HID Host build | 成功 |
| 必須API（HCI attach、SPP、HID Device、HID Host）のIDF link check | 成功 |
| 1,788 defined symbolsの名前空間化 | 成功 |
| 再生成archiveの再現性 | SHA-256一致 |
| Arduino-ESP32 3.3.11への独自host link | 成功、SPP例827,172 B |
| ESP32 2台の独自host SPP Peer test | 1 passed（接続、binary echo、切断、再初期化、再接続） |
| 独自hostのHID Device / HID Host profile実機init/deinit | 1 passed（両profile同時） |
| 通常NimBLE BLEのESP32 Peer regression | 2 passed（GATT read/write、反復discovery） |
| host unit test | 7 passed |
| ESP32-S3 CompileSmoke | 成功、274,253 B。Classic archiveは非リンク |

core内蔵hostを使った先行SPP試験も同じPeer testで成功したが、最終構成には使わない。
Arduino coreの設定はSPP有効・Classic HID無効であり、core `libbt.a`にHID Device/Host APIの
実体はない。独自archiveでは両方のAPIが存在することをsymbol検査済みである。

## 次の実装

1. Classic HID Device / Hostのbackend adapterと、callback contextから`update()`へ渡すevent queue。
2. ESP32 2台をHID Device / Hostにしたreport送受信Peer test。
3. SPPとHIDを同じClassic host上で同時に使うprofile lifecycle test。
4. brokerのHCI opcode所有権、connection handle routing、Number Of Completed Packetsの配分を設計。
5. controllerをBTDMで起動し、NimBLE＋Classicの同時利用へ進む。

同時host段階ではHCI command creditを共有し、Command Complete / Statusを発行元へ戻す必要がある。
ACLはconnection handleでroutingできるが、接続確立前event、advertising、inquiry、security、
controller-wide commandは単純なpacket type分岐では扱えない。排他構成のbrokerをそのまま
「BLE eventはNimBLE、Classic eventはBluedroid」と拡張してはならない。
