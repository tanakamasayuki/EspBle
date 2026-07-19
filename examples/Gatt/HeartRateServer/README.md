# Gatt/HeartRateServer

Publishes the standard Heart Rate Service (`0x180D`) with a notify-only Heart
Rate Measurement (`0x2A37`) and readable Body Sensor Location (`0x2A38`). The
variable-length measurement contains an 8-bit heart rate and an RR interval.

Send `+` / `-` over Serial to change the heart rate and notify subscribers. Use
it with [HeartRateClient](../HeartRateClient/).
