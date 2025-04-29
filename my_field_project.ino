#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <DHT.h>

// Wi-Fi credentials
const char* ssid = "realme 10 Pro 5G";
const char* password = "        ";

// DHT setup
#define DHTPIN D4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Pins
#define LED_PIN D2
#define TV_PIN D3
#define FAN_PIN D1

// States
bool ledState = false;
bool tvState = false;
bool fanState = false;
bool fanAutoMode = true;

float temp = 0.0, hum = 0.0;
unsigned long lastSensorUpdate = 0;
const unsigned long sensorInterval = 250;

ESP8266WebServer server(80);
WebSocketsServer webSocket(81);

void setup() {
  Serial.begin(9600);
  dht.begin();

  pinMode(LED_PIN, OUTPUT);
  pinMode(TV_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(TV_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

  server.on("/", handleMainPage);
  server.on("/dashboard", handleRoot);
  server.on("/led", handleLED);
  server.on("/tv", handleTV);
  server.on("/fan", handleFan);
  server.on("/fan/mode", handleFanMode);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
}

void loop() {
  server.handleClient();
  webSocket.loop();

  unsigned long currentMillis = millis();
  if (currentMillis - lastSensorUpdate >= sensorInterval) {
    lastSensorUpdate = currentMillis;

    float newTemp = dht.readTemperature();
    float newHum = dht.readHumidity();
    if (!isnan(newTemp) && !isnan(newHum)) {
      temp = newTemp;
      hum = newHum;
    }

    if (fanAutoMode) {
      fanState = (temp >= 30);
      digitalWrite(FAN_PIN, fanState ? HIGH : LOW);
    }

    String json = "{\"temp\":" + String(temp) + ",\"hum\":" + String(hum) + "}";
    webSocket.broadcastTXT(json);
  }
}
void handleMainPage() {
  String html = R"rawliteral(
    <html><head>
    <title>Smart Home Sense</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
      body { background-color: pink; font-family: Arial; text-align: center; padding: 10vh 5vw; margin: 0; }
      h1 { font-size: 8vw; margin-bottom: 2vh; }
      p { font-size: 5vw; }
      button {
        font-size: 5vw; padding: 2vh 5vw; background-color: #f5425d; color: white;
        border: none; border-radius: 12px; cursor: pointer;
      }
    </style></head>
    <body>
      <h1>Smart Home Sense</h1>
      <p>Welcome! Enhance your smart living experience.</p>
      <button onclick="location.href='/dashboard'">Go To Dashboard</button>
    </body></html>
  )rawliteral";
  server.send(200, "text/html", html);
}
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Smart Home Sense</title>
<style>
body { font-family: Arial; background: #ffb6c1; margin:0; padding:0; }
h1 { text-align: center; color: #444; padding: 10px; }
.panel { background: #f8d7da; margin: 15px auto; padding: 20px; max-width: 350px;
  border-radius: 12px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); }
.panel h2 { margin-top: 0; font-size: 24px; color: #333; }
.btn { display: block; width: 100%; padding: 15px; font-size: 18px;
  margin-top: 10px; border: none; border-radius: 8px; color: #fff;
  cursor: pointer; transition: background 0.3s ease; }
.on { background: #28a745; }
.off { background: #dc3545; }
.neutral { background: #6c757d; }
.status { margin: 10px 0; font-size: 18px; }
</style></head><body>
<h1>Smart Home Sense Dashboard</h1>

<div class="panel">
  <h2>LED</h2>
  <button class="btn %LED_CLASS%" onclick="location.href='/led'">%LED_TEXT%</button>
</div>

<div class="panel">
  <h2>TV</h2>
  <button class="btn %TV_CLASS%" onclick="location.href='/tv'">%TV_TEXT%</button>
</div>

<div class="panel">
  <h2>Fan</h2>
  <button class="btn neutral" onclick="location.href='/fan/mode'">Switch to %FAN_MODE%</button>
  <button class="btn %FAN_CLASS%" onclick="location.href='/fan'">%FAN_TEXT%</button>
  <div class="status" id="status">Temp: -- °C | Hum: -- %</div>
</div>

<script>
let ws = new WebSocket('ws://' + location.hostname + ':81/');
ws.onmessage = function(event) {
  let data = JSON.parse(event.data);
  document.getElementById("status").innerText = 
    "Temp: " + data.temp + " °C | Hum: " + data.hum + " %";
};
</script>
</body></html>
)rawliteral";

  html.replace("%LED_CLASS%", ledState ? "off" : "on");
  html.replace("%LED_TEXT%", ledState ? "Turn OFF" : "Turn ON");
  html.replace("%TV_CLASS%", tvState ? "off" : "on");
  html.replace("%TV_TEXT%", tvState ? "Turn OFF" : "Turn ON");
  html.replace("%FAN_CLASS%", fanState ? "off" : "on");
  html.replace("%FAN_TEXT%", fanAutoMode ? (fanState ? "Fan ON (Auto)" : "Fan OFF (Auto)")
                                          : (fanState ? "Turn OFF" : "Turn ON"));
  html.replace("%FAN_MODE%", fanAutoMode ? "Manual Mode" : "Auto Mode");

  server.send(200, "text/html", html);
}

void handleLED() {
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  server.sendHeader("Location", "/dashboard");
  server.send(303);
}

void handleTV() {
  tvState = !tvState;
  digitalWrite(TV_PIN, tvState ? HIGH : LOW);
  server.sendHeader("Location", "/dashboard");
  server.send(303);
}

void handleFan() {
  if (!fanAutoMode) {
    fanState = !fanState;
    digitalWrite(FAN_PIN, fanState ? HIGH : LOW);
  }
  server.sendHeader("Location", "/dashboard");
  server.send(303);
}

void handleFanMode() {
  fanAutoMode = !fanAutoMode;
  server.sendHeader("Location", "/dashboard");
  server.send(303);
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  // Not used here, but kept for future controls if needed
}
