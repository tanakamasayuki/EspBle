#ifndef ESP_BLE_HCI_BROKER_H
#define ESP_BLE_HCI_BROKER_H

#include <stdbool.h>
#include <stdint.h>

#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
  ESPBLE_HCI_HOST_NIMBLE = 0,
  ESPBLE_HCI_HOST_CLASSIC = 1,
  ESPBLE_HCI_HOST_COUNT = 2,
} espble_hci_host_t;

typedef struct
{
  void (*notify_send_available)(void);
  int (*notify_receive)(uint8_t *data, uint16_t length);
} espble_hci_host_callbacks_t;

typedef struct
{
  uint32_t tx_acl[ESPBLE_HCI_HOST_COUNT];
  uint32_t rx_acl[ESPBLE_HCI_HOST_COUNT];
  uint32_t completed_acl[ESPBLE_HCI_HOST_COUNT];
  uint32_t unknown_acl;
  uint16_t last_tx_handle[ESPBLE_HCI_HOST_COUNT];
  uint16_t last_rx_handle[ESPBLE_HCI_HOST_COUNT];
  uint8_t last_tx_pb[ESPBLE_HCI_HOST_COUNT];
  uint8_t classic_mode;
  uint32_t classic_mode_changes;
} espble_hci_broker_diagnostics_t;

// Register a logical host with the broker. Production builds remain single-host.
// Defining ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL admits both hosts through the H4
// ownership router; this switch remains explicit until lifecycle, queued send
// arbitration, and sustained-load tests have passed.
esp_err_t espble_hci_broker_register(
  espble_hci_host_t host, const espble_hci_host_callbacks_t *callbacks);
void espble_hci_broker_unregister(espble_hci_host_t host);

bool espble_hci_broker_can_send(espble_hci_host_t host);
esp_err_t espble_hci_broker_send(
  espble_hci_host_t host, const uint8_t *data, uint16_t length);

// Internal validation snapshot for the experimental dual-host transport.
// Counters are monotonic for the current controller session.
void espble_hci_broker_get_diagnostics(
  espble_hci_broker_diagnostics_t *diagnostics);

#ifdef __cplusplus
}
#endif

#endif
