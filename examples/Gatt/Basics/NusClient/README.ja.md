# Gatt/NusClient

Nordic UART Service互換Serverをscanし、TX Notificationを購読します。Serialへ
入力された空でない行をWrite Without ResponseでRXへ送り、TX Notificationを
Serialへ表示します。

NUSが汎用GATT Client APIの組合せで構築できることを示すexampleです。stream意味論や
自動packet framingは提供しません。

[NusServer](../NusServer/)と組み合わせて使用できます。
