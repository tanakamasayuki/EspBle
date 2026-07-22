# Gatt/HeartRateClient

標準Heart Rate Service（`0x180D`）を広告するPeripheralへ接続し、Body Sensor
Locationを読み、Heart Rate Measurementを購読します。Measurement flagsに従い、
8/16-bit心拍数、Energy Expended、複数RR-Intervalを境界検証しながらdecodeします。

[HeartRateServer](../HeartRateServer/)と組み合わせて使用できます。
