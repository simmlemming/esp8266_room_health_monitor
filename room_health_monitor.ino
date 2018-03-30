#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <RTC_DS3231_DST.h>
#include <ArduinoJson.h>

#define DHTTYPE DHT11
#define DHTPIN  2
#define DISPLAY_BACKLIGHT_PIN  12
#define PHOTORESISTOR_PIN  A0
#define DISPLAY_BACKLIGHT_LEVEL_DAY  1023
#define DISPLAY_BACKLIGHT_LEVEL_NIGHT  80

const char* ssid = "Cloud_2";
const char* ssid_password = "";

const char* mqtt_server = "192.168.0.110";
const char* mqtt_device_name = "temp_hum_01";

const char* outTopic = "home/out";
const char* inTopic = "home/in";
const char* debugTopic = "home/debug";

const char* tempSensorName = "temp_sensor_01";
const char* humiditySensorName = "humidity_sensor_01";
const char* roomName = "brown_bedroom";

const char* CMD_STATE = "state";
const char* NAME_ALL = "all";

WiFiClient espClient;
PubSubClient client(espClient);
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2); // i2c address, columns, lines
RTC_DS3231_DST rtc;

int humidity, temp, light;
int display_backlight_level = DISPLAY_BACKLIGHT_LEVEL_NIGHT;

int lastSentHumidity = 0, lastSentTemp = 0;

boolean wifi_connecting = false, wifi_connected = false, wifi_error = false;
boolean mqtt_connecting = false, mqtt_connected = false, mqtt_error = false;

long wifiStrength = 0;
long lastSentWifiStrength = 0;
/*
    Run this in setup to set date and time to RTC module.
    IMPORTANT: lcd.init() must be called before setting time (for unknown reason)
    
    delay(1000);
    ds_clock.setYear(17);
    ds_clock.setMonth(5);
    ds_clock.setDate(4);
    ds_clock.setDoW(3);
    ds_clock.setHour(22);
    ds_clock.setMinute(23);
    ds_clock.setSecond(20); // +40 sec for compilation and uploading
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
  analogWrite(DISPLAY_BACKLIGHT_PIN, display_backlight_level);
}

void loop() {
  setup_wifi();

  if (wifi_connected) {
    setup_mqtt();
  }

  bool valuesUpdated = updateValues();
  
  if (valuesUpdated && mqtt_connected) {
      sendLastValues();
  }

  updateDisplay();

//  debugPrint();
  client.loop();
  delay(500);
}

void setup_mqtt() {
  mqtt_connected = client.connected();

  if (mqtt_connected) {
    return;
  }

  if (!mqtt_connecting) {
    mqtt_connecting = true;

    if (client.connect(mqtt_device_name)) {
      mqtt_error = false;
      mqtt_connecting = false;
      mqtt_connected = true;
      client.subscribe(inTopic);
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
  if (!eq(topic, inTopic)) {
    return;
  }
  
  DynamicJsonBuffer jsonBuffer;
  char jsonMessageBuffer[256];

  JsonObject& message = jsonBuffer.parseObject(payload);

  if (!message.success())
  {
    client.publish(debugTopic, mqtt_device_name);
    client.publish(debugTopic, "cannot parse message");
    return;
  }

//  message.printTo(jsonMessageBuffer, sizeof(jsonMessageBuffer));  
//  Serial.print("<-- ");
//  Serial.println(jsonMessageBuffer);

  const char* messageName = message["name"];
  if (!eq(messageName, tempSensorName) && !eq(messageName, humiditySensorName) && !eq(messageName, NAME_ALL)) {
    return;
  }

  const char* messageCmd = message["cmd"];
  if (eq(messageCmd, CMD_STATE)) {
    sendLastValues();
  }
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
  DateTime now = rtc.now();
  int hour = now.hour();
  int minute = now.minute();    
  
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

//    lcd.print(light, DEC);
//    if (light < 1000) {
//      lcd.print(" ");
//    }
  }
  
  updateDisplayBacklightLevel(hour);
}

void updateDisplayBacklightLevel(byte hour) {
  int new_level;
  
  if (hour > 6 && hour < 21) {
    new_level = DISPLAY_BACKLIGHT_LEVEL_DAY;
  } else {
    new_level = DISPLAY_BACKLIGHT_LEVEL_NIGHT;
  }

  if (new_level != display_backlight_level) {
    display_backlight_level = new_level;
    analogWrite(DISPLAY_BACKLIGHT_PIN, new_level);
  }
}

void sendLastValues() {
  sendTemp();
  sendHumidity();

  lastSentWifiStrength = wifiStrength;
}

void sendTemp() {
  DynamicJsonBuffer jsonBuffer;
  char jsonMessageBuffer[256];

  JsonObject& root = jsonBuffer.createObject();
  root["name"] = tempSensorName;
  root["type"] = "temp_sensor";
  root["room"] = roomName;
  root["signal"] = wifiStrength;
  root["state"] = 1;
  root["value"] = temp;

  root.printTo(jsonMessageBuffer, sizeof(jsonMessageBuffer));
  
  Serial.println(jsonMessageBuffer);
  client.publish(outTopic, jsonMessageBuffer);
  
  lastSentTemp = temp;
}

void sendHumidity() {
  DynamicJsonBuffer jsonBuffer;
  char jsonMessageBuffer[256];

  JsonObject& root = jsonBuffer.createObject();
  root["name"] = humiditySensorName;
  root["type"] = "humidity_sensor";
  root["room"] = roomName;
  root["signal"] = wifiStrength;
  root["state"] = 1;
  root["value"] = humidity;
  
  root.printTo(jsonMessageBuffer, sizeof(jsonMessageBuffer));
  
  Serial.println(jsonMessageBuffer);
  client.publish(outTopic, jsonMessageBuffer);
  
  lastSentHumidity = humidity;
}

bool updateValues() {
  // float to int conversion here
  int t = dht.readTemperature();
  int h = dht.readHumidity();

  if (t > 0 && t < 99) {
    temp = t;
  }

  if (h > 0 && h < 99) {
    humidity = h;
  }

  if (wifi_connected) {
    wifiStrength = WiFi.RSSI();
  } else {
    wifiStrength = 0;
  }

  return (lastSentHumidity != humidity || lastSentTemp != temp|| abs(lastSentWifiStrength - wifiStrength) > 3);
}

boolean eq(const char* a1, const char* a2) {
  return strcmp(a1, a2) == 0;
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

  DateTime now = rtc.now();
  Serial.print(now.hour());
  Serial.print(":");
  
  Serial.print(now.minute());
  Serial.print(":");
  
  Serial.println(now.second());
  Serial.println();
}

