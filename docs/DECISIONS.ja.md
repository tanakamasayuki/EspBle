# 設計決定の台帳

## 確定

1. Arduino向け単一ライブラリ`EspBle`として提供する。
2. Arduino-ESP32に同梱されたBLEライブラリのNimBLE backendを使用し、外部NimBLE-Arduinoを必須依存にしない。Bluedroid backendは対象外とする。
3. Bluetooth Classicは扱わない。
4. Central / PeripheralとGATT Client / Serverを同じスタック所有者で扱う。
5. APIを単一接続前提に固定しない。
6. 標準Profileと独自Serviceを同じGATT Serverへ合成可能にする。
7. 初期実機ターゲットと自動Peer環境はESP32-S3 2台とする。
8. pytest-embedded-cli上の親DUTと2台目Peerは、既存環境と同じ`s3_peer_host` / `s3_peer_device` profileで識別する。この名前にBLE roleの意味を持たせない。
9. Peerでは両方のsketchを転送・実行でき、両方のSerialを観測・操作できる。初期構成は親側sketchをCentral、`peer_device/`側sketchをPeripheralに固定し、役割を交換しない。EspBle PeripheralはPeer側の結果を主にassertして検証する。
10. Peerの一方は可能な範囲でArduino-ESP32同梱BLE低レベルAPIを直接使い、EspBle同士だけの自己整合テストにしない。
11. 初期プロファイルはHID KeyboardとBattery Serviceに絞る。
12. `memo.ja.md`は正式文書への移行確認後に削除する。
13. 初期自動Peer環境は常設ESP32-S3 2台とする。3台必要な複数接続またはBLE-to-BLE bridge testは、manual用ESP32-S3を追加Peerとして利用し、Peerディレクトリを増やして拡張する。
14. 対象可否はBLE内蔵SoCかどうかではなく、Arduino-ESP32がNimBLEを提供する構成かで判断する。ESP32-P4 + ESP32-C6などのHosted BLEも対象候補に含め、専用build/実機試験後に対応済みとする。
15. 公開APIと文書はBluetooth LEの標準用語を基本とし、Central/Peripheral、GATT Client/Server、HID Host/Deviceを同一視しない。stack ownerは役割中立の`EspBle`とする。
16. examplesの変数名は役割の明確さを優先する。複数roleが登場する場合は`hidKeyboardHost` / `hidKeyboardDevice`のように明示し、単一roleで自明なexampleでは`keyboard`などの短い名前を個別判断で許容する。

## 仮置き

1. 通常イベントはstack callbackからqueueへ移し、`update()` contextで配送する案から開始する。ただし公開APIとして確定せず、Notify/IndicateとHIDでlatency、queue、排他、Arduinoでの使い勝手を確認して、明示`update()`、内部task、または選択式のどれにするか決める。
2. `begin()`前にGATT Server構成を登録し、開始後の動的Service追加は初期版で禁止する。
3. Characteristic valueはbyte sequenceを基本とし、型変換をcodecへ分離する。
4. Connectionはbackend handleの再利用を検出できるlibrary identityを持つ。
5. 初期の同時接続数は制限してよいが、接続単位APIを維持する。
6. Pairing、Bonding、認証方式の実装は基本接続/GATTの後から追加する。ただしCharacteristicのsecurity permissionはGATT Server開始前に必要になり得るため、構成拡張点とConnectionのsecurity状態は初期API設計で塞がない。

## GAPスパイクで確認済み（公開API確定前）

1. root objectは`EspBle ble`とし、`begin()`でArduino-ESP32同梱NimBLEを初期化できる。
2. `ble.advertising()`と`ble.scanner()`から役割を固定せずGAP操作へアクセスできる。
3. Scan Resultはbackend callback引数を外へ露出せず、name、address、RSSI、Service UUID、Manufacturer Dataなどを持つ値へcopyする。
4. stack callback内でユーザーcallbackを実行せずqueueへ積み、現在は`ble.update()`から配送する。Peer testでloop task contextから呼ばれることを確認済み。
5. USB系と同様に操作は`bool`を返し、`lastErrorName()` / `lastErrorDetail()`で失敗理由を確認できる試行APIとする。
6. Arduino-ESP32 BLE stackが外部で初期化済みの場合は所有権競合として拒否する。
7. Legacy Advertisingの31-byte上限で要求fieldが欠落する場合は明示的なargument errorとする。
8. Advertisingのservice UUIDはサイズ（16/32/128-bit）ごとに1つの「Complete List」AD構造へまとめ、同一AD typeをpayload内に複数出現させない（CSS Part A 1.1）。`advertise_payload` Peerテストでraw payloadを検証する。

