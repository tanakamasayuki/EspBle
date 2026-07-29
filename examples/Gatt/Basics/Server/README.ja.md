# Server

> English: [README.md](README.md)

Read/Write可能なCharacteristicとDescriptorを1つずつ持つ独自GATT Serviceを登録し、advertiseします。Characteristicは応答あり/なし両方のWriteに対応します。あわせて、**読まれた瞬間に値を作る**Characteristicも1つ持ちます。

2台目のボードで[Gatt/Client](../Client/) example（同じUUIDを対象にしています）を動かすか、nRF Connectなどの汎用GATT Clientアプリから操作できます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral / GATT Server）
- GATT Client × 1（Gatt/Clientを動かす2台目のボード、またはスマートフォンアプリ）

## 動作

- `begin()`前にService `10da4dd0-…`、Characteristic `10da4dd1-…`、Descriptor `10da4dd2-…`、Read専用の`10da4dd3-…`を登録します
- 初期値を`ready`に設定します
- Clientからの書込みをConnection IDと一緒に表示します
- `10da4dd3-…`が読まれたときは、その場で `millis()` を値にして返します
- ClientがみつけられるようにService UUIDをadvertiseします

## ハンドルで組み立てる

登録は**3段のハンドル連鎖**になります。`addService()` が返すハンドルを `addCharacteristic()` に渡し、それが返すハンドルを `addDescriptor()` に渡す、という形です。

```cpp
const EspBleGattService service = gattServer.addService(SERVICE_UUID);
characteristic = gattServer.addCharacteristic(service, CHARACTERISTIC_UUID, valueConfig);
descriptor = gattServer.addDescriptor(characteristic, DESCRIPTOR_UUID, descriptorConfig);
```

以降の値設定・送信・イベント判定はすべてこのハンドルで行い、UUIDでは指定しません。**UUIDは「型」であって「どれか」を表さない**ためです。仕様上、1台が同じUUIDのServiceを複数持てますし、Client側から見れば同じUUIDのCharacteristicが並ぶ相手（HIDのReportなど）も普通にあります。

ハンドルはグローバル変数などに保持してください。失敗すると無効なハンドルが返るので、`valid()` で判定できます。

## 読まれた瞬間に値を作る

`setValue()` で先に値を置いておく形は、値が変わったときにこちらが更新できる場合に向きます。センサーのように「読まれた時点の値」を返したい場合は、`onRead()` を使います。

```cpp
gattServer.onRead([](const EspBleGattReadRequest &request) {
  if (request.characteristic != liveCharacteristic) return;
  ble.gattServer().setValue(liveCharacteristic, String(millis()));
});
```

コールバックの中で `setValue()` した値が、そのまま相手へ返ります。定期的に `setValue()` を呼び続ける必要がなくなり、**誰も読まないなら値を作る処理自体が走りません**。

**このコールバックだけは `update()` ではなくBLEスタックのタスクで走ります。** ATTの読み取り応答を返す前に値が必要で、後回しにできる場所がないためです。したがって次の2点に注意してください。

- **短く保つこと。** ここで待たせるとスタック全体が止まり、相手からは読み取りのタイムアウトに見えます。Serial出力もこの中では避けてください
- **`loop()` と同時に走ります。** 共有変数を触るなら、他のコールバックと違って排他制御が必要です

## 主なAPI

- `ble.gattServer().addService(uuid)` — Serviceを登録してハンドルを返す。`begin()`前に呼ぶ必要があります
- `addCharacteristic(service, uuid, config)` — Serviceのハンドルを渡してCharacteristicを登録し、そのハンドルを返します
- `EspBleGattCharacteristicConfig` — `readable`、`writable`のほか`notifiable`、`indicatable`、暗号化/認証permission
- `addDescriptor(characteristic, uuid, config)` / `EspBleGattDescriptorConfig` / `setDescriptorValue(descriptor, value)` — Descriptor定義、permission、binary-safeな値
- `gattServer.setValue(characteristic, value)` / `gattServer.value(characteristic, out)` — 保持値（binary-safeな`String`。pointer+length overloadもあります）
- `gattServer.onWritten(callback)` — `connectionId`、書き込まれたCharacteristicのハンドル、値を持つ`EspBleGattWrite`
- `gattServer.onRead(callback)` — 読み取り要求。`EspBleGattReadRequest`は`connectionId`と対象のハンドルを持ちます
- `gattServer.onDescriptorWritten(callback)` — Descriptorのハンドルと値を持つ`EspBleGattDescriptorWrite`

## 注意

- **コールバックは全Characteristic共通です。** 複数登録している場合は `write.characteristic == myHandle` で対象を判定してください。イベントにはUUID文字列も入っていますが、同じUUIDが複数あると区別できないため、ハンドルで比べるのが確実です。
- **1つのServiceの中に同じUUIDのCharacteristicを2つ置くことはできません。** 同梱backendがGATTへ登録せず既存を再利用してしまうため、`addCharacteristic()` が無効なハンドルを返して拒否します。黙って送信が届かない状態を避けるための挙動です。
- **`onRead()` は1つだけです。** 他のイベントのように `add*Listener()` で複数登録することはできません。「返す値を作る」責任を持てるのは1箇所だけだからです。
- **MTUを超える値は分割して読まれます。** ATTの1回の応答に収まらない場合、Clientは続きを要求します（Read Long）。Server側は値を置くだけで、分割はスタックが扱います。
- 登録はすべて `begin()` より前に行う必要があります。`begin()` 後の `addService()` は `InvalidState` で失敗します。

## 期待されるSerial出力

```
Connection 1 wrote: hello from Central
Descriptor 10da4dd2-8eaa-4c69-9003-676174747277 wrote: descriptor value
```
