#include <sdkconfig.h>

#if defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_NIMBLE_ENABLED) && \
  (!defined(ESPBLE_CLASSIC_ONLY) || defined(ESPBLE_CLASSIC_CUSTOM_HOST))

#include "EspBleHciBroker.h"

#include <stddef.h>
#include <esp_bt.h>
#include <esp_log.h>

static const char *TAG = "EspBleHciBroker";
static const espble_hci_host_callbacks_t *hosts[ESPBLE_HCI_HOST_COUNT];

static bool valid_host(espble_hci_host_t host)
{
  return host >= ESPBLE_HCI_HOST_NIMBLE && host < ESPBLE_HCI_HOST_COUNT;
}

static size_t registered_host_count(void)
{
  size_t count = 0;
  for (size_t i = 0; i < ESPBLE_HCI_HOST_COUNT; ++i)
  {
    if (hosts[i] != NULL) ++count;
  }
  return count;
}

static void physical_send_available(void)
{
  // Only one host is admitted by register() in the pass-through phase.
  for (size_t i = 0; i < ESPBLE_HCI_HOST_COUNT; ++i)
  {
    const espble_hci_host_callbacks_t *callbacks = hosts[i];
    if (callbacks != NULL && callbacks->notify_send_available != NULL)
    {
      callbacks->notify_send_available();
    }
  }
}

static int physical_receive(uint8_t *data, uint16_t length)
{
  for (size_t i = 0; i < ESPBLE_HCI_HOST_COUNT; ++i)
  {
    const espble_hci_host_callbacks_t *callbacks = hosts[i];
    if (callbacks != NULL && callbacks->notify_receive != NULL)
    {
      return callbacks->notify_receive(data, length);
    }
  }
  return 0;
}

static void dummy_send_available(void)
{
}

static int dummy_receive(uint8_t *data, uint16_t length)
{
  (void)data;
  (void)length;
  return 0;
}

static const esp_vhci_host_callback_t physical_callbacks = {
  .notify_host_send_available = physical_send_available,
  .notify_host_recv = physical_receive,
};

static const esp_vhci_host_callback_t dummy_callbacks = {
  .notify_host_send_available = dummy_send_available,
  .notify_host_recv = dummy_receive,
};

esp_err_t espble_hci_broker_register(
  espble_hci_host_t host, const espble_hci_host_callbacks_t *callbacks)
{
  if (!valid_host(host) || callbacks == NULL || callbacks->notify_receive == NULL)
  {
    return ESP_ERR_INVALID_ARG;
  }
  if (hosts[host] == callbacks)
  {
    return ESP_OK;
  }
  if (hosts[host] != NULL)
  {
    return ESP_ERR_INVALID_STATE;
  }
  if (registered_host_count() != 0)
  {
    return ESP_ERR_NOT_SUPPORTED;
  }

  hosts[host] = callbacks;
  esp_err_t result = esp_vhci_host_register_callback(&physical_callbacks);
  if (result != ESP_OK)
  {
    hosts[host] = NULL;
    return result;
  }
  ESP_LOGI(TAG, "registered host %d in single-host pass-through mode", (int)host);
  return ESP_OK;
}

void espble_hci_broker_unregister(espble_hci_host_t host)
{
  if (!valid_host(host)) return;
  hosts[host] = NULL;
  if (registered_host_count() == 0)
  {
    esp_vhci_host_register_callback(&dummy_callbacks);
  }
}

bool espble_hci_broker_can_send(espble_hci_host_t host)
{
  return valid_host(host) && hosts[host] != NULL &&
    esp_vhci_host_check_send_available();
}

esp_err_t espble_hci_broker_send(
  espble_hci_host_t host, const uint8_t *data, uint16_t length)
{
  if (!valid_host(host) || hosts[host] == NULL)
  {
    return ESP_ERR_INVALID_STATE;
  }
  if (data == NULL || length == 0)
  {
    return ESP_ERR_INVALID_ARG;
  }
  esp_vhci_host_send_packet((uint8_t *)data, length);
  return ESP_OK;
}

#endif
