# ScanResponse

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 2章「GAP編 — 探してつながる」

広告データを **advertising payload** と **scan response payload** の2面に分ける例です。

Legacy advertisingのpayloadは31byteしかありません。しかしscannerが**active scan**を行うと、advertiserへScan Requestを送り、advertiserはもう1つの31byteである**Scan Response**を返します。つまり合計62byteまで使えます。

| | advertising payload | scan response payload |
|---|---|---|
| 誰に届くか | 近くの**全員**（passive scanでも見える） | **active scan**で要求してきた相手だけ |
| 置くべきもの | 相手を判別するための最小限（Service UUIDなど） | 説明的な項目（name、appearance、manufacturer data） |
| Flags | 自動で付与される | **載せられない**（仕様上advertising payload専用） |

EspBleでは `advertising().data()` と `advertising().scanResponse()` が同じbuilderを返すので、どちらの面にどの項目を置くかを自分で決められます。

## 既定の動作との関係

scan responseに何も設定しない場合、EspBleは**device nameを自動的にscan responseへ置きます**。31byteのadvertising payloadを名前で消費しないための既定動作です。

scan responseに何か1つでも設定すると、この自動配置は解除されます。名前も出したい場合は、このexampleのように `scanResponse().setName(...)` を明示してください。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Peripheral）
- 受信側 — [Info/ScanDump](../../Info/ScanDump/)を動かす2台目のボード、またはnRF Connect等のスキャナアプリ

## 動作

- advertising payloadに128bitのService UUID、appearance、Tx Powerを載せます（flagsを含めて31byte中28byte）
- scan response payloadにname（22byte）とmanufacturer data（7byte）を載せます（31byte中29byte）
- passive scanでは前者しか見えず、active scanでは両方がマージされて1件の結果になります

各AD構造は値のほかに2byte（length + type）を消費します。sketchには各面の内訳をコメントで書いてあるので、配分を変えるときの目安にしてください。

## 主なAPI

- `ble.advertising().data()` — advertising payloadのbuilder。`setName()`等の既存setterはこれへの転送
- `ble.advertising().scanResponse()` — scan response payloadのbuilder
- `EspBleAdvertisingData::setName()` / `addServiceUuid()` / `setManufacturerData()` / `addServiceData()` / `setAppearance()` / `setTxPowerIncluded()`
- `ble.advertising().setScanResponseEnabled(false)` — scan responseそのものを無効化（純粋なbroadcaster用。[Beacon](../Beacon/)を参照）

## 注意

- **Tx Powerの値はコントローラが埋めます。** sketchで指定するのは「載せるかどうか」だけで、実際の送信電力は無線側が書き込みます。受信側は `txPowerLevel` と `rssi` の差（経路損失）から距離を推定できます。
- Appearanceはスマホ側でアイコン表示に使われます。受信側の `EspBleScanResult` からは `appearance` / `hasAppearance()` で読めます。
- どちらの面も31byteを超えると `start()` が `InvalidArgument` で失敗し、**どのフィールドが入らなかったか**が `lastErrorDetail()` に出ます。

  ```
  Advertising failed: INVALID_ARGUMENT (name does not fit in the 31-byte scan response payload)
  ```
- **Flagsをscan responseへ置くことはできません。** Bluetooth Core Specification（CSS Part A）がFlags AD typeをadvertising payload専用と定めており、scan responseに入れると仕様違反になるためです。EspBleはadvertising payloadにのみ自動で付与します。

## 期待されるSerial出力

```
Advertising. Passive scanners see only the service UUID.
```

[Info/ScanDump](../../Info/ScanDump/)側（active scan）では次のように見えます。

```
d0:cf:13:58:fd:95 type=0 rssi=-38 connectable scannable name="EspBle Scan Response" uuid=5266f727-49d7-4eaf-a6f1-7363616e7270 manufacturer[5]=ffff010203
```

## 関連するガイド

- [BLE入門ガイド §2 GAP編](../../../docs/GUIDE_BLE_BASICS.ja.md#2-gap編--探してつながる) — advertising・scan・接続

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[ScanResponse.ino](ScanResponse.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[ScanDump](../../Info/ScanDump/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

ScanResponse - 広告データを2つのpayloadへ分ける。Legacy advertisingは31byteだが、 active scannerは2つ目の31byteであるscan responseを要求するので、かさばる項目を そちらへ置くと使える容量が倍になる。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `advertising.data().addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。

## 確認と変更のポイント

広告とScan Responseの容量は別です。探索に必要な項目を意図した領域へ置き、Scan ResponseはActive Scanで確認します。名前をそちらへ移すとPassive Scanからの見え方が変わります。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
