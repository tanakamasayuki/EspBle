# EspBle

> English: [README.md](README.md)

ESP32 Arduino向けの汎用Bluetooth Low Energyライブラリです。**NimBLE Host APIを直接呼び出します。**
Arduino-ESP32同梱の`BLEDevice` / `BLEClient` / `BLEServer`などのラッパは
経由しません。Central / Peripheral、GATT Client / Server、Security、HID、BLE MIDIを
1つの`EspBle`基盤の上で組み合わせられます。

無印ESP32——S3やC3などの付かない初代のESP32——では、これに加えてBluetooth Classic
（SPP、HID device / host、A2DP、AVRCP、HFP）を`EspBleClassic`で使えます。
**BLEの公開APIはESP32シリーズのどのチップでも同じで、変わるのは使える無線機能と
検証範囲だけです**——違いは[対応環境](#対応環境)の表にまとめてあります。

> [!IMPORTANT]
> **ESP32-S3 / C3 / C6 / H2**はCore同梱のNimBLEをそのまま使う標準構成です。
> **無印ESP32**はCoreのプリビルドがBluedroidのため、EspBleがNimBLE hostを同梱して動かします。
> BLE 4.2 controllerなので使えない機能があり、代わりにBluetooth Classicが使えます。
> **ESP32-P4**はBLE無線を持たず、SDIOでつないだslave側のチップがBLEを担当します（ESP-Hosted）。
> slaveには**このライブラリとは別のfirmware**が要り、使える範囲はそのチップとversionで変わります。

## なぜEspBleを使うのか

- **Coreが選定したNimBLEをそのまま使う:** Arduino-ESP32が選定・buildしたESP-IDFの
  NimBLE Host、ControllerまたはESP-Hosted HCI構成を直接利用し、別のBLE stackを重ねません。
  Core同梱が無い無印ESP32だけは、同じ構成のhostをEspBleが持ち込みます。
- **低レベルの正確さをArduino向けAPIで扱う:** GATT attributeをUUIDだけでなく
  handleでも指定でき、同一UUIDのService/Characteristic、Descriptor、接続ごとの
  discovery snapshotを区別できます。非同期GATT操作はtimeout付きqueueで直列化されます。
- **CentralとPeripheralを同じ設計で組み合わせる:** scan、advertising、複数接続、
  GATT Client/Server、pairing/bondingを同じ`EspBle` instanceで構成できます。
- **HIDとBLE MIDIを一から組まなくてよい:** Keyboard、Mouse、Consumer/System Control、
  Gamepad、Custom HIDを1つのHID Serviceへ合成でき、HID HostとBLE MIDIの
  Device/Host helperも同じeventモデルで利用できます。
- **callbackの実行場所が予測可能:** 接続、GATT操作完了、通知、HIDなどの非同期eventは、
  `ble.update()`を呼ぶloop taskから配送します。同期応答が必要なGATT Serverの
  `onRead()`だけはstack task上で動く明示的な例外です。
- **実機testで振る舞いを固定:** ESP32を2台つないだPeer testで接続、GATT、Security、
  HID、再接続、異常系を検証し、host unit testと複数SoCのexample buildも実行します。

## 機能

| 分野 | できること | 主な挙動・API |
| --- | --- | --- |
| Advertising / Scan | 通常のAdvertising、active/passive scan、non-connectable Beacon、iBeacon、Service Data | Scan Resultはaddress、name、RSSI、Service UUID、Manufacturer Dataなどを保持する値型。Advertising payloadとScan Responseを個別に構成可能 |
| 接続管理 | CentralからScan Result/addressへ接続、Peripheralとして接続受け入れ、切断、複数同時接続 | アプリ向けの安定したconnection ID、接続状態snapshot、接続parameter/PHY更新、想定外切断からのauto-reconnect（既定off） |
| GATT Server | 独自Service、Characteristic、Descriptorの登録、Read/Write、Notify/Indicate | UUIDまたはattribute handleで対象を識別。同一UUIDのService/Characteristicにも対応。接続別の購読状態を追跡 |
| GATT Client | Database全体または既知UUIDのDiscovery、Characteristic/Descriptor Read/Write、購読/解除 | 接続ごとのdiscovery snapshot、handle指定、long read、timeout、自動operation queue。persistent subscriptionは再接続時に自動復元（既定on） |
| ATT / Link | MTU交換、payload上限検証、Connection Parameter、LE PHY | MTUと接続状態をconnection snapshotへ反映し、非同期の完了eventとして通知 |
| Security / Privacy | LE Secure ConnectionsによるJust Works・passkey・Numeric Comparison、Bonding、暗号化/認証permission | public / random static / Resolvable Private Address（RPA）を選択可能。P4/C6 HostedのSecurityは既知制限あり |
| HID Device | Keyboard（6KRO/NKRO）、Mouse、Consumer/System Control、Gamepad、Vendor、任意Custom Report | 複数profileを1つのHID Serviceへ合成。Report送信、Battery、LED Output、Boot Keyboard Protocolを提供 |
| HID Host | BLE keyboard/mouse/gamepadなどのDiscovery、購読、Report解析 | 6KRO/NKROをusage snapshotへ正規化、19 keyboard layout、LED出力、Vendor Input/Output/Feature Reportに対応 |
| BLE MIDI | MIDI Device / Host、Note、Control Change、Program Change、Pitch Bend、SysEx | timestamp、running status、複数BLE packetにまたがるSysExをcodec/helperが処理 |
| Event / Lifecycle | 接続、GATT完了、通知、Security、HIDなどの非同期event配送と`begin()` / `end()` | 非同期callbackは`ble.update()`を呼ぶloop taskから配送。同期応答が必要なGATT Serverの`onRead()`だけはstack task上で実行 |

API単位の対応状況と制限は[機能対応マトリクス](docs/FEATURE_MATRIX.ja.md)、
用途別のexampleは[examplesの目次](examples/README.ja.md)を参照してください。

上記の全機能はESP32-S3 2台の自動Peerテストとhost上のunit testで検証しています。SoCごとの
検証範囲は次の[対応環境](#対応環境)、suite単位の内容は[テスト計画](tests/TEST_PLAN.ja.md)に
あります。

## 対応環境

### SoCごとの違い

| | ESP32-S3 / C3 / C6 / H2 | 無印ESP32 | ESP32-P4 + slave（ESP-Hosted） |
|---|---|---|---|
| BLE無線 | 内蔵 | 内蔵 | **slave側のチップ**（SDIO接続。別途firmwareが必要） |
| NimBLE host | Core同梱 | **EspBleが同梱**（`src/nimble_esp32/`） | Core同梱（HCIをSDIOでslaveへ流す） |
| BLEの公開API | 同一 | 同一 | 同一 |
| Security / bonding | 対応 | 対応 | **不可**（上流のECC不具合） |
| 2M / Coded PHY | 対応 | **不可**（BLE 4.2 controller） | slaveのチップとfirmware次第（代表suiteの対象外） |
| Bluetooth Classic | 無線が無いため不可 | **対応**（SPP / HID / A2DP / AVRCP / HFP。Core 3.2.0以上、HFP audioのみ3.3.8以上） | 不可 |
| 検証範囲 | S3の2台で全機能をPeer test（C3 / C6 / H2はCIのbuild検証） | 2台でPeer testを両role掃引し、通った範囲のみ | 代表suite（接続、GATT、notify、MTU、Wi-Fi共存）。**C6 slave / firmware 2.12.11でのみ確認** |

シリーズ共通の制限もあります。Extended / Periodic Advertisingは、Coreが同梱するNimBLEが
`CONFIG_BT_NIMBLE_EXT_ADV`無効でbuildされているためどのtargetでも使えません。同時接続数の上限は
controller由来で、検証済みの構成では3です。API単位の対応状況は[機能対応マトリクス](docs/FEATURE_MATRIX.ja.md)を
参照してください。

NimBLEを提供しない構成は、コンパイル時に`#error`で拒否します。

### 無印ESP32が他と違う理由

**1. BLE hostをEspBleが持ち込みます。** Arduino-ESP32のプリビルドがこのチップだけBluedroid固定で、
Core同梱のNimBLEが存在しないためです。EspBleはesp-idfがpinするesp-nimbleと同一のスナップショットを
`src/nimble_esp32/`へ同梱し、設定値は他ターゲットと同じ値に固定します（利用者の上書きは拒否します）。
**このhostの保守をライブラリ側で負う**ぶん、対応は他のチップと同格ではありません。実機Peerテストで
確認できた範囲——GATT read/write/discovery、MTU、接続パラメータ更新、pairing・bonding、HID Device、
HID Host、BLE MIDI Device、Central / Peripheral両役割——だけを対応済みとし、タイミング依存の挙動が
他ターゲットと一致することは保証しません（[無印ESP32対応計画](docs/PLAN_ESP32.ja.md)）。

**2. BLE 4.2 controllerです。** LE 2M / Coded PHYが使えません。

**3. Bluetooth Classicが使えます。** Arduino-ESP32でBR/EDR無線を持つのはこのチップだけです。
Classicは必要なprofileを有効にして独自buildした、名前空間化済みのBluedroid hostを使います。

ここはArduinoのmixed library形式を意図的に使います。同梱NimBLE hostはsourceからcompileし、
別途生成したClassic-only Bluedroid hostは`src/esp32/libespble_bluedroid_classic.a`として同梱します。
build・更新条件が異なるため、両者の成果物形式は揃えません。

- SPP（byte streamとArduino `Stream`）、generic HID Device / Host、A2DP raw transport、
  AVRCP CT/TG、HFP Client / Audio Gateway
- 送信電力、page timeout、暗号鍵の最小長といった無線設定
- HIDはBLEと同じAPI形状です。`hidKeyboard()` / `hidMouse()` / `hidConsumerControl()` /
  `hidSystemControl()` / `hidGamepad()`が同名・同signatureで、Report Descriptorとreport packingは
  両transportで同じmoduleを共有します
- HID Report Descriptorの合成上限（1つのSDP recordを共有し、descriptorとdevice名などで214 byte）は
  登録前に検査し、黙って失敗させません

どちらを選ぶかと、両方にある機能の差は[BLEとClassicの選び方](docs/CLASSIC_VS_BLE.ja.md)、
機能ごとの「実機検証済み / 未検証 / 未実装」は[Classic機能の棚卸し](docs/CLASSIC_FEATURE_INVENTORY.ja.md)に
書いてあります。Classicは次回releaseの対象です。

**BLEとClassicの同時利用（dual-host）は実験扱いです。** どちらのhostが動くかはsketchが何を
`begin()`したかだけで決まり、build flagはありません。片方だけなら間に入るHCI brokerはpass-through、
`EspBle`と`EspBleClassic`の両方を`begin()`すればbrokerがHCIをroutingします。Classic HID通信中のLE接続、
GATT read反復、BLE GATT接続を維持したA2DP / AVRCP / HFP mSBC SCOまで実機検証済みですが、送信側bufferを
2つのhost間で按分していないなどの制約が残るため実験扱いのままです。不安定な場合は一方を`end()`して
単一hostで使ってください（[Classic実装計画](docs/PLAN_ESP32_CLASSIC.ja.md)、
[開発状況](docs/STATUS.ja.md)）。

### ESP32-P4 + ESP32-C6（ESP-Hosted）

P4はBLE無線を持たないため、無線を持つ別チップをESP-Hosted slaveとしてSDIOで接続し、Core提供の
ESP-Hosted NimBLE構成で利用します。**この構成ではBLEを実行するのはP4ではなくslave側のチップです。**

> [!IMPORTANT]
> **slave側には、このライブラリとは別のESP-Hosted co-processor firmwareが載ります。**
> EspBleはこのfirmwareを同梱も更新もしません。書き込みはArduino-ESP32 Core同梱の
> `ESP_HostedOTA`（またはEspressifの手順）で別途行ってください。**動作はfirmwareのversionで
> 変わります**——実機でもslave 2.3.2と2.12.11で挙動が違いました。Host側とSlave側のversionは
> 揃えてください。
>
> **使える範囲はslaveのチップでも変わります。** BLEのversion、対応PHY、同時接続数は
> slave側の無線とfirmwareが決めるためです。Core 3.3.11のP4向けプリビルドは
> `esp32c6`をslave targetの既定値として持ちますが、ESP-Hosted 2.12.2以降のCoreは
> 実機のco-processorへ問い合わせてtarget名を得て、更新用firmwareもその名前から選びます。
> したがってC5などC6以外のslaveも仕組みの上では成立しますが、**EspBleがPeer testで
> 確認したのはP4 + C6の組み合わせだけです。**

検証済みのHost/Slave version、slave firmwareの更新方法、対応済み範囲は
[ESP-Hostedセットアップ](docs/ESP_HOSTED_SETUP.ja.md)にあります。

Core 3.3.11とC6 slaveの組み合わせでは、同梱IDFのP4 ECC不具合により、LE Secure Connections、bonding、
それを前提とするHID、および`end()`後の複数回の再`begin()`に
[既知制限](docs/ESP_HOSTED_LIMITATIONS.ja.md)があります。

Tab5や独自基板でSDIO pin配置がgeneric P4と異なる場合は、正しいboard variantを選ぶか、初期化前にCoreの
pin設定を上書きします（[SDIO pinの選択と上書き](docs/ESP_HOSTED_SETUP.ja.md#sdio-pinの選択と上書き)、
[Hosted/CustomPins](examples/Hosted/CustomPins/)）。

### coreバージョン

開発とPeerテストはarduino-esp32 3.3.11で行っています。対応下限はチップごとに違い、
その理由は「BLE hostを誰が持ってくるか」で決まります。

| 対象 | hostの出所 | 最小Core | なぜそこか |
| --- | --- | --- | --- |
| 無印ESP32（BLE / Classic） | **EspBleが持ち込む**（NimBLE source + Classic archive） | **3.2.0** | Coreの版にほぼ依存しないため。Classic hostはbuildに使ったAPI headerを`src/esp32/include/`へ同梱していて宣言と構造体レイアウトがずれず、CoreからはFreeRTOS等の安定APIだけを使う。3.2.0未満は未測定で、理由付きの`#error`で止まる |
| 無印ESP32のHFP audio（SCO）のみ | 同上（ただしcontrollerはCore同梱） | **3.3.8** | 3.3.7以前のCoreはprebuilt controllerがPCM audio path（外部codec chip向け）でbuildされており、EspBleが使うHCI経由のSCOを受けられない。hostを持ち込んでいても、controllerのbinaryはCore側なので変更できない |
| ESP32-S3 / C3 / C6 / H2 | **Core同梱のNimBLE** | **3.3.0** | 3.2.x世代のprebuilt libraryはBLE hostがBluedroidで、EspBleが呼ぶNimBLEが存在しない。CoreがNimBLEへ切り替えたのが3.3.0。該当版では`EspBle requires the NimBLE backend`の`#error`で止まる |
| ESP32-P4（+C6 ESP-Hosted） | **Coreが提供**（Hosted経由のNimBLE） | **3.3.1** | 3.3.0のP4はHosted構成のNimBLEが未提供 |

無印ESP32の範囲は実測です。3.2.0〜3.3.11のcompile / linkが全て通り、3.2.1 / 3.3.0 /
3.3.10 / 3.3.11は実機でも確認済みです（[core版数のテスト計画](docs/PLAN_CORE_VERSION_MATRIX.ja.md)）。
他チップの範囲はCIが計測します。

- **Core Compatibility Matrix** ワークフロー → `docs/COMPATIBILITY.<version>.md`（代表BLE exampleをarduino-esp32の各リリースに対してビルド）
- **Board Build Coverage** ワークフロー → `docs/BOARDS.<version>.md`（workflow実行時点のexampleを1つのCoreバージョンでESP32-S3 / ESP32 / C3 / C6 / H2 / P4に対してビルド）

どちらもフルsweepが全sketchを書き換えて再ビルドするため、手動実行（`workflow_dispatch`）です。
なお3.3.11より新しいCoreは、壊れると分かっているのではなく未検証という扱いで、
無印ESP32では`#warning`を出したうえでbuildは通します。

## はじめかた

Arduino Library Managerで`EspBle`を検索するか、Arduino CLIでinstallします。

```sh
arduino-cli lib install EspBle
```

各exampleには検証済みArduino-ESP32バージョンを固定した`sketch.yaml`が同梱されています:

```sh
arduino-cli compile --profile esp32s3 examples/Gap/Scan
```

Bluetooth Classicのexampleは無印ESP32専用なので、`esp32` profileでbuildします:

```sh
arduino-cli compile --profile esp32 examples/Classic/SppServer
```

全examplesの一覧と組み合わせは[examplesの目次](examples/README.ja.md)を参照してください。最小のスキャナは次のとおりです:

```cpp
#include <EspBle.h>

EspBle ble;

void setup() {
  Serial.begin(115200);
  ble.begin();
  ble.scanner().onResult([](const EspBleScanResult &result) {
    Serial.printf("%s RSSI=%d\n", result.address.c_str(), result.rssi);
  });
  ble.scanner().start();
}

void loop() {
  ble.update();  // すべてのcallbackはここから配送されます
  delay(1);
}
```

## 文書

**BLEがはじめての方は[BLE通信の入門ガイド](docs/GUIDE_BLE_BASICS.ja.md)から** — 相手を探すところからデータのやり取りまで、何が起きているのかを説明し、話題ごとに対応するexampleへ案内します。

**動くsketchができたあとは[EspBleを深く使う](docs/GUIDE_ADVANCED.ja.md)へ** — callbackがどのtaskで動くか、queueの上限と満杯時の挙動、backpressure、再接続、dual-hostの内部、sizeの測り方、既知の不具合の見取り図をまとめています。

**特定の文書を探すときは[ドキュメント案内](docs/README.ja.md)へ** — 読む順序と各文書の役割をまとめています。「今どこまで進んでいるか」を最短で把握するには [docs/STATUS.ja.md](docs/STATUS.ja.md) → [docs/DECISIONS.ja.md](docs/DECISIONS.ja.md) の順です。

- [BLE通信の入門ガイド](docs/GUIDE_BLE_BASICS.ja.md)
- [Classic通信の入門ガイド](docs/GUIDE_CLASSIC_BASICS.ja.md)
- [BLEとClassicの選び方](docs/CLASSIC_VS_BLE.ja.md)
- [EspBleを深く使う（上級）](docs/GUIDE_ADVANCED.ja.md)
- [他のライブラリからの移行](docs/GUIDE_MIGRATION.ja.md)
- [HID Report Descriptorを書く](docs/GUIDE_HID_DESCRIPTORS.ja.md)
- [開発状況とTODO](docs/STATUS.ja.md)
- [要件](docs/REQUIREMENTS.ja.md)
- [コア設計](docs/CORE_DESIGN.ja.md)
- [API設計](docs/API_DESIGN.ja.md)
- [HID Device仕様](docs/HID_DEVICE_SPEC.ja.md)
- [HID Host仕様](docs/HID_HOST_SPEC.ja.md)
- [用語と命名規則](docs/TERMINOLOGY.ja.md)
- [設計決定](docs/DECISIONS.ja.md)
- [機能対応マトリクス](docs/FEATURE_MATRIX.ja.md)
- [テスト計画](tests/TEST_PLAN.ja.md)
- [リリースチェックリスト](docs/RELEASE_CHECKLIST.ja.md)
- [ESP32-P4 / ESP-Hostedセットアップ](docs/ESP_HOSTED_SETUP.ja.md)
- [ESP32-P4 / ESP-Hostedの既知制限](docs/ESP_HOSTED_LIMITATIONS.ja.md)

## 関連ライブラリ

- [EspUsbHost](https://github.com/tanakamasayuki/EspUsbHost) — USB Hostライブラリ。EspBleはkeyboard layout tableとHID usageの扱いを共有しています
- [EspUsbDevice](https://github.com/tanakamasayuki/EspUsbDevice) — 組み合わせテストに使用するUSB Deviceライブラリ

## ライセンス

EspBle独自コードには[MIT License](LICENSE)が適用されます。この配布物には、それぞれ上流の
ライセンスが適用される第三者コンポーネントも含まれ、MITへ再ライセンスするものではありません。
同梱NimBLE sourceとprecompiled Classic-only Bluedroid archiveを含む詳細は
[第三者コンポーネントのライセンス](THIRD_PARTY_NOTICES.md)を参照してください。
