#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <DS3231.h>
#include <string.h>

#define DHTTYPE DHT11
#define DHTPIN  2
#define DISPLAY_BACKLIGHT_PIN  12
#define DISPLAY_BACKLIGHT_LEVEL_DAY  1023
#define DISPLAY_BACKLIGHT_LEVEL_NIGHT  92

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
DS3231 ds_clock;

int humidity, temp;

unsigned long previousMillis = 0;
const long sensorReadInterval = 2000;

boolean wifi_connecting = false, wifi_connected = false, wifi_error = false;
boolean mqtt_connecting = false, mqtt_connected = false, mqtt_error = false;

/*
    Run this in setup to set date and time to RTC module.

    ds_clock.setYear(17);
    ds_clock.setMonth(5);
    ds_clock.setDate(4);
    ds_clock.setDoW(3);
    ds_clock.setHour(22);
    ds_clock.setMinute(23);
    ds_clock.setSecond(20);
 */
void setup() {
  delay(100);
  Serial.begin(115200);
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  dht.begin();
  
  lcd.init();
  lcd.display();
  lcd.backlight();
  lcd.clear();

  pinMode(DISPLAY_BACKLIGHT_PIN, OUTPUT);
  analogWrite(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_LEVEL_NIGHT);
}

void loop() {
  bool valuesUpdated = updateHealthValues();
  
  setup_wifi();

  if (wifi_connected) {
    setup_mqtt();
  }

  if (valuesUpdated && mqtt_connected) {
      sendLastValues();
  }

  updateDisplay();

//  debugPrint();
  client.loop();
  delay(200);
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

void updateDisplay() {
  // Temp
  lcd.setCursor(0, 0);
  lcd.print(temp, DEC);    
  lcd.print(" C");

  // Humidity
  lcd.setCursor(0, 1);
  lcd.print(humidity, DEC);
  lcd.print(" %");

  // Time
  bool h12, am;
  byte hour = ds_clock.getHour(h12, am);
  byte minute = ds_clock.getMinute();    
  
  lcd.setCursor(11, 0);
  if (hour < 10) {
    lcd.print(0);
  }
  
  lcd.print(hour, DEC);
  lcd.print(":");

  if (minute < 10) {
    lcd.print(0);
  }
  
  lcd.print(minute, DEC);

  // Status
  lcd.setCursor(12, 1);
  if (!wifi_connected) {
    lcd.print("WIFI");
  } else if (!mqtt_connected) {
    lcd.print("MQTT");
  } else {    
    lcd.print("    ");
  }

  updateDisplayBacklightLevel(hour);
}

void updateDisplayBacklightLevel(byte hour) {
  int level;
  
  if (hour > 7 && hour < 21) {
    level = DISPLAY_BACKLIGHT_LEVEL_DAY;
  } else {
    level = DISPLAY_BACKLIGHT_LEVEL_NIGHT;
  }

  analogWrite(DISPLAY_BACKLIGHT_PIN, level);
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

  // float to int conversion here
  temp = dht.readTemperature();
  humidity = dht.readHumidity();

  if (temp < 0 || temp > 99) {
    temp = 0;
  }

  if (humidity < 0 || humidity > 99) {
    humidity = 0;
  }
//  
//  if (isnan(humidity) || isnan(temp)) {
//    Serial.println("Failed to read from DHT sensor!");
//  }

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

  bool h12, am;
  Serial.print(ds_clock.getHour(h12, am));
  Serial.print(":");
  
  Serial.print(ds_clock.getMinute());
  Serial.print(":");
  
  Serial.println(ds_clock.getSecond());
  Serial.println();
}

