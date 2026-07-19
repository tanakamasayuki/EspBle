# Gatt/HeartRateServer

標準Heart Rate Service（`0x180D`）でNotify専用のHeart Rate Measurement
（`0x2A37`）と、Read可能なBody Sensor Location（`0x2A38`）を公開します。
Measurementは8-bit心拍数とRR-Intervalを含む可変長wire形式です。

Serialから`+` / `-`を送ると心拍数を変更して通知します。
[HeartRateClient](../HeartRateClient/)と組み合わせて使用できます。
