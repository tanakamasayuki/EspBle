#include <EspBle.h>
#include <WiFi.h>
#include "esp32-hal-hosted.h"

#ifndef WIFI_TEST_SSID
#define WIFI_TEST_SSID ""
#endif
#ifndef WIFI_TEST_PASSWORD
#define WIFI_TEST_PASSWORD ""
#endif

static constexpr const char *TEST_SERVICE_UUID = "98d46f50-c2a7-4a71-9003-636f65786973";
static constexpr const char *TEST_CHARACTERISTIC_UUID = "98d46f51-c2a7-4a71-9003-636f65786973";

EspBle ble;
bool bleStarted = false;
bool connectionRequested = false;
EspBleConnectionId connectionId = 0;

static void printHostedState(const char *label)
{
  Serial.printf(
    "%s wifi_connected=%u ip=%u hosted=%u wifi=%u ble=%u\n",
    label,
    WiFi.STA.connected() ? 1 : 0,
    WiFi.STA.hasIP() ? 1 : 0,
    hostedIsInitialized() ? 1 : 0,
    hostedIsWiFiActive() ? 1 : 0,
    hostedIsBLEActive() ? 1 : 0);
}

static void configureBleCallbacks()
{
  ble.onConnected([](const EspBleConnection &connection) {
    connectionId = connection.id;
    Serial.println(
      ble.discoverCharacteristic(connection.id, TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID)
        ? "DISCOVER_REQUESTED"
        : "DISCOVER_REQUEST_FAILED");
  });
  ble.onCharacteristicDiscovered([](const EspBleGattResult &result) {
    Serial.printf(
      "DISCOVER success=%u read=%u write=%u notify=%u wifi=%u\n",
      result.success ? 1 : 0,
      result.readable ? 1 : 0,
      result.writable ? 1 : 0,
      result.notifiable ? 1 : 0,
      WiFi.STA.connected() ? 1 : 0);
    if (result.success)
    {
      Serial.println(
        ble.readCharacteristic(result.connectionId, TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID)
          ? "READ_REQUESTED"
          : "READ_REQUEST_FAILED");
    }
  });
  ble.onCharacteristicRead([](const EspBleGattResult &result) {
    Serial.printf(
      "READ success=%u value=%s wifi=%u\n",
      result.success ? 1 : 0,
      result.value.c_str(),
      WiFi.STA.connected() ? 1 : 0);
    if (result.success)
    {
      Serial.println(
        ble.writeCharacteristic(
          result.connectionId,
          TEST_SERVICE_UUID,
          TEST_CHARACTERISTIC_UUID,
          String("p4-write"),
          true)
          ? "WRITE_REQUESTED"
          : "WRITE_REQUEST_FAILED");
    }
  });
  ble.onCharacteristicWritten([](const EspBleGattResult &result) {
    Serial.printf(
      "WRITE success=%u wifi=%u\n",
      result.success ? 1 : 0,
      WiFi.STA.connected() ? 1 : 0);
    if (result.success)
    {
      Serial.println(
        ble.subscribe(result.connectionId, TEST_SERVICE_UUID, TEST_CHARACTERISTIC_UUID, true)
          ? "SUBSCRIBE_REQUESTED"
          : "SUBSCRIBE_REQUEST_FAILED");
    }
  });
  ble.onSubscribed([](const EspBleGattResult &result) {
    Serial.printf(
      "SUBSCRIBED success=%u wifi=%u hosted=%u wifi_active=%u ble_active=%u\n",
      result.success ? 1 : 0,
      WiFi.STA.connected() ? 1 : 0,
      hostedIsInitialized() ? 1 : 0,
      hostedIsWiFiActive() ? 1 : 0,
      hostedIsBLEActive() ? 1 : 0);
  });
  ble.onNotification([](const EspBleGattNotification &notification) {
    Serial.printf(
      "NOTIFICATION value=%s wifi=%u\n",
      notification.value.c_str(),
      WiFi.STA.connected() ? 1 : 0);
  });
  ble.onDisconnected([](const EspBleConnection &) {
    connectionId = 0;
    Serial.println("DISCONNECTED");
  });
  ble.scanner().onResult([](const EspBleScanResult &result) {
    if (connectionRequested || !result.advertisesService(TEST_SERVICE_UUID)) return;
    ble.scanner().stop();
    connectionRequested = ble.connect(result);
    Serial.println(connectionRequested ? "CONNECT_REQUESTED" : "CONNECT_REQUEST_FAILED");
  });
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  configureBleCallbacks();
  Serial.println("READY");
}

void loop()
{
  if (Serial.available() > 0)
  {
    const char command = Serial.read();
    if (command == 'w')
    {
      if (WIFI_TEST_SSID[0] == '\0')
      {
        Serial.println("WIFI_CREDENTIALS_MISSING");
      }
      else if (!WiFi.STA.begin(false) ||
               !WiFi.STA.connect(WIFI_TEST_SSID, WIFI_TEST_PASSWORD))
      {
        Serial.println("WIFI_START_FAILED");
      }
      else
      {
        const uint32_t deadline = millis() + 30000;
        while ((!WiFi.STA.connected() || !WiFi.STA.hasIP()) &&
               static_cast<int32_t>(millis() - deadline) < 0)
        {
          delay(50);
        }
        printHostedState("WIFI_STARTED");
      }
    }
    else if (command == 'b' && !bleStarted)
    {
      EspBleConfig config;
      config.deviceName = "EspBle Hosted Coexistence";
      bleStarted = ble.begin(config);
      printHostedState(bleStarted ? "BLE_STARTED" : "BLE_START_FAILED");
    }
    else if (command == 's' && bleStarted && !connectionRequested)
    {
      EspBleScanConfig scanConfig;
      scanConfig.active = true;
      Serial.println(ble.scanner().start(scanConfig) ? "SCAN_STARTED" : "SCAN_START_FAILED");
    }
    else if (command == 'e' && bleStarted)
    {
      ble.end();
      bleStarted = false;
      connectionRequested = false;
      connectionId = 0;
      delay(100);
      printHostedState("BLE_ENDED");
    }
    else if (command == 'q')
    {
      WiFi.STA.disconnect(false, 5000);
      const bool ended = WiFi.STA.end();
      delay(100);
      Serial.printf("WIFI_END_RESULT %u\n", ended ? 1 : 0);
      printHostedState("WIFI_ENDED");
    }
  }

  if (bleStarted) ble.update();
  delay(1);
}
