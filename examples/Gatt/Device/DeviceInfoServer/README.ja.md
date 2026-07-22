# Gatt/DeviceInfoServer

標準Device Information Service（`0x180A`）でManufacturer Name、Model Number、
Firmware Revision、PnP IDを公開します。PnP IDは7-byteの標準wire形式
（Vendor ID Sourceとlittle-endianのVendor ID / Product ID / Product Version）です。

小さなread-only Serviceなのでprofile専用ownerは増やさず、汎用GATT Server APIで
構成します。[DeviceInfoClient](../DeviceInfoClient/)と組み合わせて使用できます。
