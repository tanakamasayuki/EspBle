// This app is not shipped. It makes the host build fail if any required
// Classic-only public API disappears from the selected ESP-IDF revision.
#include <stdbool.h>
#include <stdint.h>

#include "esp_bluedroid_hci.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_bt_main.h"
#include "esp_err.h"
#include "esp_hf_ag_api.h"
#include "esp_hf_client_api.h"
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

static void a2dp_callback(
    esp_a2d_cb_event_t event, esp_a2d_cb_param_t *parameter)
{
    (void)event;
    (void)parameter;
}

static void a2dp_audio_callback(
    esp_a2d_conn_hdl_t connection, esp_a2d_audio_buff_t *audio)
{
    (void)connection;
    esp_a2d_audio_buff_free(audio);
}

static void avrc_ct_callback(
    esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *parameter)
{
    (void)event;
    (void)parameter;
}

static void avrc_tg_callback(
    esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t *parameter)
{
    (void)event;
    (void)parameter;
}

static void hf_client_callback(
    esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *parameter)
{
    (void)event;
    (void)parameter;
}

static void hf_client_audio_callback(
    esp_hf_sync_conn_hdl_t connection,
    esp_hf_audio_buff_t *audio,
    bool bad_frame)
{
    (void)connection;
    (void)bad_frame;
    esp_hf_client_audio_buff_free(audio);
}

static void hf_ag_callback(
    esp_hf_cb_event_t event, esp_hf_cb_param_t *parameter)
{
    (void)event;
    (void)parameter;
}

static void hf_ag_audio_callback(
    esp_hf_sync_conn_hdl_t connection,
    esp_hf_audio_buff_t *audio,
    bool bad_frame)
{
    (void)connection;
    (void)bad_frame;
    esp_hf_ag_audio_buff_free(audio);
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
    ESP_ERROR_CHECK(esp_a2d_register_callback(a2dp_callback));
    ESP_ERROR_CHECK(esp_a2d_sink_register_audio_data_callback(a2dp_audio_callback));
    ESP_ERROR_CHECK(esp_a2d_sink_init());
    ESP_ERROR_CHECK(esp_a2d_source_init());
    ESP_ERROR_CHECK(esp_a2d_source_audio_data_send(0, NULL));
    ESP_ERROR_CHECK(esp_avrc_ct_register_callback(avrc_ct_callback));
    ESP_ERROR_CHECK(esp_avrc_ct_init());
    ESP_ERROR_CHECK(esp_avrc_tg_register_callback(avrc_tg_callback));
    ESP_ERROR_CHECK(esp_avrc_tg_init());
    ESP_ERROR_CHECK(esp_hf_client_register_callback(hf_client_callback));
    ESP_ERROR_CHECK(esp_hf_client_register_audio_data_callback(
        hf_client_audio_callback));
    ESP_ERROR_CHECK(esp_hf_client_init());
    ESP_ERROR_CHECK(esp_hf_client_audio_data_send(0, NULL));
    ESP_ERROR_CHECK(esp_hf_ag_register_callback(hf_ag_callback));
    ESP_ERROR_CHECK(esp_hf_ag_register_audio_data_callback(
        hf_ag_audio_callback));
    ESP_ERROR_CHECK(esp_hf_ag_init());
    ESP_ERROR_CHECK(esp_hf_ag_audio_data_send(0, NULL));
}
