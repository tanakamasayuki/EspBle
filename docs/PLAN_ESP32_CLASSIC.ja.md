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
| dual-host command inventory / flow-control仮想化 | 1 passed（NimBLE 19種、Classic 32〜34種、再attach時Reset 1件＋flow設定2件を仮想完了） |
| dual-host security / bonding | 1 passed（Classic HID接続中にpairing・bond保存、BLE bond再接続、暗号化必須GATT read、両側`encrypted=1 bonded=1`） |
| host-based RPA / bond再起動 | 1 passed（初回pairing、両端再起動後のLTK復元、暗号化GATT、復元bond削除、再pairing。scan/connectionはOTA RPAを維持） |
| dual-host + host-based RPA | 1 passed（Classic HID ACL接続中に双方RPAでpairing、暗号化GATT、bond済みLE再接続。2秒timeoutでPeripheral advertisingとCentral scan双方のpreempt・再開およびRPA rotationを検証。両端再起動後もNVSのIRK/LTKから復元） |
| dual-host LE / BR-EDR連続切断 | 1 passed（両handleの切断を正しいhostへ配送後、正常停止・再起動・両destructor順成功） |
| dual-host正常停止 | 1 passed（両側ともNimBLE→Classic、解除時in-flight command 0） |
| dual-host再登録 | 1 passed（同一instance・GATT/HID定義でClassic→NimBLE再起動→正常停止を100サイクル、両基板とも102点のheap測定でfree heap減少0 byte） |
| dual-host event mask union | 1 passed（両側ともmask command 4、host要求からのunion書換え1） |
| NimBLE継続中のClassic再attach | 1 passed（HCI Reset仮想完了後も同じGATT接続を維持し、Classic HID再接続・双方向通信成功） |
| dual-host任意順停止 | 1 passed（Classic先行停止後もNimBLE GATT継続、最後のhost解除でcontroller停止・再起動成功） |
| dual-host任意順destructor | 1 passed（Classic先行／NimBLE先行の両方で残存host継続、controller停止後の再起動成功） |
| 通常NimBLE BLEのESP32 Peer regression | 2 passed（GATT read/write、反復discovery） |
| host unit test | 10 passed（controller policyのGeneral / Page 2 / LE mask独立cacheを含む） |
| ESP32-S3 CompileSmoke | 成功、274,253 B。Classic archiveは非リンク |
| ESP32-S3 RPA regression compile | Central 640,276 B、Peripheral 641,360 B。無印ESP32専用host privacy patchは非適用 |

core内蔵hostを使った先行SPP試験も同じPeer testで成功したが、最終構成には使わない。
Arduino coreの設定はSPP有効・Classic HID無効であり、core `libbt.a`にHID Device/Host APIの
実体はない。独自archiveでは両方のAPIが存在することをsymbol検査済みである。

## 次の実装

1. dual-hostのcommand同時発行と実機queue overflowを長時間反復する。
2. controller / hostの任意順停止・再登録を数時間級のsoakで反復する。100サイクルのheap記録では減少0 byteを確認済み。
3. 取得済みcommand inventoryを基に、未処理のcontroller-wide設定と接続単位commandを仕様表へ分類する。
4. HID接続失敗と異常長Reportを両transport同時状態で試験する。

## 将来の配布形式統一

現在はNimBLE hostを`src/nimble_esp32/`のソースとして同梱し、Classic-only Bluedroid hostを
`src/esp32/libespble_bluedroid_classic.a`として同梱している。機能とlifecycleが安定した段階で、
両hostを同じ配布形式へ統一する。候補は次の二つで、現時点では決定しない。

- 両方をソース同梱: toolchainへの固定を弱め、変更・解析・ESP-IDF upstream化を容易にできる。
  一方でBluedroidの大量sourceがArduino build時間とライブラリ解決へ与える影響を測る必要がある。
- 両方を再現可能な`.a`として同梱: Arduino buildは軽いが、Arduino Core / ESP-IDF / GCC ABIへの
  厳密なversion pin、全symbol名前空間化、設定値とarchiveの一致検査が必要になる。

判断条件は、ESP32以外のclean build時間、無印ESP32のflash/RAM、再生成物の再現性、debuggability、
複数Arduino Core versionの互換matrix、ESP-IDF componentとして切り出す際の差分量とする。
形式を切り替えるまでは現在の二方式を正とし、機能検証と配布形式変更を同時に行わない。

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
