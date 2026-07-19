# Gatt/DeviceInfoClient

標準Device Information Service（`0x180A`）を広告するPeripheralへ接続し、
Manufacturer Name、Model Number、Firmware Revision、PnP IDを順番に読みます。
PnP IDの7-byte wire値はlittle-endianの各IDへdecodeして表示します。

[DeviceInfoServer](../DeviceInfoServer/)と組み合わせて使用できます。
