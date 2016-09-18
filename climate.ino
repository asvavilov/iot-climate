// see config.example.h
#include "./config.h"

#include <Wire.h>
#include <SoftwareSerial.h>;
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
// https://github.com/mathworks/thingspeak-arduino
#include <ThingSpeak.h>

// I2C (for LCD):
// power - 5v
// D3/GPIO0 - SDA display, D4/GPIO2 - SCL display
#define SDA_PIN 0
#define SCL_PIN 2

// DHT11:
// power - 3.3v or 5v
// D5/GPIO14
#define DHT_PIN 14

// MH-Z19:
// power - 5v
// D6 - TX sensor, D7 - RX sensor
#define CO2_TX D6
#define CO2_RX D7

int wifiStatus = WL_IDLE_STATUS;

SoftwareSerial co2Serial(CO2_TX, CO2_RX);
DHT dht(DHT_PIN, DHT11);
// 0x27 | 0x3F
LiquidCrystal_I2C lcd (0x3F, 16, 2);
WiFiClient wifiClient;

/*
  dht_data:
  0 - hudimity
  1 - temperature
  2 - heat index
*/
bool readDHT(float *dht_data)
{
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return false;
  }

  // Compute heat index in Celsius (isFahreheit = false)
  float hi = dht.computeHeatIndex(t, h, false);

  dht_data[0] = h;
  dht_data[1] = t;
  dht_data[2] = hi;

  return true;
}

int readCO2()
{
  byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  char response[9];

  co2Serial.write(cmd, 9); //request PPM CO2
  co2Serial.readBytes(response, 9);

  byte crc = 0;
  for (int i = 1; i < 8; i++)
  {
    crc += response[i];
  }
  crc = 255 - crc;
  crc++;

  if (response[8] != crc)
  {
    Serial.println("Wrong crc from co2 sensor!");
    return -1;
  }
  if (response[0] != 0xFF)
  {
    Serial.println("Wrong starting byte from co2 sensor!");
    return -1;
  }
  if (response[1] != 0x86)
  {
    Serial.println("Wrong command from co2 sensor!");
    return -1;
  }

  int responseHigh = (int) response[2];
  int responseLow = (int) response[3];
  int ppm = (256 * responseHigh) + responseLow;
  return ppm;
}

void setup()
{
  Serial.begin(9600);

  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.begin();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Welcome to");
  lcd.setCursor(0, 1);
  lcd.print("climate system");
  // если маленькое время, то почему-то не подключается к wifi
  delay(15000);

  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(SSID_NAME);
  int connect_try = 0;
  while (wifiStatus != WL_CONNECTED) {
    connect_try++;
    lcd.setCursor(0, 0);
    lcd.print("Connection (" + String(connect_try) + ")");
    wifiStatus = WiFi.begin(SSID_NAME, SSID_PASS);
    delay(10000);
  }

  ThingSpeak.begin(wifiClient);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Network");
  lcd.setCursor(0, 1);
  lcd.print("connected");

  co2Serial.begin(9600);

  delay(5000);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("CO2PPM TC HIC H%");
  
  // в примерах этого не было, но без него после переподключения
  // начинается не со стартового байта (какой-то сдвиг ответа)
  co2Serial.flush();
}

void loop()
{
  int co2_ppm = readCO2();

  float dht_data[] = { -1, -1, -1};
  bool dht_status = readDHT(dht_data);

  Serial.print("CO2: ");
  Serial.print(co2_ppm);
  Serial.print(" ppm\t");
  Serial.print("Humidity: ");
  Serial.print(dht_data[0]);
  Serial.print(" %\t");
  Serial.print("Temperature: ");
  Serial.print(dht_data[1]);
  Serial.print(" *C\t");
  Serial.print("Heat index: ");
  Serial.print(dht_data[2]);
  Serial.print(" *C\n");

  lcd.setCursor(0, 1);
  lcd.print("                ");
  lcd.setCursor(0, 1);
  lcd.print(co2_ppm);
  lcd.setCursor(7, 1);
  lcd.print(dht_data[1], 0);
  lcd.setCursor(10, 1);
  lcd.print(dht_data[2], 0);
  lcd.setCursor(14, 1);
  lcd.print(dht_data[0], 0);

  ThingSpeak.setField(TS_F_CO2, co2_ppm);
  ThingSpeak.setField(TS_F_TEMPERATURE, dht_data[1]);
  ThingSpeak.setField(TS_F_HEAT_INDEX, dht_data[2]);
  ThingSpeak.setField(TS_F_HUDIMITY, dht_data[0]);
  ThingSpeak.writeFields(TS_CH_ID, TS_CH_WKEY);

  delay(10000);
}

