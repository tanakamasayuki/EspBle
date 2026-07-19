# KeyboardNkro

BLE HID keyboard using the 29-byte NKRO bitmap report shared with EspUsbDevice.
Call `enableNkro()` before `configure()`. Use an MTU of at least 32 on both peers.
