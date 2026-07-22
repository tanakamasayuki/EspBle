# Gatt/NusServer

Implements the Nordic UART Service UUID layout with the generic GATT Server API:
RX accepts writes with or without response, and TX sends notifications. Received
RX data is echoed to subscribed TX clients.

NUS is a packet-oriented GATT convention, not Bluetooth Classic SPP. Payloads
must fit the connection's ATT/MTU limit and applications must add framing when
messages can span multiple packets.

Pair with [NusClient](../NusClient/).
