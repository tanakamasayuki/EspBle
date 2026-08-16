// Peer for the core-host A2DP interoperability test. It links no EspBle code:
// the A2DP Source and the AVRCP Controller come from the ESP-IDF Bluedroid API
// Arduino-ESP32 ships, and the SBC encoding happens inside that stack. The DUT
// runs EspBle's independently built Classic host as the Sink, so the codec
// negotiation, the RTP framing and the AVRCP commands all cross a stack
// boundary. There is no wrapper class for A2DP, which is why this sketch calls
// the C API directly.
#include <Arduino.h>
// Arduino-ESP32 3.3.11 releases the Classic BT memory during startup unless a
// Bluetooth library is linked, and a sketch that calls the ESP-IDF API directly
// links none. This header's constructor is what declares the memory as in use;
// without it btStart() fails before any of the code below runs. Cores that
// predate the header never release that memory, so its absence needs no
// replacement -- this sketch is also built against older cores to measure which
// of them still interoperate.
#if __has_include(<esp32-hal-alloc-bt-classic-mem.h>)
#include <esp32-hal-alloc-bt-classic-mem.h>
#endif
#include <esp32-hal-bt.h>
#include <esp_a2dp_api.h>
#include <esp_avrc_api.h>
#include <esp_bt_device.h>
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>

bool connected = false;
bool streaming = false;
volatile uint32_t sentSamples = 0;
esp_bd_addr_t peerAddress = {0};

String addressText(const uint8_t *address)
{
  char text[18];
  snprintf(text, sizeof(text), "%02x:%02x:%02x:%02x:%02x:%02x", address[0],
    address[1], address[2], address[3], address[4], address[5]);
  return String(text);
}

bool parseAddress(const String &text, esp_bd_addr_t address)
{
  unsigned values[6];
  if (sscanf(text.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", &values[0], &values[1],
        &values[2], &values[3], &values[4], &values[5]) != 6)
  {
    return false;
  }
  for (size_t index = 0; index < 6; ++index)
  {
    address[index] = static_cast<uint8_t>(values[index]);
  }
  return true;
}

void reportReady()
{
  Serial.printf("A2DPPEER_READY address=%s\n",
    addressText(esp_bt_dev_get_address()).c_str());
  Serial.printf("A2DPPEER_STATE connected=%u streaming=%u samples=%u\n",
    connected ? 1 : 0, streaming ? 1 : 0,
    static_cast<unsigned>(sentSamples));
}

// The stack pulls PCM from here and encodes it to SBC itself. A ramp is enough:
// the Sink receives encoded frames, so the test asserts framing and counts
// rather than sample values.
int32_t supplyAudio(uint8_t *buffer, int32_t length)
{
  if (buffer == nullptr || length <= 0) return 0;
  int16_t *samples = reinterpret_cast<int16_t *>(buffer);
  const int32_t count = length / 2;
  static uint16_t phase = 0;
  for (int32_t index = 0; index < count; ++index)
  {
    samples[index] = static_cast<int16_t>((phase += 137) & 0x7fff);
  }
  sentSamples += static_cast<uint32_t>(count);
  return length;
}

void gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
  switch (event)
  {
    case ESP_BT_GAP_CFM_REQ_EVT:
      Serial.printf("A2DPPEER_SSP_CONFIRM value=%lu\n",
        static_cast<unsigned long>(param->cfm_req.num_val));
      esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
      break;
    case ESP_BT_GAP_AUTH_CMPL_EVT:
      Serial.printf("A2DPPEER_AUTH status=%d\n", param->auth_cmpl.stat);
      break;
    default:
      break;
  }
}

void a2dpCallback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
  switch (event)
  {
    case ESP_A2D_CONNECTION_STATE_EVT:
      connected = param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED;
      Serial.printf("A2DPPEER_CONNECTION state=%d peer=%s\n",
        param->conn_stat.state, addressText(param->conn_stat.remote_bda).c_str());
      if (connected)
      {
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY);
      }
      break;
    case ESP_A2D_AUDIO_STATE_EVT:
      streaming = param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED;
      Serial.printf("A2DPPEER_AUDIO_STATE state=%d\n", param->audio_stat.state);
      break;
    case ESP_A2D_MEDIA_CTRL_ACK_EVT:
      Serial.printf("A2DPPEER_MEDIA_ACK cmd=%d status=%d\n",
        param->media_ctrl_stat.cmd, param->media_ctrl_stat.status);
      if (param->media_ctrl_stat.cmd == ESP_A2D_MEDIA_CTRL_CHECK_SRC_RDY
          && param->media_ctrl_stat.status == ESP_A2D_MEDIA_CTRL_ACK_SUCCESS)
      {
        esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_START);
      }
      break;
    default:
      break;
  }
}