## Connection/GATTスパイクで確認済み（公開API確定前）

1. Arduino-ESP32 backendの待機型Connect、Discovery、Read、Writeは内部taskで実行し、公開操作の受理時にはloopをblockしない。
2. Connectionはbackend handleとは別のlibrary生成ID、peer address、local role、MTUを持つ値snapshotとし、接続と切断イベントで同じIDを通知できる。
3. GATT ServerのService/Characteristicは`begin()`前に登録し、Security permissionを後から定義へ追加できる順序にする。
4. GATT Server書込みイベントとCentral側のDiscovery/Read/Write結果は、現在は`ble.update()`からloop task contextで配送する。
5. 最小Discoveryは既知Service/Characteristic UUIDを指定して存在とpropertyを確認する。全Service/Characteristic列挙は別途設計する。
6. 初期実装はCentral側GATT operationを同時に1件へ制限し、callbackから次のoperationを連鎖できる。queue、operation id、cancelは未確定とする。
7. GATT値はpointer+lengthを基本に扱える一方、公開値containerは`String`で試行する。HID実装後に最終型を決める。
8. Notification/Indicationの購読、解除、受信payloadとServer側CCCD変更は値イベントへcopyし、`ble.update()`から配送できる。
9. Server側Notification/Indication送信は内部taskで実行し、Indication確認待ちでloopをblockしない。送信結果は別イベントで通知する。
10. Arduino-ESP32 3.3.10のNimBLE Indicationではcontroller確認成功後に同期wrapper由来のtimeout statusが重複するため、先に観測した`SUCCESS_INDICATE`を保持するbackend workaroundを内部に置く。
11. 現在のServer送信は該当方式を購読する全Connection向けとする。Connection指定送信と購読者ごとの結果は複数接続実装時に決める。
12. 希望MTUは`begin()`前に23〜517で設定し、同梱NimBLE backendが接続時に交換したMTUをConnection snapshotへ反映する。接続後の再交換APIは現時点で設けない。
13. MTU変更はConnection snapshotと変更前MTUを持つ値イベントとしてqueueへcopyし、`ble.update()` contextで配送できる。
14. Notification/Indication payloadの上限は`mtu - 3`とし、backendによる黙示的な切詰めを避けるため超過を送信前に拒否する。複数接続ではactiveな全Peripheral Connectionの最小値を使う保守的な判定から開始する。
15. connection eventのqueueが満杯のとき、lifecycle・完了イベント（Connected/Disconnected/Failed/GattResult/SecurityChanged等）は最古のNotificationを追い出して保持し、drop対象はNotificationに限定する。drop数（追い出し含む）は`EspBle::droppedEventCount()`で観測できる。
16. Central `BLEClient`は切断時・接続失敗時にretireし、次の`update()`でloop task上のreapが解放する。同梱backendには`deleteClient()`がなく`BLEDevice::deinit(false)`が最後に生成した1個だけを解放するため、最新のclientはEspBle側では解放せず、後続のclient生成または`end()`まで保持する。`end()`は残りの全clientを解放してから`deinit(false)`を呼ぶ。
17. 内部worker task（GATT operation、Server送信）は完了イベントをpushしてからbusy flagをクリアする。`end()`はbusy flagのクリアを待ってから共有状態を破棄するため、この順序でuse-after-freeを防ぐ。
18. retired clientのreapは、client解放前に該当Connection IDのHID Host slot（remote characteristicポインタ）を無効化し、GATT operation実行中はreapを次の`update()`へ遅延する。
19. 初期化済みインスタンスへの2回目の`begin()`は、同一configなら成功を返し、異なるconfigなら`InvalidState`で失敗する。黙って旧設定のまま成功を返さない。
20. `end()`は実行中のconnect試行を`ble_gap_conn_cancel()`で中断し（connect timeoutまでblockしない）、Scannerの未配送resultをflushして次のsessionへ持ち越さない。
21. Peripheral向けイベント（Server書込み・購読変更・HID Output Report）でConnection IDを解決できない場合（ID 0）は、無効IDのまま配送せずdropしてカウントする。

## Securityスパイクで確認済み（公開API確定前）

