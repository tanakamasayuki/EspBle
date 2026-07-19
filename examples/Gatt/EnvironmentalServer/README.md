# Gatt/EnvironmentalServer

Publishes Temperature (`0x2A6E`), Humidity (`0x2A6F`), and Pressure (`0x2A6D`)
through the standard Environmental Sensing Service (`0x181A`). Values use the
standard little-endian integer scales: 0.01 °C, 0.01%, and 0.1 Pa.

Temperature supports Read / Notify, while the other two support Read. Send `+`
or `-` over Serial to change temperature by 0.25 °C and notify subscribers. Use
it with [EnvironmentalClient](../EnvironmentalClient/).
