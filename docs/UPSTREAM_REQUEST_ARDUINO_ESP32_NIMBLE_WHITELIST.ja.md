# Arduino-ESP32 NimBLE white list APIのリンクエラー報告案

## 対象

- repository: `espressif/arduino-esp32`
- 確認version: 3.3.10（NimBLE backend、`CONFIG_NIMBLE_ENABLED`）
- 対象: `libraries/BLE/src/BLEDevice.cpp` / `BLEDevice.h` / `BLEAddress.h`

## 問題1: `BLEDevice::m_whiteList` に定義がなくリンクできない

`BLEDevice::whiteListAdd()` / `whiteListRemove()` / `onWhiteList()` を1つでも参照するスケッチは、
NimBLE backendでリンクに失敗します。

```
ld: BLEDevice.cpp.o:(.literal._ZN9BLEDevice11onWhiteListER10BLEAddress+0x0):
    undefined reference to `_ZN9BLEDevice11m_whiteListE'
```

`static std::vector<BLEAddress> m_whiteList;` は `BLEDevice.h:294` で**宣言**されていますが、
`BLEDevice.cpp` に**定義がありません**（同ファイル内の他のstaticメンバは冒頭で定義されています）。
NimBLEパスの `whiteListAdd()`（`BLEDevice.cpp:591`付近）はこのメンバを実際に使用するため、
参照した時点でリンクエラーになります。

結果として、**NimBLE backendではwhite list（Filter Accept List）関連APIが一切使用できません**。

## 問題2: `BLEAddress` を `ble_addr_t` へreinterpret_castしている

問題1を修正しても、NimBLEパスの実装には型レイアウトの不一致があります。

```cpp
// BLEDevice.cpp:593
int errRc = ble_gap_wl_set(reinterpret_cast<ble_addr_t *>(&m_whiteList[0]), m_whiteList.size());
```

- `BLEAddress` のレイアウトは `{ uint8_t m_address[6]; uint8_t m_addrType; }`（`BLEAddress.h:122`付近）
- `ble_addr_t` のレイアウトは `{ uint8_t type; uint8_t val[6]; }`

**先頭フィールドが逆**のため、castした配列をそのまま渡すとアドレスの1バイト目がtypeとして、
typeがアドレスの最終バイトとして解釈されます。`std::vector<BLEAddress>` の要素サイズが
`ble_addr_t` と一致する保証もありません。

## 希望する修正

1. `BLEDevice.cpp` に `std::vector<BLEAddress> BLEDevice::m_whiteList;` の定義を追加する。
2. NimBLEパスで `ble_gap_wl_set()` へ渡す前に、`BLEAddress` から `ble_addr_t` へ
   フィールド単位で変換する（`type` と `val[6]` を明示的に埋める）。

## EspBle側の回避

EspBleは自前でaccept listのミラーを保持し、`ble_gap_wl_set()` を直接呼んで
コントローラのリストを上書きしています（`EspBle::syncAcceptList()`）。
`ble_gap_wl_set()` は「コントローラのwhite listを指定内容で上書きする」APIなので、
ミラーを唯一の正としてまとめて書き込む形にできます。
`BLEAddress` はNimBLEビルドでは内部バッファをNimBLEの逆順で保持しており、
これは `ble_addr_t::val` の期待する並びと一致するため、`getNative()` の6バイトを
そのままコピーし、`type` は別途設定しています。

上記が修正されればwrapper APIへ戻せますが、`ble_gap_wl_set()` が
「一括上書き」のセマンティクスである以上、ミラー方式のほうが素直なため、
EspBle側は修正後も現行実装を維持する可能性があります。

---

# 補遺: 同一UUIDの重複が扱えない

上記とは別件ですが、同じ`libraries/BLE`で見つかった制約です。Bluetooth Core Specificationは、1つのデバイスが**同じUUIDのServiceやCharacteristicを複数**持つことを認めています（HIDのReport Characteristicが代表例）。同梱wrapperはこれを2箇所で扱えません。

## 問題3: Server側 — 同一Service内の同一UUID Characteristicが登録できない

`BLEService::addCharacteristic()`（`BLEService.cpp:252`付近）はNimBLEパスで次のように振る舞います。

```cpp
BLECharacteristic *pExisting = m_characteristicMap.getByUUID(pCharacteristic->getUUID());
#if defined(CONFIG_NIMBLE_ENABLED)
  if (pExisting != nullptr) {
    pExisting->m_removed = 0;   // 既存を復活させるだけ
  } else
#endif
  { m_characteristicMap.setByUUID(pCharacteristic, pCharacteristic->getUUID()); }
```

新しく渡された`BLECharacteristic`は**マップに入らず、GATTにも登録されず、解放もされません**（リーク）。`createCharacteristic()`の戻り値は有効なポインタなので、呼び出し側は成功したと誤解します。以降その characteristic への`notify()`等は宛先のない属性に対する操作になります。

実機で確認: 1つのServiceに同一UUIDのCharacteristicを2つ作ると、Central側から見えるのは1つだけです。

## 問題4: Client側 — 同一UUIDのリモートServiceが1つに潰れる

`BLEClient::m_servicesMap` は `std::map<std::string, BLERemoteService *>` でUUID文字列がキーです（`BLEClient.h:166`）。discovery時の挿入は次のとおりです。

```cpp
// BLEClient.cpp:1055 付近
client->m_servicesMap.insert(std::pair<std::string, BLERemoteService *>(
  pRemoteService->getUUID().toString().c_str(), pRemoteService));
```

`std::map::insert` は**キーが既存なら何もしません**。したがって同一UUIDのServiceを複数公開している相手に接続すると、2つ目以降は破棄され（生成した`BLERemoteService`はリーク）、アプリケーションからは到達できません。

なおCharacteristicについては`m_characteristicMapByHandle`にも登録されるため、**同一Service内の同一UUID Characteristicはハンドル経由で区別できます**（EspBleのHID Host はこの経路を使っています）。問題はServiceの方だけです。

## 希望する修正

1. `BLEService::addCharacteristic()` のNimBLEパスで、同一UUIDでも別インスタンスとして登録できるようにする（マップは既に`BLECharacteristic*`キーなので、`pExisting`による早期returnをやめれば足りるはずです）。
2. `BLEClient::m_servicesMap` をハンドルキーにするか、`m_servicesMapByHandle`を併設して同一UUIDの複数Serviceへ到達できるようにする。

## EspBle側の回避

問題3は回避できないため、EspBleは`addCharacteristic()`が同一Service内の重複UUIDを**明示的に拒否**します（黙って動かない状態を避けるため）。

問題4は、wrapperのGATT Client APIを使わないことで回避しました。EspBleは`ble_gattc_disc_all_svcs()` / `disc_all_chrs()` / `disc_all_dscs()`を自前で走らせて属性テーブルを記録し、read / write / CCCD書き込みを`ble_gattc_read()` / `ble_gattc_write_flat()`で属性ハンドルに対して直接行い、Notificationは`BLE_GAP_EVENT_NOTIFY_RX`を(接続ハンドル, 値ハンドル)で引いて配送します。同一UUIDのServiceを複数公開する相手とも全操作が可能です。

なお`BLEClient::getServices()`は毎回`clearServices()`してから全discoveryをやり直す作りで、そのときUUIDが重複したServiceの2つ目以降に生成した`BLERemoteService`を確保したまま捨てます。discoveryごとにヒープが減っていく原因になるため、修正時はこのリークも併せて塞ぐことを希望します。
