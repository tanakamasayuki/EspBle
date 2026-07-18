# 【作業メモ／ドラフト】スキャン→接続→通信の入門ガイド

> ⚠️ **これは作業メモ（ドラフト）です。まだ公開ドキュメントではありません。**
>
> 目的: BLEをまったく知らない人が「スキャンから接続して通信するまで」の主要APIと
> その順番を、なんとなく学べる教材にすること。最終的な置き場所は
> [examples/README.ja.md](../examples/README.ja.md) と [examples/README.md](../examples/README.md)（相互リンク付き）です。
>
> **なぜ今すぐ examples/README に載せないか**: このガイドで説明する
> 「UUIDでの絞り込み」は、0.2.0以降に予定している2つのAPI変更
> （[STATUS.ja.md](STATUS.ja.md) の A/B/C ラップ改善のうち **UUID型の導入**と
> **scanフィルタhelperの導入**）の真上に乗っています。そのAPIが変わってから
> 公開教材のコード断片を確定させたいので、それまではこのドラフトに置きます。
> 一方、**概念の説明（BLEの流れ・UUIDとは・フル/短縮形の理由と注意点）は
> API変更の影響を受けない**ので、この内容はそのまま流用できます。
>
> **昇格の手順（0.2.0でUUID型・scanフィルタhelperが確定したら）**:
> 1. 下記コード断片を新APIの書き方に更新する
> 2. 本ガイドを examples/README.ja.md に統合し、英語版を examples/README.md に反映する
> 3. 両README冒頭の相互リンク（`> English:` / `> 日本語版:`）に合わせる
> 4. このドラフトを削除する

---

## 全体像 — BLEの通信は3段階

BLEの通信は、大きく **「探す → つながる → やり取りする」** の3段階で考えると分かりやすいです。

1. **探す（Scanning / Advertising）** — 相手を見つける
2. **つながる（Connection）** — 見つけた相手に接続する
3. **やり取りする（GATT: Read / Write / Notify）** — 接続した相手とデータを交換する

登場する役割は2組あります（詳しくは [examples/README.ja.md](../examples/README.ja.md) の「BLEとは」節）。

- **リンクの役割**: 探して接続しにいく側が **Central（親機）**、放送して待つ側が **Peripheral（周辺機器）**
- **データの役割**: 値を持つ側が **GATT Server**、値を使う側が **GATT Client**

典型的には「Central=Client」「Peripheral=Server」ですが、この2組は独立した概念です。
このガイドは **Central=Client** 側（＝相手を探して接続し、値を読み書きする側）の流れを追います。
サーバ側の例は [Gatt/Server](../examples/Gatt/Server/) を参照してください。

## 大原則 — 「要求」と「イベント」は別のタイミング

EspBleのAPIを読むうえで最初に知っておくべき約束事です。

- **要求API**（`scanner().start()`、`connect()`、`readCharacteristic()` など）は
  「お願いを受け付けたか」だけを **その場で `bool` で返します**。実際の完了はまだです。
- **完了・失敗は、あとから「イベント」として届きます**（`onConnected`、`onCharacteristicRead` など）。
- そして **すべてのイベントは、`loop()` の中で呼ぶ `ble.update()` から配送されます**。
  BLEスタックの割り込みスレッドからコールバックが呼ばれることはありません。

```cpp
void loop() {
  ble.update();  // ここで初めて、溜まっていたイベントがコールバックへ配送される
  delay(1);
}
```

つまり「操作を頼む → 次の操作は、その完了イベントの中で頼む」という
**連鎖（チェーン）** で書くのが基本形です。Central側のGATT操作は同時に1件ずつなので、
前の操作が終わってから次を出します。

## ステップ順（現行0.1.0 API）

> 下のコードは現行（0.1.0）のAPIです。UUIDは文字列、絞り込みは `onResult` 内で手動判定します。
> 0.2.0でUUID型・scanフィルタhelperが入るとこの書き方は変わります（前掲の昇格手順を参照）。

### 0. 初期化

```cpp
#include <EspBle.h>
EspBle ble;

EspBleConfig config;
config.deviceName = "EspBle Central";
if (!ble.begin(config)) {
  Serial.printf("BLE init failed: %s\n", ble.lastErrorDetail().c_str());
}
```

### 1. 探す — スキャンを始める

```cpp
EspBleScanConfig scanConfig;
scanConfig.active = true;   // active scan: 追加情報（名前など）も要求する
ble.scanner().start(scanConfig);
```

`onResult` に、見つかった1件ごとのコールバックを登録します。周囲の**すべての**放送が
届くので、ここで「目的の相手か」を判定します。

