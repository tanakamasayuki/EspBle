# Gatt/DeviceInfoClient

Connects to a peripheral advertising the standard Device Information Service
(`0x180A`) and sequentially reads Manufacturer Name, Model Number, Firmware
Revision, and PnP ID. The 7-byte PnP ID wire value is decoded into its
little-endian identifiers.

Use it with [DeviceInfoServer](../DeviceInfoServer/).
