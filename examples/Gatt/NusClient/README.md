# Gatt/NusClient

Scans for a Nordic UART Service compatible server, subscribes to TX
notifications, and sends each non-empty Serial line to RX using Write Without
Response. TX notifications are printed back to Serial.

This demonstrates that NUS can be built by composing the generic GATT Client
API; no stream semantics or automatic packet framing are implied.

Pair with [NusServer](../NusServer/).
