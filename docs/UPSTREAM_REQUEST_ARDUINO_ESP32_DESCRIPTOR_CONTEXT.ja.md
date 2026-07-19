# Arduino-ESP32 BLEDescriptor callbackへの接続context追加依頼案

## 対象

- repository: `espressif/arduino-esp32`
- 確認version: 3.3.10
- 対象: `libraries/BLE/src/BLEDescriptor.h` / `BLEDescriptor.cpp`

## 問題

NimBLEの`BLEDescriptor::handleGATTServerEvent()`は`conn_handle`を受け取りますが、
`BLEDescriptorCallbacks::onRead()` / `onWrite()`へは`BLEDescriptor *`だけを渡し、
接続情報を破棄しています。このため複数Centralが接続するGATT Serverでは、
DescriptorをRead/Writeした接続をapplicationから特定できません。

`BLECharacteristicCallbacks`には既にNimBLE用の
`onRead(BLECharacteristic *, ble_gap_conn_desc *)`と
`onWrite(BLECharacteristic *, ble_gap_conn_desc *)`があり、DescriptorだけAPIが非対称です。

## 希望する変更

`BLEDescriptorCallbacks`へCharacteristicと同じ形のoverloadを追加してください。

```cpp
#if defined(CONFIG_NIMBLE_ENABLED)
virtual void onRead(BLEDescriptor *descriptor, ble_gap_conn_desc *description);
virtual void onWrite(BLEDescriptor *descriptor, ble_gap_conn_desc *description);
#endif
```

既定実装は既存overloadへforwardし、既存applicationとの互換性を維持します。

```cpp
void BLEDescriptorCallbacks::onWrite(
  BLEDescriptor *descriptor,
  ble_gap_conn_desc *)
{
  onWrite(descriptor);
}
```

NimBLE event handlerでは`ble_gap_conn_find(conn_handle, &description)`で取得した
`ble_gap_conn_desc`を新overloadへ渡します。ReadとWriteの両方を対象にします。

Bluedroid側も`BLECharacteristicCallbacks`と同様に
`esp_ble_gatts_cb_param_t *`を受け取るoverloadを追加できれば、backend間のAPIが揃います。

## 受入条件

- 従来の`onRead(BLEDescriptor *)` / `onWrite(BLEDescriptor *)` overrideが引き続き呼ばれる。
- 新overloadからNimBLE connection handleを取得できる。
- 2接続から同じDescriptorへWriteしたとき、各callbackで異なるhandleを識別できる。
- Read callbackでも同じconnection contextを取得できる。

## EspBle側の利用予定

採用後、`EspBleGattDescriptorWrite`と将来のDescriptor Read requestへ
`EspBleConnectionId connectionId`を追加し、Characteristic Writeと同じ正確な
connection-scoped eventとして`ble.update()`から配送します。
