#include "EspBleHciCommandScheduler.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {
int failures;
void check(const char *name, bool value) {
  if (!value) { std::printf("FAIL %s\n", name); ++failures; }
}
}

int main()
{
  espble_hci_command_scheduler_t scheduler;
  espble_hci_command_scheduler_init(&scheduler);

  const uint8_t classicReset[] = {0x01, 0x03, 0x0c, 0x00};
  const uint8_t nimbleFeatures[] = {0x01, 0x03, 0x20, 0x00};
  check("enqueue classic", espble_hci_command_scheduler_enqueue(
    &scheduler, 1, classicReset, sizeof(classicReset)) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_OK);
  check("enqueue nimble", espble_hci_command_scheduler_enqueue(
    &scheduler, 2, nimbleFeatures, sizeof(nimbleFeatures)) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_OK);

  uint8_t owner = 0;
  const uint8_t *packet = nullptr;
  size_t length = 0;
  check("peek FIFO head", espble_hci_command_scheduler_peek(
    &scheduler, &owner, &packet, &length) == ESPBLE_HCI_COMMAND_SCHEDULER_OK &&
    owner == 1 && length == sizeof(classicReset) &&
    std::memcmp(packet, classicReset, length) == 0);
  check("mark first sent", espble_hci_command_scheduler_mark_sent(&scheduler) ==
    ESPBLE_HCI_COMMAND_SCHEDULER_OK);
  check("one response command in flight", espble_hci_command_scheduler_peek(
    &scheduler, &owner, &packet, &length) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_BLOCKED);

  const uint8_t wrongComplete[] = {0x04, 0x0e, 0x04, 0x01, 0x03, 0x20, 0x00};
  check("mismatched response rejected", espble_hci_command_scheduler_on_event(
    &scheduler, wrongComplete, sizeof(wrongComplete)) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_RESPONSE_MISMATCH);
  check("mismatch keeps command blocked", espble_hci_command_scheduler_peek(
    &scheduler, &owner, &packet, &length) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_BLOCKED);

  const uint8_t resetCompleteNoCredit[] = {
    0x04, 0x0e, 0x04, 0x00, 0x03, 0x0c, 0x00};
  check("matching response accepted", espble_hci_command_scheduler_on_event(
    &scheduler, resetCompleteNoCredit, sizeof(resetCompleteNoCredit)) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_OK);
  check("zero controller credit blocks", espble_hci_command_scheduler_peek(
    &scheduler, &owner, &packet, &length) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_BLOCKED);

  const uint8_t unsolicitedCredit[] = {
    0x04, 0x0f, 0x04, 0x00, 0x01, 0xff, 0xfc};
  check("later event restores credit", espble_hci_command_scheduler_on_event(
    &scheduler, unsolicitedCredit, sizeof(unsolicitedCredit)) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_OK);
  check("second command unblocked", espble_hci_command_scheduler_peek(
    &scheduler, &owner, &packet, &length) == ESPBLE_HCI_COMMAND_SCHEDULER_OK &&
    owner == 2);
  check("mark second sent", espble_hci_command_scheduler_mark_sent(&scheduler) ==
    ESPBLE_HCI_COMMAND_SCHEDULER_OK);
  const uint8_t featureStatus[] = {0x04, 0x0f, 0x04, 0x00, 0x01, 0x03, 0x20};
  check("command status completes in-flight", espble_hci_command_scheduler_on_event(
    &scheduler, featureStatus, sizeof(featureStatus)) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_OK);

  // Host Number Of Completed Packets is not command-flow-controlled and has
  // no Command Complete/Status response.
  const uint8_t hostCompleted[] = {
    0x01, 0x35, 0x0c, 0x05, 0x01, 0x40, 0x00, 0x01, 0x00};
  scheduler.command_credits = 0;
  check("enqueue no-response command", espble_hci_command_scheduler_enqueue(
    &scheduler, 2, hostCompleted, sizeof(hostCompleted)) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_OK);
  check("no-response ignores zero credit", espble_hci_command_scheduler_peek(
    &scheduler, &owner, &packet, &length) == ESPBLE_HCI_COMMAND_SCHEDULER_OK);
  check("no-response sent", espble_hci_command_scheduler_mark_sent(&scheduler) ==
    ESPBLE_HCI_COMMAND_SCHEDULER_OK && !scheduler.awaiting_response);

  const uint8_t copied[] = {0x01, 0x01, 0xfc, 0x01, 0xaa};
  uint8_t mutableCommand[sizeof(copied)];
  std::memcpy(mutableCommand, copied, sizeof(copied));
  scheduler.command_credits = 1;
  check("enqueue copied packet", espble_hci_command_scheduler_enqueue(
    &scheduler, 1, mutableCommand, sizeof(mutableCommand)) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_OK);
  mutableCommand[4] = 0xbb;
  check("queue owns packet storage", espble_hci_command_scheduler_peek(
    &scheduler, &owner, &packet, &length) == ESPBLE_HCI_COMMAND_SCHEDULER_OK &&
    std::memcmp(packet, copied, sizeof(copied)) == 0);
  check("send copied packet", espble_hci_command_scheduler_mark_sent(&scheduler) ==
    ESPBLE_HCI_COMMAND_SCHEDULER_OK);
  const uint8_t vendorComplete[] = {0x04, 0x0e, 0x04, 0x01, 0x01, 0xfc, 0x00};
  check("complete copied packet", espble_hci_command_scheduler_on_event(
    &scheduler, vendorComplete, sizeof(vendorComplete)) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_OK);

  for (size_t i = 0; i < ESPBLE_HCI_COMMAND_SCHEDULER_CAPACITY; ++i) {
    check("fill scheduler queue", espble_hci_command_scheduler_enqueue(
      &scheduler, 1, classicReset, sizeof(classicReset)) ==
        ESPBLE_HCI_COMMAND_SCHEDULER_OK);
  }
  check("scheduler overflow", espble_hci_command_scheduler_enqueue(
    &scheduler, 2, nimbleFeatures, sizeof(nimbleFeatures)) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_QUEUE_FULL);

  const uint8_t malformed[] = {0x01, 0x03, 0x0c, 0x01};
  espble_hci_command_scheduler_t empty;
  espble_hci_command_scheduler_init(&empty);
  check("malformed command rejected", espble_hci_command_scheduler_enqueue(
    &empty, 1, malformed, sizeof(malformed)) ==
      ESPBLE_HCI_COMMAND_SCHEDULER_INVALID_PACKET);
  check("empty queue", espble_hci_command_scheduler_peek(
    &empty, &owner, &packet, &length) == ESPBLE_HCI_COMMAND_SCHEDULER_EMPTY);

  if (failures) return 1;
  std::puts("OK");
  return 0;
}
