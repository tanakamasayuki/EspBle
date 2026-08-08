#include <sdkconfig.h>

#if defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_NIMBLE_ENABLED) && \
  (!defined(ESPBLE_CLASSIC_ONLY) || defined(ESPBLE_CLASSIC_CUSTOM_HOST))

#include "EspBleHciBroker.h"
#include "EspBleHciRouter.h"

#include <stddef.h>
#include <esp_bt.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>

static const char *TAG = "EspBleHciBroker";
static const espble_hci_host_callbacks_t *hosts[ESPBLE_HCI_HOST_COUNT];
static espble_hci_router_t router;
static portMUX_TYPE broker_lock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t next_send_host;

static espble_hci_route_t host_route(size_t host)
{
  return host == ESPBLE_HCI_HOST_NIMBLE ? ESPBLE_HCI_ROUTE_NIMBLE :
    ESPBLE_HCI_ROUTE_CLASSIC;
}

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
  // Rotate the first notification. A busy logical host must not permanently
  // win every newly available physical VHCI slot.
  const uint8_t first = next_send_host;
  next_send_host = (uint8_t)((next_send_host + 1u) % ESPBLE_HCI_HOST_COUNT);
  for (size_t offset = 0; offset < ESPBLE_HCI_HOST_COUNT; ++offset)
  {
    const size_t i = (first + offset) % ESPBLE_HCI_HOST_COUNT;
    const espble_hci_host_callbacks_t *callbacks = hosts[i];
    if (callbacks != NULL && callbacks->notify_send_available != NULL)
    {
      callbacks->notify_send_available();
    }
  }
}

static int physical_receive(uint8_t *data, uint16_t length)
{
#if defined(ESPBLE_HCI_TRACE)
  if (length >= 3 && data[0] == 0x04)
  {
    if (data[1] == 0x0e && length >= 7)
      ESP_LOGE(TAG, "TRACE RX command complete opcode=0x%02x%02x status=0x%02x",
        data[5], data[4], data[6]);
    else if (data[1] == 0x0f && length >= 7)
      ESP_LOGE(TAG, "TRACE RX command status opcode=0x%02x%02x status=0x%02x",
        data[6], data[5], data[3]);
    else if (data[1] == 0x3e && length >= 5)
      ESP_LOGE(TAG, "TRACE RX LE meta subevent=0x%02x status=0x%02x",
        data[3], data[4]);
  }
#endif
#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)
  if (registered_host_count() > 1)
  {
    uint8_t filtered[ESPBLE_HCI_HOST_COUNT][258];
    size_t filtered_length[ESPBLE_HCI_HOST_COUNT] = {};
    const uint8_t *delivery[ESPBLE_HCI_HOST_COUNT] = {data, data};
    const espble_hci_host_callbacks_t *callbacks[ESPBLE_HCI_HOST_COUNT] = {};

    portENTER_CRITICAL(&broker_lock);
    const espble_hci_route_t route =
      espble_hci_router_route_incoming(&router, data, length);
    for (size_t i = 0; i < ESPBLE_HCI_HOST_COUNT; ++i)
    {
      if ((route & host_route(i)) == 0 || hosts[i] == NULL) continue;
      callbacks[i] = hosts[i];
      if (length >= 2 && data[0] == 0x04 && data[1] == 0x13)
      {
        if (espble_hci_router_packet_for_host(
              &router, host_route(i), data, length, filtered[i],
              sizeof(filtered[i]), &filtered_length[i]) != ESPBLE_HCI_ROUTER_OK)
        {
          callbacks[i] = NULL;
          continue;
        }
        delivery[i] = filtered[i];
      }
      else
      {
        filtered_length[i] = length;
      }
    }
    portEXIT_CRITICAL(&broker_lock);

    int result = 0;
    for (size_t i = 0; i < ESPBLE_HCI_HOST_COUNT; ++i)
    {
      if (callbacks[i] != NULL && callbacks[i]->notify_receive != NULL)
        result |= callbacks[i]->notify_receive(
          (uint8_t *)delivery[i], (uint16_t)filtered_length[i]);
    }
    return result;
  }
#endif
#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)
  // Even before the second host attaches, retain command and connection state.
  // Classic profile setup is asynchronous, so an outstanding response can
  // legitimately arrive after registration transitions from one host to two.
  portENTER_CRITICAL(&broker_lock);
  (void)espble_hci_router_route_incoming(&router, data, length);
  portEXIT_CRITICAL(&broker_lock);
#endif
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
#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)
    hosts[host] = callbacks;
    ESP_LOGW(TAG, "registered second host %d in experimental routed mode", (int)host);
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
  }

  hosts[host] = callbacks;
  espble_hci_router_init(&router);
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
  if (!esp_vhci_host_check_send_available()) return ESP_ERR_INVALID_STATE;
#if defined(ESPBLE_HCI_TRACE)
  if (length >= 4 && data[0] == 0x01)
    ESP_LOGE(TAG, "TRACE TX host=%d command opcode=0x%02x%02x",
      (int)host, data[2], data[1]);
#endif
#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)
  if (registered_host_count() > 0)
  {
    portENTER_CRITICAL(&broker_lock);
    const espble_hci_router_result_t tracked =
      espble_hci_router_track_outgoing(&router, host_route(host), data, length);
    portEXIT_CRITICAL(&broker_lock);
    if (tracked != ESPBLE_HCI_ROUTER_OK)
    {
      ESP_LOGE(TAG, "rejected host %d H4 packet: router error %d",
        (int)host, (int)tracked);
      if (tracked == ESPBLE_HCI_ROUTER_QUEUE_FULL)
      {
        for (size_t i = 0; i < router.pending_count; ++i)
          ESP_LOGE(TAG, "pending command %u: host=%u opcode=0x%04x",
            (unsigned)i, (unsigned)router.pending[i].owner,
            (unsigned)router.pending[i].opcode);
      }
      return tracked == ESPBLE_HCI_ROUTER_QUEUE_FULL ? ESP_ERR_NO_MEM :
        ESP_ERR_INVALID_ARG;
    }
  }
#endif
  esp_vhci_host_send_packet((uint8_t *)data, length);
  return ESP_OK;
}

#endif
