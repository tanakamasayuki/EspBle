# Core Design

ライブラリの層構成と境界。個別の設計判断とその理由は[DECISIONS.ja.md](DECISIONS.ja.md)、公開APIの形は[API_DESIGN.ja.md](API_DESIGN.ja.md)が正本。

## レイヤ構成

```text
Application / ESP32KeyBridge adapter
  Profile: HID / BLE MIDI / 標準Service（examples）
  Generic GATT Client / Server
  Connection / Security
  GAP: Scanner / Advertiser
  EspBle stack owner
  NimBLE host（ホストAPIを直接使用。core同梱、無印ESP32だけEspBle同梱の`src/nimble_esp32/`）
```

無印ESP32ではBluetooth Classicが同じ形で並びます。

```text
Application
  Profile: Classic HID / SPP / A2DP / AVRCP / HFP
  EspBleClassic stack owner
  名前空間化したClassic-only Bluedroid host（`src/esp32/libespble_bluedroid_classic.a`）
  HCI broker（両hostが1つのBTDM controllerを共有するときだけroutingする）
```

`EspBle`と`EspBleClassic`はそれぞれのスタックの唯一の所有者。Scanner、Advertiser、GATT Server、各Profileは所有者から取得または登録し、個別にスタックを初期化しない。

## ライフサイクル

1. `EspBle`と必要なService/Profileを構築する。
2. Advertising、Security、GATT Server構成を登録する。
3. `begin()`でスタックとGATT databaseを開始する。
4. Advertising/Scanning/Connection操作を開始する。
5. `end()`で操作を止め、接続を閉じ、スタック資源を解放する。

`begin()`後のGATT Server構成変更は禁止し、再構成は`end()`後に行う。`end()`は登録済みのService/Characteristic定義を保持しハンドルだけを捨てるため、次の`begin()`が同じ構成を再登録する。

## 状態モデル

状態を単一enumへ押し込まない。Advertising、Scanning、複数Connectionは同時に成立するため、集約状態は個別状態から算出する。

- Stack lifecycle: uninitialized / initializing / ready / stopping / error
- Scanner state: idle / scanning / stopping
- Advertising state: idle / advertising / stopping
- Connection state: connecting / connected / disconnecting / disconnected

## Connection identity

Connectionはlibrary生成の安定したid、backend handle、peer addressとaddress type、local role、MTUと接続パラメータ、encryption/authentication/bond状態、GATT discoveryとsubscription状態を持つ。

切断でbackend handleが再利用されても、古いConnection参照を別接続として扱わない。

## データ所有権

- UUIDと小さなイベント情報は値として保持する。Characteristic valueの基本表現はbyte sequence。
- callbackへ渡す一時bufferの寿命はcallback終了まで。callback後も必要なScan Result / Discovery Resultは利用者所有の値へcopyできる。
- Service/Characteristic定義は`EspBle`より長生きしてはならず、原則として`EspBle`が所有する。
- 固定容量か動的確保かは対象データごとに決め、上限超過は明示的なresource errorとして返す。

## 非同期処理と実行文脈

スタックcallback内では状態更新とイベントqueue投入だけを行い、利用者callbackは`update()`を呼んだコンテキストから配送する。

- `update()`は単一コンテキストから呼ぶ。stack taskと`update()`間はライブラリが同期する。
- callback内から同期wait APIを呼ぶことは禁止する。
- タイムアウト付き同期helperは非同期操作の上に構築し、無期限にblockしない。
- latencyが重要なraw callbackを追加する場合は、stack contextであることをAPI名と文書で明示する。

## Backend境界

公開APIはArduino-ESP32同梱BLEライブラリの型を通常利用に要求しない。backend固有のinclude、callback変換、error変換、機能差は内部層へ閉じ込める。

同梱BLE API自体はBluedroid/NimBLEの両backendを扱えるが、EspBleのBLE側はNimBLE hostだけを対象とする。`CONFIG_NIMBLE_ENABLED`やHosted BLEを示す`CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE`などのcompile-time設定を確認する。coreのプリビルドがBluedroid固定の無印ESP32は、そのBluedroidをBLEに使うのではなく、EspBleが同梱するNimBLE hostで動かす。Bluedroidを使うのはClassic側だけで、そちらもcore内蔵ではなく名前空間化した独自archiveである。

境界はnative controllerとHosted controllerの両方を許容する。ESP32-P4 + ESP32-C6のようなHosted BLEでもtransport差をアプリケーションへ露出させないが、利用可能機能とresource上限はbackend capabilityとして個別に判定する。

## Profile境界

Profileは、必要なService/Characteristic/Descriptor定義をGATT層へ登録し、標準wire formatをencode/decodeし、Profile固有イベントと操作を提供する。

Profileは、BLE stackを初期化しない。GATT Server全体やAdvertising全体を所有しない。Connectionをグローバル変数として隠さない。独自拡張を標準UUIDの値へ混入しない。

## ESP32KeyBridgeとの境界

adapterはスケッチ所有の`EspBle`参照を受け取り、`begin()`は呼ばない。HID Profileイベントと操作をBridgeのinput/output interfaceへ変換する。Pairing開始、Bond削除、Advertising開始などの運用操作はBridge coreではなく、EspBleまたはadapterの明示操作として提供する。
