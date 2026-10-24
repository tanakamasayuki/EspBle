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

## 実行前の確認

導入、ボード選択、接続と購読の違いは[サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)を参照してください。[CustomClient.ino](CustomClient.ino)を開いてボードとポートを選び、書き込み後にシリアルモニタを `Serial.begin(...)` の速度へ合わせます。操作キーは上記の表とスケッチの起動メッセージで確認します。

対応する例は[CustomDevice](../CustomDevice/README.ja.md)です。2台で試す場合は相手側を先に起動し、この例のフィルタ条件に合う広告／サービスを用意してください。

## この例の構成

CustomClient - 汎用GATT clientでCustom HIDデバイスの任意Report Descriptorを読み、 Reportを駆動する。Hid/CustomDevice とペア。HIDデバイスは同一UUID 0x2A4Dの Report characteristicを複数持つため、対象はすべて個別のattribute handleで指定する。 各Reportの役割はReport Reference descriptor（0x2908、report ID＋type）から読み、 その指定もhandleで行う。Report Referenceはどれも「0x2A4Dの下の0x2908」なので、 Service/Characteristic/Descriptor UUIDの組では選び分けられない。

## コードを読む順序とAPI

次の項目はソース中で最初に登場する順です。コールバックの登録順は実行順ではありません。イベントが届いたときに、その関数が実行されます。

1. `ble.begin(...)` — 使用するスタック／プロファイルを初期化し、戻り値を確認してから後続処理を開始します。BLEのGATT Serverでは、スタック初期化前にデータベースを登録します。
2. `ble.onConnected(...)` — このイベントで接続成立を確認し、プロファイルに必要な相手／セッション情報を保存します。connect要求の受理はそれより前で、利用可能なリンクの成立を意味しません。
3. `ble.discoverServices(...)` — 相手のデータベース探索を開始します。要求直後にhandleがそろったと考えず、完了イベントで結果を使います。
4. `ble.onDisconnected(...)` — 利用可能フラグと保存した接続／セッション情報をここで解除します。古いIDを新しい相手に使わないようにし、スキャンや広告の再開はコードにある処理を確認します。
5. `ble.onServicesDiscovered(...)` — 探索の成否と取得したデータベースを確認してから、Service・Characteristic・Descriptorのhandleを使います。
6. `ble.subscribe(...)` — 相手のCharacteristic更新を購読します。NotifyとIndicateの購読方式を合わせ、購読の完了を待ちます。
7. `ble.onNotification(...)` — Characteristicと接続を絞り、バイナリ形式を検証してから更新をデコードします。IndicateのデータもこのAPIへ届きます。
8. `ble.scanner().onResult(...)` — 探索結果を確認してから相手を選びます。名前が含まれない場合があり、機器を発見しただけでは接続後の目的のサービスまで使えるとは限りません。
9. `ble.connect(...)` — 選んだ相手へ接続を要求します。接続イベントを待ってから、接続IDの利用やプロファイルの通信を始めます。
10. `ble.writeCharacteristic(...)` — 相手へのWriteをキューへ登録し、完了イベントを確認します。Write Without Responseには、相手アプリの処理を保証するATT応答はありません。

## 確認と変更のポイント

対応HID Deviceを使い、無線接続だけでなくプロファイル探索／準備完了まで待ちます。IDとDescriptorに応じてReportを振り分けます。キーボードLEDなどの出力はHost→Device方向で、キー入力の受信とは別です。

Vendor定義Reportには、相手側にも対応するアプリ／解析処理が必要です。Descriptorのバイトを変えたらReport宣言と送受信両方の処理を合わせ、機器名の見た目から形式を推測しないようにします。

`ble.update()` をloopで継続して呼びます。入力待ちや周期処理を長い待機で実装すると、キューの処理と次の通知が遅れます。

失敗を調べる際は「初期化→探索／公開→接続→必要なセキュリティ→探索／購読→値の通信」のどこまで進んだかを確認します。この例で使わない段階は省き、表示が少ないServer例は相手側のReadや通知受信で確認します。boolを返す要求では、その場の受理と後の完了イベントを区別します。すべての呼び出し結果を表示しない最小例をアプリへ組み込む際は、各戻り値と失敗時の処理も追加してください。

[サンプル一覧](../../README.ja.md) · [サンプル実行ガイド](../../../docs/GUIDE_EXAMPLES.ja.md)
