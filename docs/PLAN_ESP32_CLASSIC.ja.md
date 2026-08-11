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
環境構築、生成手順、成果物の照合方法は[CLASSIC_HOST_BUILD.ja.md](CLASSIC_HOST_BUILD.ja.md)を正本とする。

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

共存時はClassicがBTDM controllerを起動した直後に停止責任をbrokerへ委譲する。NimBLEはcontrollerを
再初期化せずhostだけをattachし、Resetを省略する。最後のlogical hostを解除したbrokerだけが
controllerを停止する。Set Event Mask / Page 2 / LE Set Event Maskはhost別に要求値をcacheし、
brokerがORしたmaskを物理controllerへ送る。現在のGeneral Event MaskはClassic要求とNimBLE要求から
HCI wire順`ff ff ff ff ff ff bf 3d`になり、Classic eventを維持したままLE Metaを有効化する。
NimBLE接続中にClassicを再attachする場合、Bluedroid bootstrapのHCI Resetはcontrollerへ送らず、
broker taskから成功Command CompleteをClassicだけへ返す。

## 2026-08-08 技術検証結果

| 検証 | 結果 |
|---|---|
| IDF v5.5.5 host-only / Classic-only / SPP / HID Device / HID Host build | 成功 |
| 必須API（HCI attach、SPP、HID Device、HID Host）のIDF link check | 成功 |
| Audio profileを含む2,796 defined symbolsの名前空間化 | 成功 |
| A2DP/AVRCP/HFP external codec APIのIDF link check | 成功 |
| 再生成archiveの同一環境比較 | SHA-256一致。cleanな固定環境からの再現確認はrelease gateとして残る |
| Arduino-ESP32 3.3.11への独自host link | 成功、SPP例827,172 B |
| ESP32 2台の独自host SPP Peer test | 接続、binary echo、切断、再初期化、再接続を確認 |
| 独自hostのHID Device / HID Host profile実機init/deinit | 両profile同時に確認 |
| 公開HID APIのDevice→Host Input / Host→Device Output | 双方向成功 |
| 公開HID APIの切断・host全体再初期化・再登録・再接続 | 同じPeer test内で確認 |
| 同一Classic host・同一ACL上のHID＋SPP同時利用 | SPP echo後もHID双方向継続 |
| HCI router host unit test | command、handle、ACL、切断、mixed completed packetを確認 |
| HCI command scheduler host unit test | FIFO、credit 0、no-response command、copy所有権、overflow、opcode不一致を確認 |
| NimBLE＋独自Classic host同時利用 | Classic HID双方向→LE接続・GATT readのsmoke成功 |
| dual-host ACL反復負荷 | GATT read反復後もClassic HID双方向、unknown handleなし |
| dual-host command scheduler負荷 | 両hostから送信、投入＝物理送信、overflow / opcode不一致なし |
| dual-host command inventory / flow-control仮想化 | 観測commandを分類し、再attach時Resetとflow設定を仮想完了 |
| dual-host security / bonding | Classic HID接続中にpairing・bond保存、BLE bond再接続、暗号化必須GATT readを確認 |
| host-based RPA / bond再起動 | 初回pairing、両端再起動後のLTK復元、暗号化GATT、bond削除、再pairingを確認 |
| dual-host + host-based RPA | Classic HID接続中のRPA更新、bond済みLE再接続、両端再起動後のIRK/LTK復元を確認 |
| dual-host LE / BR-EDR連続切断 | 両handleの切断を正しいhostへ配送後、正常停止・再起動・両destructor順成功 |
| dual-host正常停止 | 両側ともNimBLE→Classic、解除時in-flight commandなし |
| dual-host再登録 | 同一instance・GATT/HID定義で反復し、heap低下なし |
| dual-host event mask union | host別要求からのunion書換えを両側で確認 |
| NimBLE継続中のClassic再attach | HCI Reset仮想完了後もGATT接続を維持し、Classic HIDを再接続 |
| dual-host任意順停止 | Classic先行停止後もNimBLE GATT継続、最後のhost解除でcontroller停止・再起動成功 |
| dual-host任意順destructor | Classic先行／NimBLE先行の両方で残存host継続、controller停止後の再起動成功 |
| dual-host異常report / peer消失 | 不正report拒否後の接続維持、突然再起動後のbond済みBLEとClassic HID復旧 |
| dual-host接続 / pairing失敗 | 誤passkey失敗後の再pairing、HID Host非同期接続失敗後のBLE維持とClassic再接続を確認 |
| dual-host HFP / SCO | BLE GATT接続中にSLC、発信、mSBC SCO双方向payload、SCO中と切断後のGATT readを確認。unknown handle / command mismatch / queue fullなし |
| dual-host A2DP / AVRCP | BLE GATT接続中にA2DP SBC media、AVRCP Play / absolute volume、stream中と切断後のGATT readを確認。ESP A2DP coexistence commandを分類しbroker異常なし |
| 通常NimBLE BLEのESP32 Peer regression | GATT read/write、反復discoveryを確認 |
| host unit test | controller policyを含むhost非依存ロジックを確認 |
| ESP32-S3 CompileSmoke | 成功、274,253 B。Classic archiveは非リンク |
| ESP32-S3 RPA regression compile | Central 640,276 B、Peripheral 641,360 B。無印ESP32専用host privacy patchは非適用 |

