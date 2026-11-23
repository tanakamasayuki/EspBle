/* Vendored by tools/vendor_nimble_esp32.py -- do not edit. */
#include <sdkconfig.h>
#if defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_NIMBLE_ENABLED) && \
    !defined(ESPBLE_CLASSIC_ONLY)
#include "nimble_esp32/include/espble_nimble_config.h"
/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <stddef.h>
#include <stdbool.h>
#include "nimble_esp32/include/os/os.h"
#include "nimble_esp32/include/sysinit/sysinit.h"
#include "nimble_esp32/include/esp_nimble_cfg.h"

#if CONFIG_BT_NIMBLE_ENABLED
#include "nimble_esp32/include/host/ble_hs.h"
#endif //CONFIG_BT_NIMBLE_ENABLED

#include "nimble_esp32/include/nimble/nimble_port.h"
#include "nimble_esp32/include/nimble/nimble_port_freertos.h"
#ifdef ESP_PLATFORM
#include "esp_log.h"
#endif
#include "soc/soc_caps.h"

#include "esp_intr_alloc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if CONFIG_BT_CONTROLLER_ENABLED
#include "esp_bt.h"
/* The broker owns the shared controller once either host starts it, and
 * answers whether a Classic host is linked at all. */
#include "EspBleHciBroker.h"
#endif
#if !SOC_ESP_NIMBLE_CONTROLLER && CONFIG_BT_CONTROLLER_ENABLED
#include "nimble_esp32/include/esp_nimble_hci.h"
#endif
#if !CONFIG_BT_CONTROLLER_ENABLED
#include "nimble_esp32/include/nimble/transport.h"
#endif
#include "nimble_esp32/include/bt_common.h"
#if (BT_HCI_LOG_INCLUDED == TRUE)
#include "hci_log/bt_hci_log.h"
#endif
/* bt_osal and the shared profile task are off in every Arduino-ESP32
 * original-ESP32 build (CONFIG_BT_OSAL_ENABLED, CONFIG_BT_PRF_TASK_ENABLED
 * unset) and this host uses neither; their headers exist only from
 * Arduino-ESP32 3.3.13 on, so they are left out. */
#if CONFIG_BLE_LOG_ENABLED && !CONFIG_BT_CONTROLLER_ENABLED
#include "ble_log.h"
#endif

#define NIMBLE_PORT_LOG_TAG          "BLE_INIT"

extern void os_msys_init(void);

#if CONFIG_BT_NIMBLE_ENABLED 
extern void ble_hs_deinit(void);
#endif

#if MYNEWT_VAL(BLE_STATIC_TO_DYNAMIC)
#include "nimble_esp32/include/esp_nimble_mem.h"
typedef struct {
    struct ble_npl_eventq eventq;
    struct ble_npl_sem stop_sem;
    struct ble_npl_event ev_stop;
    struct ble_npl_event ev_stop_begin;
    int stop_result;
#if CONFIG_BT_NIMBLE_ENABLED
    struct ble_hs_stop_listener listener;
#endif
} ble_npl_ctx_t;

static ble_npl_ctx_t *ble_npl_ctx;

static bool ble_npl_deiniting = false;
static struct ble_npl_eventq g_eventq_shutdown_fallback = {0};

#define g_eventq_dflt   (ble_npl_ctx->eventq)
#define ble_hs_stop_sem (ble_npl_ctx->stop_sem)
#define ble_hs_ev_stop  (ble_npl_ctx->ev_stop)
#define ble_hs_ev_stop_begin (ble_npl_ctx->ev_stop_begin)
#define ble_hs_stop_result (ble_npl_ctx->stop_result)

#if CONFIG_BT_NIMBLE_ENABLED
#define stop_listener   (ble_npl_ctx->listener)
#endif

#else
static struct ble_npl_eventq g_eventq_dflt;
static struct ble_npl_sem ble_hs_stop_sem;
static struct ble_npl_event ble_hs_ev_stop;
static struct ble_npl_event ble_hs_ev_stop_begin;
static int ble_hs_stop_result;

#if CONFIG_BT_NIMBLE_ENABLED
static struct ble_hs_stop_listener stop_listener;
#endif //CONFIG_BT_NIMBLE_ENABLED

#endif

static bool nimble_port_run_active;

extern void os_msys_init(void);
extern void os_mempool_module_init(void);

