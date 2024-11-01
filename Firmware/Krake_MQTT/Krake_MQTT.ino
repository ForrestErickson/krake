// File name: Krake_MQTT
// Author: Nagham Kheir, Forrest Lee Erickson
// Date: 20241014
// LICENSE "GNU Affero General Public License, version 3 "
// Hardware ESP32 kit
// 20241101 Update for GPAD API and topics "_ALM" and "_ACK"

// Customized this by changing these defines
#define PROG_NAME "Krake_MQTT "
#define VERSION "V0.0.1 "
#define MODEL_NAME "KRAKE_"
#define DEVICE_UNDER_TEST "20240421_USA4"  //A Serial Number  
#define LICENSE "GNU Affero General Public License, version 3 "
#define ORIGIN "LB"

// Description:
// The Krake (an ESP32) subscribes and publishes to an MQTT broker with topics
// defined in user manual "Topic is “KRAKE_” plus a serial number plus a suffix"

#include <WiFi.h>
#include <PubSubClient.h> // From library https://github.com/knolleary/pubsubclient

#define BAUDRATE 115200

// WiFi credentials
//const char* ssid = "ADT";
//const char* password = "adt@12345";

//Maryville network
const char* ssid = "VRX";
const char* password = "textinsert";


// MQTT Broker
const char* mqtt_server = "public.cloud.shiftr.io";
const char* mqtt_user = "public";
const char* mqtt_password = "public";

// MQTT Topics
// User must modify the device serial number. In this case change the part "USA4" as approprate.
const char* subscribe_Alarm_Topic = "KRAKE_20240421_USA4_ALM";
const char* publish_Ack_Topic = "KRAKE_20240421_USA4_ACK";

// LED Pins
// To Do Define ESP32 pins for actual Krake LEDs
// Homework2 Assembly, akd PMD LAMPS aka Other LEDs
const int LED_D9 = 23;  // Mute1 LED on PMD
const int LAMP1 = 15;   // D5 cold food
const int LAMP2 = 4;    // D3 baby crying
const int LAMP3 = 5;    // D8 high BP1
const int LAMP4 = 18;   // D7 shield failure
const int LAMP5 = 19;   // D6 lost sock

// Initialize WiFi and MQTT clients
WiFiClient espClient;
PubSubClient client(espClient);

//Functions
void proccessPayloadOnLamps(String &payload) {
  //void proccessPayloadOnLamps(char* &payload) {
  // Proccess payload on LAMPs
  digitalWrite(LAMP1, LOW);
  digitalWrite(LAMP2, LOW);
  digitalWrite(LAMP3, LOW);
  digitalWrite(LAMP4, LOW);
  digitalWrite(LAMP5, LOW);

  if (payload < "1") {
    //Turn off all LAMPS
  } else if (payload == "a1MessageFromProcessing_PMD:1") {
    //Turn on only LAMP 1
    digitalWrite(LAMP1, HIGH);
    digitalWrite(LAMP2, LOW);
    digitalWrite(LAMP3, LOW);
    digitalWrite(LAMP4, LOW);
    digitalWrite(LAMP5, LOW);
  } else if (payload == "a2MessageFromProcessing_PMD:2") {
    //Turn on only LAMP 2
    digitalWrite(LAMP1, LOW);
    digitalWrite(LAMP2, HIGH);
    digitalWrite(LAMP3, LOW);
    digitalWrite(LAMP4, LOW);
    digitalWrite(LAMP5, LOW);
  } else if (payload == "a3MessageFromProcessing_PMD:3") {
    //Turn on only LAMP 3
    digitalWrite(LAMP1, LOW);
    digitalWrite(LAMP2, LOW);
    digitalWrite(LAMP3, HIGH);
    digitalWrite(LAMP4, LOW);
    digitalWrite(LAMP5, LOW);
  } else if (payload == "a4MessageFromProcessing_PMD:4") {
    //Turn on only LAMP 4
    digitalWrite(LAMP1, LOW);
    digitalWrite(LAMP2, LOW);
    digitalWrite(LAMP3, LOW);
    digitalWrite(LAMP4, HIGH);
    digitalWrite(LAMP5, LOW);
  } else if (payload == "a5MessageFromProcessing_PMD:5") {
    //Turn on only LAMP 5
    digitalWrite(LAMP1, LOW);
    digitalWrite(LAMP2, LOW);
    digitalWrite(LAMP3, LOW);
    digitalWrite(LAMP4, LOW);
    digitalWrite(LAMP5, HIGH);
  } else if (payload == "a6MessageFromProcessing_PMD:6") {
    //Turn on all lamps
    turnOnAllLamps();
  }// end parsing message
}// end proccessPayloadOnLamps


// A periodic message identifying the subscriber (Krake) is on line.

void publishOnLineMsg(void) {
  static unsigned long lastMillis = 0;  // Sets timing for periodic MQTT publish message
  // publish a message roughly every second.
  if (millis() - lastMillis > 10000) {
    lastMillis = millis();
    client.publish(publish_Ack_Topic, " is online");
    digitalWrite(LED_D9, !digitalRead(LED_D9));   // Toggle
  }
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP32_Receiver", mqtt_user, mqtt_password)) {
      Serial.println("success!");
      client.subscribe(subscribe_Alarm_Topic);    // Subscribe to GPAD API alarms
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(5000);
    }
  }
}

// Function to turn on all lamps
void turnOnAllLamps() {
  digitalWrite(LED_D9, HIGH);  // Turn on Mute1 LED
}
//end Functions

// Handeler for MQTT subscrived messages
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  if (String(topic) ==  subscribe_Alarm_Topic) {
    Serial.println("Got MessageFromProcessing_PMD");
    proccessPayloadOnLamps(message);  // Change LAMPS baised on the payload
  }
}//end call back

//Call backs

//end Call backs

void setup() {
  const int LED_BUILTIN = 2;    // ESP32 Kit
  pinMode(LED_BUILTIN, OUTPUT);      // set the LED pin mode
  digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)

  Serial.begin(BAUDRATE);
  delay(500);
  //Serial splash
  Serial.println(F("==================================="));
  Serial.print(PROG_NAME);
  Serial.println(VERSION);
  Serial.println(MODEL_NAME);
  Serial.println(DEVICE_UNDER_TEST);
  Serial.print("Alarm Topic: ");
  Serial.println(subscribe_Alarm_Topic);
  Serial.print(F("Compiled at: "));
  Serial.println(F(__DATE__ " " __TIME__));  //compile date that is used for a unique identifier
  Serial.println(LICENSE);
  Serial.println(F("==================================="));
  Serial.println();

  // Set LED pins as outputs
  pinMode(LED_D9, OUTPUT);
  pinMode(LAMP1, OUTPUT);
  pinMode(LAMP2, OUTPUT);
  pinMode(LAMP3, OUTPUT);
  pinMode(LAMP4, OUTPUT);
  pinMode(LAMP5, OUTPUT);

  // Turn off all LEDs initially
  turnOnAllLamps();

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  Serial.begin(115200);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB
  }
  digitalWrite(LED_BUILTIN, LOW);
}// end setup()

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  publishOnLineMsg();
  wink();
}//end loop();
