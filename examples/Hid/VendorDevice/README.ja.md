# Hid/VendorDevice

固定Report ID 6のVendor-defined HIDを公開します。`hidVendor().sendInput()`で
Inputを通知し、`onOutputReport()` / `onFeatureReport()`でHostからの書込みを
`ble.update()` contextに受け取ります。Report sizeは1〜64 bytesで構成できます。

API名とReport IDはEspUsbDeviceのVendor HIDに揃えています。
[VendorHost](../VendorHost/)と組み合わせて使用できます。
