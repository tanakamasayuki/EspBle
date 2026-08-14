# 他のライブラリからEspBleへ

> English: [GUIDE_MIGRATION.md](GUIDE_MIGRATION.md)

Arduino-ESP32同梱の`BLEDevice`系、NimBLE-Arduino、`BluetoothSerial`でsketchを書いてきた人向けに、
既知の概念をEspBleへ対応づける文書です。tutorialではなく対応表なので、概念そのものは
[BLE入門ガイド](GUIDE_BLE_BASICS.ja.md)と[Classic入門ガイド](GUIDE_CLASSIC_BASICS.ja.md)、
負荷時の振る舞いは[EspBleを深く使う](GUIDE_ADVANCED.ja.md)を参照してください。

## 1. 構造が変わる3点

残りは名前の読み替えです。この3つはsketchの形を変えるので、1行ずつ移す前に読んでください。

**1. `update()`を呼ぶ必要があります。**EspBleはeventをすべて`EspBle::update()`
（および`EspBleClassic::update()`）から自分のtask上で配送するので、`loop()`が呼ばなければ
なりません。stack taskから配送するlibraryにはこれが要りません。忘れた移植は「接続はするのに
その後黙る無線」に見えます。

```cpp
void loop() {
  ble.update();      // 必須
  // 自分の処理
}
```

**2. callbackはsubclassではなく値です。**callback classを継承してinstanceを渡すのではなく、
`std::function`を代入します。lambdaとcaptureが使え、`new`するものは無く、1つのeventを複数の
observerが見られます（`addXListener()`。eventごとにprimary + 4件）。

```cpp
ble.onConnected([](const EspBleConnection &connection) { /* ... */ });
```

**3. GATT databaseは`begin()`より前に組みます。**service、characteristic、descriptorを先に
登録し、`begin()`が確定して開始します。backendは後からserviceを追加できないので、接続を
契機にserviceを作るsketchは構造を変える必要があります。handleは小さな値型
（`EspBleGattService`、`EspBleGattCharacteristic`）で返り、自分が所有するpointerではなく、
保持して渡し直すものになります。

## 2. core同梱のBLEラッパから

Arduino-ESP32は`BLEDevice` / `BLEServer` / `BLEClient`を同梱しています（無印ESP32では
Bluedroid、それ以外ではNimBLE）。EspBleはこのラッパ全体を置き換えます。1つのsketchで
混ぜないでください。

| 同梱ラッパ | EspBle |
|---|---|
| `BLEDevice::init("name")` | `EspBleConfig::deviceName`を設定して`ble.begin(config)` |
| `BLEDevice::deinit()` | `ble.end()` |
| `BLEDevice::createServer()` | `ble.gattServer()`——serverは1つで常に在る |
| `server->createService(uuid)` | `begin()`前に`gattServer().addService(uuid)` |
| `service->createCharacteristic(uuid, PROPERTY_READ \| PROPERTY_NOTIFY)` | `gattServer().addCharacteristic(service, uuid, config)`。`EspBleGattCharacteristicConfig`の項目（`readable`、`writable`、`writableWithoutResponse`、notify / indicate）で指定する |
| CCCDのための`new BLE2902()` | 追加するものは無い。購読状態は`onSubscriptionChanged()`で届く |
| `characteristic->setValue(...)` | `gattServer().setValue(characteristic, value)` |
| `characteristic->notify()` | `gattServer().notify(characteristic, value)`（indicationは`indicate()`） |
| `setCallbacks(new MyCharacteristicCallbacks())` | `gattServer().onWritten(...)`、`onRead(...)`、`onDescriptorWritten(...)`、`onSubscriptionChanged(...)`、`onSent(...)` |
| `BLEDevice::getAdvertising()`とstart / stop | `ble.advertising()` |
| `BLEDevice::getScan()`、`setActiveScan()`、`start(seconds)` | `EspBleScanConfig`を渡した`ble.scanner()`。結果は`update()`からscan callbackへ届く |
| peerごとの`BLEClient`と`connect(address)` | `ble.connect(scanResult)`または`ble.connect(address, addressType)`。相手はconnection idで識別する |
| `client->getService(uuid)->getCharacteristic(uuid)->readValue()` | `ble.readCharacteristic(connectionId, serviceUuid, characteristicUuid)`——非同期でcallbackが答える。属性handle指定も可 |
| `characteristic->registerForNotify(cb)` | `ble.subscribe(...)`。notificationはnotification callbackへ届く |
| `BLESecurity`、`setStaticPIN()` | `EspBleConfig::security`（IO capability、bonding、MITM）、`providePasskey()`、`confirmNumericComparison()` |

移植で引っかかる挙動の違い:

- **readとwriteは非同期です。**文字列を返す`readValue()`はありません。結果はcallbackで受け、
  GATT操作は実行中1件だけ（queueは8件）です。characteristic 10件を読む`for`loopは連鎖に
  書き換える必要があります。
- **peerごとのclient objectではなくconnection idです。**複数同時接続が1つの`EspBle`を共有し、
  接続ごとにcacheと購読を持ちます。
- **service discoveryは明示です**（`discover()`）。結果はpointerの木を辿るのではなく報告されます。

## 3. NimBLE-Arduinoから

NimBLE-Arduino（`NimBLEDevice`、`NimBLEServer`など）はEspBleと同じhostを呼ぶので、概念は
そのまま対応し、主に所有modelが違います。

