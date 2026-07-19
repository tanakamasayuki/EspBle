# Gatt/EnvironmentalServer

標準Environmental Sensing Service（`0x181A`）でTemperature（`0x2A6E`）、
Humidity（`0x2A6F`）、Pressure（`0x2A6D`）を公開します。各値は仕様の
little-endian整数とスケール（0.01 ℃、0.01 %、0.1 Pa）で保持します。

TemperatureはRead / Notify、ほか2つはRead可能です。Serialから`+` / `-`を
送ると温度を0.25 ℃変更して通知します。[EnvironmentalClient](../EnvironmentalClient/)
と組み合わせて使用できます。
