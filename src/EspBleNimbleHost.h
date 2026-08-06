#ifndef ESP_BLE_NIMBLE_HOST_H
#define ESP_BLE_NIMBLE_HOST_H

// en: Single point where EspBle picks up the NimBLE host API. Every target uses
//     the NimBLE bundled with Arduino-ESP32 through the plain include paths; the
//     original ESP32 is the exception, because its prebuilt libraries are built
//     with Bluedroid and EspBle bundles its own NimBLE host under
//     src/nimble_esp32/ (see docs/PLAN_ESP32.ja.md).
//     The bundled headers must stay under nimble_esp32/include/ and be reached
//     through the prefixed paths below: <lib>/src is on the include path for
//     every target, so headers placed directly under src/ would shadow the
//     core's own NimBLE headers on the other chips.
// ja: EspBleがNimBLEホストAPIを取り込む唯一の場所。通常はArduino-ESP32同梱の
//     NimBLEを素のincludeパスで使い、無印ESP32だけはプリビルドがBluedroidのため
//     src/nimble_esp32/へ同梱したNimBLEホストを使う（docs/PLAN_ESP32.ja.md）。
//     同梱ヘッダはnimble_esp32/include/配下に置き、下記のプレフィックス付きパスで
//     参照する——`<lib>/src`は全ターゲットでinclude pathに入るため、src/直下へ
//     置くと他チップでcore同梱のNimBLEヘッダを覆ってしまう。
#include <sdkconfig.h>

#if defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_NIMBLE_ENABLED)

#include "nimble_esp32/include/espble_nimble_config.h"

#include "nimble_esp32/include/host/ble_gap.h"
#include "nimble_esp32/include/host/ble_hs.h"
#include "nimble_esp32/include/host/ble_hs_id.h"
#include "nimble_esp32/include/host/ble_hs_mbuf.h"
#include "nimble_esp32/include/host/ble_sm.h"
#include "nimble_esp32/include/host/ble_store.h"
#include "nimble_esp32/include/host/ble_uuid.h"
#include "nimble_esp32/include/host/util/util.h"
#include "nimble_esp32/include/nimble/nimble_port.h"
#include "nimble_esp32/include/nimble/nimble_port_freertos.h"
#include "nimble_esp32/include/os/os_mbuf.h"
#include "nimble_esp32/include/services/gap/ble_svc_gap.h"
#include "nimble_esp32/include/services/gatt/ble_svc_gatt.h"

#else

#include <host/ble_gap.h>
#include <host/ble_hs.h>
#include <host/ble_hs_id.h>
#include <host/ble_hs_mbuf.h>
#include <host/ble_sm.h>
#include <host/ble_store.h>
#include <host/ble_uuid.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <os/os_mbuf.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>

#endif

#endif
