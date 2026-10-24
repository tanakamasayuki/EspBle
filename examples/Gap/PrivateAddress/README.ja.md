# PrivateAddress

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 2章「GAP編 — 探してつながる」

`EspBleConfig::ownAddressType`で選択し、工場出荷のpublic addressの代わりにprivate addressでadvertiseします。connectableなPeripheralの例です。2台目のボードで組み合わせる[Scan](../Scan/) exampleでaddress typeを確認できます。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral）
- BLEスキャナ — 2台目のボードで[Scan](../Scan/) example、またはnRF Connect等のスキャナアプリ

## 2つのモード

sketch冒頭の `USE_RESOLVABLE_PRIVATE_ADDRESS` で切り替えます。

| | RandomStatic（既定） | ResolvablePrivate（RPA） |
|---|---|---|
| アドレス | `begin()`で生成する固定random | controllerが周期的に変える |
| 追跡耐性 | public addressは隠せるが、この固定値で追跡はできる | 回転するので追跡されにくい |
| bonding | 不要 | **必須**。peerはIRKでアドレスを解決する |
| 単体で動くか | 動く | securityなしでは相手が再接続できない |

RPAの回転周期は同梱NimBLEの `CONFIG_BT_NIMBLE_RPA_TIMEOUT`（900秒）で固定されており、アプリからは変更できません。

## 動作

- 選んだモードに応じて `config.ownAddressType` を設定します。RPAモードでは `config.security.enabled` / `bonding` も併せて有効にします
- connectableなPeripheralとしてadvertiseし、スキャナがaddress typeを観測できるようにします
- 接続時に相手のアドレスとbonded状態を表示します

## 主なAPI

- `EspBleConfig::ownAddressType` — `Public`（既定） / `RandomStatic` / `ResolvablePrivate`
- `EspBleConfig::security.enabled` / `bonding` — RPAを使う場合に必要
- `EspBleConnection::peerAddress` / `bonded` — 相手側から見えたアドレスとbonding状態

## メモ

- `Public` — 工場出荷のpublic address。`RandomStatic` — `begin()`で生成する固定random static address（回転しない固定identity）。`ResolvablePrivate` — controllerが周期回転させるRPA（`CONFIG_BT_NIMBLE_RPA_TIMEOUT`、同梱ビルドで900秒）。bonded peerがIRKで解決するためsecurity/bonding併用時のみ有用で、未bondのスキャナには変化するrandom addressにしか見えません。
- スキャナからはaddress typeが**Random**（Publicではない）に見えます。static random addressは先頭octetの上位2bitが`0b11`です。
- Extended / Periodic Advertisingは同梱NimBLEが`CONFIG_BT_NIMBLE_EXT_ADV`無効でビルドされているため利用できません。

- accept listはアドレスで照合するため、RPAを使う相手はbondingしてidentity addressが効くようになるまで登録できません（[AcceptList](../AcceptList/)参照）。

## 期待されるSerial出力

```
Advertising with a random static address.
Connected id=1 peer=d0:cf:13:58:fd:95 bonded=0
Disconnected id=1
```

## 関連するガイド

- [BLE入門ガイド §2 GAP編](../../../docs/GUIDE_BLE_BASICS.ja.md#2-gap編--探してつながる) — advertising・scan・接続

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[PrivateAddress.ino](PrivateAddress.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[ScanDump](../../Info/ScanDump/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

PrivateAddress - 工場出荷のpublic addressの代わりにprivate addressでadvertiseし、 周囲からアドレスで追跡されないようにする。 RandomStatic      = 固定random address（public addressを隠すが回転はしない） ResolvablePrivate = controllerが回転させるRPA。bonded peerはbonding時に交換した IRKで解決でき、それ以外からは変化するアドレスに見える USE_RESOLVABLE_PRIVATE_ADDRESS で切り替える。Gap/ScanやInfo/ScanDumpで受信すると どちらもaddress typeはRandomになる。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。

## 確認と変更のポイント

切り替え設定でStatic RandomとResolvable Privateを比較します。アドレスの秘匿とデータの暗号化は別です。回転するアドレスへの再接続では必要なsecurity／bonding設定を保ちます。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
