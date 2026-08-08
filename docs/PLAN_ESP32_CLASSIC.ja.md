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

通常buildのbrokerはhostを1つだけ受け付ける。`ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL`を定義した
無印ESP32だけは2 hostを受け付け、純粋Cの`EspBleHciRouter`でH4 command応答、LE / BR-EDR
connection handle、ACL、切断、Number Of Completed Packetsを振り分ける。実験flagを付けない
無印ESP32と他SoCの経路は変更しない。

共存時はClassicを先にBTDMで起動してcontroller lifecycleを所有させる。NimBLEはcontrollerを
再初期化せずhostだけをattachし、Resetを省略する。NimBLE起動後にBluedroid DUMO相当のunion
event mask（HCI wire順`ff ff ff ff ff ff bf 3d`）を設定し、Classic eventを維持したまま
LE Metaを有効化する。

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
| 公開HID APIのDevice→Host Input / Host→Device Output | 1 passed |
| 公開HID APIの切断・host全体再初期化・再登録・再接続 | 1 passed（同じPeer test内） |
| 同一Classic host・同一ACL上のHID＋SPP同時利用 | 1 passed（SPP echo後もHID双方向継続） |
| HCI router host unit test | 1 passed（command、handle、ACL、切断、mixed completed packet） |
| HCI command scheduler host unit test | 1 passed（FIFO、credit 0、no-response command、copy所有権、overflow、opcode不一致） |
| NimBLE＋独自Classic host同時利用 | 1 passed（Classic HID双方向→LE接続・GATT readのsmoke） |
| dual-host ACL反復負荷 | 1 passed（GATT read 25回後もClassic HID双方向、両側LE ACL tx/rx/completed=36/36/36） |
| dual-host command scheduler負荷 | 1 passed（両hostから送信、投入＝物理送信、最大queue深度3、overflow / opcode不一致0） |
| 通常NimBLE BLEのESP32 Peer regression | 2 passed（GATT read/write、反復discovery） |
| host unit test | 9 passed |
| ESP32-S3 CompileSmoke | 成功、274,253 B。Classic archiveは非リンク |

core内蔵hostを使った先行SPP試験も同じPeer testで成功したが、最終構成には使わない。
Arduino coreの設定はSPP有効・Classic HID無効であり、core `libbt.a`にHID Device/Host APIの
実体はない。独自archiveでは両方のAPIが存在することをsymbol検査済みである。

## 次の実装

1. dual-hostでcommand同時発行、同時切断、実機queue overflowを反復する。
2. controller / hostの停止順を共有lifecycleとしてAPI化し、BLE→Classicの順以外を安全に拒否する。
3. hard-coded union event maskをhost要求maskのbroker側union / cacheへ置き換える。
4. HID接続失敗、異常長Report、security / bondingを両transport同時状態で試験する。

実験実装はCommand Complete / Statusをopcode所有者へ戻し、connection handleでACLを分離する。
HCI commandは`send()`時にbroker所有の16 packet FIFOへcopyし、専用taskだけが物理VHCIへ送る。
Command Complete / Statusの`Num_HCI_Command_Packets`とopcodeを照合し、応答を要するcommandは
保守的に1件だけin-flightとする。`Host Number Of Completed Packets`は応答なしcommandとして
creditを消費しない。物理VHCIの`available`確認と送信はmutexでACL送信とも直列化する。
`can_send()`でlogical hostへslotを予約する試作は、Bluedroidが送信直前以外にも可否を確認するため
NimBLEを飢餓させ、実機でHCI ACK timeoutになることを確認した。この方式は採用しない。
FIFOだけの試作ではpacketが物理VHCIへ出た後もClassic ACLが停滞した。HCI traceとbroker counterで、
Classic Bluedroidが有効化したcontroller→host ACL flow controlに対し、NimBLEへroutingしたLE ACLの
creditが返らず、両側ともLE RX 20 packetで共有bufferが枯渇することを特定した。dual-host実験buildは
Bluedroidの設定commandをflow control無効へ正規化し、同じ負荷でHID双方向とLE接続の継続を確認した。
長期的にはbrokerが全incoming ACLを数え、host別配送完了後にcontrollerへcreditを一元返却する。
command所有権は専用taskが物理slotを確保した直後に記録するため、送信されなかったpacketがpending
command表へ残る問題を防いでいる。完全再ビルドのdual-host負荷試験では両ESP32ともFIFO投入数と
物理送信数が一致し、queue overflowとopcode不一致は0だった。
ACLはconnection handleでroutingできるが、接続確立前event、advertising、inquiry、security、
controller-wide commandは単純なpacket type分岐では扱えない。排他構成のbrokerをそのまま
「BLE eventはNimBLE、Classic eventはBluedroid」と拡張してはならない。
