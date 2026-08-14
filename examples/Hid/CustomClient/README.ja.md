# CustomClient

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 6章「HID編 — キーボードやマウスとして振る舞う」

**汎用GATTクライアント**（Central）でCustom HIDデバイスの任意Report Descriptorを読み、Reportを駆動します。[CustomDevice](../CustomDevice/) exampleとペアです。

HIDデバイスは同一UUID `0x2A4D` のReport characteristicを複数持つため、対象はすべて個別の **attribute handle** で指定します。各Reportの役割は、**Report Reference descriptor**（`0x2908`、report ID 1byte＋type 1byte: 1=Input / 2=Output / 3=Feature）から読みます——HIDが本来そう宣言しているからです。そのdescriptorの指定もhandleで行います。Report Referenceはどれも「`0x2A4D` のcharacteristicの下の `0x2908`」なので、Service/Characteristic/Descriptor UUIDの組では**全部に一致してどれにも特定できません**。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central / GATTクライアント）
- [CustomDevice](../CustomDevice/) を動かすESP32-S3 × 1（HID Device / Peripheral）

## 動作

- HID Service（`0x1812`）をadvertiseするデバイスをactive scanで探して接続します
- 接続時にserviceをdiscoverし、完了後に各 `0x2A4D` characteristicを自分の `0x2908` descriptorと対応付けます。descriptorは1つのcharacteristicに属し、その紐付けは持ち主の値ハンドル（`EspBleGattDescriptorInfo::characteristicHandle`）です
- 各Report Referenceを**handle指定で**読みます。操作は自動でキューへ積まれ順に実行されるため、まとめて発行できます
- type byteで役割を決め、Input Reportはhandleで購読し、Output Reportのhandleは書き込み用に保持します
- 2byteの入力Report（符号付きダイヤル差分＋ボタン）をデコードします
- `o` で1byteの出力Report（`0x02`、LED状態）をhandleで書き込み

## 主なAPI

- `ble.discoverServices(connectionId)` / `ble.onServicesDiscovered(cb)` — GATT Discoveryの起動と受信
- `ble.discoveredCharacteristicCount(connectionId, serviceUuid)` / `ble.discoveredCharacteristic(connectionId, index, info, serviceUuid)` — characteristicを列挙。`EspBleGattCharacteristicInfo` は `characteristicUuid`、`handle`、`notifiable`、`writable` を持つ
- `ble.discoveredDescriptorCount(...)` / `ble.discoveredDescriptor(...)` — descriptorを列挙。`EspBleGattDescriptorInfo` は `descriptorUuid`、`handle`、そして持ち主の `characteristicHandle` を持つ
- `ble.readDescriptor(connectionId, descriptorHandle)` / `ble.onDescriptorRead(cb)` — descriptorをattribute handleで読む。結果の `descriptorHandle` が読んだdescriptor、`handle` がそれを持つcharacteristic
- `ble.subscribe(connectionId, handle, true)` — attribute handleで購読
- `ble.onNotification(cb)` — 送信元 `handle` と `value` を持つ `EspBleGattNotification`
- `ble.writeCharacteristic(connectionId, handle, data, length, response)` — handleで書込み

## メモ

- CustomDeviceはsecurity有効で動作するため、bondingしないクライアントは拒否される場合があります。暗号化なしで試すにはデバイス側のsecurityを無効化する（またはこのクライアントにbondingを追加する）ようにしてください。
- discoverされるUUIDは128-bit形式（`0000XXXX-...`）で返るため、sketchは16-bit短縮形とどちらでも一致させます。
- UUID指定の `readDescriptor(connectionId, serviceUuid, characteristicUuid, descriptorUuid)` も用意されており、characteristicのUUIDが一意なときはそちらが素直です。ここでは使えません——最初に見つかった `0x2A4D` に一致してしまい、それが目的のReportとは限らないためです。
- propertyから推測せずtypeを読むのは、**Output ReportもFeature Reportもwritable**で区別できないからです。propertyにも意味はあります（Write Without Responseを持つのはOutputだけ。Featureは設定なので必ず応答付き書き込みになる）が、デバイスが実際に宣言しているのはtype byteです。

## 期待されるSerial出力

```
Scanning for a Custom HID device. Send 'o' to write the output LED report.
Reading 2 Report Reference descriptors
Input report: id=1 handle=42
Output report: id=1 handle=45
Input report: dial delta=5 buttons=1
```

## 関連するガイド

- [BLE入門ガイド §6 HID編](../../../docs/GUIDE_BLE_BASICS.ja.md#6-hid編--キーボードやマウスとして振る舞う) — reportとdescriptor、Hostが期待するもの
- [HID Report Descriptorを書く](../../../docs/GUIDE_HID_DESCRIPTORS.ja.md) — 自作descriptorの書き方と確かめ方
