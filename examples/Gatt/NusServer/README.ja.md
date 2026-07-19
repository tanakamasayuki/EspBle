# Gatt/NusServer

Nordic UART ServiceのUUID構成を汎用GATT Server APIで実装します。RXは応答あり・
なしWriteを受け取り、TXはNotificationを送ります。受信したRXデータを、購読中の
TX Clientへechoします。

NUSはpacket指向のGATT慣例であり、Bluetooth Classic SPPではありません。payloadは
接続のATT/MTU上限内に収め、複数packetへ分割する場合はapplication側でframingします。

[NusClient](../NusClient/)と組み合わせて使用できます。
