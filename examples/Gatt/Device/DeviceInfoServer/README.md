# Gatt/DeviceInfoServer

Publishes Manufacturer Name, Model Number, Firmware Revision, and PnP ID through
the standard Device Information Service (`0x180A`). PnP ID uses its standard
7-byte wire format: Vendor ID Source followed by little-endian Vendor ID,
Product ID, and Product Version.

This small read-only service is composed with the generic GATT Server API rather
than a profile-specific owner. Use it with [DeviceInfoClient](../DeviceInfoClient/).
