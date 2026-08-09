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

typedef bool (*espble_hci_controller_stop_callback_t)(void);

#define ESPBLE_HCI_DIAGNOSTIC_OPCODE_CAPACITY 64

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
  uint32_t command_enqueued[ESPBLE_HCI_HOST_COUNT];
  uint32_t command_sent[ESPBLE_HCI_HOST_COUNT];
  uint32_t command_queue_full;
  uint32_t command_response_mismatch;
  uint32_t command_unregister_busy;
  uint16_t command_queue_high_water;
  uint32_t event_mask_commands;
  uint32_t event_mask_unions;
  uint32_t virtual_resets;
  uint32_t virtual_flow_control_commands;
  uint32_t virtual_completed_packets;
  uint32_t security_events[ESPBLE_HCI_HOST_COUNT];
  uint8_t last_security_event[ESPBLE_HCI_HOST_COUNT];
  uint8_t last_security_status[ESPBLE_HCI_HOST_COUNT];
  uint8_t last_encryption_enabled[ESPBLE_HCI_HOST_COUNT];
  uint16_t command_opcodes[ESPBLE_HCI_HOST_COUNT]
    [ESPBLE_HCI_DIAGNOSTIC_OPCODE_CAPACITY];
  uint8_t command_opcode_count[ESPBLE_HCI_HOST_COUNT];
  uint32_t command_opcode_overflow[ESPBLE_HCI_HOST_COUNT];
} espble_hci_broker_diagnostics_t;

// Register a logical host with the broker. Production builds remain single-host.
// Defining ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL admits both hosts through the H4
// ownership router; this switch remains explicit until lifecycle, queued send
// arbitration, and sustained-load tests have passed.
esp_err_t espble_hci_broker_register(
  espble_hci_host_t host, const espble_hci_host_callbacks_t *callbacks);
void espble_hci_broker_unregister(espble_hci_host_t host);

// Transfers shutdown responsibility for an already-running controller to the
// broker.  The callback is invoked once, after the final logical host leaves.
esp_err_t espble_hci_broker_adopt_controller(
  espble_hci_controller_stop_callback_t stop_callback);
esp_err_t espble_hci_broker_shutdown_controller(void);
bool espble_hci_broker_has_adopted_controller(void);

bool espble_hci_broker_can_send(espble_hci_host_t host);
void espble_hci_broker_set_receive_enabled(
  espble_hci_host_t host, bool enabled);
esp_err_t espble_hci_broker_send(
  espble_hci_host_t host, const uint8_t *data, uint16_t length);

// Internal validation snapshot for the experimental dual-host transport.
// Counters are monotonic for the current controller session.
void espble_hci_broker_get_diagnostics(
  espble_hci_broker_diagnostics_t *diagnostics);

#if defined(ESPBLE_HCI_BACKPRESSURE_TEST)
// Hardware-validation hook. It is compiled only by the dual-host test sketch:
// hold physical command dispatch, exercise the normal enqueue path, then
// discard the deliberately unsent commands without exposing responses to a
// logical host. begin() requires an idle scheduler.
esp_err_t espble_hci_broker_test_begin_backpressure(void);
esp_err_t espble_hci_broker_test_end_backpressure(
  uint16_t *queued, uint16_t *high_water, uint32_t *queue_full);
#endif

#ifdef __cplusplus
}
#endif

#endif