| NimBLE-Arduino | EspBle |
|---|---|
| `NimBLEDevice::init("name")` | `ble.begin(config)` |
| `NimBLEServer` / `NimBLEService` / `NimBLECharacteristic`のpointer | `gattServer().addService()` / `addCharacteristic()`が返す値handle |
| `NimBLECharacteristicCallbacks`の継承 | serverの`onWritten()` / `onRead()` / `onSubscriptionChanged()` |
| `NimBLEClient::connect()`、`getService()`、`getCharacteristic()` | `ble.connect()`、`discover()`、その後UUIDまたはhandle指定の操作 |
| `NimBLEAdvertising` | `ble.advertising()` |
| `NimBLEDevice::setMTU()` | `EspBleConfig::preferredMtu`（既定247）と`onMtuChanged()`の実効値 |
| stack taskで走るcallback | `update()`が配送するcallback（例外4つは[深く使う §1](GUIDE_ADVANCED.ja.md#1-callbackはどこで動くか)） |

EspBleが上に足すもの: 接続ごとのcacheを持つ複数同時接続、再接続後に復元されるpersistent
subscription、auto-reconnect、合成できるHID device / host profile、BLE MIDI、そして無印ESP32では
Bluetooth Classicと両無線の同時利用。

EspBleが公開しないもの: 生の`ble_gap_*` / `ble_gattc_*`呼び出し。NimBLEを直接叩くことに
依存しているsketchにとって、EspBleはdrop-in replacementではありません。

## 4. `BluetoothSerial`から（Classic SPP）

`BluetoothSerial`は「Bluetoothをserial portとして使う」APIです。EspBleでは`EspBleClassic`と
SPPが同じ範囲を担い、出発点は[`Classic/SppStream`](../examples/Classic/SppStream/)です。

| `BluetoothSerial` | EspBle |
|---|---|
| `SerialBT.begin("name")`（device側） | `EspBleClassicConfig::deviceName`、`classic.begin(config)`、`classic.spp().startServer()` |
| `SerialBT.begin("name", true)`（master側） | `classic.spp().connect(address)`または`connectToChannel(address, channel)` |
| `SerialBT.available()` / `read()` / `write()` / `print()` | sessionへattachした`EspBleClassicSppStream`。Arduinoの`Stream`なので`readStringUntil()`や`parseInt()`も使える |
| `SerialBT.hasClient()` | `spp().onConnected()` / `onDisconnected()`のsession event、または`spp().sessionCount()` |
| `SerialBT.setPin()` | legacy PIN pairingは意図して拒否する。`EspBleClassicSecurityConfig`を設定し、passkeyまたはnumeric comparisonのeventへ応答する |
| `SerialBT.discover()` | `classic.inquiry()`——address、name、Class of Device、RSSI。加えて`requestServices()` / `requestName()` |
| `SerialBT.end()` | `classic.end()` |

移植前に知っておく違い:

- **`write()` 1回が1 SPP packetになります**（最大990 byte）。1文字ずつより1行ずつが圧倒的に
  安いです。
- **送信queueは有限です**（8 write）。`Stream::write()`は`setWriteTimeout()`（既定1000 ms、
  0なら待たない）まで待って書けた分を返します。bufferが尽きた`Serial`と同じ挙動です。
- **`classic.update()`は`loop()`で呼びます。**session eventはそこで届きます。
- **1台でSPP serviceを複数公開できます**（4件）。それぞれ別のRFCOMM channelを持つので、
  client側に`connectToChannel()`があります。
- **`0x00`はデータです。**終端ではなく、byte streamは双方向でbinary safeです。

## 5. 音声libraryから

よく使われるA2DP sink実装はPCMを渡してI2Sを駆動します。EspBleは意図して1層手前で止まります
——`a2dpSink()`が報告するのは**encode済み**のSBC frameで、HFPは生のSCO frameです。復号、PCM
処理、device I/Oは上に載る別libraryの担当です（この境界がBluetooth側を単体で試験可能にして
います）。

したがって移植はこうなります。EspBleがcodec設定（`onCodecConfigured()`）とencode済みpayload
（`onMedia()`。callbackの中でだけ有効なので、残すなら`copy`する）を渡し、自分のdecoderがPCMを
作ります。AVRCPの操作（再生・一時停止キー、absolute volume）とA2DPのdelay reportingは
`avrcp()`と`setDelay()`で使えます。

decoderを書かずにPCMが欲しいなら、その部分はaudio libraryに任せ、EspBleはtransportとして
使ってください。EspBleがdecoderを持つようになるのを待つのではなく。

## 6. 対応するものが無いもの

これらを前提に移植計画を立てないでください。

- **生HCI、生NimBLE / Bluedroid APIはありません。**無印ESP32では、HCI brokerのopcode分類が
  両無線の同時利用を安全にしています。
- **VFS経由のSPP**（`esp_spp_vfs_register()`）はありません。`EspBleClassicSppStream`が
  file descriptor経路を増やさずに同じ用途を満たします。
- **codec、PCM、device I/O**は上記のとおり対象外です。
- **BLE Audio（LE Audio）**はありません。
- **無印ESP32ではExtended / Periodic Advertising、LE 2M / Coded PHYが使えません。**このchipの
  controllerはBLE 4.2相当で、同梱hostはextended advertisingを無効にしてbuildしています。
  coreのhostがそれを提供するcontroller内蔵targetでは使えます。
- **ClassicのHID Hostは同時1 device**、HID Host接続も1本です。

何が在り、何が実機検証済みで、何がそうでないかは[機能対応マトリクス](FEATURE_MATRIX.ja.md)と、
Classicについては[Classic機能の棚卸し](CLASSIC_FEATURE_INVENTORY.ja.md)にあります。
