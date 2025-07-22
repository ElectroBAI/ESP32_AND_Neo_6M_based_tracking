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
const char* apiKey = "YOUR_API_KEY";          // 🛠️ Replace with your actual key
const char* deviceID = "ESP32_GPS_001";       // 🏷️ Unique name for tracker

/* ---------- State ---------- */
bool liveMode = false;

/* ---------- Styled Embedded Web ---------- */
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <title>ESP32 GeoLinker Tracker</title>
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <style>
    :root {
      --bg:#f5f5f5; --bg2:#ffffff; --fg:#111; --accent:#0077ff;
    }
    @media (prefers-color-scheme: dark) {
      :root { --bg:#121212; --bg2:#1e1e1e; --fg:#eee; --accent:#0af; }
    }
    html,body {
      margin: 0; height: 100%;
      font-family: -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Arial,sans-serif;
      background: var(--bg); color: var(--fg);
    }
    #topbar {
      padding: 20px 16px; text-align: center;
      background: var(--bg2); box-shadow: 0 2px 8px rgba(0,0,0,.1);
    }
    #topbar h2 { margin: 0; font-size: 20px; }
    #bottom {
      position: fixed; bottom: 0; left: 0; right: 0;
      padding: 14px 16px; background: var(--bg2);
      box-shadow: 0 -2px 8px rgba(0,0,0,.15);
      display: flex; align-items: center; justify-content: space-between;
      z-index: 1000;
    }
    button {
      padding: 10px 22px; border: none;
      border-radius: 24px; font-size: 15px; font-weight: 600;
      cursor: pointer; transition: all .2s;
    }
    #liveBtn {
      background: #e2e2e2; color: var(--fg);
    }
    #liveBtn.active {
      background: #ff3b30; color: #fff;
      animation: pulse 1.2s infinite;
    }
    @keyframes pulse {
      0% { transform: scale(1); }
      50% { transform: scale(1.05); }
      100% { transform: scale(1); }
    }
    #status {
      display: flex; align-items: center; font-size: 14px;
    }
    #status svg {
      height: 18px; margin-right: 6px; fill: var(--accent);
    }
    #toast {
      position: fixed;
      bottom: 80px;
      left: 50%;
      transform: translateX(-50%);
      background: rgba(0,0,0,.8);
      color: #fff;
      padding: 8px 16px;
      border-radius: 6px;
      display: none;
    }
  </style>
</head>
<body>
  <div id="topbar">
    <h2>ESP32 → GeoLinker Live Tracker</h2>
  </div>

  <div id="toast"></div>

  <div id="bottom">
    <button id="liveBtn" onclick="toggleLive()">🔴 Start Live</button>
    <div id="status">
      <svg viewBox="0 0 24 24"><path d="M12 2C8 2 4 5 4 9c0 5.25 7 13 8 13s8-7.75 8-13c0-4-4-7-8-7zm0 10.5c-1.93 0-3.5-1.57-3.5-3.5S10.07 5.5 12 5.5s3.5 1.57 3.5 3.5S13.93 12.5 12 12.5z"/></svg>
      <span id="satInfo">Awaiting GPS fix…</span>
    </div>
  </div>

  <script>
    function toast(msg) {
      const t = document.getElementById("toast");
      t.textContent = msg;
      t.style.display = "block";
      setTimeout(() => t.style.display = "none", 2000);
    }

    async function toggleLive() {
      const btn = document.getElementById("liveBtn");
      const resp = await fetch("/toggle");
      const isActive = await resp.text();

      if (isActive === "1") {
        btn.classList.add("active");
        btn.textContent = "⏹ Stop Live";
        toast("🟢 Live tracking ON");
      } else {
        btn.classList.remove("active");
        btn.textContent = "🔴 Start Live";
        toast("⏹ Live tracking OFF");
      }
    }

    fetch("/status").then(r => r.text()).then(t => {
      if (t === "1") toggleLive();
    });
  </script>
</body>
</html>
)rawliteral";

/* ---------- GPS helpers ---------- */
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
    delay(1000);  // 🛰 Push every second
  }
}
