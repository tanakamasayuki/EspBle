# Arduino-ESP32 NimBLE GATT client discoveryのメモリリーク報告案

## 対象

- repository: `espressif/arduino-esp32`
- 確認version: 3.3.10（NimBLE backend）
- 対象: `libraries/BLE/src/BLEClient.cpp` / `BLERemoteService.cpp` ほかGATT client discovery経路

## 問題

Central（GATT client）で、接続 → GATT service/characteristic discovery → 切断 を繰り返すと、
1サイクルあたり一定量のヒープが解放されずに積み上がります。EspBleのPeerテスト
`lifecycle_stress::test_reconnect_cycles_do_not_leak_heap`（connect + HID discovery + disconnect を反復）で
`ESP.getFreeHeap()`が線形に減少することを確認しました（12サイクルで約31 KB、約2.6 KB/cycle、頭打ちなし）。

切り分け結果:

- **discoveryを行わない** connect/disconnect の反復ではリークしない（ヒープはノイズ範囲でほぼ一定）。
- **discoveryを行う** 反復でのみリークし、その量は **discoverしたcharacteristic数に比例** する
  （HID Serviceにcharacteristicを3つ追加したところ、約 <1.3 KB/cycle から約 2.6 KB/cycle へ増加）。
- ライブラリ利用側（EspBle）の`BLEClient`寿命は正しく、生成したclientは`~BLEClient`で解放され、
  その`m_servicesMap`（remote service tree）も破棄されます（5生成/4解放、最新1個のみ保持のbounded運用を確認）。
  それでもヒープが積み上がるため、**解放漏れは`BLEClient`が所有するC++オブジェクトの外側**（NimBLE host内部の
  discovery関連確保）にあると考えられます。
- `BLEClient::connect()`は先頭で`clearServices()`を呼ぶため、同一clientを再利用しても再接続ごとに
  再discoveryが走り、リークを回避できません。

## 影響

多数のperipheralへ順に接続してdiscoveryする用途や、再接続を長時間繰り返す用途で、
ヒープが徐々に減少します。EspBle側では回避手段がありません（上記のとおりbackendが再discoveryを強制し、
解放も`~BLEClient`だけで完結しているため）。

## 希望する調査・修正

NimBLE GATT client discovery（`ble_gattc_disc_*`のコールバックで確保される
BLERemoteService/Characteristic/Descriptor以外のhost内部確保、または`clearServices()`で
解放されない経路）のメモリが、切断・`~BLEClient`時に確実に解放されるようにしてください。

## EspBle側の緩和

- HID KeyboardのBoot Protocol characteristic群は既定offのopt-inとし、通常のdiscovery量を増やさない。
- 将来のreconnect cache機能では、同一peerへの再接続時にEspBleレベルのdiscovery cacheを用いて
  backendの再discovery自体を避けることで、同一peer反復シナリオでの本リークを踏まないようにする方針。
