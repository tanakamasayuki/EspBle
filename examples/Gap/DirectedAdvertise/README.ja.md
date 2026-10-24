# DirectedAdvertise

> English: [README.md](README.md)
> 概念の説明: [BLE通信の入門ガイド](../../../docs/GUIDE_BLE_BASICS.ja.md) 2章「GAP編 — 探してつながる」

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
- **相手がRPA（Resolvable Private Address）を使う場合は、識別用アドレスを指定します。** 解決はボンド情報を使って行われるため、**先にbondingが必要**です（[Gap/PrivateAddress](../PrivateAddress/)、[Security/JustWorksServer](../../Security/JustWorksServer/)参照）。
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

## 関連するガイド

- [BLE入門ガイド §2 GAP編](../../../docs/GUIDE_BLE_BASICS.ja.md#2-gap編--探してつながる) — advertising・scan・接続

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[DirectedAdvertise.ino](DirectedAdvertise.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

## この例の構成

DirectedAdvertise - 相手を1台に限定してadvertiseする。有向advertisingはPDUに 宛先アドレスを載せるため、その相手だけが接続でき、payloadは一切載らない。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
4. `advertising.addServiceUuid(...)` — Service UUIDで探すスキャナが発見できるよう広告へ含めます。UUIDを広告するだけではGATT Serviceは作られません。

## 確認と変更のポイント

対象Centralのアドレスと種別に置き換えます。Directed Advertisingは宛先を絞ります。探索の確認時は、例の操作で通常の広告へ戻して比較します。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
