# Gatt/HeartRateClient

Connects to a peripheral advertising the standard Heart Rate Service (`0x180D`),
reads Body Sensor Location, and subscribes to Heart Rate Measurement. It decodes
8/16-bit heart rate, Energy Expended, and multiple RR intervals according to the
flags while validating every variable-length boundary.

Use it with [HeartRateServer](../HeartRateServer/).
