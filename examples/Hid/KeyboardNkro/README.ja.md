# KeyboardNkro

EspUsbDeviceと同じ29バイトのビットマップReportを使うBLE HID NKRO keyboardです。
`configure()`より前に`enableNkro()`を呼び、双方のMTUを32以上に設定します。
