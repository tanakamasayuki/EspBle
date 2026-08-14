#ifndef ESP_BLE_HCI_ACL_CREDITS_H
#define ESP_BLE_HCI_ACL_CREDITS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Controller-to-host ACL flow control for a shared controller.
//
// With two logical hosts, neither one can run this loop: Classic Bluedroid
// enables flow control and then only credits the packets routed to it, while
// the vendored NimBLE never returns credits at all. The controller's host
// buffer pool then drains on LE traffic and both transports stall. The broker
// therefore owns the loop for both hosts: it answers the hosts' configuration
// commands virtually, configures the physical controller itself, and returns
// one credit per ACL packet it handed to a host.
//
// This module holds only the arithmetic and the packet encoding so it can be
// exercised without a controller. It never allocates and never blocks.

#define ESPBLE_HCI_ACL_CREDITS_MAX_CONNECTIONS 16
// H4 type, opcode, parameter length, handle count, then handle/count pairs.
#define ESPBLE_HCI_ACL_CREDITS_MAX_COMMAND_LENGTH \
  (5u + 4u * ESPBLE_HCI_ACL_CREDITS_MAX_CONNECTIONS)

typedef struct
{
  uint16_t handle;
  uint16_t delivered;
  bool used;
} espble_hci_acl_credit_entry_t;

typedef struct
{
  espble_hci_acl_credit_entry_t entries[ESPBLE_HCI_ACL_CREDITS_MAX_CONNECTIONS];
  uint16_t acl_data_length;
  uint16_t acl_packet_count;
  uint16_t synchronous_data_length;
  uint16_t synchronous_packet_count;
  bool controller_buffers_known;
  uint16_t pending_total;
  uint16_t flush_threshold;
  uint32_t dropped_credits;
} espble_hci_acl_credits_t;

// A threshold of 0 or 1 returns a credit for every delivered packet. Larger
// values batch credits, at the cost of holding that many controller buffers.
void espble_hci_acl_credits_init(
  espble_hci_acl_credits_t *credits, uint16_t flush_threshold);

// Learns the controller's buffer geometry from the Read Buffer Size response.
// Returns true when this event carried it.
bool espble_hci_acl_credits_observe_event(
  espble_hci_acl_credits_t *credits, const uint8_t *packet, size_t length);

bool espble_hci_acl_credits_ready(const espble_hci_acl_credits_t *credits);

// Builds the configuration the broker sends in place of the hosts' own
// requests. Both return 0 when the controller geometry is still unknown or the
// buffer is too small.
size_t espble_hci_acl_credits_build_host_buffer_size(
  const espble_hci_acl_credits_t *credits, uint8_t *output, size_t capacity);
size_t espble_hci_acl_credits_build_flow_control_enable(
  const espble_hci_acl_credits_t *credits, uint8_t *output, size_t capacity);

// Counts one ACL packet handed to a logical host. Packets for an untracked
// handle are counted as dropped credits instead of silently displacing a live
// connection's record.
void espble_hci_acl_credits_on_delivered(
  espble_hci_acl_credits_t *credits, uint16_t handle);

// The controller frees its buffers on disconnection, and the host must not
// credit a handle that no longer exists.
void espble_hci_acl_credits_forget_handle(
  espble_hci_acl_credits_t *credits, uint16_t handle);

// Builds Host Number Of Completed Packets for everything accumulated so far.
// Without force, it builds nothing until the threshold is reached. Returns the
// packet length, or 0 when there is nothing to send or the buffer is too small.
// Credits are cleared only for the handles the returned packet carries.
size_t espble_hci_acl_credits_build_credits(
  espble_hci_acl_credits_t *credits, bool force, uint8_t *output,
  size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
