/*You Can Create YOUR own Geolinker API by going to 
https://circuitdigest.com/tutorial/gps-visualizer-for-iot-based-gps-tracking-projects#what-is-circuitdigest-cloud   and follow the steps*/


#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <HTTPClient.h>

/* ---------- GPS ---------- */
TinyGPSPlus gps;
HardwareSerial gpsSerial(2); // RX2=16, TX2=17

/* ---------- Wi-Fi ---------- */
const char* ssid = "ESP32-GPS";
const char* pass = "12345678";
AsyncWebServer server(80);

/* ---------- GeoLinker Config ---------- */
const char* geoAPI = "https://api.circuitdigest.cloud/geolinker";
const char* apiKey = "YOUR_API_KEY";       // 🛠️ Replace with your actual key
const char* deviceID = "ESP32_GPS_001";    // 🔖 Give your tracker a name

/* ---------- State ---------- */
bool liveMode = false;

/* ---------- HTML ---------- */
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8" />
  <title>ESP32 GPS Tracker</title>
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <style>
    body{font-family:sans-serif;padding:32px;text-align:center}
    button{padding:12px 24px;font-size:16px;border:none;border-radius:8px;cursor:pointer}
    #liveBtn{background:#0077ff;color:#fff}
    #liveBtn.active{background:#ff3b30}
    #status{margin-top:20px;font-size:18px}
  </style>
</head>
<body>
  <h2>ESP32 GPS Tracker</h2>
  <button id="liveBtn" onclick="toggleLive()">🔴 Start Live</button>
  <div id="status">Status: Idle</div>

  <script>
    async function toggleLive(){
      const btn=document.getElementById("liveBtn");
      const stat=document.getElementById("status");
      const resp=await fetch("/toggle");
      const isActive=await resp.text();

      if(isActive==="1"){
        btn.textContent="⏹ Stop Live";
        btn.classList.add("active");
        stat.textContent="Status: Live tracking ON";
      }else{
        btn.textContent="🔴 Start Live";
        btn.classList.remove("active");
        stat.textContent="Status: Idle";
      }
    }

    fetch("/status").then(r=>r.text()).then(t=>{
      if(t==="1") toggleLive();
    });
  </script>
</body>
</html>
)rawliteral";

/* ---------- GPS ---------- */
bool feedGPS() {
  while (gpsSerial.available()) gps.encode(gpsSerial.read());
  return gps.location.isValid();
}

/* ---------- GeoLinker Push ---------- */
void postToGeoLinker() {
  if (!gps.location.isValid()) return;

  HTTPClient http;
  http.begin(geoAPI);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-KEY", apiKey);

  String payload = "{";
  payload += "\"device_id\":\"" + String(deviceID) + "\",";
  payload += "\"timestamp\":[\"" + String(millis()) + "\"],";
  payload += "\"lat\":[" + String(gps.location.lat(), 6) + "],";
  payload += "\"long\":[" + String(gps.location.lng(), 6) + "],";
  payload += "\"battery\":[100],";
  payload += "\"payload\":[{ \"sat\":" + String(gps.satellites.value()) + " }]";
  payload += "}";

  int code = http.POST(payload);
  Serial.println("GeoLinker POST: " + String(code));
  http.end();
}

/* ---------- Web ---------- */
void setupWeb() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
    r->send_P(200, "text/html", index_html);
  });

  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *r){
    liveMode = !liveMode;
    r->send(200, "text/plain", liveMode ? "1" : "0");
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *r){
    r->send(200, "text/plain", liveMode ? "1" : "0");
  });

  server.begin();
}

/* ---------- Arduino ---------- */
void setup() {
  Serial.begin(115200);
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);

  WiFi.softAP(ssid, pass);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  setupWeb();
}

void loop() {
  if (liveMode && feedGPS()) {
    postToGeoLinker();
    delay(1000);  // 1-second update
  }
}
