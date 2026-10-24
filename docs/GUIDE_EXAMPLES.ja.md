# サンプルを動かして自分の用途へ組み込む

> English: [GUIDE_EXAMPLES.md](GUIDE_EXAMPLES.md)

[サンプル一覧](../examples/README.ja.md)から機能別に例を選べます。各フォルダには日英READMEがあり、相手の準備、シリアル操作、APIの流れ、確認結果を説明しています。このガイドでは共通の手順を扱います。Bluetoothの概念は[BLE入門](GUIDE_BLE_BASICS.ja.md)と[Classic入門](GUIDE_CLASSIC_BASICS.ja.md)、環境の対応状況は[ライブラリREADME](../README.ja.md)とリンク先の表で確認します。ESP32という名前でも無線機能がすべて同じとは限りません。

## 小さな組み合わせから始める

| 目的 | 最初の例 | 相手／次の例 |
|---|---|---|
| BLEの広告を観察する | [Gap/Scan](../examples/Gap/Scan/README.ja.md) | 別の基板で[Gap/Advertise](../examples/Gap/Advertise/README.ja.md) |
| 値を読み書きする | [Gatt/Basics/Client](../examples/Gatt/Basics/Client/README.ja.md) | [Server](../examples/Gatt/Basics/Server/README.ja.md) |
| 変化を継続して受け取る | [SubscribeClient](../examples/Gatt/Basics/SubscribeClient/README.ja.md) | [NotifyServer](../examples/Gatt/Basics/NotifyServer/README.ja.md) |
| 確認応答付きの更新を見る | [IndicateClient](../examples/Gatt/Basics/IndicateClient/README.ja.md) | [IndicateServer](../examples/Gatt/Basics/IndicateServer/README.ja.md) |
| BLEでシリアルの行を交換する | [NusClient](../examples/Gatt/Basics/NusClient/README.ja.md) | [NusServer](../examples/Gatt/Basics/NusServer/README.ja.md) |
| センサー値を実装する | [BatteryServer](../examples/Gatt/Device/BatteryServer/README.ja.md) | [BatteryClient](../examples/Gatt/Device/BatteryClient/README.ja.md)、その後に目的のSensor／Health／Fitnessの組 |
| ESP32から文字を入力する | [Hid/KeyboardDevice](../examples/Hid/KeyboardDevice/README.ja.md) | 対応PC／スマートフォン、その後に[KeyboardHost](../examples/Hid/KeyboardHost/README.ja.md) |
| 認証付きペアリングを理解する | [NumericComparisonServer](../examples/Security/NumericComparisonServer/README.ja.md) | [NumericComparisonClient](../examples/Security/NumericComparisonClient/README.ja.md) |
| MIDIイベントを通信する | [MidiDevice](../examples/Midi/MidiDevice/README.ja.md) | [MidiHost](../examples/Midi/MidiHost/README.ja.md)またはBLE MIDIアプリ |
| Classicのバイトストリームを使う | [SppServer](../examples/Classic/SppServer/README.ja.md) | [SppClient](../examples/Classic/SppClient/README.ja.md)。無印ESP32を使う |
| 広告や接続を詳しく見る | [ScanDump](../examples/Info/ScanDump/README.ja.md) | [ConnectionInspector](../examples/Info/ConnectionInspector/README.ja.md) |
| 無線相手付きP4を使う | [Hostedセットアップ](ESP_HOSTED_SETUP.ja.md) | [Hosted例](../examples/Hosted/CustomPins/README.ja.md) |

Centralは相手を探してBLE接続を開始し、Peripheralは広告して接続を受け付けます。GATT Clientは値の要求や購読を行い、GATT Serverは値を公開します。これらは別の役割ですが、多くの例ではCentral／ClientとPeripheral／Serverを組み合わせています。HID DeviceとHID Hostは入力Reportを扱う役割で、USBの配線を示すものではありません。ClassicのInquiry・SPP・音声はBLEの広告・GATTとは別なので、同じ方式の相手を選びます。

## 準備して書き込む

