# Gatt/BatteryClient

Scans for the standard Battery Service, connects asynchronously, reads the
one-byte Battery Level, then subscribes to Battery Level notifications.

The operation chain uses the generic GATT Client completion callbacks and can
also be used with commercial devices that expose the standard service.

Pair with [BatteryServer](../BatteryServer/).
