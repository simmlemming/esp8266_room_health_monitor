#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <string.h>

#define DHTTYPE DHT11
#define DHTPIN  2

const char* ssid = "Cloud_2";
const char* ssid_password = "";

const char* mqtt_server = "192.168.0.110";
const char* clientID = "health_monitor_nicole";
const char* outTopic = "nicole/health";
const char* inTopic = "nicole/health";

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); // i2c address, columns, lines

float humidity, temp;

unsigned long previousMillis = 0;
const long sensorReadInterval = 2000;

boolean wifi_connecting = false, wifi_connected = false, wifi_error = false;
boolean mqtt_connecting = false, mqtt_connected = false, mqtt_error = false;

void setup() {
  Serial.begin(115200);
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  dht.begin();
  
  lcd.init();
  lcd.display();
  lcd.backlight();

  lcd.setCursor(3,0);
  lcd.clear();
  lcd.print("Started");
}

void loop() {
  bool valuesUpdated = updateHealthValues();

  if (valuesUpdated) {
    displayLastValues();
    if (mqtt_connected) {
      sendLastValues();
    }
  }
  
  setup_wifi();

  if (wifi_connected) {
    setup_mqtt();
  }

//  debugPrint();
  displayLastValues();

  client.loop();
  delay(1000);
}

void setup_mqtt() {
  mqtt_connected = client.connected();

  if (mqtt_connected) {
    return;
  }

  if (!mqtt_connecting) {
    mqtt_connecting = true;

    if (client.connect(clientID)) {
      mqtt_error = false;
      mqtt_connecting = false;
      mqtt_connected = true;
//      client.subscribe(inTopic);
    } else {
      mqtt_error = true;
      mqtt_connecting = false;
      mqtt_connected = false;
    }
  }
}

void setup_wifi() {
  wifi_connected = WiFi.status() == WL_CONNECTED;
  wifi_error = WiFi.status() == WL_CONNECT_FAILED;
  if (wifi_connected || wifi_error) {
    wifi_connecting = false;
  }
  
  if (wifi_connected) {
    return;
  }

  if (!wifi_connecting) {
    wifi_connecting = true;  
    WiFi.begin(ssid, ssid_password);    
  }
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

void displayLastValues() {
  char t[3];
  char h[2];
  dtostrf(temp, 2, 0, t);
  dtostrf(humidity, 2, 0, h);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(t);
  
  lcd.setCursor(3, 0);
  lcd.print("C");

  lcd.setCursor(0, 1);
  lcd.print(h);
  
  lcd.setCursor(3, 1);
  lcd.print("%");

  if (wifi_connected) {
    lcd.setCursor(9, 0);
    lcd.print(ssid);
  } else {
    lcd.setCursor(10, 0);
    lcd.print("WIFI -");
  }

  lcd.setCursor(9, 1);
  if (mqtt_connected) {
    lcd.print("MQTT OK");
  } else {
    lcd.print("MQTT  -");
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

void debugPrint() {
  Serial.print("wifi connecting = ");
  Serial.println(wifi_connecting);

  Serial.print("wifi connected = ");
  Serial.println(wifi_connected);
  
  Serial.print("wifi error = ");
  Serial.println(wifi_error);

  Serial.print("mqtt connecting = ");
  Serial.println(mqtt_connecting);

  Serial.print("mqtt connected = ");
  Serial.println(mqtt_connected);
  
  Serial.print("mqtt error = ");
  Serial.println(mqtt_error);

  Serial.print("mqtt state = ");
  Serial.println(client.state());

  Serial.println();
}

