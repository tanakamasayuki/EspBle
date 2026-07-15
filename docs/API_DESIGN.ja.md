# API Design

この文書は公開APIの設計規則と、実装中の試行APIを記録します。具体的なclass名とsignatureは各vertical sliceのPeer実機検証を通して確定します。

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

`EspBleConnection`はlibrary connection id、backend handle、peer address/address type、local role、MTU、暗号化・認証・Bond状態、暗号鍵長の値snapshotです。初期実装は最大4接続を内部管理しますが、この上限と設定方法はまだ公開仕様として確定していません。

## MTU vertical sliceの試行API

希望MTUはstack初期化前の設定として指定します。値域はATTで定められた23〜517で、範囲外は`begin()`が`INVALID_ARGUMENT`で拒否します。同梱NimBLE backendが接続時にMTU交換を行い、合意した値をConnection snapshotと`onMtuChanged()`へ反映します。

```cpp
EspBleConfig config;
config.preferredMtu = 185;
ble.begin(config);

ble.onMtuChanged([](const EspBleMtuChanged &event) {
  Serial.printf("MTU: %u -> %u\n", event.previousMtu, event.connection.mtu);
  Serial.printf("notification payload: %u\n",
    static_cast<unsigned>(event.connection.maximumNotificationPayload()));
});
```

`maximumNotificationPayload()`は現在のMTUからATT notification/indicationの3-byte headerを除いた`mtu - 3`を返します。Server送信はbackendによる黙示的な切詰めを避けるため、activeなPeripheral Connectionのうち最小の上限を超えるpayloadを`INVALID_ARGUMENT`で拒否します。この判定は購読者だけでなく全active Peripheral Connectionを対象にする保守的な暫定実装です。

接続後にアプリケーションからMTU交換を再要求するAPIはまだ提供しません。希望MTUの動的変更、複数接続ごとの送信可否判定、MTU callbackの厳密な順序は、backendの制約と複数接続テストを踏まえて確定します。

Legacy Advertising payloadが31 bytesへ収まらない場合、Arduino-ESP32 backendの黙示的なfield欠落をそのまま通さず、EspBleは`INVALID_ARGUMENT`を返します。

## Generic GATT vertical sliceの試行API

GATT ServerのService/Characteristic定義は`begin()`前に登録します。この順序により、後からSecurity permissionをCharacteristic定義へ追加できます。

```cpp
auto &gattServer = ble.gattServer();

EspBleGattCharacteristicConfig valueConfig;
valueConfig.readable = true;
valueConfig.writable = true;

gattServer.addService(serviceUuid);
gattServer.addCharacteristic(serviceUuid, characteristicUuid, valueConfig);
gattServer.setValue(serviceUuid, characteristicUuid, String("ready"));
gattServer.onWritten([](const EspBleGattWrite &write) {
  // connectionId、Service/Characteristic UUID、書込み値を保持する値イベント。
});

ble.begin();
```

Central側のDiscovery、Read、Writeは要求の受理と完了を分離します。同梱backendの待機型操作は内部taskで実行し、結果は`update()` contextのcallbackへ配送します。

```cpp
ble.onCharacteristicDiscovered([](const EspBleGattResult &result) {
  if (result.success && result.readable) {
    ble.readCharacteristic(result.connectionId, serviceUuid, characteristicUuid);
  }
});
ble.onCharacteristicRead([](const EspBleGattResult &result) {
  if (result.success) {
    ble.writeCharacteristic(
      result.connectionId, serviceUuid, characteristicUuid, String("new value"), true);
  }
});

ble.discoverCharacteristic(connectionId, serviceUuid, characteristicUuid);
```

現在のDiscoveryは既知のService/Characteristic UUIDを指定し、存在とpropertyを取得する最小形です。Service一覧やCharacteristic一覧を列挙するAPIは未実装です。また、初期実装は同時に1件のCentral GATT操作だけを受理します。operation queue、operation id、cancelは実装経験を基に後で確定します。

