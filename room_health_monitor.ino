/*
 *  This sketch sends data via HTTP GET requests to data.sparkfun.com service.
 *
 *  You need to get streamId and privateKey at data.sparkfun.com and paste them
 *  below. Or just customize this script to talk to other HTTP servers.
 *
 */

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <string.h>

#define DHTTYPE DHT11
#define DHTPIN  4

const char* ssid = "Cloud_2";
const char* ssid_password = "";

const char* mqtt_server = "192.168.0.110";
const char* clientID = "health_monitor_nicole";
const char* outTopic = "nicole/health";
const char* inTopic = "nicole/health";

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);

float humidity, temp;

unsigned long previousMillis = 0;
const long sensorReadInterval = 2000;

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, ssid_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Conver the incoming byte array to a string
//  payload[length] = '\0'; // Null terminator used to terminate the char array
//  String message = (char*)payload;
//
//  Serial.print("Message arrived on topic: [");
//  Serial.print(topic);
//  Serial.print("], ");
//  Serial.println(message);

//  if(message == "temperature, c"){
//    gettemperature();
//    Serial.print("Sending temperature:");
//    Serial.println(temp_c);
//    dtostrf(temp_c , 2, 2, msg);
//    client.publish(outTopic, msg);
//  } else if (message == "humidity"){
//    gettemperature();
//    Serial.print("Sending humidity:");
//    Serial.println(humidity);
//    dtostrf(humidity , 2, 2, msg);
//    client.publish(outTopic, msg);
//  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientID)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(outTopic, clientID);
      // ... and resubscribe
      client.subscribe(inTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 1 seconds");
      // Wait 1 seconds before retrying
      delay(1000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  dht.begin();
}


void loop() {

  if (!client.connected()) {
    reconnect();
  }
  
  client.loop();

  bool valuesUpdated = updateHealthValues();
  if (valuesUpdated) {
    sendLastValues();
  }
}

void sendLastValues() {

  char t[6];
  char h[6];
  dtostrf(temp , 2, 2, t);
  dtostrf(humidity , 2, 2, h);

  char message[96] = {0};
  
  strcat(message, "{\"t\" = ");
  strcat(message, t);
  strcat(message, ", \"h\" = ");
  strcat(message, h);
  strcat(message, "}");

  Serial.println(message);

  client.publish(outTopic, message);
}

bool updateHealthValues() {
  unsigned long currentMillis = millis();
 
  if (currentMillis - previousMillis < sensorReadInterval) {
    return false;
  }

  previousMillis = currentMillis;   

  temp = dht.readTemperature();
  humidity = dht.readHumidity();
  
  if (isnan(humidity) || isnan(temp)) {
    Serial.println("Failed to read from DHT sensor!");
  }

  return true;
}