1. ルートREADMEの手順でEspBleを導入し、実際のArduinoボード、ポート、必要なボード設定を選びます。コアと基板の互換性はルートの対応情報で確認し、別のESP32機種の設定をそのまま流用しないようにします。
2. 選んだ `.ino` を開きます。組になる例には通常2台の基板、または基板と対応スマートフォン／PC／機器が必要です。Server／Device側を先に開始します。Classic例で必要な場合は相手を発見可能／ペアリング待ちにします。
3. 相手アドレス、機器名、UUIDフィルタ、該当例のWi-Fi接続情報など、用途に必要な設定だけを変えます。アドレスとアドレス種別は一緒に扱います。広告名を変えても相手側のUUIDフィルタは自動では変わりません。
4. 書き込み後、シリアルモニタを `Serial.begin(...)` の速度に合わせます。多くの例は操作キーを表示します。バイナリ値を公開するServerは表示が少ない場合があるため、対応Clientから確認します。
5. まず例のデータで通信を再現します。その後で値の生成元を1つずつ置き換え、符号化、項目の単位、検証を残します。

ESP-Hostedは、リンク先のセットアップ資料に従って配線と無線側ファームウェアを準備します。BLEとWi-FiはTransportを共有するため、独自の端子指定はどちらかが初期化する前に行います。対応するボード定義があればそちらを使います。

## スケッチの流れを追う

BLE Serverでは `ble.begin()` より前にService・Characteristic・初期値を登録します。HID／MIDI Deviceヘルパーもスタック初期化前にServerを構成します。一方、Client用ヘルパーでは初期化後にコールバックを設定する場合があります。Device側の順序をHost側へ一律に当てはめず、対象の例の順序に従います。

`begin()` と設定／開始APIの結果を確認します。相手を探索するか自分のサービスを広告し、接続を要求してから接続イベントを待ちます。`connect()` の即時のtrueは試行の受理で、接続成立ではありません。同様に、Readは値が返る前に要求が戻り、購読も接続後の別の段階です。どこで次へ進むかはコールバック名と結果型で確認できます。

`loop()` から `ble.update()` または `bluetooth.update()` を継続して呼びます。`onConnected(...)` は関数を登録する操作で、登録時にその中身が実行されるわけではありません。コールバックは短く保ち、状態を更新して残りをloopで処理できます。長いシリアル入力待ち、delay、センサー処理で他のイベントまで遅らせないようにします。

実行場所には次の例外があります。

| コールバック | 実行場所とデータの扱い |
|---|---|
| 通常のBLEイベントとClassicの制御イベント | 対応するupdate()から配送される。loopが進むように短く保つ |
| GATT ServerのonRead() | BLEスタックのタスクで同期実行。setValue()で素早く応答を用意し、遅い入出力や待機を避ける |
| Classic A2DPのonMedia()とHFPのonAudio() | Bluetooth Host側で実行。参照中の符号化データを容量制限付きキューへコピーして戻り、デコードや再生は別で行う |

イベント引数への参照やポインタを、そのまま後の処理用に保存しないようにします。必要な値をアプリ所有の領域へコピーします。音声・Report・SysExなどポインタを含むviewは、その先のデータもコピーする必要があります。viewのコピーだけでは参照先のバイトまで複製されるとは限りません。HFP音声コールバック内では、そのコールバックの再登録／解除や `end()` を行わないようにします。

## 戻り値とバイナリの意味を確認する

| 操作 | 確認するもの |
|---|---|
| ローカルGATT構成 | handleの.valid()またはbool、その後のスタック初期化成功 |
| 接続要求 | 即時の受理、その後の接続成立／失敗イベント |
| 相手の値のRead／Write | 対応する完了結果。success、接続、UUID／handleも見る |
| 購読 | 購読完了、その後の該当する更新。Notify／Indicate方式も合わせる |
| setValue() | 保存値の更新。これだけではNotifyしない |
| notify() | 即時のキュー受理、使用する場合は送信結果。相手からのATT配送確認はない |
| indicate() | 即時のキュー受理、その後の相手ごとのATT確認応答を送信結果で見る |
| Pairing／Security要求 | 結果と、encrypted・authenticated・bondedを別々に見る |

WriteやIndicateが成功しても、相手アプリが実際の装置を動かしたことまでは確認できません。その区別が必要ならアプリ側の応答を用意します。Write Without Responseには相手からのATT書き込み応答がありません。

