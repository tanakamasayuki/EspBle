#!/usr/bin/env python3
"""Vendor the NimBLE host used for the original ESP32 into src/nimble_esp32/.

The original ESP32 is the only target whose Arduino-ESP32 prebuilt libraries are
built with Bluedroid, so EspBle brings its own NimBLE host for it. See
docs/PLAN_ESP32.ja.md for the reasoning and the delivery rules this script
implements:

* Sources come from espressif/esp-nimble at the exact commit that the matching
  esp-idf release pins, plus the ESP glue that lives in esp-idf itself. That is
  the same snapshot the prebuilt NimBLE of the other targets is built from.
* The file list and the include directories are transcribed from
  components/bt/host/nimble/CMakeLists.txt of that esp-idf release, resolved for
  the original ESP32 configuration (controller enabled, legacy VHCI transport,
  no ISO, no mesh, SOC_ESP_NIMBLE_CONTROLLER = 0, no blufi).
* Every vendored .c is wrapped in the original-ESP32 guard, so the other targets
  compile them as empty translation units and keep using the NimBLE bundled with
  the core.
* Vendored headers are isolated under src/nimble_esp32/include/ and every include
  that resolves into that tree is rewritten to the prefixed form. Without this
  the other targets pick up these headers instead of the core's.

Run it only when bumping the pinned versions:

    python3 tools/vendor_nimble_esp32.py

Downloads are cached under tools/cache/ (ignored by Git).
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import sys
import tarfile
import urllib.request
from pathlib import Path

# --- pinned upstream versions -------------------------------------------------

# esp-idf v5.5.5 pins this commit as components/bt/host/nimble/nimble.
ESP_NIMBLE_SHA = "685675c0128deafdd201c9eb82e61d227364646c"
ESP_IDF_TAG = "v5.5.5"
# The Arduino-ESP32 release whose prebuilt libraries were built from ESP_IDF_TAG.
# Its ESP32-S3 sdkconfig is the source of the frozen NimBLE configuration.
REFERENCE_CORE = "3.3.11"
REFERENCE_TARGET = "esp32s3"

REPO_ROOT = Path(__file__).resolve().parent.parent
CACHE_DIR = REPO_ROOT / "tools" / "cache"
OUT_DIR = REPO_ROOT / "src" / "nimble_esp32"
INCLUDE_PREFIX = "nimble_esp32/include/"

GUARD_OPEN = (
    "/* Vendored by tools/vendor_nimble_esp32.py -- do not edit. */\n"
    "#include <sdkconfig.h>\n"
    "#if defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_NIMBLE_ENABLED) && \\\n"
    "    !defined(ESPBLE_CLASSIC_ONLY)\n"
    '#include "' + INCLUDE_PREFIX + 'espble_nimble_config.h"\n'
)
GUARD_CLOSE = ("\n#endif /* CONFIG_IDF_TARGET_ESP32 && !CONFIG_NIMBLE_ENABLED && "
               "!ESPBLE_CLASSIC_ONLY */\n")

# --- file lists (transcribed from the esp-idf CMakeLists) ---------------------

# Paths inside the esp-nimble checkout. The CMakeLists refers to them as
# nimble/<path>, because the submodule sits at components/bt/host/nimble/nimble.
NIMBLE_SOURCES = [
    "nimble/transport/src/transport.c",
    "nimble/host/util/src/addr.c",
    "nimble/host/services/gatt/src/ble_svc_gatt.c",
    "nimble/host/services/tps/src/ble_svc_tps.c",
    "nimble/host/services/ias/src/ble_svc_ias.c",
    "nimble/host/services/ipss/src/ble_svc_ipss.c",
    "nimble/host/services/ans/src/ble_svc_ans.c",
    "nimble/host/services/hr/src/ble_svc_hr.c",
    "nimble/host/services/htp/src/ble_svc_htp.c",
    "nimble/host/services/gap/src/ble_svc_gap.c",
    "nimble/host/services/bas/src/ble_svc_bas.c",
    "nimble/host/services/dis/src/ble_svc_dis.c",
    "nimble/host/services/lls/src/ble_svc_lls.c",
    "nimble/host/services/prox/src/ble_svc_prox.c",
    "nimble/host/services/cts/src/ble_svc_cts.c",
    "nimble/host/services/hid/src/ble_svc_hid.c",
    "nimble/host/services/sps/src/ble_svc_sps.c",
    "nimble/host/services/cte/src/ble_svc_cte.c",
    "nimble/host/services/ras/src/ble_svc_ras.c",
    "nimble/host/src/ble_cs.c",
    "nimble/host/src/ble_hs_conn.c",
    "nimble/host/src/ble_store_util.c",
    "nimble/host/src/ble_sm.c",
    "nimble/host/src/ble_hs_shutdown.c",
    "nimble/host/src/ble_l2cap_sig_cmd.c",
    "nimble/host/src/ble_hs_hci_cmd.c",
    "nimble/host/src/ble_hs_id.c",
    "nimble/host/src/ble_att_svr.c",
    "nimble/host/src/ble_gatts_lcl.c",
    "nimble/host/src/ble_ibeacon.c",
    "nimble/host/src/ble_hs_atomic.c",
    "nimble/host/src/ble_sm_alg.c",
    "nimble/host/src/ble_hs_stop.c",
    "nimble/host/src/ble_hs.c",
    "nimble/host/src/ble_hs_hci_evt.c",
    "nimble/host/src/ble_hs_mqueue.c",
    "nimble/host/src/ble_hs_periodic_sync.c",
    "nimble/host/src/ble_att.c",
    "nimble/host/src/ble_ead.c",
    "nimble/host/src/ble_aes_ccm.c",
    "nimble/host/src/ble_gattc.c",
    "nimble/host/src/ble_store.c",
    "nimble/host/src/ble_sm_lgcy.c",
    "nimble/host/src/ble_hs_cfg.c",
    "nimble/host/src/ble_att_clt.c",
    "nimble/host/src/ble_l2cap_coc.c",
    "nimble/host/src/ble_hs_mbuf.c",
    "nimble/host/src/ble_att_cmd.c",
    "nimble/host/src/ble_hs_log.c",
    "nimble/host/src/ble_eddystone.c",
    "nimble/host/src/ble_hs_startup.c",
    "nimble/host/src/ble_l2cap_sig.c",
    "nimble/host/src/ble_gap.c",
    "nimble/host/src/ble_sm_cmd.c",
    "nimble/host/src/ble_uuid.c",
    "nimble/host/src/ble_hs_pvcy.c",
    "nimble/host/src/ble_hs_flow.c",
    "nimble/host/src/ble_l2cap.c",
    "nimble/host/src/ble_sm_sc.c",
    "nimble/host/src/ble_hs_misc.c",
    "nimble/host/src/ble_gatts.c",
    "nimble/host/src/ble_hs_adv.c",
    "nimble/host/src/ble_hs_hci.c",
    "nimble/host/src/ble_hs_hci_util.c",
    "nimble/host/src/ble_hs_resolv.c",
    "nimble/host/store/ram/src/ble_store_ram.c",
    "nimble/host/store/config/src/ble_store_config.c",
    "nimble/host/store/config/src/ble_store_nvs.c",
    "nimble/host/src/ble_gattc_cache.c",
    "nimble/host/src/ble_gattc_cache_conn.c",
    "nimble/host/src/ble_eatt.c",
    "porting/nimble/src/nimble_port.c",
    # BT_NIMBLE_LEGACY_VHCI_ENABLE: default y for IDF_TARGET_ESP32.
    "nimble/transport/esp_ipc_legacy/src/hci_esp_ipc_legacy.c",
    # not SOC_ESP_NIMBLE_CONTROLLER: the host brings its own porting layer.
    "porting/nimble/src/endian.c",
    "porting/nimble/src/mem.c",
    "porting/nimble/src/os_mbuf.c",
    "porting/nimble/src/os_msys_init.c",
    "porting/npl/freertos/src/npl_os_freertos.c",
    "porting/npl/freertos/src/nimble_port_freertos.c",
]

# Private headers that live next to the sources above.
NIMBLE_SOURCE_HEADER_DIRS = [
    "nimble/host/src",
    "nimble/host/store/config/src",
    "porting/npl/freertos/src",
]

# Include directories, in the order the CMakeLists appends them, tagged with the
# repository they come from. Earlier entries win when two directories provide the
# same relative path, which is what the compiler's include order does.
INCLUDE_DIRS = [
    ("nimble", "nimble/host/include"),
    ("nimble", "nimble/include"),
    ("nimble", "nimble/host/services/ans/include"),
    ("nimble", "nimble/host/services/bas/include"),
    ("nimble", "nimble/host/services/dis/include"),
    ("nimble", "nimble/host/services/gap/include"),
    ("nimble", "nimble/host/services/gatt/include"),
    ("nimble", "nimble/host/services/hr/include"),
    ("nimble", "nimble/host/services/htp/include"),
    ("nimble", "nimble/host/services/ias/include"),
    ("nimble", "nimble/host/services/ipss/include"),
    ("nimble", "nimble/host/services/lls/include"),
    ("nimble", "nimble/host/services/prox/include"),
    ("nimble", "nimble/host/services/cts/include"),
    ("nimble", "nimble/host/services/tps/include"),
    ("nimble", "nimble/host/services/hid/include"),
    ("nimble", "nimble/host/services/sps/include"),
    ("nimble", "nimble/host/services/cte/include"),
    ("nimble", "nimble/host/services/ras/include"),
    ("nimble", "nimble/host/util/include"),
    ("nimble", "nimble/host/store/ram/include"),
    ("nimble", "nimble/host/store/config/include"),
    ("nimble", "porting/nimble/include"),
    ("idf", "components/bt/host/nimble/port/include"),
    ("idf", "components/bt/host/nimble/esp-hci/include"),
    ("nimble", "nimble/transport/include"),
    ("idf", "components/bt/porting/include"),
    ("nimble", "porting/npl/freertos/include"),
]


# Adjustments for the Arduino prebuilt configuration, applied after copying.
# Each entry fails loudly when `old` is gone, so a version bump surfaces it
# instead of silently dropping the change. (file inside src/, old, new, why)
PATCHES = [
    (
        "porting/nimble/src/nimble_port.c",
        "    struct ble_npl_sem stop_sem;\n"
        "    struct ble_npl_event ev_stop;\n",
        "    struct ble_npl_sem stop_sem;\n"
        "    struct ble_npl_event ev_stop;\n"
        "    struct ble_npl_event ev_stop_begin;\n"
        "    int stop_result;\n",
        "store the host-task stop request in the restartable NimBLE context",
    ),
    (
        "porting/nimble/src/nimble_port.c",
        "#define ble_hs_ev_stop  (ble_npl_ctx->ev_stop)\n",
        "#define ble_hs_ev_stop  (ble_npl_ctx->ev_stop)\n"
        "#define ble_hs_ev_stop_begin (ble_npl_ctx->ev_stop_begin)\n"
        "#define ble_hs_stop_result (ble_npl_ctx->stop_result)\n",
        "expose the restartable host-task stop request",
    ),
    (
        "porting/nimble/src/nimble_port.c",
        "static struct ble_npl_sem ble_hs_stop_sem;\n"
        "static struct ble_npl_event ble_hs_ev_stop;\n",
        "static struct ble_npl_sem ble_hs_stop_sem;\n"
        "static struct ble_npl_event ble_hs_ev_stop;\n"
        "static struct ble_npl_event ble_hs_ev_stop_begin;\n"
        "static int ble_hs_stop_result;\n",
        "store the host-task stop request in the static NimBLE context",
    ),
    (
        "porting/nimble/src/nimble_port.c",
        "static void\n"
        "ble_hs_stop_cb(int status, void *arg)\n"
        "{\n"
        "    ble_npl_sem_release(&ble_hs_stop_sem);\n"
        "}\n\n"
        "static void\n"
        "nimble_port_stop_cb(struct ble_npl_event *ev)\n",
        "static void\n"
        "ble_hs_stop_cb(int status, void *arg)\n"
        "{\n"
        "    ble_hs_stop_result = status;\n"
        "    ble_npl_sem_release(&ble_hs_stop_sem);\n"
        "}\n\n"
        "static void\n"
        "nimble_port_stop_begin_cb(struct ble_npl_event *ev)\n"
        "{\n"
        "    (void)ev;\n"
        "    int result = ble_hs_stop(&stop_listener, ble_hs_stop_cb, NULL);\n"
        "    if (result != 0) {\n"
        "        ble_hs_stop_result = result;\n"
        "        ble_npl_sem_release(&ble_hs_stop_sem);\n"
        "    }\n"
        "}\n\n"
        "static void\n"
        "nimble_port_stop_cb(struct ble_npl_event *ev)\n",
        "start host shutdown on the NimBLE event task to serialize its queue",
    ),
    (
        "porting/nimble/src/nimble_port.c",
        "    /* Initiate a host stop procedure. */\n"
        "    err = ble_hs_stop(&stop_listener, ble_hs_stop_cb,\n"
        "                     NULL);\n"
        "    if (err != 0) {\n"
        "        ble_npl_sem_deinit(&ble_hs_stop_sem);\n"
        "        return err;\n"
        "    }\n\n"
        "    /* Wait till the host stop procedure is complete */\n"
        "    ble_npl_sem_pend(&ble_hs_stop_sem, BLE_NPL_TIME_FOREVER);\n",
        "    /* Serialize stop with callout events already dequeued by the host. */\n"
        "    ble_hs_stop_result = 0;\n"
        "    ble_npl_event_init(&ble_hs_ev_stop_begin,\n"
        "                       nimble_port_stop_begin_cb, NULL);\n"
        "    ble_npl_eventq_put(&g_eventq_dflt, &ble_hs_ev_stop_begin);\n\n"
        "    /* Wait till the host stop procedure is complete. */\n"
        "    ble_npl_sem_pend(&ble_hs_stop_sem, BLE_NPL_TIME_FOREVER);\n"
        "    err = ble_hs_stop_result;\n"
        "    if (err != 0) {\n"
        "        ble_npl_sem_deinit(&ble_hs_stop_sem);\n"
        "        return err;\n"
        "    }\n",
        "serialize NimBLE stop initiation with event queue consumption",
    ),
    (
        "porting/nimble/src/nimble_port.c",
        "    ble_npl_sem_pend(&ble_hs_stop_sem, BLE_NPL_TIME_FOREVER);\n\n"
        "    ble_npl_sem_deinit(&ble_hs_stop_sem);\n\n"
        "    return ESP_OK;\n",
        "    ble_npl_sem_pend(&ble_hs_stop_sem, BLE_NPL_TIME_FOREVER);\n\n"
        "    /* The stop marker ran after stop_begin, so this event is no longer\n"
        "     * referenced by the host task and can safely return to the pool. */\n"
        "    ble_npl_event_deinit(&ble_hs_ev_stop_begin);\n"
        "    ble_npl_sem_deinit(&ble_hs_stop_sem);\n\n"
        "    return ESP_OK;\n",
        "release the serialized stop request only after the host task exits",
    ),
    (
        "nimble/host/src/ble_hs.c",
        "#include <string.h>\n",
        "#include <string.h>\n#include \"EspBleHciBroker.h\"\n",
        "expose NimBLE receive readiness to the dual-host broker",
    ),
    (
        "nimble/host/src/ble_hs.c",
        "    if (rc != 0) {\n"
        "        return rc;\n"
        "    }\n\n"
        "    ble_hs_parent_task = ble_npl_get_current_task_id();\n",
        "    if (rc != 0) {\n"
        "        return rc;\n"
        "    }\n\n"
        "#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)\n"
        "    espble_hci_broker_set_receive_enabled(\n"
        "        ESPBLE_HCI_HOST_NIMBLE, true);\n"
        "#endif\n\n"
        "    ble_hs_parent_task = ble_npl_get_current_task_id();\n",
        "open NimBLE receive delivery only after the host enters ON state",
    ),
    (
        "nimble/host/src/ble_hs_stop.c",
        "#include <assert.h>\n",
        "#include <assert.h>\n#include \"EspBleHciBroker.h\"\n",
        "expose NimBLE stop completion to the dual-host broker",
    ),
    (
        "nimble/host/src/ble_hs_stop.c",
        "    ble_hs_unlock();\n\n"
        "    SLIST_FOREACH(listener, &slist, link) {\n",
        "    ble_hs_unlock();\n\n"
        "#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)\n"
        "    espble_hci_broker_set_receive_enabled(\n"
        "        ESPBLE_HCI_HOST_NIMBLE, false);\n"
        "#endif\n\n"
        "    SLIST_FOREACH(listener, &slist, link) {\n",
        "close NimBLE receive delivery as soon as the host reaches OFF state",
    ),
    (
        "esp-idf/esp_nimble_hci.c",
        "void bt_record_hci_data(uint8_t *data, uint16_t len)\n",
        "static void bt_record_hci_data(uint8_t *data, uint16_t len)\n",
        "keep NimBLE's HCI logging helper private when Bluedroid is linked too",
    ),
    (
        "porting/nimble/src/nimble_port.c",
        "#if CONFIG_IDF_TARGET_ESP32 && CONFIG_BT_CONTROLLER_ENABLED\n"
        "    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);\n"
        "#endif\n",
        "#if CONFIG_IDF_TARGET_ESP32 && CONFIG_BT_CONTROLLER_ENABLED && \\\n"
        "    !defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)\n"
        "    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);\n"
        "#endif\n",
        "retain Classic controller memory in the opt-in dual-host experiment",
    ),
    (
        "porting/nimble/src/nimble_port.c",
        "    esp_bt_controller_config_t config_opts = BT_CONTROLLER_INIT_CONFIG_DEFAULT();\n"
        "\n"
        "    ret = esp_bt_controller_init(&config_opts);\n",
        "    esp_bt_controller_config_t config_opts = BT_CONTROLLER_INIT_CONFIG_DEFAULT();\n"
        "#if CONFIG_IDF_TARGET_ESP32\n"
        "    /* EspBle: Arduino-ESP32 builds the prebuilt libraries with Bluedroid, so\n"
        "     * BT_CONTROLLER_INIT_CONFIG_DEFAULT() describes the dual-mode controller\n"
        "     * (CONFIG_BTDM_CTRL_MODE_BTDM). esp_bt_controller_enable(ESP_BT_MODE_BLE)\n"
        "     * below requires the enabled mode to match the initialised one, so select\n"
        "     * BLE here and size the controller for the host's connection count. An\n"
        "     * ESP-IDF build with NimBLE enabled gets both from its own sdkconfig. */\n"
        "    config_opts.mode = ESP_BT_MODE_BLE;\n"
        "    config_opts.ble_max_conn = CONFIG_BT_NIMBLE_MAX_CONNECTIONS;\n"
        "#endif\n"
        "\n"
        "    ret = esp_bt_controller_init(&config_opts);\n",
        "the prebuilt controller is configured for dual mode, but the host runs BLE only",
    ),
    (
        "porting/nimble/src/nimble_port.c",
        "#if CONFIG_BT_CONTROLLER_ENABLED\n"
        "    esp_bt_controller_config_t config_opts = BT_CONTROLLER_INIT_CONFIG_DEFAULT();\n",
        "#if CONFIG_BT_CONTROLLER_ENABLED\n"
        "#if defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)\n"
        "    /* Classic starts BTDM, then delegates its shutdown to the broker.\n"
        "     * NimBLE attaches only its host and HCI transport. */\n"
        "    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED) {\n"
        "        ESP_LOGE(NIMBLE_PORT_LOG_TAG, \"dual-host controller is not running\\n\");\n"
        "        return ESP_ERR_INVALID_STATE;\n"
        "    }\n"
        "#else\n"
        "    esp_bt_controller_config_t config_opts = BT_CONTROLLER_INIT_CONFIG_DEFAULT();\n",
        "attach NimBLE to the Classic-owned BTDM controller in dual-host mode",
    ),
    (
        "porting/nimble/src/nimble_port.c",
        "        return ret;\n"
        "    }\n"
        "#endif\n"
        "\n"
        "    ret = esp_nimble_init();\n",
        "        return ret;\n"
        "    }\n"
        "#endif\n"
        "#endif\n"
        "\n"
        "    ret = esp_nimble_init();\n",
        "close the dual-host controller-lifecycle branch",
    ),
    (
        "porting/nimble/src/nimble_port.c",
        "#if CONFIG_BT_CONTROLLER_ENABLED\n"
        "\t// Disable and deinit controller to free memory\n",
        "#if CONFIG_BT_CONTROLLER_ENABLED && \\\n"
        "    !defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)\n"
        "\t// Disable and deinit controller to free memory\n",
        "do not tear down a Classic-owned controller after NimBLE init failure",
    ),
    (
        "porting/nimble/src/nimble_port.c",
        "#if CONFIG_BT_CONTROLLER_ENABLED\n"
        "    ret = esp_bt_controller_disable();\n",
        "#if CONFIG_BT_CONTROLLER_ENABLED && \\\n"
        "    !defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)\n"
        "    ret = esp_bt_controller_disable();\n",
        "leave the shared controller running when the NimBLE host stops",
    ),
    (
        "nimble/host/src/ble_hs_startup.c",
        "    rc = ble_hs_startup_reset_tx();\n"
        "    if (rc != 0) {\n"
        "        return rc;\n"
        "    }\n",
        "#if !defined(ESPBLE_HCI_DUAL_HOST_EXPERIMENTAL)\n"
        "    rc = ble_hs_startup_reset_tx();\n"
        "    if (rc != 0) {\n"
        "        return rc;\n"
        "    }\n"
        "#endif\n",
        "do not reset the controller underneath an initialized Classic host",
    ),
    (
        "nimble/host/src/ble_sm.c",
        "        ble_hs_unlock();\n\n"
        "        if (proc == NULL) {\n"
        "            break;\n"
        "        }\n",
        "        ble_hs_unlock();\n\n"
        "        if (proc == NULL) {\n"
        "            /* An unsolicited controller encryption event has no SM\n"
        "             * procedure to retire, but it still changes GAP security\n"
        "             * state and must be reported to the application. */\n"
        "            if (res && res->enc_cb) {\n"
        "                if (res->app_status != BLE_HS_ENOTCONN) {\n"
        "                    ble_gap_pairing_complete_event(conn_handle, res->sm_err);\n"
        "                }\n"
        "                ble_gap_enc_event(conn_handle, res->app_status,\n"
        "                                  res->restore, res->bonded);\n"
        "            }\n"
        "            break;\n"
        "        }\n",
        "deliver successful controller encryption changes without an SM procedure",
    ),
    (
        "nimble/host/src/ble_sm.c",
        "    struct ble_sm_proc *proc;\n"
        "    int authenticated;\n"
        "    int bonded;\n"
        "    int key_size;\n\n"
        "    memset(&res, 0, sizeof res);\n\n"
        "    /* Assume no change in authenticated and bonded statuses. */\n"
        "    authenticated = 0;\n"
        "    bonded = 0;\n"
        "    key_size = 0;\n\n"
        "    ble_hs_lock();\n",
        "    struct ble_sm_proc *proc;\n"
        "    struct ble_store_value_sec stored_bond;\n"
        "    int authenticated;\n"
        "    int bonded;\n"
        "    int key_size;\n"
        "    int stored_bond_valid;\n\n"
        "    memset(&res, 0, sizeof res);\n\n"
        "    /* If no local procedure survives until Encryption Change, recover\n"
        "     * the established security properties from the peer's bond.  The\n"
        "     * lookup must run without the host mutex held. */\n"
        "    stored_bond_valid = evt_status == 0 && encrypted &&\n"
        "        ble_sm_read_bond(conn_handle, &stored_bond) == 0;\n\n"
        "    /* Assume no change in authenticated and bonded statuses. */\n"
        "    authenticated = 0;\n"
        "    bonded = 0;\n"
        "    key_size = 0;\n\n"
        "    ble_hs_lock();\n",
        "prepare stored bond metadata for an unsolicited encryption change",
    ),
    (
        "nimble/host/src/ble_sm.c",
        "            break;\n"
        "        }\n"
        "    }\n\n"
        "    if (evt_status == 0) {\n",
        "            break;\n"
        "        }\n"
        "    } else if (stored_bond_valid) {\n"
        "        authenticated = stored_bond.authenticated;\n"
        "        bonded = 1;\n"
        "        key_size = stored_bond.key_size;\n"
        "        res.restore = 1;\n"
        "    }\n\n"
        "    if (evt_status == 0) {\n",
        "restore bond flags when Encryption Change arrives without an SM procedure",
    ),
    (
        "esp-idf/esp_nimble_hci.c",
        '#include "esp_bt.h"\n#endif\n',
        '#include "esp_bt.h"\n#endif\n#include "EspBleHciBroker.h"\n',
        "route the vendored NimBLE host through EspBle's injectable HCI broker",
    ),
    (
        "esp-idf/esp_nimble_hci.c",
        "    esp_vhci_host_send_packet(data, len);\n",
        "    espble_hci_broker_send(ESPBLE_HCI_HOST_NIMBLE, data, len);\n",
        "route NimBLE HCI transmission through the broker",
    ),
    (
        "esp-idf/esp_nimble_hci.c",
        "esp_vhci_host_check_send_available()",
        "espble_hci_broker_can_send(ESPBLE_HCI_HOST_NIMBLE)",
        "route the first NimBLE VHCI availability check through the broker",
    ),
    (
        "esp-idf/esp_nimble_hci.c",
        "esp_vhci_host_check_send_available()",
        "espble_hci_broker_can_send(ESPBLE_HCI_HOST_NIMBLE)",
        "route the second NimBLE VHCI availability check through the broker",
    ),
    (
        "esp-idf/esp_nimble_hci.c",
        "static void dummy_controller_rcv_pkt_ready(void)\n"
        "{\n"
        "  /* Dummy function */\n"
        "}\n\n",
        "",
        "let the broker own the physical VHCI dummy callback",
    ),
    (
        "esp-idf/esp_nimble_hci.c",
        "static int dummy_host_rcv_pkt(uint8_t *data, uint16_t len)\n"
        "{\n"
        "    /* Dummy function */\n"
        "    return 0;\n"
        "}\n\n",
        "",
        "let the broker own the physical VHCI dummy receiver",
    ),
    (
        "esp-idf/esp_nimble_hci.c",
        "static const esp_vhci_host_callback_t vhci_host_cb = {\n"
        "    .notify_host_send_available = controller_rcv_pkt_ready,\n"
        "    .notify_host_recv = host_rcv_pkt,\n"
        "};\n\n"
        "static const esp_vhci_host_callback_t dummy_vhci_host_cb = {\n"
        "    .notify_host_send_available = dummy_controller_rcv_pkt_ready,\n"
        "    .notify_host_recv = dummy_host_rcv_pkt,\n"
        "};\n",
        "static const espble_hci_host_callbacks_t vhci_host_cb = {\n"
        "    .notify_send_available = controller_rcv_pkt_ready,\n"
        "    .notify_receive = host_rcv_pkt,\n"
        "};\n",
        "register NimBLE as a logical broker host instead of a physical VHCI host",
    ),
    (
        "esp-idf/esp_nimble_hci.c",
        "esp_vhci_host_register_callback(&vhci_host_cb)",
        "espble_hci_broker_register(ESPBLE_HCI_HOST_NIMBLE, &vhci_host_cb)",
        "attach the NimBLE callback to the broker",
    ),
    (
        "esp-idf/esp_nimble_hci.c",
        "    ble_transport_deinit();\n\n"
        "    esp_vhci_host_register_callback(&dummy_vhci_host_cb);\n",
        "    espble_hci_broker_unregister(ESPBLE_HCI_HOST_NIMBLE);\n\n"
        "    ble_transport_deinit();\n",
        "detach NimBLE from the broker before disabling its receive transport",
    ),
]

# esp-idf files, relative to the repository root of esp-idf.
IDF_SOURCES = [
    "components/bt/host/nimble/esp-hci/src/esp_nimble_hci.c",
    "components/bt/host/nimble/port/src/nvs_port.c",
    "components/bt/host/nimble/port/src/esp_nimble_mem.c",
    "components/bt/porting/mem/os_mempool.c",
]

# esp-idf headers that are not part of an include directory above. The Arduino
# prebuilt include tree of the original ESP32 does not export this one.
IDF_HEADERS = [
    ("components/bt/common/include/bt_common.h", "bt_common.h"),
    ("components/bt/common/include/bt_user_config.h", "bt_user_config.h"),
]

RAW_IDF = "https://raw.githubusercontent.com/espressif/esp-idf/" + ESP_IDF_TAG + "/"
NIMBLE_TARBALL_URL = (
    "https://codeload.github.com/espressif/esp-nimble/tar.gz/" + ESP_NIMBLE_SHA
)

# --- helpers ------------------------------------------------------------------


def fetch(url: str, dest: Path) -> Path:
    if dest.exists():
        return dest
    dest.parent.mkdir(parents=True, exist_ok=True)
    print("fetch", url)
    with urllib.request.urlopen(url, timeout=120) as response:
        data = response.read()
    dest.write_bytes(data)
    return dest


def nimble_checkout() -> Path:
    tarball = fetch(
        NIMBLE_TARBALL_URL, CACHE_DIR / ("esp-nimble-" + ESP_NIMBLE_SHA + ".tar.gz")
    )
    root = CACHE_DIR / ("esp-nimble-" + ESP_NIMBLE_SHA)
    if not root.exists():
        print("extract", tarball.name)
        tmp = CACHE_DIR / ("extract-" + ESP_NIMBLE_SHA)
        if tmp.exists():
            shutil.rmtree(tmp)
        with tarfile.open(tarball) as archive:
            archive.extractall(tmp)
        inner = next(tmp.iterdir())
        inner.rename(root)
        shutil.rmtree(tmp)
    return root


def idf_file(path: str) -> Path:
    return fetch(RAW_IDF + path, CACHE_DIR / ("esp-idf-" + ESP_IDF_TAG) / path)


# --- configuration header ------------------------------------------------------


def parse_sdkconfig(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text().splitlines():
        match = re.match(r"^(CONFIG_[A-Za-z0-9_]+)=(.*)$", line)
        if not match:
            continue
        name, raw = match.group(1), match.group(2).strip()
        if raw == "y":
            values[name] = "1"
        elif raw == "n":
            continue
        else:
            values[name] = raw
    return values


def write_config_header(reference: Path, target: Path, out: Path) -> int:
    """Freeze the NimBLE host configuration to the reference target's values."""
    wanted = parse_sdkconfig(reference)
    already = parse_sdkconfig(target)
    lines = [
        "/* Generated by tools/vendor_nimble_esp32.py -- do not edit.",
        " *",
        " * The NimBLE host configuration for the original ESP32, frozen to the",
        " * values Arduino-ESP32 " + REFERENCE_CORE + " uses for " + REFERENCE_TARGET + ".",
        " * Keeping them identical is what lets the original ESP32 behave like the",
        " * other targets; see docs/PLAN_ESP32.ja.md. Overriding any of them would",
        " * change the vendored headers without changing this file's assumptions,",
        " * so an override is rejected instead of silently accepted.",
        " */",
        "",
        "#ifndef ESPBLE_NIMBLE_CONFIG_H",
        "#define ESPBLE_NIMBLE_CONFIG_H",
        "",
        "#include <sdkconfig.h>",
        "",
    ]
    count = 0
    for name, value in sorted(wanted.items()):
        if not name.startswith("CONFIG_BT_NIMBLE_"):
            continue
        if name in already:
            continue
        lines += [
            "#ifdef " + name,
            '#error "' + name + ' is fixed by EspBle for the original ESP32 and cannot be overridden"',
            "#endif",
            "#define " + name + " " + value,
            "",
        ]
        count += 1
    lines += ["#endif /* ESPBLE_NIMBLE_CONFIG_H */", ""]
    out.write_text("\n".join(lines))
    return count


