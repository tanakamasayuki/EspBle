#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESPBLE_HCI_COMMAND_SCHEDULER_CAPACITY 16
// H4 type + three-byte command header + the maximum 255-byte parameter body.
#define ESPBLE_HCI_COMMAND_MAX_LENGTH 259

typedef enum {
  ESPBLE_HCI_COMMAND_SCHEDULER_OK = 0,
  ESPBLE_HCI_COMMAND_SCHEDULER_EMPTY,
  ESPBLE_HCI_COMMAND_SCHEDULER_BLOCKED,
  ESPBLE_HCI_COMMAND_SCHEDULER_QUEUE_FULL,
  ESPBLE_HCI_COMMAND_SCHEDULER_INVALID_PACKET,
  ESPBLE_HCI_COMMAND_SCHEDULER_RESPONSE_MISMATCH,
} espble_hci_command_scheduler_result_t;

typedef struct {
  uint8_t owner;
  uint16_t length;
  uint8_t packet[ESPBLE_HCI_COMMAND_MAX_LENGTH];
} espble_hci_command_scheduler_entry_t;

typedef struct {
  espble_hci_command_scheduler_entry_t
    entries[ESPBLE_HCI_COMMAND_SCHEDULER_CAPACITY];
  size_t head;
  size_t count;
  uint8_t command_credits;
  bool awaiting_response;
  uint8_t in_flight_owner;
  uint16_t in_flight_opcode;
} espble_hci_command_scheduler_t;

void espble_hci_command_scheduler_init(
  espble_hci_command_scheduler_t *scheduler);

// Copies one complete H4 command into scheduler-owned storage.
espble_hci_command_scheduler_result_t espble_hci_command_scheduler_enqueue(
  espble_hci_command_scheduler_t *scheduler, uint8_t owner,
  const uint8_t *packet, size_t length);

// Returns the FIFO head only when controller command credit permits it.  The
// returned pointer remains owned by the scheduler and is valid until mark_sent.
espble_hci_command_scheduler_result_t espble_hci_command_scheduler_peek(
  const espble_hci_command_scheduler_t *scheduler, uint8_t *owner,
  const uint8_t **packet, size_t *length);

// Commits the current FIFO head after the physical transport accepted it.
espble_hci_command_scheduler_result_t espble_hci_command_scheduler_mark_sent(
  espble_hci_command_scheduler_t *scheduler);

// Applies Num_HCI_Command_Packets and verifies the response opcode for the
// conservative single in-flight transaction.
espble_hci_command_scheduler_result_t espble_hci_command_scheduler_on_event(
  espble_hci_command_scheduler_t *scheduler, const uint8_t *packet,
  size_t length);

// Removes commands that have not reached the controller.  BLOCKED means an
// already-sent command for this owner still needs its response; queued entries
// are removed even in that case.
espble_hci_command_scheduler_result_t espble_hci_command_scheduler_remove_owner(
  espble_hci_command_scheduler_t *scheduler, uint8_t owner);

#ifdef __cplusplus
}
#endif
