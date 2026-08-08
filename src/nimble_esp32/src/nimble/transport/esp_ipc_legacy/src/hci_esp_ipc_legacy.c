/* Vendored by tools/vendor_nimble_esp32.py -- do not edit. */
#include <sdkconfig.h>
#if defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_NIMBLE_ENABLED) && \
    !defined(ESPBLE_CLASSIC_ONLY)
#include "nimble_esp32/include/espble_nimble_config.h"
/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include "nimble_esp32/include/sysinit/sysinit.h"
#include "nimble_esp32/include/syscfg/syscfg.h"
#include "nimble_esp32/include/nimble/transport.h"
#include "nimble_esp32/include/esp_nimble_hci.h"

/* This file is only used by ESP32, ESP32C3 and ESP32S3. */
int
ble_transport_to_ll_cmd_impl(void *buf)
{
    return ble_hci_trans_hs_cmd_tx(buf);
}

int
ble_transport_to_ll_acl_impl(struct os_mbuf *om)
{
    return ble_hci_trans_hs_acl_tx(om);
}

void
ble_transport_ll_init(void)
{

}

void
ble_transport_ll_deinit(void)
{

}


#endif /* CONFIG_IDF_TARGET_ESP32 && !CONFIG_NIMBLE_ENABLED && !ESPBLE_CLASSIC_ONLY */