GATT値はNULを含めてcopyできる`String`を暫定的なbyte containerとして使用し、pointer+length overloadも提供します。最終的な公開値型と最大長はHID reportの実装後に確定します。

## Subscription / Notify / Indicateの試行API

Central側は既知UUIDのCharacteristicへNotificationまたはIndicationを購読します。`notifications=true`がNotification、`false`がIndicationです。CCCD書込み完了は`onSubscribed()`、解除完了は`onUnsubscribed()`、受信payloadは`onNotification()`へ配送します。

```cpp
ble.onConnected([](const EspBleConnection &connection) {
  ble.subscribe(connection.id, serviceUuid, characteristicUuid, true);
});
ble.onSubscribed([](const EspBleGattResult &result) {
  // result.subscribedToNotifications / subscribedToIndications
});
ble.onNotification([](const EspBleGattNotification &notification) {
  // connectionId、UUID、copy済みpayload、indicationフラグを持つ。
});

ble.unsubscribe(connectionId, serviceUuid, characteristicUuid);
```

GATT Server側はCCCD変更をConnection ID付きで受け取り、`notify()` / `indicate()`で送信します。どちらも要求受理時にはblockせず、Notification送信結果またはIndication確認結果を`onSent()`へ配送します。

```cpp
gattServer.onSubscriptionChanged([](const EspBleGattSubscription &subscription) {
  // notifications / indicationsで現在のCCCD状態を確認する。
});
gattServer.onSent([](const EspBleGattSendResult &result) {
  // result.successとdetailで非同期送信結果を確認する。
});

gattServer.notify(serviceUuid, characteristicUuid, String("value"));
gattServer.indicate(serviceUuid, characteristicUuid, String("confirmed value"));
```

現在のServer送信は、そのCharacteristicで該当方式を購読している全Connectionが対象です。Connectionを指定する送信、複数購読者ごとの送信結果、送信queueとbackpressureは複数接続Peerテストと合わせて設計します。

## Security / Bonding vertical sliceの試行API

初期SecurityはLE Secure Connectionsを使用し、No Input / No OutputのJust Worksと静的passkeyによるMITM Pairingを扱います。Securityは`begin()`前の設定で有効にし、接続時の自動PairingとBond保存を個別に指定できます。無効が既定値です。

```cpp
EspBleConfig config;
config.security.enabled = true;
config.security.bonding = true;
config.security.pairOnConnect = true;
ble.begin(config);

ble.onSecurityChanged([](const EspBleSecurityChanged &event) {
  if (event.success) {
    // encrypted、authenticated、bonded、encryptionKeySizeを参照できる。
  }
});
```

`pairOnConnect=false`の場合や、アプリケーションが明示的にSecurityを開始したい場合は`requestSecurity(connectionId)`を使用します。要求の受理だけを同期的に返し、完了時のConnection snapshotは`onSecurityChanged()`へ配送します。

Just Worksではlinkは暗号化されますがMITM認証されないため、`encrypted=true`、`authenticated=false`になることをPeerで確認しています。`authenticated`をPairing成功の意味には使用しません。

静的passkeyでは、表示側を`DisplayOnly`、入力側を`KeyboardOnly`として同じ6桁値を設定します。現在の`KeyboardOnly`は事前設定値をbackendへ渡す方式で、実行中にユーザー入力を待つAPIではありません。

```cpp
config.security.mitm = true;
config.security.ioCapability = EspBleSecurityIoCapability::DisplayOnly;
config.security.staticPasskeyEnabled = true;
config.security.staticPasskey = 438209;

ble.onPasskeyDisplayed([](const EspBlePasskeyDisplayed &event) {
  Serial.printf("Passkey: %06u\n", static_cast<unsigned>(event.passkey));
});
```

passkey表示イベントはstack callbackからcopyし、`ble.update()` contextへ配送します。同梱backendの表示callbackにConnection handleがないため、現在はSecurity確立前のConnectionを対応付けます。複数Connectionで同時にPairingする場合の識別方法は未確定です。passkeyは`000000`〜`999999`を受理しますが、製品では共通のhard-coded値ではなくデバイスごとの安全なprovisioningを使用します。