#if MYNEWT_VAL(BLE_STATIC_TO_DYNAMIC)
static int
ble_npl_ensure_ctx(void)
{
    if (ble_npl_ctx != NULL) {
        return 0;
    }

    /* Don't allocate during shutdown - context was already freed */
    if (ble_npl_deiniting) {
        return BLE_HS_ENOENT;
    }

    ble_npl_ctx = nimble_platform_mem_calloc(1, sizeof(*ble_npl_ctx));
    if (ble_npl_ctx == NULL) {
        return BLE_HS_ENOMEM;
    }

    return 0;
}

/* Reset the deiniting flag - called at start of new init cycle */
static void
ble_npl_reset_deinit_flag(void)
{
    ble_npl_deiniting = false;
}
#endif

/**
 * Called when the host stop procedure has completed.
 */
static void
ble_hs_stop_cb(int status, void *arg)
{
    ble_hs_stop_result = status;
    ble_npl_sem_release(&ble_hs_stop_sem);
}

static void
nimble_port_stop_begin_cb(struct ble_npl_event *ev)
{
    (void)ev;
    int result = ble_hs_stop(&stop_listener, ble_hs_stop_cb, NULL);
    if (result != 0) {
        ble_hs_stop_result = result;
        ble_npl_sem_release(&ble_hs_stop_sem);
    }
}

static void
nimble_port_stop_cb(struct ble_npl_event *ev)
{
    (void)ev;
}

/**
 * @brief esp_nimble_init - Initialize the NimBLE host stack
 * 
 * @return esp_err_t 
 */
esp_err_t esp_nimble_init(void)
{
#if MYNEWT_VAL(BLE_STATIC_TO_DYNAMIC)
    /* Reset deinit flag at start of new init cycle */
    ble_npl_reset_deinit_flag();
    
    if (ble_npl_ensure_ctx()) {
        return ESP_FAIL;
    }
#endif

#if !SOC_ESP_NIMBLE_CONTROLLER || !CONFIG_BT_CONTROLLER_ENABLED
    /* Initialize the global memory pool */
    os_mempool_module_init();

    /* Initialize the function pointers for OS porting */
    npl_freertos_funcs_init();

    if (npl_freertos_mempool_init() != 0) {
        ESP_LOGE(NIMBLE_PORT_LOG_TAG, "npl mempool init failed\n");
        npl_freertos_funcs_deinit();
#if MYNEWT_VAL(BLE_STATIC_TO_DYNAMIC)
        if (ble_npl_ctx) {
            nimble_platform_mem_free(ble_npl_ctx);
            ble_npl_ctx = NULL;
        }
#endif
        return ESP_FAIL;
    }

#if CONFIG_BT_CONTROLLER_ENABLED
    if(esp_nimble_hci_init() != ESP_OK) {
        ESP_LOGE(NIMBLE_PORT_LOG_TAG, "hci inits failed\n");
        return ESP_FAIL;
    }
#else
    esp_err_t ret;
    ret = ble_buf_alloc();
    if (ret != ESP_OK) {
        ble_buf_free();
        return ESP_FAIL;
    }
    ble_transport_init();
#endif

    /* Initialize default event queue */
    ble_npl_eventq_init(&g_eventq_dflt);
    os_msys_init();
#else
    /* Initialize default event queue */
    ble_npl_eventq_init(&g_eventq_dflt);
#endif // !SOC_ESP_NIMBLE_CONTROLLER || !CONFIG_BT_CONTROLLER_ENABLED


#if CONFIG_BT_PRF_TASK_ENABLED
#error "the vendored NimBLE host does not support CONFIG_BT_PRF_TASK_ENABLED"
    /* Start the shared BLE profile task now that the bt_osal table is up, so
     * profiles can post work without spawning their own tasks. */
    if (bt_prf_task_init() != BT_OSAL_OK) {
        ESP_LOGW(NIMBLE_PORT_LOG_TAG, "bt_prf_task_init failed");
    }
#endif

#if MYNEWT_VAL(BLE_QUEUE_CONG_CHECK)
    ble_adv_list_init();
#endif

    /* Initialize the host */
    ble_transport_hs_init();
    
    ble_transport_ll_init();

#if (BT_HCI_LOG_INCLUDED == TRUE)
    if (bt_hci_log_init() != ESP_OK) {
        ESP_LOGW(NIMBLE_PORT_LOG_TAG, "bt_hci_log_init failed");
    }
#endif

#if CONFIG_BLE_LOG_ENABLED && !CONFIG_BT_CONTROLLER_ENABLED
    if (!ble_log_init()) {
        ESP_LOGW(NIMBLE_PORT_LOG_TAG, "BLE Log init failed");
    }
#endif

    return ESP_OK;
}

/**
 * @brief esp_nimble_deinit - Deinitialize the NimBLE host stack
 * 
 * @return esp_err_t 
 */
