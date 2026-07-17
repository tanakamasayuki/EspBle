# Core Design

## レイヤ構成

```text
Application / ESP32KeyBridge adapter
  Profile: HID / Battery / future profiles
  Generic GATT Client / Server
  Connection / Security
  GAP: Scanner / Advertiser
  EspBle stack owner
  Arduino-ESP32 bundled BLE API with NimBLE backend
```

`EspBle`はスタックの唯一の所有者です。Scanner、Advertiser、GATT Server、各Profileは`EspBle`から取得または登録し、個別にスタックを初期化しません。

## 初期化と構成確定

基本ライフサイクルは次の順序とします。

1. `EspBle`と必要なService/Profileを構築する。
2. Advertising、Security、GATT Server構成を登録する。
3. `begin()`でスタックとGATT databaseを開始する。
4. Advertising/Scanning/Connection操作を開始する。
5. `end()`で操作を止め、接続を閉じ、スタック資源を解放する。

`begin()`後のGATT Server構成変更は禁止し、再構成は`end()`後に行います。これは同梱backendの確認済み制約に基づく不変条件です: `createServer()`は`ble_gatts_reset()`を呼ぶためそれ以前の登録は消え、serviceの実登録は`BLEServer::start()`（Advertising開始が自動で呼ぶ）時の1回だけで、start後に追加したserviceは二度と登録されません（`CONFIG_BT_NIMBLE_DYNAMIC_SERVICE`無効ビルド）。将来動的Service追加を検討する場合もこの制約が前提になります。

## 状態モデル

状態を単一enumへ押し込みません。

- Stack lifecycle: uninitialized / initializing / ready / stopping / error
- Scanner state: idle / scanning / stopping
- Advertising state: advertising set単位のidle / advertising / stopping
- Connection state: connecting / connected / disconnecting / disconnected

Advertising、Scanning、複数Connectionは同時に成立できます。集約状態は個別状態から算出します。

## Connection identity

Connectionは次を持ちます。

- library生成の安定したconnection id
- backend connection handle
- peer addressとaddress type
- local role（central/peripheral）
- lifecycle、MTU、接続パラメータ
- encryption/authentication/bond状態
- GATT discoveryとsubscription状態

切断でbackend handleが再利用されても古いConnection参照を別接続として扱いません。公開APIでは世代を識別できるhandleまたはライブラリ管理参照を使用し、無効化を検出します。

## データ所有権

- UUIDと小さなイベント情報は値として保持する。
- Characteristic valueの基本表現はbyte sequenceとする。
- callbackへ渡す一時bufferの寿命をcallback終了までと明示する。
- callback後も必要なScan Result/Discovery Resultは利用者所有の値へcopyできるようにする。
- Service/Characteristic定義は`EspBle`より長生きしてはならず、原則として`EspBle`が所有する。
- backend native objectへのアクセスはborrowed referenceであり、保存を保証しない。

固定容量か動的確保かは対象データごとに決定し、上限超過を明示的なresource errorとして返します。

## 非同期処理と実行文脈

BLE処理は非同期を基本とします。スタックcallback内では状態更新とイベントqueue投入だけを行い、利用者callbackは`update()`を呼んだコンテキストから配送します（確定仕様。内部task配送・選択式は採用しません）。`update()`を呼ばない限り、connect/discover等の完了通知も配送されません。

- `update()`は単一コンテキストから呼ぶ。
- stack taskと`update()`間はライブラリが同期する。
- callback内から同期wait APIを呼ぶことは禁止する。
- queue overflowは破棄数カウンタ（`droppedEventCount()`等）で観測可能にし、lifecycle・完了イベントを優先保持する。専用のoverflowイベントは設けない。queue容量はcompile-time定数で、実行時設定APIは設けない。
- latencyが重要なraw callbackを追加する場合は、stack contextであることをAPI名と文書で明示する。

タイムアウト付き同期helperは非同期操作の上に構築し、無期限にblockしません。

## Backend境界

公開APIはArduino-ESP32同梱BLEライブラリの型を通常利用に要求しません。backend固有のinclude、callback変換、error変換、機能差は内部層へ閉じ込めます。

Arduino-ESP32 3.3.10の同梱BLE API自体はBluedroid/NimBLEの両backendを扱えますが、EspBleはそのうちNimBLE backendだけを対象とします。`CONFIG_NIMBLE_ENABLED`またはHosted BLEを示す`CONFIG_ESP_HOSTED_ENABLE_BT_NIMBLE`など、Arduino-ESP32が提供するcompile-time設定を確認し、Bluedroid構成では明示的にunsupportedとします。外部NimBLE-Arduinoとの互換性は保証しません。

Backend境界はnative controllerとHosted controllerの両方を許容します。ESP32-P4へ規定GPIOでESP32-C6などを接続するHosted BLEでも、Arduino-ESP32の共通BLE APIより下のtransport差をアプリケーションへ露出させません。ただし利用可能機能とresource上限はbackend capabilityとして個別に判定します。

Peer testの`s3_peer_host` / `s3_peer_device`はpytest-embedded-cliが2台を識別するprofile名であり、BLE Central / Peripheral、GATT Client / Serverなどのroleを表しません。両方のsketchは転送・実行され、両方のSerialを同じpytestから観測・操作できます。初期構成では親側sketchをCentral、`peer_device/`側sketchをPeripheralに固定し、役割交換は行いません。

## Profile境界

Profileは次を行います。

- 必要なService/Characteristic/Descriptor定義をGATT層へ登録する。
- 標準wire formatをencode/decodeする。
- Profile固有イベントと操作を提供する。

Profileは次を行いません。

- BLE stackを初期化する。
- GATT Server全体またはAdvertising全体を所有する。
- Connectionをグローバル変数として隠す。
- 独自拡張を標準UUIDの値へ混入する。

## ESP32KeyBridgeとの境界

ESP32KeyBridgeのadapterはスケッチ所有の`EspBle`参照を受け取ります。adapterは`begin()`を呼ばず、HID Profileイベントと操作をBridgeのinput/output interfaceへ変換します。Pairing開始、Bond削除、Advertising開始などの運用操作はBridge coreではなくEspBleまたはadapterの明示操作として提供します。
