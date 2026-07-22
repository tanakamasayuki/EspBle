# Gatt/BatteryClient

標準Battery Serviceをscanし、非同期接続後に1-byteのBattery LevelをReadして、
続けてBattery Level Notificationを購読します。

汎用GATT Clientの完了callbackを連鎖しており、標準Serviceを公開する市販機器にも
同じ構成を使用できます。

[BatteryServer](../BatteryServer/)と組み合わせて使用できます。
