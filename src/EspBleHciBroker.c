#include <sdkconfig.h>

#if defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_NIMBLE_ENABLED) && \
  (!defined(ESPBLE_CLASSIC_ONLY) || defined(ESPBLE_CLASSIC_CUSTOM_HOST))

#include "EspBleHciBroker.h"
#include "EspBleHciCommandScheduler.h"
#include "EspBleHciRouter.h"

#include <stddef.h>
#include <string.h>
#include <esp_bt.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

static const char *TAG = "EspBleHciBroker";
static const espble_hci_host_callbacks_t *hosts[ESPBLE_HCI_HOST_COUNT];
static espble_hci_router_t router;
static espble_hci_broker_diagnostics_t diagnostics;
static portMUX_TYPE broker_lock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t next_send_host;
#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)
static espble_hci_command_scheduler_t command_scheduler;
static SemaphoreHandle_t physical_send_mutex;
static TaskHandle_t command_task_handle;
static uint32_t command_generation;
#endif

static espble_hci_route_t host_route(size_t host)
{
  return host == ESPBLE_HCI_HOST_NIMBLE ? ESPBLE_HCI_ROUTE_NIMBLE :
    ESPBLE_HCI_ROUTE_CLASSIC;
}

static uint16_t h4_acl_handle(const uint8_t *data)
{
  return ((uint16_t)data[1] | ((uint16_t)data[2] << 8)) & 0x0fff;
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

#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)
static bool dual_host_active(void)
{
  return registered_host_count() > 1;
}

static void wake_command_task(void)
{
  if (command_task_handle != NULL) xTaskNotifyGive(command_task_handle);
}

static void command_task(void *argument)
{
  (void)argument;
  uint8_t packet[ESPBLE_HCI_COMMAND_MAX_LENGTH];

  for (;;)
  {
    (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    for (;;)
    {
      uint8_t owner = 0;
      const uint8_t *queued_packet = NULL;
      size_t length = 0;
      uint32_t generation = 0;

      portENTER_CRITICAL(&broker_lock);
      const espble_hci_command_scheduler_result_t ready =
        router.pending_count == 0 ? espble_hci_command_scheduler_peek(
          &command_scheduler, &owner, &queued_packet, &length) :
          ESPBLE_HCI_COMMAND_SCHEDULER_BLOCKED;
      if (ready == ESPBLE_HCI_COMMAND_SCHEDULER_OK)
      {
        memcpy(packet, queued_packet, length);
        generation = command_generation;
      }
      portEXIT_CRITICAL(&broker_lock);
      if (ready != ESPBLE_HCI_COMMAND_SCHEDULER_OK) break;

      if (xSemaphoreTake(physical_send_mutex, portMAX_DELAY) != pdTRUE) break;
      if (!esp_vhci_host_check_send_available())
      {
        xSemaphoreGive(physical_send_mutex);
        break;
      }

      portENTER_CRITICAL(&broker_lock);
      uint8_t current_owner = 0;
      const uint8_t *current_packet = NULL;
      size_t current_length = 0;
      const bool still_current = generation == command_generation &&
        dual_host_active() && hosts[owner] != NULL &&
        router.pending_count == 0 &&
        espble_hci_command_scheduler_peek(
          &command_scheduler, &current_owner, &current_packet,
          &current_length) == ESPBLE_HCI_COMMAND_SCHEDULER_OK &&
        current_owner == owner && current_length == length &&
        memcmp(current_packet, packet, length) == 0;
      const espble_hci_router_result_t tracked = still_current ?
        espble_hci_router_track_outgoing(
          &router, host_route(owner), packet, length) :
        ESPBLE_HCI_ROUTER_INVALID_PACKET;
      const espble_hci_command_scheduler_result_t sent =
        tracked == ESPBLE_HCI_ROUTER_OK ?
          espble_hci_command_scheduler_mark_sent(&command_scheduler) :
          ESPBLE_HCI_COMMAND_SCHEDULER_BLOCKED;
      if (tracked == ESPBLE_HCI_ROUTER_OK &&
          sent == ESPBLE_HCI_COMMAND_SCHEDULER_OK)
      {
        ++diagnostics.command_sent[owner];
      }
      portEXIT_CRITICAL(&broker_lock);

      if (!still_current)
      {
        xSemaphoreGive(physical_send_mutex);
        break;
      }

      if (tracked != ESPBLE_HCI_ROUTER_OK ||
          sent != ESPBLE_HCI_COMMAND_SCHEDULER_OK)
      {
        ESP_LOGE(TAG, "command scheduler/router divergence: %d/%d",
          (int)sent, (int)tracked);
        xSemaphoreGive(physical_send_mutex);
        break;
      }

#if defined(ESPBLE_HCI_TRACE)
      ESP_LOGE(TAG, "TRACE TX scheduled host=%u command opcode=0x%02x%02x",
        (unsigned)owner, packet[2], packet[1]);
#endif
      esp_vhci_host_send_packet(packet, (uint16_t)length);
      xSemaphoreGive(physical_send_mutex);
      /* A no-response command may leave command credit available, but VHCI
       * still has only one physical TX slot.  Re-check it before continuing. */
    }
  }
}

static esp_err_t ensure_command_transport(void)
{
  if (physical_send_mutex == NULL)
  {
    physical_send_mutex = xSemaphoreCreateMutex();
    if (physical_send_mutex == NULL) return ESP_ERR_NO_MEM;
  }
  if (command_task_handle == NULL)
  {
    if (xTaskCreate(command_task, "espble_hci_cmd", 3072, NULL,
          configMAX_PRIORITIES - 2, &command_task_handle) != pdPASS)
    {
      return ESP_ERR_NO_MEM;
    }
  }
  return ESP_OK;
}
#endif

static void physical_send_available(void)
{
#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)
  wake_command_task();
#endif
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
    const espble_hci_command_scheduler_result_t command_event =
      espble_hci_command_scheduler_on_event(&command_scheduler, data, length);
    if (command_event == ESPBLE_HCI_COMMAND_SCHEDULER_RESPONSE_MISMATCH)
      ++diagnostics.command_response_mismatch;
    if (length >= 5 && data[0] == 0x02)
    {
      bool known = false;
      for (size_t i = 0; i < ESPBLE_HCI_HOST_COUNT; ++i)
      {
        if ((route & host_route(i)) == 0) continue;
        ++diagnostics.rx_acl[i];
        diagnostics.last_rx_handle[i] = h4_acl_handle(data);
        known = true;
      }
      if (!known) ++diagnostics.unknown_acl;
    }
    if (length >= 9 && data[0] == 0x04 && data[1] == 0x14 &&
        data[3] == 0x00)
    {
      diagnostics.classic_mode = data[6];
      ++diagnostics.classic_mode_changes;
    }
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
        diagnostics.completed_acl[i] += filtered[i][3];
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
    if (length >= 2 && data[0] == 0x04 &&
        (data[1] == 0x0e || data[1] == 0x0f))
      wake_command_task();
    return result;
  }
#endif
#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)
  // Even before the second host attaches, retain command and connection state.
  // Classic profile setup is asynchronous, so an outstanding response can
  // legitimately arrive after registration transitions from one host to two.
  portENTER_CRITICAL(&broker_lock);
  (void)espble_hci_router_route_incoming(&router, data, length);
  (void)espble_hci_command_scheduler_on_event(
    &command_scheduler, data, length);
  portEXIT_CRITICAL(&broker_lock);
  if (length >= 2 && data[0] == 0x04 &&
      (data[1] == 0x0e || data[1] == 0x0f))
    wake_command_task();
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
    wake_command_task();
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
  }

  esp_err_t result;
#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)
  result = ensure_command_transport();
  if (result != ESP_OK) return result;
#endif

  hosts[host] = callbacks;
  espble_hci_router_init(&router);
  memset(&diagnostics, 0, sizeof(diagnostics));
#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)
  espble_hci_command_scheduler_init(&command_scheduler);
  ++command_generation;
#endif
  result = esp_vhci_host_register_callback(&physical_callbacks);
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
#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)
  portENTER_CRITICAL(&broker_lock);
  const espble_hci_command_scheduler_result_t removed =
    espble_hci_command_scheduler_remove_owner(
      &command_scheduler, (uint8_t)host);
  if (removed == ESPBLE_HCI_COMMAND_SCHEDULER_BLOCKED)
    ++diagnostics.command_unregister_busy;
  ++command_generation;
  hosts[host] = NULL;
  const bool no_hosts = registered_host_count() == 0;
  if (no_hosts)
  {
    espble_hci_command_scheduler_init(&command_scheduler);
    espble_hci_router_init(&router);
  }
  portEXIT_CRITICAL(&broker_lock);
  wake_command_task();
  if (removed == ESPBLE_HCI_COMMAND_SCHEDULER_BLOCKED)
    ESP_LOGW(TAG, "unregistered host %d with a command response pending", (int)host);
#else
  hosts[host] = NULL;
  const bool no_hosts = registered_host_count() == 0;
#endif
  if (no_hosts)
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
#if defined(ESPBLE_HCI_TRACE)
  if (length >= 4 && data[0] == 0x01)
    ESP_LOGE(TAG, "TRACE TX host=%d command opcode=0x%02x%02x",
      (int)host, data[2], data[1]);
#endif
#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)
  const uint8_t *physical_data = data;
  uint8_t flow_control_command[5];
  // Bluedroid enables controller-to-host ACL flow control before NimBLE
  // attaches.  It can only return credits for the Classic ACL packets routed
  // to it, so LE traffic would exhaust the controller's shared host buffers.
  // Keep physical VHCI flow control disabled until the broker owns credit
  // accounting for both logical hosts.
  if (host == ESPBLE_HCI_HOST_CLASSIC && length == sizeof(flow_control_command) &&
      data[0] == 0x01 && data[1] == 0x31 && data[2] == 0x0c &&
      data[3] == 0x01 && data[4] != 0x00)
  {
    memcpy(flow_control_command, data, sizeof(flow_control_command));
    flow_control_command[4] = 0x00;
    physical_data = flow_control_command;
    ESP_LOGW(TAG, "disabled controller-to-host flow control for dual host");
  }

  if (dual_host_active() && length >= 1 && data[0] == 0x01)
  {
    portENTER_CRITICAL(&broker_lock);
    const espble_hci_command_scheduler_result_t queued =
      espble_hci_command_scheduler_enqueue(
        &command_scheduler, (uint8_t)host, physical_data, length);
    if (queued == ESPBLE_HCI_COMMAND_SCHEDULER_OK)
    {
      ++diagnostics.command_enqueued[host];
      if (command_scheduler.count > diagnostics.command_queue_high_water)
        diagnostics.command_queue_high_water = command_scheduler.count;
    }
    else if (queued == ESPBLE_HCI_COMMAND_SCHEDULER_QUEUE_FULL)
    {
      ++diagnostics.command_queue_full;
    }
    portEXIT_CRITICAL(&broker_lock);
    if (queued != ESPBLE_HCI_COMMAND_SCHEDULER_OK)
    {
      ESP_LOGE(TAG, "rejected host %d HCI command: scheduler error %d",
        (int)host, (int)queued);
      return queued == ESPBLE_HCI_COMMAND_SCHEDULER_QUEUE_FULL ?
        ESP_ERR_NO_MEM : ESP_ERR_INVALID_ARG;
    }
    wake_command_task();
    return ESP_OK;
  }
#endif

#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)
  if (xSemaphoreTake(physical_send_mutex, portMAX_DELAY) != pdTRUE)
    return ESP_ERR_INVALID_STATE;
#endif
  if (!esp_vhci_host_check_send_available())
  {
#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)
    xSemaphoreGive(physical_send_mutex);
#endif
    return ESP_ERR_INVALID_STATE;
  }
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
      xSemaphoreGive(physical_send_mutex);
      return tracked == ESPBLE_HCI_ROUTER_QUEUE_FULL ? ESP_ERR_NO_MEM :
        ESP_ERR_INVALID_ARG;
    }
  }
#endif
  esp_vhci_host_send_packet(
#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)
    (uint8_t *)physical_data,
#else
    (uint8_t *)data,
#endif
    length);
#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)
  xSemaphoreGive(physical_send_mutex);
  if (length >= 5 && data[0] == 0x02)
  {
    portENTER_CRITICAL(&broker_lock);
    ++diagnostics.tx_acl[host];
    diagnostics.last_tx_handle[host] = h4_acl_handle(data);
    diagnostics.last_tx_pb[host] = (data[2] >> 4) & 0x03;
    portEXIT_CRITICAL(&broker_lock);
  }
#endif
  return ESP_OK;
}

void espble_hci_broker_get_diagnostics(
  espble_hci_broker_diagnostics_t *output)
{
  if (output == NULL) return;
  portENTER_CRITICAL(&broker_lock);
  *output = diagnostics;
  portEXIT_CRITICAL(&broker_lock);
}

#endif
