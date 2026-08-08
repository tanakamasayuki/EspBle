#include "EspBleHciCommandScheduler.h"

#include <string.h>

#define ESPBLE_HCI_PACKET_COMMAND 0x01
#define ESPBLE_HCI_PACKET_EVENT 0x04
#define ESPBLE_HCI_EVENT_COMMAND_COMPLETE 0x0e
#define ESPBLE_HCI_EVENT_COMMAND_STATUS 0x0f
#define ESPBLE_HCI_OPCODE_HOST_NUM_COMPLETED_PACKETS 0x0c35

static uint16_t command_opcode(const uint8_t *packet)
{
  return (uint16_t)packet[1] | ((uint16_t)packet[2] << 8);
}

static bool command_expects_response(const uint8_t *packet)
{
  /* HCI Host Number Of Completed Packets is explicitly not acknowledged. */
  return command_opcode(packet) != ESPBLE_HCI_OPCODE_HOST_NUM_COMPLETED_PACKETS;
}

static bool valid_command(const uint8_t *packet, size_t length)
{
  return packet != NULL && length >= 4 &&
    length == (size_t)packet[3] + 4 &&
    packet[0] == ESPBLE_HCI_PACKET_COMMAND;
}

void espble_hci_command_scheduler_init(
  espble_hci_command_scheduler_t *scheduler)
{
  if (scheduler == NULL) return;
  memset(scheduler, 0, sizeof(*scheduler));
  /* HCI starts with one command packet available.  Later values always come
   * from Command Complete / Command Status events. */
  scheduler->command_credits = 1;
}

espble_hci_command_scheduler_result_t espble_hci_command_scheduler_enqueue(
  espble_hci_command_scheduler_t *scheduler, uint8_t owner,
  const uint8_t *packet, size_t length)
{
  if (scheduler == NULL || !valid_command(packet, length) ||
      length > ESPBLE_HCI_COMMAND_MAX_LENGTH) {
    return ESPBLE_HCI_COMMAND_SCHEDULER_INVALID_PACKET;
  }
  if (scheduler->count == ESPBLE_HCI_COMMAND_SCHEDULER_CAPACITY) {
    return ESPBLE_HCI_COMMAND_SCHEDULER_QUEUE_FULL;
  }

  const size_t tail = (scheduler->head + scheduler->count) %
    ESPBLE_HCI_COMMAND_SCHEDULER_CAPACITY;
  espble_hci_command_scheduler_entry_t *entry = &scheduler->entries[tail];
  entry->owner = owner;
  entry->length = (uint16_t)length;
  memcpy(entry->packet, packet, length);
  ++scheduler->count;
  return ESPBLE_HCI_COMMAND_SCHEDULER_OK;
}

espble_hci_command_scheduler_result_t espble_hci_command_scheduler_peek(
  const espble_hci_command_scheduler_t *scheduler, uint8_t *owner,
  const uint8_t **packet, size_t *length)
{
  if (scheduler == NULL || owner == NULL || packet == NULL || length == NULL) {
    return ESPBLE_HCI_COMMAND_SCHEDULER_INVALID_PACKET;
  }
  if (scheduler->count == 0) {
    return ESPBLE_HCI_COMMAND_SCHEDULER_EMPTY;
  }

  const espble_hci_command_scheduler_entry_t *entry =
    &scheduler->entries[scheduler->head];
  if (command_expects_response(entry->packet) &&
      (scheduler->awaiting_response || scheduler->command_credits == 0)) {
    return ESPBLE_HCI_COMMAND_SCHEDULER_BLOCKED;
  }

  *owner = entry->owner;
  *packet = entry->packet;
  *length = entry->length;
  return ESPBLE_HCI_COMMAND_SCHEDULER_OK;
}

espble_hci_command_scheduler_result_t espble_hci_command_scheduler_mark_sent(
  espble_hci_command_scheduler_t *scheduler)
{
  if (scheduler == NULL) {
    return ESPBLE_HCI_COMMAND_SCHEDULER_INVALID_PACKET;
  }
  if (scheduler->count == 0) {
    return ESPBLE_HCI_COMMAND_SCHEDULER_EMPTY;
  }

  espble_hci_command_scheduler_entry_t *entry =
    &scheduler->entries[scheduler->head];
  if (command_expects_response(entry->packet)) {
    if (scheduler->awaiting_response || scheduler->command_credits == 0) {
      return ESPBLE_HCI_COMMAND_SCHEDULER_BLOCKED;
    }
    scheduler->awaiting_response = true;
    scheduler->in_flight_owner = entry->owner;
    scheduler->in_flight_opcode = command_opcode(entry->packet);
    --scheduler->command_credits;
  }

  scheduler->head = (scheduler->head + 1) %
    ESPBLE_HCI_COMMAND_SCHEDULER_CAPACITY;
  --scheduler->count;
  return ESPBLE_HCI_COMMAND_SCHEDULER_OK;
}

espble_hci_command_scheduler_result_t espble_hci_command_scheduler_on_event(
  espble_hci_command_scheduler_t *scheduler, const uint8_t *packet,
  size_t length)
{
  if (scheduler == NULL || packet == NULL || length < 3 ||
      packet[0] != ESPBLE_HCI_PACKET_EVENT ||
      length != (size_t)packet[2] + 3) {
    return ESPBLE_HCI_COMMAND_SCHEDULER_INVALID_PACKET;
  }

  uint8_t credits;
  uint16_t opcode;
  if (packet[1] == ESPBLE_HCI_EVENT_COMMAND_COMPLETE) {
    if (length < 6) return ESPBLE_HCI_COMMAND_SCHEDULER_INVALID_PACKET;
    credits = packet[3];
    opcode = (uint16_t)packet[4] | ((uint16_t)packet[5] << 8);
  } else if (packet[1] == ESPBLE_HCI_EVENT_COMMAND_STATUS) {
    if (length < 7) return ESPBLE_HCI_COMMAND_SCHEDULER_INVALID_PACKET;
    credits = packet[4];
    opcode = (uint16_t)packet[5] | ((uint16_t)packet[6] << 8);
  } else {
    return ESPBLE_HCI_COMMAND_SCHEDULER_OK;
  }

  scheduler->command_credits = credits;
  if (!scheduler->awaiting_response) {
    return ESPBLE_HCI_COMMAND_SCHEDULER_OK;
  }
  if (opcode != scheduler->in_flight_opcode) {
    return ESPBLE_HCI_COMMAND_SCHEDULER_RESPONSE_MISMATCH;
  }

  scheduler->awaiting_response = false;
  scheduler->in_flight_owner = 0;
  scheduler->in_flight_opcode = 0;
  return ESPBLE_HCI_COMMAND_SCHEDULER_OK;
}