```cpp
ble.scanner().onResult([](const EspBleScanResult &r) {
  // r.address          … 相手のアドレス（String）
  // r.rssi             … 受信強度（dBm。0に近いほど近い。例: -40は近い、-90は遠い）
  // r.serviceUuids[]   … 放送に載っていたService UUIDの一覧
  // r.manufacturerData … Manufacturer Specific Data（iBeacon等はここに入る）
  Serial.printf("%s RSSI=%d\n", r.address.c_str(), r.rssi);
});
```

### 2. 絞り込む → つなぐ

目的のService UUIDを放送している相手だけを選んで接続します。
`advertisesService()` は放送に載っていたUUID群と比較して真偽を返します
（UUIDの詳細は後述）。

```cpp
static constexpr const char *TARGET = "5266f727-49d7-4eaf-a6f1-636f6e6e6563";
bool requested = false;

ble.scanner().onResult([](const EspBleScanResult &r) {
  if (requested || !r.advertisesService(TARGET)) return;  // 対象外は無視
  ble.scanner().stop();                 // 接続の前にスキャンを止める
  requested = ble.connect(r);           // 受理されれば true（完了はイベントで届く）
});
```

### 3. 接続の結果を受け取る

```cpp
ble.onConnected([](const EspBleConnection &c) {
  // c.id … 接続ごとに安定した識別子（backend handleとは別物）。以後この id で操作する
  Serial.printf("Connected id=%u (%s)\n", c.id, c.peerAddress.c_str());
});
ble.onDisconnected([](const EspBleConnection &c) {
  Serial.printf("Disconnected id=%u\n", c.id);
});
ble.onConnectionFailed([](const EspBleConnectionFailure &f) {
  // 到達不能・timeout など。connect() が true を返した後でも、非同期に失敗しうる
  Serial.printf("Connect failed: %s\n", f.detail.c_str());
});
```

### 4. やり取りする — Discovery → Read → Write

接続できたら、使いたいCharacteristicを **UUID指定でDiscovery** し、
完了イベントを合図に Read、さらに Write…と連鎖させます。

```cpp
// 接続できた → 既知UUIDのCharacteristicをDiscovery
ble.onConnected([](const EspBleConnection &c) {
  ble.discoverCharacteristic(c.id, SERVICE_UUID, CHARACTERISTIC_UUID);
});

// Discovery完了 → Read要求
ble.onCharacteristicDiscovered([](const EspBleGattResult &res) {
  if (!res.success) { Serial.println(res.detail.c_str()); return; }
  ble.readCharacteristic(res.connectionId, SERVICE_UUID, CHARACTERISTIC_UUID);
});

// Read完了 → 値を表示し、Write要求（最後の true = Write with Response）
ble.onCharacteristicRead([](const EspBleGattResult &res) {
  Serial.printf("Read: %s\n", res.value.c_str());
  ble.writeCharacteristic(res.connectionId, SERVICE_UUID, CHARACTERISTIC_UUID,
                          String("hello"), true);
});

ble.onCharacteristicWritten([](const EspBleGattResult &res) {
  Serial.println(res.success ? "Write done" : "Write failed");
});
```

値の変化を継続的に受け取りたい場合は、Read/Writeの代わりに **購読（Subscribe）** します。
サーバ側が `notify`（確認応答なし）または `indicate`（確認応答あり）で値を送ってきます。
実例は [Gatt/SubscribeClient](../examples/Gatt/SubscribeClient/) と
[Gatt/IndicateClient](../examples/Gatt/IndicateClient/) を参照してください。

### 通信全体の流れ（時系列）

```
Central(=Client)                             Peripheral(=Server)
   |  scanner().start()                              |  advertising...
   |  <---------- advertisement -------------------  |
   |  onResult() で advertisesService() 判定          |
   |  scanner().stop(); connect() ----------------->  |
   |  <==================== 接続確立 ===============>  |
   |  onConnected(id)                                 |
   |  discoverCharacteristic(id,svc,chr) ---------->  |
   |  onCharacteristicDiscovered()                    |
   |  readCharacteristic(...) --------------------->  |
   |  <---------------- value ----------------------  |
   |  onCharacteristicRead(value)                     |
   |  writeCharacteristic(...) -------------------->  |
   |  onCharacteristicWritten()                       |
   |  （購読すれば）  <-------- notify/indicate ------  |
   |                                                  |
   ※ すべてのコールバックは loop() の ble.update() から配送
```

---

## UUIDとは何か（初心者向け）

### UUIDは「機能の型」を表す名札

BLEでは、Service（機能のまとまり）やCharacteristic（個々の値）が
何であるかを **UUID（Universally Unique IDentifier）** で表します。
UUIDは **128ビット（16バイト）の世界で一意な識別子**で、こう書きます。

