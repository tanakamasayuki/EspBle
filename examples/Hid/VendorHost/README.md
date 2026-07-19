# Hid/VendorHost

Receives vendor-defined HID Input through `hidHost().onVendorInput()` and writes
Report ID 6 Output / Feature through `sendVendorOutput()` /
`sendVendorFeature()`. It uses the same post-security `discover()` flow as all
other HID report types.

Use it with [VendorDevice](../VendorDevice/).
