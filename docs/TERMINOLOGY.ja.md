# 用語と命名規則

公開API、文書、examplesで使うBluetooth LE用語と命名規則。コアではBluetooth標準の概念を正確に表し、examplesでは利用者が役割を取り違えない読みやすさを優先する。

## 基本原則

1. 公開型、enum、状態、仕様書ではBluetooth LEの標準用語を使う。
2. Central / Peripheral、GATT Client / GATT Server、HID Host / HID Deviceを**別の概念**として扱う。
3. スタック全体をCentralまたはPeripheralへ固定する名前を付けない。
4. `Host`、`Device`、`Client`、`Server`を文脈なしで単独使用しない。
5. examplesの変数名は短さより役割の明確さを優先する。ただし文脈から役割が自明な小さいexampleでは短い名前も許す。

## 基本用語

| 概念 | 使用する用語 | 説明 |
|---|---|---|
| ライブラリ全体 | `EspBle` | Central/Peripheralのどちらにも固定しないstack owner |
| 接続を開始するlink role | Central | Scan結果などから接続を開始するlocal role |
| 接続を受け入れるlink role | Peripheral | Advertisingを通して接続を受け入れるlocal role |
| 接続 | Connection | local roleに依存しない接続単位 |
| 接続相手 | Peer | 相手がCentral/Peripheralのどちらでも使える中立語 |
| 周辺探索 | Scanner / Scan Result | Centralそのものとは呼ばない |
| 広告 | Advertising / Advertiser | Peripheralそのものとは呼ばない |
| Attribute提供側 | GATT Server | Peripheralと同義ではない |
| Attribute利用側 | GATT Client | Centralと同義ではない |
| HID入力を提供する側 | HID Device | 通常はPeripheral + GATT Serverだが概念は分ける |
| HIDを利用する側 | HID Host | 通常はCentral + GATT Clientだが概念は分ける |

## 同一視しない概念

```text
Central      != GATT Client
Peripheral   != GATT Server
HID Host     != Central
HID Device   != Peripheral
```

典型的なHID over GATTでは `HID Host = Central + GATT Client` / `HID Device = Peripheral + GATT Server` になるが、これをコアの制約にはしない。

## 公開APIの命名

root objectは役割中立の`EspBle`とし、stack owner全体にlink roleを含む名前を付けない。

```cpp
EspBle ble;

ble.scanner().start();
ble.connect(scanResult);
ble.advertising().start();
ble.gattServer().addService(serviceUuid);
```

roleはConnectionの属性として表す。

```cpp
connection.localRole == EspBleRole::Central;
connection.localRole == EspBleRole::Peripheral;
```

Profileの型名にはprofile roleを含める（`EspBleHidHost` / `EspBleHidKeyboard` / `EspBleMidiDevice` / `EspBleMidiHost`）。roleを省略して意味が変わる型名は避ける。

## Examplesの変数名

型が正確でも、変数名だけを読んだときに役割を誤解しやすい場合は少し長い名前を使う。

```cpp
// 複数roleが登場する例
auto &hidKeyboardDevice = ble.hidKeyboard();
auto &hidKeyboardHost = ble.hidHost();

// 避けたい例
auto client = ...;   // BLE link roleかGATT roleか不明
auto device = ...;   // local/peer、HID/BLEのどれか不明
```

HID Keyboard Deviceだけを説明する短いexampleなど、ファイル名・見出し・型から役割が明らかな場合は`auto &keyboard = ble.hidKeyboard();`のような短い名前でよい。すべてのexampleへ同じ変数名を強制しない。

## ESP32KeyBridge adapter

`Input` / `Output`はBluetooth標準用語ではないが、ESP32KeyBridgeのデータフローでは意味が明確なのでadapter側の名前に使う（`EspBleHidInputAdapter` / `EspBleHidOutputAdapter`）。EspBleコアへInput/Outputというroleを持ち込まない。

## 文書レビュー規則

- `Host`はHCI Host、HID Host、pytest親側など、対象を必ず明示する。
- `Device`はHID Deviceなどprofileで定義された場合だけ単独概念として使う。
- `Client` / `Server`はGATTまたはprofile名を付ける。
- 「接続側」「待受側」のような独自用語で標準roleを置き換えない。
- exampleでは宣言箇所から離れたコードを読んでもroleを判断できるか確認する。