```
5266f727-49d7-4eaf-a6f1-636f6e6e6563   （8-4-4-4-12桁の16進数）
```

たとえば「Battery Level（電池残量）」というCharacteristicには決まったUUIDが割り当てられており、
どのメーカーの機器でも同じUUIDを使います。だから相手の機種を知らなくても
「このUUIDのCharacteristicを読めば電池残量が分かる」と決め打ちできます。
UUIDは名前ではなく **型（種類）を表す名札** だと考えてください。

### 標準UUIDと独自UUID

- **標準（Bluetooth SIG割当）UUID**: Battery ServiceやHIDなど、仕様で決まった機能。
  世界中で共通の値です。
- **独自（カスタム）UUID**: 自分のアプリ専用の機能。自分でランダムに128ビットを生成して使います
  （上の `5266f727-...` はexample用の独自UUIDです）。

### フル形（128ビット）と短縮形（16ビット）

標準UUIDには **16ビットの短縮形** があります。たとえばBattery Serviceは短縮形で `180F`。
これは、次の **Base UUID** の中の4桁に短縮形を差し込んだ128ビットUUIDの「省略表記」です。

```
Base UUID:  0000____-0000-1000-8000-00805F9B34FB
                ↑ここに16ビット短縮形が入る
180F の実体: 0000180F-0000-1000-8000-00805F9B34FB
```

つまり **短縮形とフル形は同じUUIDを指す別表記** です。EspBleの `advertisesService()` は
UUIDを **値として** 比較する（内部で16ビットをBase UUIDへ展開して突き合わせる）ので、
`advertisesService("180F")` と `advertisesService("0000180f-0000-1000-8000-00805f9b34fb")`
はどちらでも同じ相手に一致します。大文字小文字も区別しません。

### なぜスキャン時に絞り込むのか

`scanner().onResult()` には、目的の機器だけでなく **周囲のBLE機器の放送がすべて** 届きます
（近所のイヤホン、スマホ、他のESP32…）。そのままでは目的の相手を判別できないので、
放送に載っているService UUIDで **「これは自分の探している機能を持つ機器か？」** を
絞り込みます。これがフィルタリングです。

- **標準機能を探す**とき（例: 心拍計、電池サービス）は、短縮形 `180D` などで絞れます。
- **自作機器を探す**ときは、その機器の独自128ビットUUIDフル形で絞ります。

### 気をつける点

1. **独自サービスは必ず128ビットのフル形で書く。** 短縮形（16ビット）はSIGが割り当てた
   標準UUID専用の表記です。自作サービスに勝手に16ビットを使ってはいけません。
2. **桁とハイフン位置は正確に。** 大文字小文字は無視されますが、`8-4-4-4-12` の形が崩れると別物です。
3. **放送に載るUUIDには数と長さの制限がある。** Legacy Advertisingのペイロードは31バイトしかなく、
   Service UUIDを全部載せられるとは限りません。**「放送に出ていない＝そのサービスが無い」ではない**点に注意。
   確実に知りたいときは、接続後にGATT Discoveryで確認します（このガイドのステップ4）。
4. **Service UUIDを一切放送しない機器もある。** iBeaconのように名前もUUIDも出さず
   Manufacturer Dataだけの機器は、`advertisesService()` では絞れません。その場合は
   `r.address`（アドレス）や `r.manufacturerData`、名前などで判定します。
   放送の全フィールドを観察するには [Info/ScanDump](../examples/Info/ScanDump/) が便利です。
5. **短縮形が意味を持つのはSIG登録済みUUIDだけ。** 未登録の16ビット値には決まった意味がありません。

---

## 0.2.0での変更予定（このドラフトのコードが変わる箇所）

- **UUID型の導入（ラップ改善 C）**: 現在は `const char *` / `String` の文字列でUUIDを渡すが、
  専用のUUID型を導入して型安全にする予定。導入後は `advertisesService(...)` や
  `discoverCharacteristic(...)` の引数の書き方が変わる。
- **scanフィルタhelperの導入（ラップ改善 D）**: 現在は `onResult` 内で手動に
  `advertisesService()` 判定しているが、`scanner().start()` 側にフィルタ指定を渡せる
  helperを検討中。導入後は「絞り込み」の書き方が変わる。

いずれも [STATUS.ja.md](STATUS.ja.md) の「HID以外のラップ改善（A/B/C）は0.2.0以降」に該当します。
**このガイドの概念説明（3段階の流れ・非同期モデル・UUIDの意味・フル/短縮形・注意点）は
これらの変更後も変わりません。** 変わるのは上記のコード断片だけです。