esp_err_t esp_nimble_deinit(void)
{
#if MYNEWT_VAL(BLE_STATIC_TO_DYNAMIC)
    if (ble_npl_ctx == NULL) {
        return ESP_OK;
    }
    ble_npl_deiniting = true;
#endif

#if CONFIG_BLE_LOG_ENABLED && !CONFIG_BT_CONTROLLER_ENABLED
    ble_log_deinit();
#endif

    ble_transport_ll_deinit();

#if (BT_HCI_LOG_INCLUDED == TRUE)
    bt_hci_log_deinit();
#endif

#if MYNEWT_VAL(BLE_QUEUE_CONG_CHECK)
    ble_adv_list_deinit();
#endif

#if !SOC_ESP_NIMBLE_CONTROLLER || !CONFIG_BT_CONTROLLER_ENABLED
#if CONFIG_BT_CONTROLLER_ENABLED
    if(esp_nimble_hci_deinit() != ESP_OK) {
        ESP_LOGE(NIMBLE_PORT_LOG_TAG, "hci deinit failed\n");
#if MYNEWT_VAL(BLE_STATIC_TO_DYNAMIC)
        ble_npl_deiniting = false;  /* Reset flag before early return */
#endif
        return ESP_FAIL;
    }
#else
    ble_transport_deinit();
    ble_buf_free();
#endif

#endif

    ble_hs_deinit();

    ble_npl_eventq_deinit(&g_eventq_dflt);

#if !SOC_ESP_NIMBLE_CONTROLLER || !CONFIG_BT_CONTROLLER_ENABLED
    npl_freertos_funcs_deinit();
#endif

#if !SOC_ESP_NIMBLE_CONTROLLER || !CONFIG_BT_CONTROLLER_ENABLED
    npl_freertos_mempool_deinit();
#endif

#if MYNEWT_VAL(BLE_STATIC_TO_DYNAMIC)
    if (ble_npl_ctx) {
        nimble_platform_mem_free(ble_npl_ctx);
        ble_npl_ctx = NULL;
    }
    /* Keep ble_npl_deiniting = true until nimble_port_deinit completes.
     * This prevents controller deinit from re-allocating ble_npl_ctx when it
     * calls nimble_port_get_dflt_eventq(). The flag will be reset at the START
     * of the next init cycle (in ble_npl_reset_deinit_flag). */

#endif

#if CONFIG_BT_PRF_TASK_ENABLED
    /* Stop the shared BLE profile task while the bt_osal table is still up,
     * symmetrically with the init in esp_nimble_init(). */
    bt_prf_task_deinit();
#endif


#if MYNEWT_VAL(MP_RUNTIME_ALLOC)
    os_mempool_deinit_all(false);
#endif

    return ESP_OK;
}

/**
 * @brief nimble_port_stop_controller - Stop the controller this host started
 *
 * Registered with the broker so the controller outlives a NimBLE-only stop
 * whenever a Classic host is still attached.
 *
 * @return bool
 */
#if CONFIG_BT_CONTROLLER_ENABLED
static bool
nimble_port_stop_controller(void)
{
    if (esp_bt_controller_disable() != ESP_OK) {
        ESP_LOGE(NIMBLE_PORT_LOG_TAG, "controller disable failed\n");
        return false;
    }

    if (esp_bt_controller_deinit() != ESP_OK) {
        ESP_LOGE(NIMBLE_PORT_LOG_TAG, "controller deinit failed\n");
        return false;
    }

    return true;
}
#endif

/**
 * @brief nimble_port_init - Initialize controller and NimBLE host stack
 *
 * @return esp_err_t
 */
