# Gatt/EnvironmentalClient

Connects to a peripheral advertising the standard Environmental Sensing Service
(`0x181A`), sequentially reads and decodes little-endian Temperature, Humidity,
and Pressure values, then subscribes to Temperature notifications.

Use it with [EnvironmentalServer](../EnvironmentalServer/).
