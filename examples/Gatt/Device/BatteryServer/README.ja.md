# Gatt/BatteryServer

標準Battery Service（`0x180F`）と、Read・Notify可能なBattery Level
Characteristic（`0x2A19`）を公開します。Serialから`+` / `-`を送ると、
1-byteの残量百分率を更新して購読中ClientへNotificationします。

小さなwire形式の標準Serviceはprofile専用ownerを増やさず合成できるため、
このexampleは汎用GATT Server APIをそのまま使用します。

[BatteryClient](../BatteryClient/)と組み合わせて使用できます。
