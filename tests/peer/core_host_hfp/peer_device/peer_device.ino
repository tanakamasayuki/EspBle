// Peer for the core-host HFP interoperability test. It links no EspBle code:
// the Audio Gateway comes from the ESP-IDF Bluedroid API Arduino-ESP32 ships,
// so the service-level connection, the indicators and the AT exchanges all
// cross a stack boundary to EspBle's independently built Classic host.
//
// SCO audio is deliberately not exercised: the core is built with
// CONFIG_BT_HFP_AUDIO_DATA_PATH_PCM, so this side routes voice to an external
// codec chip instead of over HCI and an application here never sees it.
#include <Arduino.h>
// Without this the Core releases the Classic BT memory during startup, because
// this sketch links none of the Bluetooth libraries that would claim it.
#include <esp32-hal-alloc-bt-classic-mem.h>
#include <esp32-hal-bt.h>
#include <esp_bt_device.h>
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>
#include <esp_hf_ag_api.h>

bool slcConnected = false;
esp_bd_addr_t clientAddress = {0};
String lastDialedNumber;
unsigned answeredCalls = 0;
unsigned endedCalls = 0;

String addressText(const uint8_t *address)
{
  char text[18];
  snprintf(text, sizeof(text), "%02x:%02x:%02x:%02x:%02x:%02x", address[0],
    address[1], address[2], address[3], address[4], address[5]);
  return String(text);
}

void reportReady()
{
  Serial.printf("HFPPEER_READY address=%s\n",
    addressText(esp_bt_dev_get_address()).c_str());
  Serial.printf("HFPPEER_STATE slc=%u answered=%u ended=%u dialed=%s\n",
    slcConnected ? 1 : 0, answeredCalls, endedCalls,
    lastDialedNumber.length() != 0 ? lastDialedNumber.c_str() : "none");
}

void gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
  switch (event)
  {
    case ESP_BT_GAP_CFM_REQ_EVT:
      esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
      Serial.println("HFPPEER_SSP_CONFIRMED");
      break;
    case ESP_BT_GAP_AUTH_CMPL_EVT:
      Serial.printf("HFPPEER_AUTH status=%d\n", param->auth_cmpl.stat);
      break;
    default:
      break;
  }
}