void avrcpCallback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param)
{
  switch (event)
  {
    case ESP_AVRC_CT_CONNECTION_STATE_EVT:
      Serial.printf("A2DPPEER_AVRCP connected=%u\n",
        param->conn_stat.connected ? 1 : 0);
      break;
    case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
      Serial.printf("A2DPPEER_AVRCP_RSP key=%d state=%d\n",
        param->psth_rsp.key_code, param->psth_rsp.key_state);
      break;
    default:
      break;
  }
}

void avrcpTargetCallback(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t *param)
{
  switch (event)
  {
    case ESP_AVRC_TG_CONNECTION_STATE_EVT:
      Serial.printf("A2DPPEER_AVRCP_TG connected=%u\n",
        param->conn_stat.connected ? 1 : 0);
      break;
    default:
      break;
  }
}

void sendPassthrough(uint8_t keyCode)
{
  static uint8_t transaction = 0;
  esp_avrc_ct_send_passthrough_cmd(transaction++, keyCode, ESP_AVRC_PT_CMD_STATE_PRESSED);
  delay(30);
  esp_avrc_ct_send_passthrough_cmd(transaction++, keyCode, ESP_AVRC_PT_CMD_STATE_RELEASED);
  Serial.printf("A2DPPEER_AVRCP_SENT key=%u\n", keyCode);
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  if (!btStart())
  {
    Serial.println("A2DPPEER_INIT_FAILED controller");
    return;
  }
  if (esp_bluedroid_init() != ESP_OK || esp_bluedroid_enable() != ESP_OK)
  {
    Serial.println("A2DPPEER_INIT_FAILED bluedroid");
    return;
  }

  esp_bt_dev_set_device_name("EspBle CoreHost A2DP Source");
  esp_bt_gap_register_callback(gapCallback);

  esp_bt_io_cap_t ioCapability = ESP_BT_IO_CAP_NONE;
  esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &ioCapability,
    sizeof(ioCapability));

  esp_a2d_register_callback(a2dpCallback);
  esp_a2d_source_register_data_callback(supplyAudio);
  esp_a2d_source_init();

  const esp_err_t ctInit = esp_avrc_ct_init();
  esp_avrc_ct_register_callback(avrcpCallback);
  // A real Source (a phone, say) publishes both AVRCP roles. Without the Target
  // record the Sink's own AVRCP connect attempt fails its SDP lookup, and no
  // AVCTP channel comes up for the Controller commands below to travel on.
  const esp_err_t tgInit = esp_avrc_tg_init();
  esp_avrc_tg_register_callback(avrcpTargetCallback);
  // The supported set has to be built explicitly: handing back the allowed set
  // is rejected with ESP_ERR_NOT_SUPPORTED, and without a supported command the
  // Target publishes no usable record for the Sink's SDP lookup.
  esp_avrc_psth_bit_mask_t commands;
  memset(&commands, 0, sizeof(commands));
  esp_avrc_psth_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &commands,
    ESP_AVRC_PT_CMD_PLAY);
  esp_avrc_psth_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &commands,
    ESP_AVRC_PT_CMD_PAUSE);
  esp_avrc_psth_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &commands,
    ESP_AVRC_PT_CMD_STOP);
  const esp_err_t filter =
    esp_avrc_tg_set_psth_cmd_filter(ESP_AVRC_PSTH_FILTER_SUPPORTED_CMD, &commands);
  Serial.printf("A2DPPEER_AVRCP_INIT ct=%d tg=%d filter=%d\n", ctInit, tgInit,
    filter);

  // A Source initiates, so it does not need to be visible to inquiry.
  esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);

  reportReady();
}

void loop()
{
  if (Serial.available())
  {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (command == "?")
    {
      reportReady();
    }
    else if (command.startsWith("c"))
    {
      if (!parseAddress(command.substring(1), peerAddress))
      {
        Serial.println("A2DPPEER_CONNECT requested=0");
      }
      else
      {
        const esp_err_t result = esp_a2d_source_connect(peerAddress);
        Serial.printf("A2DPPEER_CONNECT requested=%u\n", result == ESP_OK ? 1 : 0);
      }
    }
    else if (command == "u")
    {
      esp_a2d_media_ctrl(ESP_A2D_MEDIA_CTRL_SUSPEND);
      Serial.println("A2DPPEER_SUSPEND requested=1");
    }
    else if (command == "p")
    {
      sendPassthrough(ESP_AVRC_PT_CMD_PLAY);
    }
    else if (command == "P")
    {
      // Ask the stack to open AVRCP explicitly, for the case where neither side
      // brings it up on its own after the media connection.
      const esp_err_t result = esp_avrc_ct_send_passthrough_cmd(0,
        ESP_AVRC_PT_CMD_PLAY, ESP_AVRC_PT_CMD_STATE_PRESSED);
      Serial.printf("A2DPPEER_AVRCP_PROBE result=%d\n", result);
    }
    else if (command == "d")
    {
      esp_a2d_source_disconnect(peerAddress);
      Serial.println("A2DPPEER_DISCONNECT requested=1");
    }
  }
  delay(10);
}