# --- include rewriting ---------------------------------------------------------

INCLUDE_RE = re.compile(r'^([ \t]*#[ \t]*include[ \t]*)([<"])([^">]+)([>"])(.*)$', re.M)


def rewrite_includes(path: Path, include_root: Path, source_root: Path) -> int:
    """Point includes that resolve inside the vendored tree at the vendored copy.

    Upstream is built with the include directories of the CMakeLists on the
    command line, so a few files reach private headers through them (for example
    "../src/ble_hs_hci_priv.h", which resolves against nimble/host/include).
    Arduino only puts <lib>/src on the include path, so those have to become paths
    that resolve without extra -I flags.
    """
    text = path.read_text(errors="surrogateescape")
    changes = 0

    def replace(match: re.Match[str]) -> str:
        nonlocal changes
        head, opener, target, closer, tail = match.groups()
        if target.startswith(INCLUDE_PREFIX):
            return match.group(0)
        if (path.parent / target).exists():
            return match.group(0)  # same-directory (private) header
        if (include_root / target).exists():
            changes += 1
            return head + '"' + INCLUDE_PREFIX + target + '"' + tail
        for repo, include_dir in INCLUDE_DIRS:
            if repo != "nimble":
                continue
            candidate = (source_root / include_dir / target).resolve()
            if candidate.is_file() and source_root.resolve() in candidate.parents:
                changes += 1
                relative = os.path.relpath(candidate, path.parent.resolve())
                return head + '"' + Path(relative).as_posix() + '"' + tail
        return match.group(0)  # IDF, toolchain or system header

    new_text = INCLUDE_RE.sub(replace, text)
    if changes:
        path.write_text(new_text, errors="surrogateescape")
    return changes