void agCallback(esp_hf_cb_event_t event, esp_hf_cb_param_t *param)
{
  switch (event)
  {
    case ESP_HF_CONNECTION_STATE_EVT:
      slcConnected = param->conn_stat.state == ESP_HF_CONNECTION_STATE_SLC_CONNECTED;
      // Remember the address as soon as the link exists: the SLC queries below
      // arrive before the connection reaches its service-level state.
      if (param->conn_stat.state != ESP_HF_CONNECTION_STATE_DISCONNECTED)
        memcpy(clientAddress, param->conn_stat.remote_bda, 6);
      Serial.printf("HFPPEER_CONNECTION state=%d peer=%s\n", param->conn_stat.state,
        addressText(param->conn_stat.remote_bda).c_str());
      break;
    case ESP_HF_AUDIO_STATE_EVT:
      Serial.printf("HFPPEER_AUDIO state=%d\n", param->audio_stat.state);
      break;
    case ESP_HF_DIAL_EVT:
      // The number the Client dialled, as the AG received it.
      lastDialedNumber = param->out_call.num_or_loc != nullptr
                           ? String(param->out_call.num_or_loc)
                           : String();
      Serial.printf("HFPPEER_DIAL number=%s\n",
        lastDialedNumber.length() != 0 ? lastDialedNumber.c_str() : "none");
      esp_hf_ag_out_call(clientAddress, 0, 0, ESP_HF_CALL_STATUS_NO_CALLS,
        ESP_HF_CALL_SETUP_STATUS_OUTGOING_DIALING,
        const_cast<char *>(lastDialedNumber.c_str()),
        ESP_HF_CALL_ADDR_TYPE_UNKNOWN);
      break;
    case ESP_HF_ATA_RESPONSE_EVT:
      ++answeredCalls;
      Serial.printf("HFPPEER_ANSWER count=%u\n", answeredCalls);
      esp_hf_ag_answer_call(clientAddress, 1, 0, ESP_HF_CALL_STATUS_CALL_IN_PROGRESS,
        ESP_HF_CALL_SETUP_STATUS_IDLE, const_cast<char *>("5551234"),
        ESP_HF_CALL_ADDR_TYPE_UNKNOWN);
      break;
    case ESP_HF_CHUP_RESPONSE_EVT:
      ++endedCalls;
      Serial.printf("HFPPEER_HANGUP count=%u\n", endedCalls);
      esp_hf_ag_end_call(clientAddress, 0, 0, ESP_HF_CALL_STATUS_NO_CALLS,
        ESP_HF_CALL_SETUP_STATUS_IDLE, nullptr, ESP_HF_CALL_ADDR_TYPE_UNKNOWN);
      break;
    case ESP_HF_CIND_RESPONSE_EVT:
      // The Client asks for the indicator values during SLC setup. An AG that
      // does not answer leaves the negotiation unfinished and the link drops,
      // which is what happens without this branch.
      Serial.println("HFPPEER_CIND_QUERY");
      esp_hf_ag_cind_response(clientAddress, ESP_HF_CALL_STATUS_NO_CALLS,
        ESP_HF_CALL_SETUP_STATUS_IDLE, ESP_HF_NETWORK_STATE_AVAILABLE, 4,
        ESP_HF_ROAMING_STATUS_INACTIVE, 4, ESP_HF_CALL_HELD_STATUS_NONE);
      break;
    case ESP_HF_COPS_RESPONSE_EVT:
      Serial.println("HFPPEER_COPS_QUERY");
      esp_hf_ag_cops_response(clientAddress, const_cast<char *>("EspBle"));
      break;
    case ESP_HF_CLCC_RESPONSE_EVT:
      Serial.println("HFPPEER_CLCC_QUERY");
      esp_hf_ag_clcc_response(clientAddress, 1, ESP_HF_CURRENT_CALL_DIRECTION_INCOMING,
        ESP_HF_CURRENT_CALL_STATUS_ACTIVE, ESP_HF_CURRENT_CALL_MODE_VOICE,
        ESP_HF_CURRENT_CALL_MPTY_TYPE_SINGLE, const_cast<char *>("5551234"),
        ESP_HF_CALL_ADDR_TYPE_UNKNOWN);
      break;
    case ESP_HF_CNUM_RESPONSE_EVT:
      Serial.println("HFPPEER_CNUM_QUERY");
      esp_hf_ag_cnum_response(clientAddress, const_cast<char *>("5550000"), 129,
        ESP_HF_SUBSCRIBER_SERVICE_TYPE_VOICE);
      break;
    case ESP_HF_UNAT_RESPONSE_EVT:
      // An AT command the backend does not decode. Answering it is what keeps
      // the exchange from hanging on the Client side.
      Serial.printf("HFPPEER_UNAT value=%s\n",
        param->unat_rep.unat != nullptr ? param->unat_rep.unat : "none");
      esp_hf_ag_cmee_send(clientAddress, ESP_HF_AT_RESPONSE_CODE_OK,
        ESP_HF_CME_AG_FAILURE);
      break;
    default:
      break;
  }
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  if (!btStart())
  {
    Serial.println("HFPPEER_INIT_FAILED controller");
    return;
  }
  if (esp_bluedroid_init() != ESP_OK || esp_bluedroid_enable() != ESP_OK)
  {
    Serial.println("HFPPEER_INIT_FAILED bluedroid");
    return;
  }

  esp_bt_dev_set_device_name("EspBle CoreHost AG");
  esp_bt_gap_register_callback(gapCallback);

  esp_bt_io_cap_t ioCapability = ESP_BT_IO_CAP_NONE;
  esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &ioCapability,
    sizeof(ioCapability));

  const esp_err_t registered = esp_hf_ag_register_callback(agCallback);
  const esp_err_t initialized = esp_hf_ag_init();
  Serial.printf("HFPPEER_AG_INIT registered=%d init=%d\n", registered, initialized);

  // The AG waits for the headset side to connect, so it stays visible.
  esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

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
    else if (command == "i" && slcConnected)
    {
      // Report an incoming call: the indicators and the RING travel to the
      // Client, which is what the DUT asserts on its side.
      const esp_err_t result = esp_hf_ag_out_call(clientAddress, 0, 0,
        ESP_HF_CALL_STATUS_NO_CALLS, ESP_HF_CALL_SETUP_STATUS_INCOMING,
        const_cast<char *>("5551234"), ESP_HF_CALL_ADDR_TYPE_UNKNOWN);
      Serial.printf("HFPPEER_INCOMING requested=%d\n", result);
    }
    else if (command == "e" && slcConnected)
    {
      const esp_err_t result = esp_hf_ag_end_call(clientAddress, 0, 0,
        ESP_HF_CALL_STATUS_NO_CALLS, ESP_HF_CALL_SETUP_STATUS_IDLE, nullptr,
        ESP_HF_CALL_ADDR_TYPE_UNKNOWN);
      Serial.printf("HFPPEER_END requested=%d\n", result);
    }
  }
  delay(10);
}
