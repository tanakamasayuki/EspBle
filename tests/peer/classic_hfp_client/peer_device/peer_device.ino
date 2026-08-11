#include <EspBleClassic.h>
#include <esp_mac.h>

#define esp_hf_ag_answer_call espble_bd_esp_hf_ag_answer_call
#define esp_hf_ag_audio_buff_free espble_bd_esp_hf_ag_audio_buff_free
#define esp_hf_ag_audio_data_send espble_bd_esp_hf_ag_audio_data_send
#define esp_hf_ag_ciev_report espble_bd_esp_hf_ag_ciev_report
#define esp_hf_ag_cind_response espble_bd_esp_hf_ag_cind_response
#define esp_hf_ag_clcc_response espble_bd_esp_hf_ag_clcc_response
#define esp_hf_ag_cmee_send espble_bd_esp_hf_ag_cmee_send
#define esp_hf_ag_cnum_response espble_bd_esp_hf_ag_cnum_response
#define esp_hf_ag_cops_response espble_bd_esp_hf_ag_cops_response
#define esp_hf_ag_deinit espble_bd_esp_hf_ag_deinit
#define esp_hf_ag_end_call espble_bd_esp_hf_ag_end_call
#define esp_hf_ag_init espble_bd_esp_hf_ag_init
#define esp_hf_ag_out_call espble_bd_esp_hf_ag_out_call
#define esp_hf_ag_register_audio_data_callback \
  espble_bd_esp_hf_ag_register_audio_data_callback
#define esp_hf_ag_register_callback espble_bd_esp_hf_ag_register_callback
#define esp_bt_gap_set_scan_mode espble_bd_esp_bt_gap_set_scan_mode
#include <esp_hf_ag_api.h>
#include <esp_gap_bt_api.h>

EspBleClassic bluetooth;
esp_bd_addr_t peerAddress = {};
bool profileReady = false;
bool audioEchoed = false;

String classicAddress()
{
  uint8_t address[6] = {};
  esp_read_mac(address, ESP_MAC_BT);
  char value[18];
  snprintf(value, sizeof(value), "%02x:%02x:%02x:%02x:%02x:%02x",
    address[0], address[1], address[2], address[3], address[4], address[5]);
  return String(value);
}

void hfpAgAudio(
  esp_hf_sync_conn_hdl_t handle,
  esp_hf_audio_buff_t *audio,
  bool badFrame)
{
  if (badFrame)
  {
    esp_hf_ag_audio_buff_free(audio);
    return;
  }
  uint32_t checksum = 0;
  for (size_t index = 0; index < audio->data_len; ++index)
    checksum += audio->data[index];
  Serial.printf("HFP_AG_MEDIA handle=%u len=%u bad=%u checksum=%lu\n",
    handle, audio->data_len, badFrame ? 1 : 0,
    static_cast<unsigned long>(checksum));
  if (audioEchoed)
  {
    esp_hf_ag_audio_buff_free(audio);
    return;
  }
  audioEchoed = true;
  // Incoming mSBC can contain controller padding. The send API takes the
  // 57-byte encoded frame and supplies air-interface padding itself.
  if (audio->data_len > ESP_HF_MSBC_ENCODED_FRAME_SIZE)
    audio->data_len = ESP_HF_MSBC_ENCODED_FRAME_SIZE;
  if (esp_hf_ag_audio_data_send(handle, audio) != ESP_OK)
    esp_hf_ag_audio_buff_free(audio);
}

