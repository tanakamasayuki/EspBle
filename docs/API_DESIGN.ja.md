# API Design

この文書は公開APIを実装する前の設計規則です。具体的なclass名とsignatureはPeer用の最小GATT vertical sliceで検証してから確定します。

用語とexampleの変数命名は[TERMINOLOGY.ja.md](TERMINOLOGY.ja.md)に従います。

## 命名

- library root: `EspBle`
- standard terms: Central、Peripheral、GATT Client、GATT Server、Connection
- `Host` / `Device`だけの曖昧な表現を避ける。
- 公開定数と型は`EspBle`または`ESP_BLE_` prefixで衝突を避ける。
- Profile型では`HidHost` / `HidDevice`のようにroleを省略しない。
- examplesでは複数roleが登場する場合、`hidKeyboardHost` / `hidKeyboardDevice`のように変数名でもroleを明示する。
- 単一roleで文脈が自明なexampleでは`keyboard`などの簡潔な変数名を許容し、個別に判断する。

## 所有モデル

- スケッチが`EspBle`を所有する。
- Scanner、Advertiser、Serverは`EspBle`から取得する非所有handleとする。
- Service/Characteristic/Profileは登録後`EspBle`が寿命を管理する案を優先する。
- Connectionは切断後に無効化を判定できるlibrary handleで表す。
- backend native objectはadvanced APIからのみ参照可能にする。

## GAP / Connection vertical sliceの試行API

GAP/Connectionの最初の実装では、次の利用形をPeer実機で検証しています。汎用GATTまで通してから公開APIとして確定します。

```cpp
EspBle ble;

ble.begin();

ble.advertising().setName("EspBle Peer");
ble.advertising().addServiceUuid(serviceUuid);
ble.advertising().start();

ble.scanner().onResult([](const EspBleScanResult &scanResult) {
  if (scanResult.advertisesService(serviceUuid)) {
    // Scan Resultはcallback中だけのbackend参照ではなく、EspBleが所有する値。
    ble.scanner().stop();
    ble.connect(scanResult); // 要求の受理だけを返し、完了はcallbackで通知する。
  }
});
ble.scanner().start();

ble.onConnected([](const EspBleConnection &connection) {
  // idはbackend handleとは別の、接続ごとに生成される安定した識別子。
});
ble.onDisconnected([](const EspBleConnection &connection) {
  // 切断イベントにも接続時と同じidとlocal roleが含まれる。
});
ble.onConnectionFailed([](const EspBleConnectionFailure &failure) {
  // 非同期接続失敗。
});
```

現在の`ble.update()`はstack callbackからqueueへcopyしたScan ResultとConnection eventをユーザーcallbackへ配送します。Arduino-ESP32 backendの待機型接続処理はEspBle内部taskで実行するため、`connect()`はloopやScan Result callbackを接続完了までblockしません。これはcallbackの寿命と実行contextを検証する暫定実装であり、利用者が常に`update()`を呼ぶ最終仕様はまだ確定していません。

`EspBleConnection`はlibrary connection id、backend handle、peer address/address type、local role、MTUの値snapshotです。初期実装は最大4接続を内部管理しますが、この上限と設定方法はまだ公開仕様として確定していません。

Legacy Advertising payloadが31 bytesへ収まらない場合、Arduino-ESP32 backendの黙示的なfield欠落をそのまま通さず、EspBleは`INVALID_ARGUMENT`を返します。

## 結果型

即時に受理/拒否が決まる操作は、次を持つ共通Resultで返します。

- library error category
- backend error code
- operation
- connection id（該当時）

完了が非同期の操作はoperation idまたはConnection/Characteristic contextをイベントへ含めます。単純なgetter以外を`bool`だけで終わらせません。

## Event API

- 通常callbackは`update()` contextで配送する。
- listener登録と解除を提供する。
- Connection id、対象UUID、結果、payloadを必要に応じて含める。
- payloadの寿命を型ごとに明記する。
- callbackを使わない利用者向けに状態getterを提供する。

## GATT値とcodec

GATTコアはbyte sequenceを扱います。string、integer、Bluetooth SIG形式、HID report、Battery Levelなどは明示的なcodec/profile helperで変換します。CPUのendiannessやC++ struct layoutを暗黙にwire formatへ使用しません。

## 未確定事項

- Result型とasync operation idの具体形
- handleを値型、index+generation、参照classのどれにするか
- Service builderと明示的add APIの比較
- event queue容量とcompile-time設定方法
- 明示`update()`、内部task、または選択式のevent配送方式
- 同期helperを初期版へ含める範囲
- 後からPairing/Bondingを実装できるSecurity設定とCharacteristic permissionの形
- raw backend accessの公開範囲

これらは汎用GATT Peerテストの最初の実装前に`DECISIONS.ja.md`へ記録して確定します。
