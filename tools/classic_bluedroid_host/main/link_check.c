// This app is not shipped. It makes the host build fail if any required
// Classic-only public API disappears from the selected ESP-IDF revision.
#include <stdbool.h>
#include <stdint.h>

#include "esp_bluedroid_hci.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_hidd_api.h"
#include "esp_hidh_api.h"
#include "esp_spp_api.h"

static void send_packet(uint8_t *data, uint16_t length)
{
    (void)data;
    (void)length;
}

static bool can_send(void)
{
    return true;
}

static esp_err_t register_callbacks(
    const esp_bluedroid_hci_driver_callbacks_t *callbacks)
{
    (void)callbacks;
    return ESP_OK;
}

static void hid_device_callback(
    esp_hidd_cb_event_t event, esp_hidd_cb_param_t *parameter)
{
    (void)event;
    (void)parameter;
}

static void hid_host_callback(
    esp_hidh_cb_event_t event, esp_hidh_cb_param_t *parameter)
{
    (void)event;
    (void)parameter;
}

static void spp_callback(
    esp_spp_cb_event_t event, esp_spp_cb_param_t *parameter)
{
    (void)event;
    (void)parameter;
}

void app_main(void)
{
    const esp_bluedroid_hci_driver_operations_t operations = {
        .send = send_packet,
        .check_send_available = can_send,
        .register_host_callback = register_callbacks,
    };
    ESP_ERROR_CHECK(esp_bluedroid_attach_hci_driver(&operations));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bt_hid_device_register_callback(hid_device_callback));
    ESP_ERROR_CHECK(esp_bt_hid_host_register_callback(hid_host_callback));
    ESP_ERROR_CHECK(esp_spp_register_callback(spp_callback));
}