void hfpAgCallback(esp_hf_cb_event_t event, esp_hf_cb_param_t *parameter)
{
  if (event == ESP_HF_PROF_STATE_EVT)
  {
    profileReady = parameter->prof_stat.state == ESP_HF_INIT_SUCCESS ||
      parameter->prof_stat.state == ESP_HF_INIT_ALREADY;
    Serial.printf("HFP_AG_PROFILE ready=%u\n", profileReady ? 1 : 0);
  }
  else if (event == ESP_HF_CONNECTION_STATE_EVT)
  {
    memcpy(peerAddress, parameter->conn_stat.remote_bda, ESP_BD_ADDR_LEN);
    Serial.printf("HFP_AG_CONNECTION state=%u peer=%02x:%02x:%02x:%02x:%02x:%02x\n",
      parameter->conn_stat.state, peerAddress[0], peerAddress[1],
      peerAddress[2], peerAddress[3], peerAddress[4], peerAddress[5]);
  }
  else if (event == ESP_HF_AUDIO_STATE_EVT)
    Serial.printf("HFP_AG_AUDIO state=%u handle=%u frame=%u\n",
      parameter->audio_stat.state, parameter->audio_stat.sync_conn_handle,
      parameter->audio_stat.preferred_frame_size);
  else if (event == ESP_HF_CIND_RESPONSE_EVT)
    (void)esp_hf_ag_cind_response(parameter->cind_rep.remote_addr,
      ESP_HF_CALL_STATUS_NO_CALLS, ESP_HF_CALL_SETUP_STATUS_IDLE,
      ESP_HF_NETWORK_STATE_AVAILABLE, 5, ESP_HF_ROAMING_STATUS_INACTIVE,
      5, ESP_HF_CALL_HELD_STATUS_NONE);
  else if (event == ESP_HF_IND_UPDATE_EVT)
  {
    (void)esp_hf_ag_ciev_report(parameter->ind_upd.remote_addr,
      ESP_HF_IND_TYPE_SERVICE, ESP_HF_NETWORK_STATE_AVAILABLE);
    (void)esp_hf_ag_ciev_report(parameter->ind_upd.remote_addr,
      ESP_HF_IND_TYPE_SIGNAL, 5);
  }
  else if (event == ESP_HF_COPS_RESPONSE_EVT)
    (void)esp_hf_ag_cops_response(parameter->cops_rep.remote_addr,
      const_cast<char *>("EspBle"));
  else if (event == ESP_HF_CNUM_RESPONSE_EVT)
    (void)esp_hf_ag_cnum_response(parameter->cnum_rep.remote_addr,
      const_cast<char *>("5550000"), 0x81,
      ESP_HF_SUBSCRIBER_SERVICE_TYPE_VOICE);
  else if (event == ESP_HF_CLCC_RESPONSE_EVT)
    (void)esp_hf_ag_clcc_response(parameter->clcc_rep.remote_addr, 0,
      ESP_HF_CURRENT_CALL_DIRECTION_OUTGOING,
      ESP_HF_CURRENT_CALL_STATUS_ACTIVE, ESP_HF_CURRENT_CALL_MODE_VOICE,
      ESP_HF_CURRENT_CALL_MPTY_TYPE_SINGLE, nullptr,
      ESP_HF_CALL_ADDR_TYPE_UNKNOWN);
  else if (event == ESP_HF_DIAL_EVT)
  {
    Serial.printf("HFP_AG_DIAL number=%s\n",
      parameter->out_call.num_or_loc ? parameter->out_call.num_or_loc : "");
    (void)esp_hf_ag_cmee_send(parameter->out_call.remote_addr,
      ESP_HF_AT_RESPONSE_CODE_OK, ESP_HF_CME_AG_FAILURE);
    (void)esp_hf_ag_out_call(parameter->out_call.remote_addr, 1, 0,
      ESP_HF_CALL_STATUS_CALL_IN_PROGRESS, ESP_HF_CALL_SETUP_STATUS_IDLE,
      parameter->out_call.num_or_loc, ESP_HF_CALL_ADDR_TYPE_UNKNOWN);
  }
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  EspBleClassicConfig config;
  config.deviceName = "EspBle HFP AG Probe";
  if (!bluetooth.begin(config))
  {
    Serial.printf("HFP_AG_STACK_FAILED %s\n",
      bluetooth.lastErrorDetail().c_str());
    return;
  }
  if (esp_hf_ag_register_callback(hfpAgCallback) != ESP_OK ||
      esp_hf_ag_register_audio_data_callback(hfpAgAudio) != ESP_OK ||
      esp_hf_ag_init() != ESP_OK)
  {
    Serial.println("HFP_AG_INIT_FAILED");
    return;
  }
  const uint32_t deadline = millis() + 5000;
  while (!profileReady && static_cast<int32_t>(millis() - deadline) < 0)
    delay(1);
  if (!profileReady || esp_bt_gap_set_scan_mode(
        ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE) != ESP_OK)
  {
    Serial.println("HFP_AG_READY_FAILED");
    return;
  }
  Serial.printf("HFP_AG_READY address=%s\n", classicAddress().c_str());
}

void loop()
{
  bluetooth.update();
  delay(1);
}
