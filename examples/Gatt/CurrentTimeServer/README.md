# Gatt/CurrentTimeServer

Publishes the standard Current Time Service (`0x1805`) with a readable and
notifiable Current Time characteristic (`0x2A2B`). Its value uses the standard
10-byte wire format. Send `t` over Serial to advance the demo time by one second
and notify subscribers.

Use it with [CurrentTimeClient](../CurrentTimeClient/).
