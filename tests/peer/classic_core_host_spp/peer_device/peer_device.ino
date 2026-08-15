// Peer for the core-host interoperability test. It deliberately links no EspBle
// code: BluetoothSerial runs on the Bluedroid host that Arduino-ESP32 ships, so
// the two boards speak SPP through two independently built stacks.
#include <BluetoothSerial.h>

BluetoothSerial serialBluetooth;
bool connectedState;

uint8_t parseNibble(char value)
{
  if (value >= '0' && value <= '9') return (uint8_t)(value - '0');
  if (value >= 'a' && value <= 'f') return (uint8_t)(value - 'a' + 10);
  if (value >= 'A' && value <= 'F') return (uint8_t)(value - 'A' + 10);
  return 0xff;
}

bool parseAddress(const String &text, uint8_t address[6])
{
  size_t index = 0;
  for (size_t byte = 0; byte < 6; ++byte)
  {
    if (index + 1 >= text.length()) return false;
    const uint8_t high = parseNibble(text[index]);
    const uint8_t low = parseNibble(text[index + 1]);
    if (high == 0xff || low == 0xff) return false;
    address[byte] = (uint8_t)((high << 4) | low);
    index += 2;
    if (byte < 5)
    {
      if (index >= text.length() || text[index] != ':') return false;
      ++index;
    }
  }
  return true;
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  // Master mode: this board is the SPP client that dials the EspBle server.
  if (!serialBluetooth.begin("EspBle Core Peer", true))
  {
    Serial.println("COREPEER_BEGIN_FAILED");
    return;
  }
  Serial.println("COREPEER_READY");
}

void loop()
{
  if (serialBluetooth.connected() != connectedState)
  {
    connectedState = !connectedState;
    Serial.printf("COREPEER_LINK connected=%u\n", connectedState ? 1 : 0);
  }

  if (serialBluetooth.available())
  {
    Serial.print("COREPEER_RX hex=");
    while (serialBluetooth.available())
      Serial.printf("%02x", (uint8_t)serialBluetooth.read());
    Serial.println();
  }

  if (Serial.available())
  {
    const String line = Serial.readStringUntil('\n');
    if (line.length() == 0) return;
    const char command = line[0];
    if (command == 'c')
    {
      uint8_t address[6] = {};
      if (!parseAddress(line.substring(1), address))
      {
        Serial.println("COREPEER_CONNECT requested=0");
        return;
      }
      // Channel 0 lets BluetoothSerial resolve the server channel over SDP,
      // which also exercises the EspBle server's service record.
      const bool requested = serialBluetooth.connect(address, 0);
      Serial.printf("COREPEER_CONNECT requested=%u\n", requested ? 1 : 0);
    }
    else if (command == 'w')
    {
      const uint8_t payload[] = {0x11, 0x00, 0x22, 0x33};
      const size_t written =
        serialBluetooth.write(payload, sizeof(payload));
      Serial.printf("COREPEER_TX written=%u\n", (unsigned)written);
    }
    else if (command == 'd')
    {
      serialBluetooth.disconnect();
      Serial.println("COREPEER_DISCONNECT requested=1");
    }
    else if (command == '?')
    {
      Serial.printf("COREPEER_STATE connected=%u heap=%u\n",
        serialBluetooth.connected() ? 1 : 0, (unsigned)ESP.getFreeHeap());
    }
  }
  delay(1);
}
