# Server

> English: [README.md](README.md)

Read/Write可能なCharacteristicとDescriptorを1つずつ持つ独自GATT Serviceを登録し、advertiseします。Characteristicは応答あり/なし両方のWriteに対応します。

2台目のボードで[Gatt/Client](../Client/) example（同じUUIDを対象にしています）を動かすか、nRF Connectなどの汎用GATT Clientアプリから操作できます。

## ハードウェア

- このsketchを動かすESP32-S3 × 1（Peripheral / GATT Server）
- GATT Client × 1（Gatt/Clientを動かす2台目のボード、またはスマートフォンアプリ）

## 動作内容

- `begin()`前にService `10da4dd0-…`、Characteristic `10da4dd1-…`、Descriptor `10da4dd2-…`を登録します
- 初期値を`ready`に設定します
- Clientからの書込みをConnection IDと一緒に表示します
- ClientがみつけられるようにService UUIDをadvertiseします

## 主なAPI

- `ble.gattServer().addService(uuid)` / `addCharacteristic(serviceUuid, characteristicUuid, config)` — `begin()`前に呼ぶ必要があります
- `EspBleGattCharacteristicConfig` — `readable`、`writable`のほか`notifiable`、`indicatable`、暗号化/認証permission
- `addDescriptor()` / `EspBleGattDescriptorConfig` / `setDescriptorValue()` — Descriptor定義、permission、binary-safeな値
- `gattServer.setValue(...)` / `gattServer.value(...)` — 保持値（binary-safeな`String`。pointer+length overloadもあります）
- `gattServer.onWritten(callback)` — `connectionId`、UUID、書込み値を持つ`EspBleGattWrite`
- `gattServer.onDescriptorWritten(callback)` — UUIDと値を持つ`EspBleGattDescriptorWrite`

## 期待されるSerial出力

```
Connection 1 wrote: hello from Central
Descriptor 10da4dd2-8eaa-4c69-9003-676174747277 wrote: descriptor value
```
