# Gatt/EnvironmentalClient

標準Environmental Sensing Service（`0x181A`）を広告するPeripheralへ接続し、
Temperature、Humidity、Pressureを順番にReadしてlittle-endian値をdecodeします。
完了後はTemperature Notificationを購読します。

[EnvironmentalServer](../EnvironmentalServer/)と組み合わせて使用できます。