def guard_source(path: Path) -> None:
    text = path.read_text(errors="surrogateescape")
    path.write_text(GUARD_OPEN + text + GUARD_CLOSE, errors="surrogateescape")


# --- main ---------------------------------------------------------------------


def idf_tree_headers(dir_path: str) -> list[str]:
    """List the header files under an esp-idf directory, recursively."""
    url = (
        "https://api.github.com/repos/espressif/esp-idf/contents/"
        + dir_path
        + "?ref="
        + ESP_IDF_TAG
    )
    with urllib.request.urlopen(url, timeout=120) as response:
        entries = json.load(response)
    found: list[str] = []
    for entry in entries:
        if entry["type"] == "dir":
            found += idf_tree_headers(entry["path"])
        elif entry["name"].endswith((".h", ".hpp")):
            found.append(entry["path"])
    return found


def copy_idf_include_dir(dir_path: str, dest: Path, owners: dict[str, str]) -> None:
    for path in sorted(idf_tree_headers(dir_path)):
        rel = path[len(dir_path) + 1 :]
        if rel in owners:
            print("  keep", rel, "from", owners[rel], "(skipping", dir_path + ")")
            continue
        owners[rel] = dir_path
        out = dest / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(idf_file(path), out)


def copy_include_dir(src: Path, dest: Path, owners: dict[str, str], label: str) -> None:
    for item in sorted(src.rglob("*")):
        if not item.is_file() or item.suffix not in (".h", ".hpp"):
            continue
        rel = item.relative_to(src).as_posix()
        if rel in owners:
            print("  keep", rel, "from", owners[rel], "(skipping", label + ")")
            continue
        owners[rel] = label
        out = dest / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(item, out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--core-libs",
        type=Path,
        default=Path.home() / ".arduino15/packages/esp32/tools",
        help="directory holding <target>-libs/<core>/sdkconfig of the reference core",
    )
    args = parser.parse_args()

    reference_sdkconfig = (
        args.core_libs / (REFERENCE_TARGET + "-libs") / REFERENCE_CORE / "sdkconfig"
    )
    target_sdkconfig = args.core_libs / "esp32-libs" / REFERENCE_CORE / "sdkconfig"
    for path in (reference_sdkconfig, target_sdkconfig):
        if not path.exists():
            print("missing sdkconfig:", path, file=sys.stderr)
            print(
                "install Arduino-ESP32 " + REFERENCE_CORE + " or pass --core-libs",
                file=sys.stderr,
            )
            return 1

    nimble = nimble_checkout()

    if OUT_DIR.exists():
        shutil.rmtree(OUT_DIR)
    include_dir = OUT_DIR / "include"
    source_dir = OUT_DIR / "src"
    include_dir.mkdir(parents=True)
    source_dir.mkdir(parents=True)

    print("headers")
    owners: dict[str, str] = {}
    for repo, rel in INCLUDE_DIRS:
        if repo == "nimble":
            src = nimble / rel
            if not src.is_dir():
                print("missing include dir:", src, file=sys.stderr)
                return 1
            copy_include_dir(src, include_dir, owners, rel)
        else:
            copy_idf_include_dir(rel, include_dir, owners)
    for path, rel in IDF_HEADERS:
        if rel in owners:
            print("  keep", rel, "from", owners[rel], "(skipping esp-idf)")
            continue
        owners[rel] = "esp-idf"
        out = include_dir / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(idf_file(path), out)

    print("sources")
    for rel in NIMBLE_SOURCES:
        src = nimble / rel
        if not src.is_file():
            print("missing source:", src, file=sys.stderr)
            return 1
        out = source_dir / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, out)
    for rel in NIMBLE_SOURCE_HEADER_DIRS:
        for item in sorted((nimble / rel).glob("*.h")):
            out = source_dir / rel / item.name
            out.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(item, out)
    for path in IDF_SOURCES:
        out = source_dir / "esp-idf" / Path(path).name
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(idf_file(path), out)

    print("patches")
    for rel, old, new, why in PATCHES:
        target = source_dir / rel
        text = target.read_text(errors="surrogateescape")
        if old not in text:
            print("patch no longer applies:", rel, "--", why, file=sys.stderr)
            return 1
        target.write_text(text.replace(old, new, 1), errors="surrogateescape")
        print("  patched", rel, "--", why)

    print("configuration")
    frozen = write_config_header(
        reference_sdkconfig, target_sdkconfig, include_dir / "espble_nimble_config.h"
    )

    print("rewrite includes")
    rewritten = 0
    for path in sorted(list(include_dir.rglob("*.h")) + list(source_dir.rglob("*"))):
        if path.is_file() and path.suffix in (".h", ".c"):
            rewritten += rewrite_includes(path, include_dir, source_dir)

    print("guard sources")
    guarded = 0
    for path in sorted(source_dir.rglob("*.c")):
        guard_source(path)
        guarded += 1

    print("license and version records")
    shutil.copyfile(nimble / "LICENSE", OUT_DIR / "LICENSE")
    shutil.copyfile(nimble / "NOTICE", OUT_DIR / "NOTICE")
    (OUT_DIR / "VERSIONS").write_text(
        "\n".join(
            [
                "# Generated by tools/vendor_nimble_esp32.py -- do not edit.",
                "# The NimBLE host vendored for the original ESP32.",
                "esp-nimble: " + ESP_NIMBLE_SHA,
                "esp-idf: " + ESP_IDF_TAG,
                "arduino-esp32: " + REFERENCE_CORE,
                "config-source: " + REFERENCE_TARGET + " sdkconfig of arduino-esp32 " + REFERENCE_CORE,
                "",
            ]
        )
    )

    print(
        "done: %d headers, %d sources guarded, %d includes rewritten, %d config values frozen"
        % (len(owners), guarded, rewritten, frozen)
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
