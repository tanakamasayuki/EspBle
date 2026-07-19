# Gatt/BatteryServer

Publishes the standard Battery Service (`0x180F`) with a readable and notifiable
Battery Level characteristic (`0x2A19`). Send `+` or `-` over Serial to update
the one-byte percentage value and notify subscribed clients.

This example intentionally uses the generic GATT Server API: standard services
whose wire format is small can be composed without adding a profile-specific
owner object.

Pair with [BatteryClient](../BatteryClient/).
