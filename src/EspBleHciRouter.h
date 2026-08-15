#ifndef ESP_BLE_HCI_ROUTER_H
#define ESP_BLE_HCI_ROUTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// This module has no ESP-IDF dependency.  Keeping H4 parsing and ownership
// bookkeeping separate from VHCI makes the safety-critical routing rules easy
// to exercise on the host.
typedef enum
{
  ESPBLE_HCI_ROUTE_NONE = 0,
  ESPBLE_HCI_ROUTE_NIMBLE = 1u << 0,
  ESPBLE_HCI_ROUTE_CLASSIC = 1u << 1,
  ESPBLE_HCI_ROUTE_BOTH = ESPBLE_HCI_ROUTE_NIMBLE | ESPBLE_HCI_ROUTE_CLASSIC,
} espble_hci_route_t;

typedef enum
{
  ESPBLE_HCI_ROUTER_OK = 0,
  ESPBLE_HCI_ROUTER_INVALID_PACKET,
  ESPBLE_HCI_ROUTER_QUEUE_FULL,
  ESPBLE_HCI_ROUTER_UNKNOWN_HANDLE,
  ESPBLE_HCI_ROUTER_NOT_FOR_HOST,
  ESPBLE_HCI_ROUTER_BUFFER_TOO_SMALL,
} espble_hci_router_result_t;

#define ESPBLE_HCI_ROUTER_MAX_PENDING_COMMANDS 16
#define ESPBLE_HCI_ROUTER_MAX_CONNECTIONS 16

typedef struct
{
  uint16_t opcode;
  uint8_t owner;
} espble_hci_pending_command_t;

typedef struct
{
  uint16_t handle;
  uint8_t owner;
  bool used;
} espble_hci_connection_t;

typedef struct
{
  espble_hci_pending_command_t pending[ESPBLE_HCI_ROUTER_MAX_PENDING_COMMANDS];
  uint8_t pending_count;
  espble_hci_connection_t connections[ESPBLE_HCI_ROUTER_MAX_CONNECTIONS];
} espble_hci_router_t;

void espble_hci_router_init(espble_hci_router_t *router);

// Returns whether a live controller connection handle belongs to the logical
// host. The PB/BC flag bits, if present, are ignored.
bool espble_hci_router_owns_handle(
  const espble_hci_router_t *router, espble_hci_route_t owner,
  uint16_t handle);

// Records ownership of an outgoing command and validates ACL ownership.  The
// owner must be exactly ESPBLE_HCI_ROUTE_NIMBLE or ESPBLE_HCI_ROUTE_CLASSIC.
espble_hci_router_result_t espble_hci_router_track_outgoing(
  espble_hci_router_t *router, espble_hci_route_t owner,
  const uint8_t *packet, size_t length);

// Updates connection ownership and returns the logical recipient(s).  A return
// value of NONE means malformed, unsolicited, or deliberately unowned input.
espble_hci_route_t espble_hci_router_route_incoming(
  espble_hci_router_t *router, const uint8_t *packet, size_t length);

// Copies an incoming packet for one host.  Number Of Completed Packets events
// are filtered and rebuilt because one physical event may contain handles from
// both logical hosts.
espble_hci_router_result_t espble_hci_router_packet_for_host(
  const espble_hci_router_t *router, espble_hci_route_t host,
  const uint8_t *packet, size_t length,
  uint8_t *output, size_t capacity, size_t *output_length);

#ifdef __cplusplus
}
#endif

#endif
