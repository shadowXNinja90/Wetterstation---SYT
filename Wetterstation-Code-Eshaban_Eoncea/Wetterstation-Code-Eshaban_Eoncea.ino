#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

// Display Konfiguration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_SDA 8
#define OLED_SCL 9
#define ADC_PIN 7

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Sensor Konfiguration
#define DHTPIN 4          // DHT11 an GPIO 4
#define DHTTYPE DHT11     // Sensor-Typ DHT11
#define LDR_PIN 5         // LDR an GPIO5 (analog)
DHT dht(DHTPIN, DHTTYPE);

// Netzwerk
const char* ssid = "hedgehog";
const char* password = "hedgehog";
const char* discordWebhook = "https://discord.com/api/webhooks/your_webhook";
const char* serverName = "http://192.168.1.141/insert_temp.php";

// NTP Client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3600, 60000);

// Funktionen
int getAverageLight(int pin, int samples = 10) {
  long sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogRead(pin);
    delay(10);
  }
  return sum / samples;
}

void sendToDiscord(String message) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(discordWebhook);
  http.addHeader("Content-Type", "application/json");
  
  int httpCode = http.POST("{\"content\":\"" + message + "\"}");
  
  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Nachricht an Discord gesendet!");
  } else {
    Serial.println("Fehler beim Senden: " + String(httpCode));
  }
  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_11);
  // Display initialisieren
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    while(true);
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  
  // Sensoren
  dht.begin();
  
  // WiFi
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  Serial.print("Verbinde mit WLAN");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); // Blink LED
  }
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("\nWLAN verbunden!");
  
  // NTP
  timeClient.begin();
  timeClient.update();
}

void loop() {
  timeClient.update();
  String formattedTime = timeClient.getFormattedTime();
  
  // Sensoren lesen
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  int lux = map(getAverageLight(LDR_PIN), 0, 4095, 1000, 0);

  // Display aktualisieren
  display.clearDisplay();
  
  // Temperatur
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Temp: ");
  display.setTextSize(2);
  display.setCursor(0, 10);
  display.print(t, 1);
  display.print(" °C");
  
  // Luftfeuchtigkeit
  display.setTextSize(1);
  display.setCursor(70, 0);
  display.print("Hum:");
  display.setTextSize(2);
  display.setCursor(70, 10);
  display.print(h, 1);
  display.print("%");
  
  // Licht
  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print("Licht: ");
  display.setTextSize(2);
  display.setCursor(0, 50);
  display.print(lux);
  display.print(" lx");
  
  display.display();

  // Serielle Ausgabe
  Serial.println("Zeit: " + formattedTime);
  Serial.println("Temp: " + String(t) + "°C, Hum: " + String(h) + "%, Lux: " + String(lux));

  // Discord Nachricht (stündlich)
  static unsigned long lastDiscordSend = 0;
  if (millis() - lastDiscordSend > 3600000) {
    String msg = "🌡 Aktuelle Daten: " + String(t) + "°C, 💧 " + String(h) + "%\nUhrzeit: " + formattedTime;
    sendToDiscord(msg);
    lastDiscordSend = millis();
  }

  // Webserver senden (alle 5 Minuten)
  if (WiFi.status() == WL_CONNECTED && millis() % 300000 < 1000) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String postData = "temperatur=" + String(t) + "&feuchtigkeit=" + String(h);
    int httpCode = http.POST(postData);
    Serial.println("Server Response: " + String(httpCode));
    http.end();
  }
  // Sleep Mode
   static unsigned long lastMillis = 0;
  if(millis() - lastMillis > 10000) {
    Serial.println("Gehe in Deep Sleep für 30 Sekunden...");
    delay(100);  // kleine Pause vor Schlaf
    esp_sleep_enable_timer_wakeup(30 * 1000000); // 30 Sekunden
    esp_deep_sleep_start();
  }
}


  delay(1000); // 1 Sekunde warten
}