esp_err_t
nimble_port_init(void)
{
    esp_err_t ret;

#if MYNEWT_VAL(BLE_STATIC_TO_DYNAMIC)
    /* Reset deinit flag at start of new init cycle.
     * This is needed because the flag remains true after deinit to prevent
     * re-allocation during controller shutdown. */
    ble_npl_reset_deinit_flag();
#endif

#if CONFIG_IDF_TARGET_ESP32 && CONFIG_BT_CONTROLLER_ENABLED
    /* Releasing Classic controller memory cannot be undone for the rest of the
     * boot, so keep it whenever a Classic host is linked into the sketch. */
    if (!espble_hci_broker_classic_host_expected()) {
        esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    }
#endif
#if CONFIG_BT_CONTROLLER_ENABLED
    if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
        /* The Classic host started the controller and handed its shutdown to
         * the broker.  Attach only the NimBLE host and HCI transport. */
        if (!espble_hci_broker_has_adopted_controller()) {
            ESP_LOGE(NIMBLE_PORT_LOG_TAG, "another stack owns the controller\n");
            return ESP_ERR_INVALID_STATE;
        }
    } else {
    esp_bt_controller_config_t config_opts = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
#if CONFIG_IDF_TARGET_ESP32
    /* EspBle: Arduino-ESP32 builds the prebuilt libraries with Bluedroid, so
     * BT_CONTROLLER_INIT_CONFIG_DEFAULT() describes the dual-mode controller
     * (CONFIG_BTDM_CTRL_MODE_BTDM). esp_bt_controller_enable() below requires
     * the enabled mode to match the initialised one, so select the mode here
     * and size the controller for the host's connection count.  A linked
     * Classic host needs BR/EDR too, so start the dual-mode controller on its
     * behalf; otherwise BLE alone keeps the radio configuration minimal.  An
     * ESP-IDF build with NimBLE enabled gets both from its own sdkconfig. */
    config_opts.mode = espble_hci_broker_classic_host_expected() ?
      ESP_BT_MODE_BTDM : ESP_BT_MODE_BLE;
    config_opts.ble_max_conn = CONFIG_BT_NIMBLE_MAX_CONNECTIONS;
#endif

    ret = esp_bt_controller_init(&config_opts);
    if (ret != ESP_OK) {
        ESP_LOGE(NIMBLE_PORT_LOG_TAG, "controller init failed\n");
        return ret;
    }

#if CONFIG_IDF_TARGET_ESP32
    ret = esp_bt_controller_enable(config_opts.mode);
#else
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
#endif
    if (ret != ESP_OK) {
        // Deinit to free any memory the controller is using.
        if(esp_bt_controller_deinit() != ESP_OK) {
            ESP_LOGE(NIMBLE_PORT_LOG_TAG, "controller deinit failed\n");
        }

        ESP_LOGE(NIMBLE_PORT_LOG_TAG, "controller enable failed\n");
        return ret;
    }

    /* The broker stops the controller only after the last logical host leaves,
     * so a Classic host attaching later keeps this controller running. */
    if (espble_hci_broker_adopt_controller(nimble_port_stop_controller) != ESP_OK) {
        ESP_LOGE(NIMBLE_PORT_LOG_TAG, "controller ownership transfer failed\n");
        (void)nimble_port_stop_controller();
        return ESP_ERR_INVALID_STATE;
    }
    }
