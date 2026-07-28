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
