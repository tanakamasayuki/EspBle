# CustomClient

> English: [README.md](README.md)

**汎用GATTクライアント**（Central）でCustom HIDデバイスの任意Report Descriptorを読み、Reportを駆動します。[CustomDevice](../CustomDevice/) exampleとペアです。HIDデバイスは同一UUID `0x2A4D` のReport characteristicを複数持つため、この例はHID Service（`0x1812`）をdiscoverして各Reportを個別の **attribute handle** へ解決し、入力Reportをhandleで購読、出力Reportをhandleで書き込みます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central / GATTクライアント）
- [CustomDevice](../CustomDevice/) を動かすESP32-S3 × 1（HID Device / Peripheral）

## 動作

- HID Service（`0x1812`）をadvertiseするデバイスをactive scanで探して接続します
- 接続時にserviceをdiscoverし、完了後にHID Serviceのcharacteristicを走査してnotifiableなものを `inputHandle`、writableなものを `outputHandle` に解決します
- 入力Reportをhandleで購読し、2byteのReport（符号付きダイヤル差分＋ボタン）をデコードします
- `o` で1byteの出力Report（`0x02`、LED状態）をhandleで書き込み

## 主なAPI

- `ble.discoverServices(connectionId)` / `ble.onServicesDiscovered(cb)` — GATT Discoveryの起動と受信
- `ble.discoveredCharacteristicCount(connectionId, serviceUuid)` / `ble.discoveredCharacteristic(connectionId, index, info, serviceUuid)` — characteristicを列挙。`EspBleGattCharacteristicInfo` は `characteristicUuid`、`handle`、`notifiable`、`writable` を持つ
- `ble.subscribe(connectionId, handle, true)` — attribute handleで購読
- `ble.onNotification(cb)` — 送信元 `handle` と `value` を持つ `EspBleGattNotification`
- `ble.writeCharacteristic(connectionId, handle, data, length, response)` — handleで書込み

## メモ

- CustomDeviceはsecurity有効で動作するため、bondingしないクライアントは拒否される場合があります。暗号化なしで試すにはデバイス側のsecurityを無効化する（またはこのクライアントにbondingを追加する）ようにしてください。
- discoverされるUUIDは128-bit形式（`0000XXXX-...`）で返るため、sketchは16-bit短縮形とどちらでも一致させます。

## 期待されるSerial出力

```
Scanning for a Custom HID device. Send 'o' to write the output LED report.
Resolved input handle=42, output handle=45
Input report: dial delta=5 buttons=1
```
