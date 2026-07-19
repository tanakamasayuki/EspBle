# Hid/VendorHost

Vendor-defined HID Inputを`hidHost().onVendorInput()`で受信し、
`sendVendorOutput()` / `sendVendorFeature()`でReport ID 6のOutput / Featureへ
書き込みます。security完了後にほかのHID種別と同じ`discover()`を使用します。

[VendorDevice](../VendorDevice/)と組み合わせて使用できます。