GATT値はArduino `String` に入っていてもバイト列です。添字で読む前に長さを確認し、任意のバイナリへ終端0前提の文字列処理を使わないようにします。flags、符号、little-endianの並び、単位を検証してから変換します。医療用SFLOAT／FLOATはC++の浮動小数点のメモリ形式とは異なります。標準センサー例の値はデモ用なので、実データの取得は別に実装します。

使えるデータ容量を決めるのは希望値だけでなく、交渉されたATT MTUです。NUSや長い独自データにはメッセージの区切りと分割を設計します。SPPはバイトストリームなので、1回のコールバックでメッセージが分割／結合され得ます。容量制限付きの解析処理で、未完の入力を保持します。

## 状態を保ちながら用途へ合わせる

接続／セッションごとに状態を持ちます。切断時に準備完了を解除し、古いIDと探索handleを捨てます。再接続しない例もあるため、スキャン／広告が自動で再開すると思わず方針を決めます。自動再接続と購読の復元は別の仕組みです。複数の相手がいる場合はRead・通知・出力の宛先を接続ごとに分けます。

主コールバックの `onX(...)` を再設定すると、以前の登録を置き換えるAPIがあります。ヘルパーとアプリの処理を併用する場合は、提供されているlistener版を使い、返るlistener IDも確認して、ヘルパーの動作を消さないようにします。キュー、送信待ち、再接続は[応用ガイド](GUIDE_ADVANCED.ja.md)にも説明があります。

HIDは接続成立後にも暗号化・探索・購読が必要になる場合があります。該当プロファイルが利用可能になってから送ります。Press／Releaseは状態を持つので保持したキーやボタンは解除し、DescriptorとReport長を合わせます。PCは入力に即反応するため、影響のないテスト用ウィンドウで試します。Descriptorを変えた場合は、Host側の登録／キャッシュを消して新しい形式を探索させる必要があることもあります。

セキュリティでは、Pairingがリンクの安全性を確立し、Bondingが信頼情報を保存します。Just Worksでは暗号化できてもMITM認証はできません。Numeric Comparisonは双方の表示を利用者が比較し、Passkey例は今回表示された値または意図して設定した値を使います。固定のデモ用パスキーは製品の運用方針ではありません。再起動だけで保存bondは消えないので、最初からの試験では双方の記録を意図して削除します。

音声のSBCやCVSD／mSBCは符号化データで、PCMではありません。交渉したコーデックに合う外部エンコーダ／デコーダと音声入出力を用意します。Sourceの `WouldBlock` では未送信フレームを保持し、容量を管理して再試行します。キューを省くために遅いデコーダをHostコールバック内へ移さないようにします。

## 最後に成功した段階から切り分ける

| 症状 | 次に確認するもの |
|---|---|
| 初期化失敗 | 実際の基板・コア・バックエンド、対応資料、失敗直後のlastErrorName()／lastErrorDetail() |
| 相手が見つからない | BLE／Classicの一致、広告／発見可能状態、UUID／アドレス条件、アドレス種別、名前に必要なActive Scan |
| 接続できたが値が来ない | Service探索、Security条件、対象Characteristic、購読成功、Notify／Indicate方式 |
| 値が欠ける／不自然 | データ長、flags、単位、交渉後のMTU、メッセージの区切り |
| コマンドが反応しなくなる | updateの継続、シリアルやコールバック内の待機、イベント／結果の破棄数 |
| ペアリング確認が再度出ない | 双方の保存bond。意図した削除は切断してから行う |
| HID接続後も入力されない | 準備完了、暗号化／購読、Descriptor、Hostのキャッシュ、入力先のフォーカス |
| 音声パケットは来るが音が出ない | コーデック設定、外部デコーダ、バッファ、実際の音声出力 |
| 通信量を増やすと要求が拒否される | 戻り値、保留中の処理、容量制限付き再試行、送信待ち、使用資源 |

具体的な操作と相手は各例のREADMEにあります。変化する互換性や検証状況はルートREADMEと状態資料を参照し、使い方の説明へ重複して転記しないようにします。
