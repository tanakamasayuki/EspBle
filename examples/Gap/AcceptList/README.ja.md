# AcceptList

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 2章「GAP編 — 探してつながる」

1つの **Filter Accept List**（旧称 white list）を**2通りに使う**例です。**接続してくる相手を制限する**（advertising側）のと、**スキャンで報告される相手を絞り込む**（scan側）の両方を、同じリストで行います。

BLEには「接続要求が来たので相手を見て承認/拒否する」というcallbackは**存在しません**。接続の可否はコントローラが **Filter Accept List**（旧称 white list）と照合して決め、拒否された相手のことはアプリケーションに一切届きません。したがって選択肢は次の3つになります。

| 手段 | 説明 |
|---|---|
| **Filter Accept List** | このexample。コントローラが弾くので最も確実で、アプリに負荷もかからない |
| 接続後に切断する | `onConnected` で相手を見て `disconnect()` する。一度は接続が成立してしまう |
| 属性側で守る | Characteristicに暗号化/認証を要求する（[Security/*](../../Security/)）。接続は許すが値は守る |

用途に応じて組み合わせます。「そもそも繋がせたくない」ならFilter Accept List、「繋がせるが値は守りたい」なら暗号化です。

accept listはコントローラが持つ1本のリストで、**advertisingとscanで共通**です。scan側では `EspBleScanConfig::acceptListOnly` を立てると、リストに載っていない相手のadvertisementはコントローラが捨て、`onResult` まで届きません。「この機器とだけやり取りする」構成では、同じリストが両方向に効きます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral）
- 相手のボード × 1 — 接続を試みるCentral（[Gap/Connect](../Connect/)）、またはadvertiseするPeripheral（[Gap/Advertise](../Advertise/)）。スマホアプリでも可

sketch内の `ALLOWED_PEER` を、**許可したい相手のアドレス**に書き換えてから使ってください。相手のアドレスは、そのボードで `ble.localAddress()` を表示させれば分かります。書き換えないままだと誰も接続できず、絞り込みscanには何も出ません（それ自体、フィルタが効いていることの確認にはなります）。

## 動作

- 許可アドレスをaccept listへ登録し、`ConnectionFromAcceptList` policyでadvertiseします
- accept listにいない相手からの接続要求はコントローラが黙って捨てます。相手側は接続がタイムアウトします
- `o` を送るとpolicyを`Any`に戻して誰でも接続可能になり、`r` で再び制限します
- `f` は `acceptListOnly` を立てた5秒間のscanを開始し、許可アドレスのadvertisementだけを表示します
- `a` は同じscanをフィルタなしで行います。周囲のすべてのadvertiserが並ぶので、`f` との差がそのまま絞り込みの効果です

## 主なAPI

- `ble.addToAcceptList(address, addressType)` — accept listへ追加する（最大8件）
- `ble.removeFromAcceptList(address, addressType)` / `ble.clearAcceptList()`
- `ble.acceptListCount()` / `ble.acceptListEntry(index, entry)`
- `ble.advertising().setFilterPolicy(policy)` — `Any` / `ScanRequestFromAcceptList` / `ConnectionFromAcceptList` / `Both`（advertising側）
- `EspBleScanConfig::acceptListOnly` — 同じリストをscan側に適用する

## 注意

- **リストはadvertisingとscanで共通です。** 片方のためにaddしたエントリは、もう片方にも効きます。別々のリストは持てません（コントローラに1本しかないためです）。
- **policyの変更はadvertisingの開始時に反映されます。** 動作中に変える場合はこのexampleのように `stop()` → `setFilterPolicy()` → `start()` としてください。
- **照合はアドレス単位です。** RPAを回転させる相手は、bondingしてidentity addressが使えるようになるまで意味のある登録ができません（[Gap/PrivateAddress](../PrivateAddress/)参照）。
- **accept listが空の状態で制限policyにすると、誰も接続できません。** 意図的にロックする用途にも使えますが、事故には注意してください。
- 拒否された相手には「拒否された」と伝わりません。Link Layerに拒否を返すPDUが無く、コントローラは接続要求を黙って捨てるだけだからです。相手側からは応答が来ないまま接続がタイムアウトしたように見えます。

## 期待されるSerial出力

```
Advertising. Only aa:bb:cc:dd:ee:ff may connect.
Commands: 'o' open policy, 'r' restrict, 'f' filtered scan, 'a' scan everyone
Scanning for 5 s (accept list only)
Advertiser aa:bb:cc:dd:ee:ff rssi=-41 (filtered scan)
Policy: open (accept list has 1 entries)
Connected id=1 from aa:bb:cc:dd:ee:ff
Disconnected id=1
```

## 関連するガイド

- [BLE入門ガイド §2 GAP編](../../../docs/GUIDE_BLE_BASICS.ja.md#2-gap編--探してつながる) — advertising・scan・接続
