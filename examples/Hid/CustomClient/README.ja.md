# CustomClient

> English: [README.md](README.md)

**汎用GATTクライアント**でCustom HIDデバイスの任意Report Descriptorを読み、Reportを駆動します。[Hid/CustomDevice](../CustomDevice/) とペアです。

HIDデバイスは同一UUID `0x2A4D` のReport characteristicを複数公開します。特定の1つを指定するため、この例はserviceをdiscoverし、`discoveredCharacteristic()` で各Reportを個別の **attribute handle** へ解決してから、入力Reportの購読と出力Reportの書き込みを **handleで** 行います（`subscribe(connectionId, handle, …)` / `writeCharacteristic(connectionId, handle, …)`）。通知は送信元の `handle` を持つため、入力Reportを一意に判別できます。

## ハードウェア

- ESP32-S3 × 1（このスケッチ。central / GATTクライアント）
- ESP32-S3 × 1（Hid/CustomDevice。HIDデバイス / peripheral）

## 動作

- HID Service（`0x1812`）をadvertiseするデバイスを探して接続
- serviceをdiscoverし、入力・出力Report characteristicをhandleへ解決
- 入力Reportをhandleで購読し、2byteのReport（ダイヤル差分＋ボタン）をデコード
- Serialで `o` を送ると1byteの出力Reportをhandleで書き込む

## 補足

- CustomDeviceの例はsecurity有効で動作するため、bondingしないクライアントは拒否される場合があります。暗号化なしで試すにはデバイス側のsecurityを無効化する（またはこのクライアントにbondingを追加する）ようにしてください。
