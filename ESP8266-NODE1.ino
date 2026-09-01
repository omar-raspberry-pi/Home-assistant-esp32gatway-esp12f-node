#include <ESP8266WiFi.h>

extern "C" {
  #include <espnow.h>
}

// ======================================================
// NODE SETTINGS
// ======================================================

#define NODE_ID       1
#define RELAY_PIN     2       // GPIO2 = D4 = Built-in LED
#define WIFI_CHANNEL  11       // نفس قناة Wi-Fi/Gateway

// MAC address of ESP32 Gateway
uint8_t GATEWAY_MAC[] = {
  0x94, 0xB9, 0x7E, 0xD5, 0x62, 0xDC  
};

// ======================================================
// PROTOCOL
// ======================================================

#define MAGIC              0xA5

#define CMD_RELAY_OFF      0x00
#define CMD_RELAY_ON       0x01
#define CMD_PING           0x30

#define MSG_STATUS         0x10
#define MSG_ACK            0x11

// ======================================================
// RELAY STATE
// ======================================================

bool relayState = false;

// ======================================================
// HELPERS
// ======================================================

void setRelay(bool state)
{
  relayState = state;

  // Built-in LED is Active LOW
  digitalWrite(RELAY_PIN, state ? LOW : HIGH);

  Serial.print("Relay = ");
  Serial.println(state ? "ON" : "OFF");
}

void printMac(const uint8_t *mac)
{
  for (int i = 0; i < 6; i++)
  {
    if (i > 0) Serial.print(":");

    if (mac[i] < 16)
      Serial.print("0");

    Serial.print(mac[i], HEX);
  }
}

// ======================================================
// SEND STATUS TO GATEWAY
// ======================================================

void sendStatus()
{
  uint8_t packet[4];

  packet[0] = MAGIC;
  packet[1] = NODE_ID;
  packet[2] = MSG_STATUS;
  packet[3] = relayState ? 1 : 0;

  uint8_t result = esp_now_send(
    GATEWAY_MAC,
    packet,
    sizeof(packet)
  );

  Serial.print("STATUS sent: ");

  if (result == 0)
    Serial.println("OK");
  else
    Serial.println("FAILED");
}

// ======================================================
// SEND CALLBACK
// ======================================================

void onDataSent(uint8_t *mac_addr, uint8_t sendStatus)
{
  Serial.print("ESP-NOW send: ");

  if (sendStatus == 0)
    Serial.println("SUCCESS");
  else
    Serial.println("FAIL");

  Serial.print("Gateway: ");
  printMac(mac_addr);
  Serial.println();
}

// ======================================================
// RECEIVE CALLBACK
// ======================================================

void onDataRecv(uint8_t *mac_addr, uint8_t *data, uint8_t len)
{
  Serial.print("RX from: ");
  printMac(mac_addr);

  Serial.print(" | LEN=");
  Serial.println(len);

  if (len < 3)
  {
    Serial.println("Invalid packet");
    return;
  }

  // Check MAGIC
  if (data[0] != MAGIC)
  {
    Serial.println("Invalid MAGIC");
    return;
  }

  // Check Node ID
  if (data[1] != NODE_ID)
  {
    Serial.println("Packet for another node");
    return;
  }

  uint8_t command = data[2];

  switch (command)
  {
    case CMD_RELAY_ON:

      Serial.println("Command: RELAY ON");

      setRelay(true);
      sendStatus();

      break;


    case CMD_RELAY_OFF:

      Serial.println("Command: RELAY OFF");

      setRelay(false);
      sendStatus();

      break;


    case CMD_PING:

      Serial.println("Command: PING");

      sendStatus();

      break;


    default:

      Serial.print("Unknown command: ");
      Serial.println(command, HEX);

      break;
  }
}

// ======================================================
// SETUP
// ======================================================

void setup()
{
  Serial.begin(115200);
  delay(200);

  Serial.println();
  Serial.println("================================");
  Serial.println(" ESP8266 ESP-NOW SMART NODE");
  Serial.println(" NODE 1");
  Serial.println("================================");

  // Relay / LED
  pinMode(RELAY_PIN, OUTPUT);

  // Start OFF
  setRelay(false);

  // ESP-NOW works in Station mode
  WiFi.mode(WIFI_STA);

  WiFi.disconnect();

  delay(100);

  // --------------------------------------------------
  // Print MAC
  // --------------------------------------------------

  Serial.print("Node MAC: ");
  Serial.println(WiFi.macAddress());

  Serial.print("Channel: ");
  Serial.println(WIFI_CHANNEL);

  Serial.print("Gateway MAC: ");
  printMac(GATEWAY_MAC);
  Serial.println();

  // --------------------------------------------------
  // Set Wi-Fi channel
  // --------------------------------------------------

  wifi_set_channel(WIFI_CHANNEL);

  // --------------------------------------------------
  // Initialize ESP-NOW
  // --------------------------------------------------

  if (esp_now_init() != 0)
  {
    Serial.println("ESP-NOW INIT FAILED");

    while (true)
    {
      delay(1000);
    }
  }

  Serial.println("ESP-NOW INIT OK");

  // --------------------------------------------------
  // Set Node role
  // --------------------------------------------------

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);

  // --------------------------------------------------
  // Callbacks
  // --------------------------------------------------

  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  // --------------------------------------------------
  // Add Gateway
  // --------------------------------------------------

  if (!esp_now_is_peer_exist(GATEWAY_MAC))
  {
    uint8_t result = esp_now_add_peer(
      GATEWAY_MAC,
      ESP_NOW_ROLE_CONTROLLER,
      WIFI_CHANNEL,
      NULL,
      0
    );

    if (result == 0)
    {
      Serial.println("Gateway peer added");
    }
    else
    {
      Serial.print("Failed to add Gateway, code: ");
      Serial.println(result);
    }
  }
  else
  {
    Serial.println("Gateway already exists");
  }

  delay(500);

  // --------------------------------------------------
  // Send initial state
  // --------------------------------------------------

  sendStatus();

  Serial.println("Node ready.");
}

// ======================================================
// LOOP
// ======================================================

void loop()
{
  // Nothing required here.
  // Node waits for ESP-NOW commands.

  delay(10);
}
