#include <EspBleClassic.h>
#include <esp_hidd_api.h>
#include <esp_hidh_api.h>

extern "C" esp_err_t espble_bd_esp_bt_hid_device_register_callback(
  esp_hd_cb_t callback);
extern "C" esp_err_t espble_bd_esp_bt_hid_device_init();
extern "C" esp_err_t espble_bd_esp_bt_hid_device_deinit();
extern "C" esp_err_t espble_bd_esp_bt_hid_host_register_callback(
  esp_hh_cb_t callback);
extern "C" esp_err_t espble_bd_esp_bt_hid_host_init();
extern "C" esp_err_t espble_bd_esp_bt_hid_host_deinit();

EspBleClassic bluetooth;
volatile bool deviceInitialized = false;
volatile bool hostInitialized = false;
volatile bool deinitStarted = false;

void deviceCallback(
  esp_hidd_cb_event_t event, esp_hidd_cb_param_t *parameter)
{
  if (event == ESP_HIDD_INIT_EVT)
  {
    Serial.printf("CLASSIC_HIDD_INIT status=%d\n", parameter->init.status);
    deviceInitialized = parameter->init.status == ESP_HIDD_SUCCESS;
  }
  else if (event == ESP_HIDD_DEINIT_EVT)
  {
    Serial.printf("CLASSIC_HIDD_DEINIT status=%d\n", parameter->deinit.status);
  }
}

void hostCallback(
  esp_hidh_cb_event_t event, esp_hidh_cb_param_t *parameter)
{
  if (event == ESP_HIDH_INIT_EVT)
  {
    Serial.printf("CLASSIC_HIDH_INIT status=%d\n", parameter->init.status);
    hostInitialized = parameter->init.status == ESP_HIDH_OK;
  }
  else if (event == ESP_HIDH_DEINIT_EVT)
  {
    Serial.printf("CLASSIC_HIDH_DEINIT status=%d\n", parameter->deinit.status);
  }
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  EspBleClassicConfig config;
  config.deviceName = "EspBle Classic HID Profile Test";
  if (!bluetooth.begin(config))
  {
    Serial.printf("CLASSIC_HID_STACK_FAILED %s\n", bluetooth.lastErrorDetail().c_str());
    return;
  }
  const esp_err_t deviceCallbackStatus =
    espble_bd_esp_bt_hid_device_register_callback(deviceCallback);
  const esp_err_t deviceInitStatus = espble_bd_esp_bt_hid_device_init();
  Serial.printf(
    "CLASSIC_HIDD_START %d %d\n", deviceCallbackStatus, deviceInitStatus);
  const esp_err_t hostCallbackStatus =
    espble_bd_esp_bt_hid_host_register_callback(hostCallback);
  const esp_err_t hostInitStatus = espble_bd_esp_bt_hid_host_init();
  Serial.printf(
    "CLASSIC_HIDH_START %d %d\n", hostCallbackStatus, hostInitStatus);
}

void loop()
{
  bluetooth.update();
  if (deviceInitialized && hostInitialized && !deinitStarted)
  {
    deinitStarted = true;
    Serial.printf(
      "CLASSIC_HID_DEINIT_START %d %d\n",
      espble_bd_esp_bt_hid_device_deinit(),
      espble_bd_esp_bt_hid_host_deinit());
  }
  delay(1);
}