GATT ServerはCharacteristic単位で暗号化を要求できます。暗号化指定には対応するread/write propertyも必要です。

```cpp
EspBleGattCharacteristicConfig valueConfig;
valueConfig.readable = true;
valueConfig.writable = true;
valueConfig.encryptedRead = true;
valueConfig.encryptedWrite = true;
```

MITM認証済みlinkを要求する場合は`authenticatedRead` / `authenticatedWrite`を使用します。静的passkey Peerテストで`authenticated=true`のConnectionだけがread/writeできることを確認しています。

Bond storeには`bondCount()`、`bond(index, result)`、`deleteBond()`、`deleteAllBonds()`でアクセスします。削除中の接続状態との不整合を避けるため、現在の試行APIではすべての接続を切断してから削除します。保存上限はArduino-ESP32の`CONFIG_BT_NIMBLE_MAX_BONDS`に従います。

実行時のpasskey入力、Numeric Comparison、Pairing確認・拒否UI、Privacyは未実装です。これらはstack contextで即時回答が必要なbackend callbackと、現在の`ble.update()`配送をどう両立するかを先に設計します。Pairing失敗理由のbackend code表現と、Bond操作を同期Resultのままにするかも今後確定します。

## HID Keyboard Device vertical sliceの試行API

HID Keyboard Deviceは`EspBle`が所有するprofile handleとして取得し、`begin()`より前に構成します。単一roleで自明なexampleでは変数名を`keyboard`とします。

```cpp
auto &keyboard = ble.hidKeyboardDevice();
EspBleHidKeyboardDeviceConfig keyboardConfig;
keyboardConfig.manufacturer = "EspBle";
keyboardConfig.initialBatteryLevel = 100;
keyboard.configure(keyboardConfig);

EspBleConfig config;
config.deviceName = "EspBle Keyboard";
config.security.enabled = true;
config.security.bonding = true;
ble.begin(config);
ble.advertising().start();
```

入力はmodifier 1 byteと同時押し最大6 keyの値型で指定します。Report IDとreserved byteを利用者に組み立てさせません。`sendInputReport()`は接続中のHID Hostへ8-byte Report payloadをNotificationで送信し、`releaseAll()`は全フィールドが0のreportを送ります。

```cpp
EspBleHidKeyboardInputReport report;
report.modifiers = EspBleHidKeyboardInputReport::LeftShift;
report.keys[0] = 0x04; // Keyboard a and A
keyboard.sendInputReport(report);
keyboard.releaseAll();
```

LED Output Reportはstack callbackからcopyされ、`ble.update()` contextで配送されます。値には送信元Connection IDとNum Lock、Caps Lock、Scroll Lock、Compose、Kanaのbitが含まれます。Battery Levelは`setBatteryLevel(0..100)`で更新します。

このsliceはReport Protocolの固定6KRO keyboardだけを扱います。Boot Protocol、HID Host、文字からkey usageへの変換、layout、auto-release、送信queueは未実装です。GATT構成とwire formatの詳細は[HID Keyboard Device仕様](HID_KEYBOARD_DEVICE_SPEC.ja.md)に記載します。

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
- GATT値の最終的なbyte container型と最大長
- 全Service/Characteristic列挙APIとDiscovery Resultの保持方法
- 複数GATT operationのqueue、operation id、cancel
- Connection指定Notify/Indicate、送信queue、backpressure
- 接続後のMTU再交換APIと複数接続時のpayload上限判定
- 明示`update()`、内部task、または選択式のevent配送方式
- 同期helperを初期版へ含める範囲
- 実行時passkey入力、Numeric Comparison、Pairing拒否と失敗理由のAPI
- raw backend accessの公開範囲

これらはHIDのvertical slice、Security拡張、複数接続試験で検証し、確定したものから`DECISIONS.ja.md`へ記録します。
