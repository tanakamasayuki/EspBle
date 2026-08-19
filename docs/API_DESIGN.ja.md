# API Design

> English: [API_DESIGN.md](API_DESIGN.md)

公開APIの**設計規則**を記録する。具体的なclass名とsignatureは`src/EspBle.h`を正とし、使い方は[../examples/](../examples/)と[GUIDE_BLE_BASICS.ja.md](GUIDE_BLE_BASICS.ja.md)が示す。**この文書に使用例を書かない**——ヘッダとexampleと三重に持つと必ず食い違う（実際にここのサンプルが古い署名のまま残っていた）。

判断の理由は[DECISIONS.ja.md](DECISIONS.ja.md)、用語は[TERMINOLOGY.ja.md](TERMINOLOGY.ja.md)に従う。

## 命名

- library root: `EspBle`。標準用語（Central、Peripheral、GATT Client、GATT Server、Connection）を使い、`Host` / `Device`だけの曖昧な表現を避ける。
- 公開定数と型は`EspBle`または`ESP_BLE_` prefixで衝突を避ける。
- Profile型では`HidHost` / `HidDevice`のようにroleを省略しない。

## 所有モデル

- スケッチが`EspBle`を所有する。
- Scanner、Advertiser、GATT Server、各Profileは`EspBle`から取得する**非所有handle**とする。
- 登録したService/Characteristic/Descriptorは`EspBle`が寿命を管理する。
- Connectionは切断後に無効化を判定できるlibrary handleで表す。
- backend native objectはborrowed referenceとして扱い、保存を保証しない。

## 操作と結果の分離

- **受理時の同期エラー**は`bool`戻り値＋`lastError()` / `lastErrorName()` / `lastErrorDetail()` / `clearError()`。`EspBleError`は`None` / `InvalidState` / `InvalidArgument` / `BackendFailure` / `ResourceExhausted` / `NotFound` / `Timeout`。
- **非同期の完了・失敗**は各イベント（`EspBleGattResult`等）のsuccess / error / detailフィールドと、Connection / Characteristic contextで通知する。
- `lastError*`は単一状態のため、操作呼び出しは単一のloop task contextから行うことを前提とする。
- operation idと個別の強制cancelは導入しない。
- 待機を伴うbackend操作は内部taskで実行し、要求APIがloopをblockしない。

## イベント

- 通常callbackは`update()` contextで配送する。stack contextで呼ぶraw callbackを追加する場合は、API名と文書でそれを明示する。
- コアGATT callbackと接続系callbackは**primary 1（`on*`）＋listener 複数（`add*Listener` / `removeGattListener`・`removeConnectionListener`・`removeListener`）**の多observerモデル。`on*`はprimaryを差し替えるだけなので単一observer用途はそのまま満たす。listener上限はowner種別ごとに4件。
- ただし**応答を求めるcallbackはprimary 1つのみ**とする（`onPasskeyDisplayed` / `onNumericComparison`）。観測は何人いてもよいが、答える責任者は1人でなければ誰が答えるのかが決まらない。
- イベントはConnection ID、対象UUIDまたは属性ハンドル、結果、payloadを必要に応じて持つ。payloadの寿命を型ごとに明記する。
- callbackを使わない利用者向けに状態getterを提供する。
- queue overflowは専用イベントではなくdropカウンタ（`droppedEventCount()` / `droppedResultCount()` / `droppedPersistentSubscriptionCount()` / `invalidInputReportCount()`）で観測する。

## 対象の指定

- UUIDは「型」であって「どれか」ではない。**同一UUIDが重複しうる対象は属性ハンドルで指定できる**ようにする。
- Server側は`addService()` / `addCharacteristic()` / `addDescriptor()`が返すハンドルを以降の操作で使う。
- Client側はUUID指定に加えてハンドル指定のoverloadを持つ（characteristicのread / write / subscribe / unsubscribe、descriptorのread / write）。結果は`handle`（characteristic）と`descriptorHandle`（descriptor）を返す。
- UUID指定は、そのUUIDが一意なときの簡便な経路として維持する。

## 値とcodec

- GATTコアはbyte sequenceを扱う。公開の値containerはpointer+lengthを基本とし、NULを含めてcopyできる`String`を便宜overloadとして提供する。
- string、integer、Bluetooth SIG形式、HID report、Battery Levelなどは明示的なcodec / profile helperで変換する。
- **CPUのendiannessやC++ struct layoutを暗黙にwire formatへ使わない。**

## 構成の順序

- GATT ServerのService/Characteristic定義とProfileの`configure()`は`begin()`前に行う。security permissionも登録時に決まる。
- `begin()`後の構成変更は禁止し、再構成は`end()`後に行う。
- 構成上限はcompile-time定数（Server: Service 8 / Characteristic 32 / Descriptor 16、Discovery snapshot: Service 16 / Characteristic 48 / Descriptor 48、Advertising Service UUID 4、Service Data 4）。上限超過は明示的なresource errorとして返す。

## Profile API

- Profileは`EspBle`から取得したhandleを`begin()`前に`configure()`する。`configured()`で状態を確認できる。
- 送信系は同期`bool`（受理）＋非同期イベント（結果）の分離に従う。ただしHIDの`setKeyboardLeds()`のようにWrite Without Responseで撃つものは、受理だけを返して配達確認をしないことを文書で明示する。
- Profileは汎用GATT callbackを**listenerとして**使い、primaryを独占しない。
- 詳細は[HID_DEVICE_SPEC.ja.md](HID_DEVICE_SPEC.ja.md) / [HID_HOST_SPEC.ja.md](HID_HOST_SPEC.ja.md)。Report Descriptorを自作する場合は[HID descriptorガイド](GUIDE_HID_DESCRIPTORS.ja.md)を参照。

## 拡張するとき

新しいAPIは既存の境界——Connection ID、非同期event、`update()`配送、byte sequence、ハンドルによる対象指定——を維持する。採用した判断は[DECISIONS.ja.md](DECISIONS.ja.md)へ記録する。未実装機能と優先順位は[FEATURE_MATRIX.ja.md](FEATURE_MATRIX.ja.md)と[STATUS.ja.md](STATUS.ja.md)で管理する。
