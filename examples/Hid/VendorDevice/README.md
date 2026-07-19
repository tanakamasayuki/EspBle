# Hid/VendorDevice

Publishes vendor-defined HID using fixed Report ID 6. Send Input with
`hidVendor().sendInput()` and receive Host writes through `onOutputReport()` /
`onFeatureReport()` in the `ble.update()` context. Report size is configurable
from 1 to 64 bytes.

Names and Report ID align with EspUsbDevice Vendor HID. Use it with
[VendorHost](../VendorHost/).
