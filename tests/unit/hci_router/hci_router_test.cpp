#include "EspBleHciRouter.h"

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
  espble_hci_router_t router;
  espble_hci_router_init(&router);

  const uint8_t reset[] = {0x01, 0x03, 0x0c, 0x00};
  const uint8_t leReadFeatures[] = {0x01, 0x03, 0x20, 0x00};
  check("track classic reset", espble_hci_router_track_outgoing(&router,
    ESPBLE_HCI_ROUTE_CLASSIC, reset, sizeof(reset)) == ESPBLE_HCI_ROUTER_OK);
  check("track nimble command", espble_hci_router_track_outgoing(&router,
    ESPBLE_HCI_ROUTE_NIMBLE, leReadFeatures, sizeof(leReadFeatures)) == ESPBLE_HCI_ROUTER_OK);

  // Responses need not be adjacent in the queue; opcode ownership is explicit.
  const uint8_t leComplete[] = {0x04, 0x0e, 0x04, 0x01, 0x03, 0x20, 0x00};
  const uint8_t resetComplete[] = {0x04, 0x0e, 0x04, 0x01, 0x03, 0x0c, 0x00};
  check("LE command complete to NimBLE", espble_hci_router_route_incoming(
    &router, leComplete, sizeof(leComplete)) == ESPBLE_HCI_ROUTE_NIMBLE);
  check("reset complete to Classic", espble_hci_router_route_incoming(
    &router, resetComplete, sizeof(resetComplete)) == ESPBLE_HCI_ROUTE_CLASSIC);
  check("unsolicited command response dropped", espble_hci_router_route_incoming(
    &router, resetComplete, sizeof(resetComplete)) == ESPBLE_HCI_ROUTE_NONE);

  const uint8_t hostCompleted[] = {
    0x01, 0x35, 0x0c, 0x05, 0x01, 0x40, 0x00, 0x01, 0x00};
  check("no-response host credits accepted", espble_hci_router_track_outgoing(
    &router, ESPBLE_HCI_ROUTE_NIMBLE, hostCompleted, sizeof(hostCompleted)) ==
      ESPBLE_HCI_ROUTER_OK);
  check("no-response host credits not queued", router.pending_count == 0);

  for (size_t i = 0; i < ESPBLE_HCI_ROUTER_MAX_PENDING_COMMANDS; ++i) {
    const uint8_t command[] = {
      0x01, static_cast<uint8_t>(i), 0xfc, 0x00};
    check("fill pending command queue", espble_hci_router_track_outgoing(
      &router, ESPBLE_HCI_ROUTE_CLASSIC, command, sizeof(command)) ==
        ESPBLE_HCI_ROUTER_OK);
  }
  const uint8_t overflowCommand[] = {0x01, 0xff, 0xfc, 0x00};
  check("pending command queue overflow", espble_hci_router_track_outgoing(
    &router, ESPBLE_HCI_ROUTE_NIMBLE, overflowCommand,
    sizeof(overflowCommand)) == ESPBLE_HCI_ROUTER_QUEUE_FULL);
  for (size_t i = 0; i < ESPBLE_HCI_ROUTER_MAX_PENDING_COMMANDS; ++i) {
    const uint8_t complete[] = {
      0x04, 0x0e, 0x04, 0x01, static_cast<uint8_t>(i), 0xfc, 0x00};
    check("drain pending command queue", espble_hci_router_route_incoming(
      &router, complete, sizeof(complete)) == ESPBLE_HCI_ROUTE_CLASSIC);
  }

  // Classic Connection Complete: status=0, handle=0x000b.
  const uint8_t classicConnection[] = {
    0x04, 0x03, 0x0b, 0x00, 0x0b, 0x00, 1, 2, 3, 4, 5, 6, 0x01, 0x00};
  // LE Connection Complete: subevent=1, status=0, handle=0x0040.
  const uint8_t leConnection[] = {
    0x04, 0x3e, 0x13, 0x01, 0x00, 0x40, 0x00, 0x00, 0x00,
    1, 2, 3, 4, 5, 6, 0x18, 0x00, 0x00, 0x00, 0xc8, 0x00, 0x00};
  check("classic connection route", espble_hci_router_route_incoming(
    &router, classicConnection, sizeof(classicConnection)) == ESPBLE_HCI_ROUTE_CLASSIC);
  check("LE connection route", espble_hci_router_route_incoming(
    &router, leConnection, sizeof(leConnection)) == ESPBLE_HCI_ROUTE_NIMBLE);

  const uint8_t classicAcl[] = {0x02, 0x0b, 0x20, 0x01, 0x00, 0xaa};
  const uint8_t leAcl[] = {0x02, 0x40, 0x20, 0x01, 0x00, 0xbb};
  check("classic ACL incoming", espble_hci_router_route_incoming(
    &router, classicAcl, sizeof(classicAcl)) == ESPBLE_HCI_ROUTE_CLASSIC);
  check("LE ACL incoming", espble_hci_router_route_incoming(
    &router, leAcl, sizeof(leAcl)) == ESPBLE_HCI_ROUTE_NIMBLE);
  check("reject cross-host ACL", espble_hci_router_track_outgoing(&router,
    ESPBLE_HCI_ROUTE_NIMBLE, classicAcl, sizeof(classicAcl)) ==
      ESPBLE_HCI_ROUTER_UNKNOWN_HANDLE);

  const uint8_t encryptionChange[] = {
    0x04, 0x08, 0x04, 0x00, 0x40, 0x00, 0x01};
  const uint8_t encryptionKeyRefresh[] = {
    0x04, 0x30, 0x03, 0x00, 0x40, 0x00};
  check("encryption change follows handle owner",
    espble_hci_router_route_incoming(
      &router, encryptionChange, sizeof(encryptionChange)) ==
        ESPBLE_HCI_ROUTE_NIMBLE);
  check("encryption key refresh follows handle owner",
    espble_hci_router_route_incoming(
      &router, encryptionKeyRefresh, sizeof(encryptionKeyRefresh)) ==
        ESPBLE_HCI_ROUTE_NIMBLE);

  // One controller event reports credits for both transports.  Each logical
  // host must see only its own record and a corrected parameter length/count.
  const uint8_t completed[] = {
    0x04, 0x13, 0x09, 0x02, 0x0b, 0x00, 0x03, 0x00,
    0x40, 0x00, 0x02, 0x00};
  check("mixed completed route", espble_hci_router_route_incoming(
    &router, completed, sizeof(completed)) == ESPBLE_HCI_ROUTE_BOTH);
  uint8_t filtered[16] = {};
  size_t filteredLength = 0;
  check("filter completed for NimBLE", espble_hci_router_packet_for_host(
    &router, ESPBLE_HCI_ROUTE_NIMBLE, completed, sizeof(completed), filtered,
    sizeof(filtered), &filteredLength) == ESPBLE_HCI_ROUTER_OK);
  const uint8_t expectedLe[] = {0x04, 0x13, 0x05, 0x01, 0x40, 0x00, 0x02, 0x00};
  check("NimBLE completed body", filteredLength == sizeof(expectedLe) &&
    std::memcmp(filtered, expectedLe, sizeof(expectedLe)) == 0);

  const uint8_t malformedCompleted[] = {
    0x04, 0x13, 0x05, 0x02, 0x0b, 0x00, 0x01, 0x00};
  check("malformed completed route dropped", espble_hci_router_route_incoming(
    &router, malformedCompleted, sizeof(malformedCompleted)) ==
      ESPBLE_HCI_ROUTE_NONE);
  check("malformed completed filter rejected", espble_hci_router_packet_for_host(
    &router, ESPBLE_HCI_ROUTE_CLASSIC, malformedCompleted,
    sizeof(malformedCompleted), filtered, sizeof(filtered), &filteredLength) ==
      ESPBLE_HCI_ROUTER_INVALID_PACKET);

  const uint8_t disconnect[] = {0x04, 0x05, 0x04, 0x00, 0x40, 0x00, 0x13};
  check("disconnect routed before removal", espble_hci_router_route_incoming(
    &router, disconnect, sizeof(disconnect)) == ESPBLE_HCI_ROUTE_NIMBLE);
  check("disconnected handle removed", espble_hci_router_route_incoming(
    &router, leAcl, sizeof(leAcl)) == ESPBLE_HCI_ROUTE_NONE);

  const uint8_t malformed[] = {0x04, 0x3e, 0xff};
  check("malformed event dropped", espble_hci_router_route_incoming(
    &router, malformed, sizeof(malformed)) == ESPBLE_HCI_ROUTE_NONE);

  if (failures) return 1;
  std::puts("OK");
  return 0;
}
