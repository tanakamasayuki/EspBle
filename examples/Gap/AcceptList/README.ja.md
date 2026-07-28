# AcceptList

> English: [README.md](README.md)

**接続してくる相手を制限する**Peripheral側の例です。

BLEには「接続要求が来たので相手を見て承認/拒否する」というcallbackは**存在しません**。接続の可否はコントローラが **Filter Accept List**（旧称 white list）と照合して決め、拒否された相手のことはアプリケーションに一切届きません。したがって選択肢は次の3つになります。

| 手段 | 説明 |
|---|---|
| **Filter Accept List** | このexample。コントローラが弾くので最も確実で、アプリに負荷もかからない |
| 接続後に切断する | `onConnected` で相手を見て `disconnect()` する。一度は接続が成立してしまう |
| 属性側で守る | Characteristicに暗号化/認証を要求する（[Security/*](../../Security/)）。接続は許すが値は守る |

用途に応じて組み合わせます。「そもそも繋がせたくない」ならFilter Accept List、「繋がせるが値は守りたい」なら暗号化です。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral）
- 接続を試みるCentral — [Gap/Connect](../Connect/)を動かす2台目のボード、またはスマホアプリ

sketch内の `ALLOWED_CENTRAL` を、**接続を許可したいCentralのアドレス**に書き換えてから使ってください。相手のアドレスは、そのボードで `ble.localAddress()` を表示させれば分かります。書き換えないままだと誰も接続できません（それ自体、フィルタが効いていることの確認にはなります）。

## 動作

- 許可アドレスをaccept listへ登録し、`ConnectionFromAcceptList` policyでadvertiseします
- accept listにいない相手からの接続要求はコントローラが黙って捨てます。相手側は接続がタイムアウトします
- `o` を送るとpolicyを`Any`に戻して誰でも接続可能になり、`r` で再び制限します

## 主なAPI

- `ble.addToAcceptList(address, addressType)` — accept listへ追加する（最大8件）
- `ble.removeFromAcceptList(address, addressType)` / `ble.clearAcceptList()`
- `ble.acceptListCount()` / `ble.acceptListEntry(index, entry)`
- `ble.advertising().setFilterPolicy(policy)` — `Any` / `ScanRequestFromAcceptList` / `ConnectionFromAcceptList` / `Both`

## 注意

- **policyの変更はadvertisingの開始時に反映されます。** 動作中に変える場合はこのexampleのように `stop()` → `setFilterPolicy()` → `start()` としてください。
- **照合はアドレス単位です。** RPAを回転させる相手は、bondingしてidentity addressが使えるようになるまで意味のある登録ができません（[Gap/PrivateAddress](../PrivateAddress/)参照）。
- **accept listが空の状態で制限policyにすると、誰も接続できません。** 意図的にロックする用途にも使えますが、事故には注意してください。
- 拒否された相手には「拒否された」と伝わりません。Link Layerに拒否を返すPDUが無く、コントローラは接続要求を黙って捨てるだけだからです。相手側からは応答が来ないまま接続がタイムアウトしたように見えます。

## 期待されるSerial出力

```
Advertising. Only aa:bb:cc:dd:ee:ff may connect.
Policy: open (accept list has 1 entries)
Connected id=1 from d0:cf:13:58:fd:95
Disconnected id=1
```
