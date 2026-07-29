# DirectedAdvertise

> English: [README.md](README.md)

**相手を1台に限定してadvertiseする**Peripheral側の例です。

通常のadvertisingが「誰でもどうぞ」と放送するのに対し、**Directed Advertising**はPDUに宛先アドレスを載せ、**その相手だけが接続できる**状態を作ります。主な用途はボンディング済みの相手への素早い再接続です。

**payloadを一切載せられない**のがこの方式の最大の特徴です。BLEの仕様上、有向advertisingのPDUは送信元と宛先の2つのアドレスだけを運びます。名前もService UUIDも送られないため、相手は「スキャンして見つけて接続する」のではなく**アドレスを指定して接続する**ことになります。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral）
- 接続するCentral — [Gap/Connect](../Connect/)を動かす2台目のボード、またはスマホアプリ

sketch内の `TARGET_CENTRAL` を**advertise先Centralのアドレス**に、`TARGET_TYPE` をそのアドレス種別に書き換えてから使ってください。相手のアドレスは、そのボードで `ble.localAddress()` / `ble.localAddressType()` を表示させれば分かります。

## 動作

- まず**無向**でadvertiseします。Centralに一度見つけてもらい、アドレスを学習させるためです
- `d` を送ると `TARGET_CENTRAL` 宛の**有向**advertisingへ切り替わります。payloadは送られません
- Central側は、スキャン結果ではなく**アドレス指定**（`ble.connect(address, addressType)`）で接続します
- `u` を送ると無向advertisingへ戻ります。payloadは有向中も保持されており、送信されていなかっただけです

## 主なAPI

- `ble.advertising().setDirectedTarget(address, addressType, highDuty)` — 宛先を指定する
- `ble.advertising().clearDirectedTarget()` — 通常のadvertisingへ戻す
- `ble.localAddress()` / `ble.localAddressType()` — 自分のアドレスを相手へ伝えるために使う

## 注意

- **payloadは送信されません。** 名前・Service UUID・Manufacturer Dataのいずれも載りません。BLEの仕様であり、ライブラリの制限ではありません。
- **相手がRPA（Resolvable Private Address）を使う場合は、識別用アドレスを指定します。** 解決はボンド情報を使って行われるため、**先にbondingが必要**です（[Gap/PrivateAddress](../PrivateAddress/)、[Security/Bonding](../../Security/Bonding/)参照）。
- **High Duty Cycle（第3引数 `true`）は1.28秒で自動的に止まります。** 3.75ミリ秒間隔で送出するため、既知の相手へ最速で再接続できる代わりに、長く出し続けることはできません。既定の `false` なら `setInterval()` の設定に従い、`stop()` するまで続きます。
- **接続が成立するとadvertisingは止まります。** 続けるなら `onDisconnected` で `start()` を呼び直してください。
- 「特定の相手だけを接続させる」だけが目的で、再接続の速さが不要なら、通常のadvertisingに[Gap/AcceptList](../AcceptList/)を組み合わせる方が扱いやすくなります。相手はスキャンでこのデバイスを見つけられます。

## 期待されるSerial出力

```
Advertising as d0:cf:13:58:fd:94. Send 'd' to direct it at aa:bb:cc:dd:ee:ff.
Directed at aa:bb:cc:dd:ee:ff. No payload is sent.
Connected id=1 from aa:bb:cc:dd:ee:ff
Disconnected id=1
Undirected: anyone may connect.
```