core内蔵hostを使った先行SPP試験も同じPeer testで成功したが、最終構成には使わない。
Arduino coreの設定はSPP有効・Classic HID無効であり、core `libbt.a`にHID Device/Host APIの
実体はない。独自archiveでは両方のAPIが存在することをsymbol検査済みである。

## 次の実装

引き継ぎ時の完了範囲、優先順位、実行コマンドは
[HANDOFF_ESP32_CLASSIC.ja.md](HANDOFF_ESP32_CLASSIC.ja.md)を正本とする。

1. dual-hostのcommand同時発行とlifecycleを、20 run・1時間41分44秒のsoakで反復済み。各runの競合／再登録100サイクルでbroker errorとheap低下はなかった。
2. 観測済みcommandをcontroller policyへ全件分類し、dual-host時の未知／別host opcodeをfail-closedにした。接続後cleanup inventoryを追加し、`Exit Sniff Mode`とClassic `Disconnect`も捕捉済み。
3. null・上限超過HID Reportの拒否とpeer突然再起動後の両transport復旧は実機確認済み。
4. HID接続失敗とBLE pairing失敗を両transport同時状態で試験済み。誤passkeyでは両側bond 0を確認してから正しいpasskeyで暗号化を復旧し、HID接続失敗後もLE GATTを維持して正しいClassic peerへ再接続した。
5. callback解除と実行が別coreで競合するlifecycle境界を監査済み。SPP/HID Device/HID Hostへ登録mutexとcallback参照数による停止barrierを追加し、clean実機lifecycle回帰を完走した。

## 配布形式の方針

現在はNimBLE hostを`src/nimble_esp32/`のソースとして同梱し、Classic-only Bluedroid hostを
`src/esp32/libespble_bluedroid_classic.a`として同梱する。このmixed distributionを次回Classic拡張の
確定方針とし、形式を揃えること自体はrelease条件にしない。NimBLEはlocal patchとtarget別条件を追跡しやすい
sourceが適し、BluedroidはKconfig、生成header、ESP-IDF component依存を固定して短いArduino buildにできる
archiveが適するためである。

Classic archiveはIDF / Arduino Core / GCC ABIをpinし、全defined symbolの名前空間化、必須API link check、
clean再生成をrelease gateとする。ESP-IDFへupstreamする場合はbinaryを提案するのではなく、host分離、HCI
broker、公開API境界のsource差分をcomponentとして切り出す。形式の再評価はABI更新、Bluedroid hostの独立
component化、またはNimBLE archive化に明確な保守上の利益が出た場合だけ行う。

Classic Audioの責務分離、external codec API、archiveへ追加するKconfig、実装順は
[Classic Audio拡張計画](PLAN_ESP32_CLASSIC_AUDIO.ja.md)を正本とする。

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

ClassicはBTDM controllerを起動した直後に`btStop()` callbackをbrokerへ委譲する。Classic先行
`end()`ではClassic profile/hostだけを停止し、NimBLEとcontrollerを維持する。最後にNimBLEを
解除したbrokerがcallbackを一度だけ実行する。Classic停止後のLE GATT read、controller停止後の
Classic→NimBLE再起動を両側で実機確認したため、明示`end()`とC++ objectの破棄順はtransport間で
制約しない。実際のdestructorもClassic先行／NimBLE先行の両順序で実行し、その後の再起動まで確認した。
NimBLE hostがOFFの初期化・停止境界ではreceive gateを閉じ、遅延eventを配送しない。
NimBLE停止開始はapp taskから直接`ble_hs_stop()`を呼ばず、NimBLE自身のevent taskへ投入する。
これによりtimer eventのdequeueとcallout停止時のqueue removeが別coreで競合する窓をなくす。

実機command inventoryではNimBLEが19 opcode、Classicが32〜34 opcodeを使用した。Classic再attachで
再発行されるHCI Reset、Set Controller To Host Flow Control、Host Buffer SizeはbrokerがClassic向けに
仮想完了し、物理flow controlを無効化したdual-host構成のHost Number Of Completed Packetsは消費する。
これにより再attach時のCommand Disallowedを除き、25回のLE GATT read後もcommand queue overflowと
opcode不一致は0だった。

Classic HID接続中のBLE pairing、bond保存、BLEだけの切断・再接続、暗号化必須GATT readも両側で成功した。
この過程で見つかったvendored NimBLEの「SM procedureなしEncryption Changeをcallbackしない」問題は、
鍵ストアからbond属性を復元してGAPへ通知する生成patchとして`tools/vendor_nimble_esp32.py`へ記録した。