1. Securityは`begin()`前の設定で有効化し、No Input / No OutputのJust Works + LE Secure Connectionsから開始する。
2. 接続時の自動Security要求と、Connection IDを指定する明示要求の両方を試行する。完了は同期戻り値ではなく値イベントとして`ble.update()` contextへ配送する。
3. Connection snapshotはencrypted、authenticated、bonded、encryption key sizeを保持する。Just Worksの成功時はencryptedかつbondedだがauthenticatedではない。
4. GATT Characteristic定義にencrypted read/writeを追加し、同梱NimBLEのATT permissionで強制する。
5. Bond列挙・特定削除・全削除は同梱NimBLE storeを使用する。現在はactive Connectionがない場合だけ削除を許可する。
6. `security_bond` Peerテストで初回Pairing、暗号化Read/Write、両側Bond保存、切断後のBond再接続、両側Bond削除を確認済み。
7. 最初のMITM方式はDisplayOnlyとKeyboardOnlyの静的6桁passkeyとする。実行時入力とNumeric Comparisonは、stack callbackへの即時回答方法を設計してから追加する。
8. 表示passkeyは値イベントへcopyして`ble.update()` contextで配送する。同梱backend callbackにConnection handleがないため、複数同時PairingでのConnection識別は未確定とする。
9. GATT Characteristicへauthenticated read/writeを追加し、`security_passkey` Peerテストで両側`authenticated=true`、認証必須Read/Write、Bond保存・削除を確認済み。

## HID Keyboard Deviceスパイクで確認済み（公開API確定前）

1. 最初のHID DeviceはReport Protocolの固定6KRO Keyboardとし、Boot Protocol、NKRO、Consumer Control、Mouse、複合HIDを含めない。
2. `ble.hidKeyboardDevice()`からprofile handleを取得し、`begin()`前に構成する。Input APIはmodifierと最大6 keyを持つ値型とし、Report IDとreserved byteのwire処理をprofileへ隠す。
3. HID Service、Device Information Service、Battery Serviceを同じGATT Serverへ登録し、HID UUIDとKeyboard appearanceをAdvertisingへ自動追加する。
4. Report MapとReport ReferenceにはconfigurableなReport IDを持たせるが、GATT characteristic payload自体はReport IDを除いた8-byte Input / 1-byte Outputとする。
5. Output ReportはConnection ID付きの値へcopyし、現在は`ble.update()` contextで配送する。
6. Arduino-ESP32 3.3.10同梱`BLEService` wrapperは同一Service内のCharacteristicをUUIDで一意に扱うため、HOGPに必要なInput/Output 2個の`0x2A4D`を登録できない。内部だけ同梱NimBLEのGATT定義APIを使い、別attributeとして登録する。
7. `hid_keyboard_device` Peerテストでは親側を同梱BLE API直接実装のHID Host、`peer_device/`側をEspBle HID Keyboard Deviceとし、Report Map、Report Reference、Battery Read、Input Notification、LED Output Write、Pairing/Bondingを確認済み。
8. securityを有効にした場合、HOGPのSecurity Mode 1 Level 2に従いHID Serviceの全attribute（Report Map、Report、Report Reference、HID Information、Control Point）へ暗号化必須のpermissionを付与する。加えてInput Reportは暗号化されていないlinkへは送信しない。Device Information / Battery Serviceは識別用途のため暗号化必須にしない（`hid_security` Peerテストで検証）。
9. Input Report / Battery Levelの通知は、GAP subscribeイベントから追跡したCCCD購読状態を持つ接続にのみ送信する。購読者が1つもない場合、`sendInputReport()`は失敗を返し、`setBatteryLevel()`は値更新のみ行う（`hid_robustness` Peerテストで検証）。

## HID Keyboard Hostスパイクで確認済み（公開API確定前）