#endif

    ret = esp_nimble_init();
    if (ret != ESP_OK) {

#if CONFIG_BT_CONTROLLER_ENABLED
	// Free the controller memory unless another host still uses it.
        if(espble_hci_broker_shutdown_controller() == ESP_ERR_INVALID_STATE) {
            ESP_LOGD(NIMBLE_PORT_LOG_TAG, "controller kept for the other host\n");
        }
#endif

	ESP_LOGE(NIMBLE_PORT_LOG_TAG, "nimble host init failed\n");
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief nimble_port_deinit - Deinitialize controller and NimBLE host stack
 *
 * @return esp_err_t
 */

esp_err_t
nimble_port_deinit(void)
{
    esp_err_t ret;

    ret = esp_nimble_deinit();
    if(ret != ESP_OK) {
        ESP_LOGE(NIMBLE_PORT_LOG_TAG, "nimble host deinit failed\n");
        return ret;
    }

#if CONFIG_BT_CONTROLLER_ENABLED
    /* The broker runs the stop callback registered at init, but only once the
     * last logical host has left.  A still-attached Classic host keeps it. */
    ret = espble_hci_broker_shutdown_controller();
    if(ret == ESP_ERR_INVALID_STATE) {
        ret = ESP_OK;
    }
    if(ret != ESP_OK) {
        ESP_LOGE(NIMBLE_PORT_LOG_TAG, "controller shutdown failed\n");
        return ret;
    }
#endif

    return ESP_OK;
}


int
nimble_port_stop(void)
{
    esp_err_t err = ESP_OK;
    ble_npl_error_t rc;

    rc = ble_npl_sem_init(&ble_hs_stop_sem, 0);

    if( rc != 0) {
        ESP_LOGE(NIMBLE_PORT_LOG_TAG, "sem init failed with reason: %d \n", rc);
	return rc;
    }

    ble_hs_stop_result = 0;
    ble_npl_event_init(&ble_hs_ev_stop_begin,
                       nimble_port_stop_begin_cb, NULL);
    if (nimble_port_run_active) {
        /* Serialize stop with callout events already dequeued by the
         * host: the host task starts the stop itself. */
        ble_npl_eventq_put(&g_eventq_dflt, &ble_hs_ev_stop_begin);
        ble_npl_sem_pend(&ble_hs_stop_sem, BLE_NPL_TIME_FOREVER);
        err = ble_hs_stop_result;
    } else {
        /* No host task to hand the request to: stop from here. */
        err = ble_hs_stop(&stop_listener, ble_hs_stop_cb, NULL);
        if (err == 0) {
            ble_npl_sem_pend(&ble_hs_stop_sem, BLE_NPL_TIME_FOREVER);
            err = ble_hs_stop_result;
        }
    }
    if (err != 0) {
        if (err != BLE_HS_EALREADY) {
            ble_npl_event_deinit(&ble_hs_ev_stop_begin);
            ble_npl_sem_deinit(&ble_hs_stop_sem);
            return err;
        }
        if (!nimble_port_run_active) {
            ble_npl_event_deinit(&ble_hs_ev_stop_begin);
            ble_npl_sem_deinit(&ble_hs_stop_sem);
            return ESP_OK;
        }
    }

    ble_npl_event_init(&ble_hs_ev_stop, nimble_port_stop_cb,
                       NULL);
    ble_npl_eventq_put(&g_eventq_dflt, &ble_hs_ev_stop);

    /* Wait till the host task services and releases the stop event. */
    ble_npl_sem_pend(&ble_hs_stop_sem, BLE_NPL_TIME_FOREVER);

    /* The stop marker ran after stop_begin, so this event is no longer
     * referenced by the host task and can safely return to the pool. */
    ble_npl_event_deinit(&ble_hs_ev_stop_begin);
    ble_npl_sem_deinit(&ble_hs_stop_sem);

    return ESP_OK;
}

#if !MYNEWT_VAL(BLE_LOW_SPEED_MODE)
IRAM_ATTR
#endif
void nimble_port_run(void)
{
    struct ble_npl_event *ev;

#if MYNEWT_VAL(BLE_STATIC_TO_DYNAMIC)
    if (ble_npl_ctx == NULL) {
        ESP_LOGE(NIMBLE_PORT_LOG_TAG, "nimble context not initialized, host task exiting");
        return;
    }
#endif
#if CONFIG_BT_DUAL_MODE_ARCH
    if (g_eventq_dflt.eventq.eventq == NULL) {
#else
    if (g_eventq_dflt.eventq == NULL) {
#endif
        ESP_LOGE(NIMBLE_PORT_LOG_TAG, "nimble event queue not initialized, host task exiting");
        return;
    }

    /* Snapshot stop_ev and stop_sem addresses before entering the loop.
     * When BLE_STATIC_TO_DYNAMIC is enabled, after ble_npl_sem_release(stop_sem)
     * at the end of stop-event handling, the caller of nimble_port_stop() can
     * wake up and free ble_npl_ctx. Any subsequent use of the &ble_hs_ev_stop
     * or &ble_hs_stop_sem macros (which dereference ble_npl_ctx) would be a
     * use-after-free. Using local snapshots avoids that.
     */
    struct ble_npl_event *stop_ev  = &ble_hs_ev_stop;
    struct ble_npl_sem  *stop_sem  = &ble_hs_stop_sem;

    nimble_port_run_active = true;
    while (1) {
        ev = ble_npl_eventq_get(&g_eventq_dflt, BLE_NPL_TIME_FOREVER);
        if (ev) {
            ble_npl_event_run(ev);
            if (ev == stop_ev) {
                ble_npl_event_deinit(stop_ev);
                nimble_port_run_active = false;
                ble_npl_sem_release(stop_sem);
                break;
            }
        }
    }
}

#if !MYNEWT_VAL(BLE_LOW_SPEED_MODE)
IRAM_ATTR
#endif
struct ble_npl_eventq *
nimble_port_get_dflt_eventq(void)
{
#if MYNEWT_VAL(BLE_STATIC_TO_DYNAMIC)
    /* Check deiniting first: context may still be non-NULL during the deinit
     * window after ble_npl_deiniting is set but before ble_npl_ctx is freed.
     * Return the fallback to prevent callers from using a deinitialized queue. */
    if (ble_npl_deiniting) {
        return &g_eventq_shutdown_fallback;
    }

    /* If context exists, return the real eventq */
    if (ble_npl_ctx != NULL) {
        return &g_eventq_dflt;
    }

    /* Normal case: try to allocate context */
    if (ble_npl_ensure_ctx()) {
        return NULL;
    }
#endif

    return &g_eventq_dflt;
}

#endif /* CONFIG_IDF_TARGET_ESP32 && !CONFIG_NIMBLE_ENABLED && !ESPBLE_CLASSIC_ONLY */
