# ConnectionParameters

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 2章「GAP編 — 探してつながる」

確立済みの接続を調整する例です。

BLEでは、**応答性と消費電力を決めるパラメータを接続時に指定できません**。接続はコントローラが決めた値で成立し、そのあとで変更を要求します。この非対称さが分かりにくいところなので、このexampleは「接続直後に何が決まっていたか」を表示してから変更します。

## 3つのパラメータ

| パラメータ | 意味 | 単位 |
|---|---|---|
| **Connection Interval** | 通信機会の周期。短いほど応答が速く、電力を食う | 1.25 ms |
| **Peripheral Latency** | 送るものがないときPeripheralが応答をスキップしてよい回数 | 回数 |
| **Supervision Timeout** | この時間だけ通信が途絶えたら切断とみなす | 10 ms |

単位はBLE仕様そのままの生の値です。`interval = 24` は 24 × 1.25 = 30 ミリ秒を意味します。

**Supervision Timeoutには制約があります。** `(1 + latency) × maxInterval × 2` より長くする必要があります。Latencyを増やすとPeripheralが長く沈黙しうるため、それを切断と誤判定しないためです。この条件を満たさない要求は相手に拒否されます。

## PHY

**PHY**は無線の変調方式です。既定の1M PHYに対し、**2M PHY**はシンボルレートが倍で、同じデータをより短い時間で送れます。電波に乗る時間が短くなるぶん1バイトあたりの消費電力が下がりますが、**到達距離は縮みます**。

接続時にPHYを指定することもできないため、これも接続後に変更します。

## 必要なもの

- このsketchを動かすESP32-S3 × 1（Central）
- 接続先のPeripheral — 2台目のボードで[Gap/Advertise](../Advertise/)、またはHID Service（`0x1812`）をadvertiseする任意の機器

## 動作

- Service UUID `0x1812` をadvertiseする相手を探して接続します
- 接続直後に、**コントローラが決めた**interval / latency / timeout / PHYを表示します
- `f` で低遅延profile（interval 15〜30 ms、latency 0）、`s` で省電力profile（interval 400〜500 ms、latency 4）を要求します
- `p` で2M PHYを要求します
- `d` で切断します

## 主なAPI

- `ble.updateConnectionParameters(id, minInterval, maxInterval, latency, timeout)` — 変更を要求する
- `ble.onConnectionParametersUpdated(callback)` — 交渉の結果を受け取る
- `ble.updatePhy(id, txPhyMask, rxPhyMask)` — PHYの変更を要求する。マスクは `EspBle::Phy1MMask` / `Phy2MMask` / `PhyCodedMask`
- `ble.onPhyUpdated(callback)` — PHYの結果を受け取る
- `EspBleConnection` — `connectionInterval` / `peripheralLatency` / `supervisionTimeout` / `txPhy` / `rxPhy`

## 注意

- **要求の戻り値は「受け付けたか」だけです。** 実際に何になったかは必ずコールバックで確認してください。相手が要求と違う値を返すことも、拒否することもあります。
- **どちらの役割からでも要求できます。** ただし最終的に決めるのはCentral側のコントローラです。Peripheralからの要求はCentralが承認して初めて反映されます。
- **2M PHYは両側の無線が対応している必要があります。** 対応していない相手では1M PHYのままになります。要求自体は成功扱いで返り、結果のPHY値が変わらないことで判別します。
- **Coded PHY（Long Range）は無線の対応次第**です。ESP32-S3は対応しますが、相手が非対応なら変化しません。

## 期待されるSerial出力

```
Scanning for a peripheral...
CONNECTED interval=40 (50.00 ms) latency=0 timeout=256 (2560 ms) phy=tx1/rx1
Commands: f fast, s slow, p 2M PHY, d disconnect
REQUEST slow accepted=1
PARAMETERS interval=400 (500.00 ms) latency=4 timeout=600 (6000 ms) phy=tx1/rx1
REQUEST 2M PHY accepted=1
PHY interval=400 (500.00 ms) latency=4 timeout=600 (6000 ms) phy=tx2/rx2
```

## 関連するガイド

- [BLE入門ガイド §2 GAP編](../../../docs/GUIDE_BLE_BASICS.ja.md#2-gap編--探してつながる) — advertising・scan・接続

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[ConnectionParameters.ino](ConnectionParameters.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

ConnectionParameters - 確立済みの接続を調整する。応答性と消費電力を決める パラメータは接続時に指定できず、controllerが決めた値で接続したあとに変更する。 この例では接続して交渉結果を表示し、低遅延profileと省電力profileを切り替え、 2M PHYへの変更も行う。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。
3. `ble.connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。
4. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
5. `ble.onConnectionFailed(...)` — 接続要求が受理されたあとに発生する、相手不在や接続拒否などの失敗を確認します。
6. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。

## 確認と変更のポイント

接続パラメータやPHYは相手と交渉します。要求値と更新イベントの値を比較し、ローカルの指定だけで相手に強制できるとは考えないようにします。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