1. Scan/ConnectはBLE共通APIに残し、接続後に`hidKeyboardHost().discover(connectionId)`でHID固有のDiscoveryと購読を開始する。
2. Host入力境界は文字イベントではなく、Connection ID、Report ID、modifier、256-bit usage現在値/変化値を持つsnapshotとする。modifier usagesもbitmapへ含め、EspUsbHostとESP32KeyBridgeの入力境界へ揃える。
3. 切断時はheld usageの全release snapshotをloop contextへ配送し、bridgeでstuck keyを残さない。
4. 初期Report Map parserはmodifier + 6-key arrayの8-byte reportだけを受理する。NKROや未知の複合reportを長さだけで推測しない。
5. 同一UUIDのReport characteristicをhandleで全列挙し、Report Reference descriptorでInput/OutputとReport IDを識別する。
6. Keyboard LED OutputはConnection IDを指定して返送する。writeを同期`bool`のままにするか非同期Resultへ揃えるかはKeyBridge adapter実装後に確定する。
7. `hid_keyboard_host` PeerテストでDiscovery、Battery Read、Input subscription、usage snapshot、LED Output、Pairing/Bondingを確認済み。Device wire形式は別テストで同梱BLE API直接実装により独立検証する。
8. `EspBleHidKeyboardState`の256-bit usage snapshotはESP32KeyBridgeの`InputAdapter::keys()`へ変換なしで写像できる。試作adapterでは`bridge.update()`からEspBleの`update()`も駆動でき、remap、modifier、切断releaseを確認した。
9. KeyBridgeのLockStateは`setKeyboardLeds(connectionId, ...)`でBLE keyboardへ返送でき、再接続・再Discovery後にも現在値を再送できる。
10. 正式adapterとsketch callbackの競合を避けるため、HID Keyboard Hostは既存の単一`on*()`に加えて、event種別ごとに最大4件の`add*Listener()`とIDによる`removeListener()`を提供する。配送開始時にcallbackをsnapshotし、callback内の登録変更は次eventから反映する。共通Event APIへの一般化は他profileで必要になった時点で判断する。
11. Report Mapの6KRO keyboard判定はバイト列一致ではなく、HID report descriptorの最小parser（`EspBleHidReportMap.h`、host unit test付き）で行う。keyboard Application collection内の「8×1-bit modifier(0xE0-0xE7) variable入力+6個以上の8-bit array入力」を持つreportを探し、そのReport IDでInput/Output Report characteristicを選択する。Report IDなしのkeyboard（Report Reference不在）も受理する。
12. ErrorRollOver等のphantom report（key slotにusage 0x01-0x03）は一般的なHost同様に無視し、直前のkey状態を維持する。
13. HID Hostのeventキューが満杯のとき、Discovery結果と切断時の全releaseイベントは最古のkey stateイベントを追い出して保持する。drop数（追い出し含む）は`droppedEventCount()`で観測できる。
14. HID Discovery実行中と`setKeyboardLeds()`実行中は対象Connection IDを共通GATT操作状態へ記録し、`disconnect()`が同一接続への要求を拒否できるようにする。
15. 長さが8以外のInput Reportは解釈せず、`invalidInputReportCount()`で観測できるようカウントする（「Discovery成功なのにキーが来ない」の診断手段）。
16. keymap変換はEspUsbHostのUnicode 4-plane表現に揃える。tableは`uint16_t KEYCODE_TO_UNICODE_XX[N][4]`（無shift/Shift/AltGr/AltGr+Shift、Unicodeコードポイント）でEspUsbHostの`src/keymap/*.h`と同一内容を共有し、主関数`espBleUsageToUnicode()`がAltGr層選択と文字ペア判定CapsLockを行う。`espBleUsageToAscii()`はLatin-1 wrapper（非Latin-1は0）として維持し、`EspBleHidKeyboardEvent`は`unicode`と`ascii`の両方を持つ。en-US系（EnUs/KoKr/ZhCn/ZhTw）のみ内蔵変換パス（table等価）を使う。
11. 再接続時は新しいConnection IDに対してSecurity完了後に再度`discover()`が必要で、Discovery完了後にinputをconnectedとして扱う。自動再接続・自動Discoveryは今回のadapter境界には含めない。

## 優先順位候補

1. HID Mouse / Consumer Control / composite HID
2. Device Information / NUS
3. BLE MIDI
4. reconnect / resubscribe / discovery cache / multiple connections
5. Sensor profile
6. Extended/Periodic Advertising、PHY、Privacy

候補は採用決定ではありません。ユースケース、実機、Peerテスト方法が揃った機能だけを正式スコープへ移します。

## 対象外

- Bluetooth Classic
- LE Audio
- Mesh
- Matter provisioning
- Apple/Google固有Serviceの標準搭載
- OTA/DFU方式の統一
- ESP-IDF native API

## 要確認

- Arduino-ESP32の最小対応版と更新ポリシー
- ESP32-S3以外の初期build matrix
- HID Keyboard Hostで追加対応するReport Mapの優先順位
- 実行時Passkey入力とNumeric Comparisonの応答context
- public object ownershipとevent queueの具体API
