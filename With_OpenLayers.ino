/*  ESP32 + NEO-6M  |  Bottom-toolbar map
    AP:  ESP32-GPS   PW: 12345678
*/
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

/* ---------- GPS ---------- */
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);   // UART2: RX2=16, TX2=17

/* ---------- Wi-Fi ---------- */
const char* ssid = "ESP32-GPS";
const char* pass = "12345678";
AsyncWebServer server(80);

/* ---------- State ---------- */
bool liveMode = false;

/* ---------- Embedded Web ---------- */
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <title>ESP32 GPS Tracker</title>
  <meta name="viewport" content="width=device-width,initial-scale=1"/>
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/ol@v8.2.0/ol.css"/>
  <style>
    :root{
      --bg:#f5f5f5;
      --bg2:#ffffff;
      --fg:#111;
      --accent:#0077ff;
    }
    @media (prefers-color-scheme: dark){
      :root{ --bg:#121212; --bg2:#1e1e1e; --fg:#eee; --accent:#0af; }
    }
    html,body,#map{height:100%;margin:0;font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Arial,sans-serif}
    body{background:var(--bg);color:var(--fg)}
    #bottom{
      position:fixed;
      bottom:0;
      left:0;
      right:0;
      padding:14px 16px;
      background:var(--bg2);
      box-shadow:0 -2px 8px rgba(0,0,0,.15);
      display:flex;
      align-items:center;
      justify-content:space-between;
      z-index:1000;
    }
    button{
      padding:10px 22px;
      border:none;
      border-radius:24px;
      font-size:15px;
      font-weight:600;
      cursor:pointer;
      transition:all .2s;
    }
    #currentBtn{background:var(--accent);color:#fff}
    #liveBtn{background:#e2e2e2;color:var(--fg)}
    #liveBtn.active{background:#ff3b30;color:#fff;animation:pulse 1.2s infinite}
    @keyframes pulse{0%{transform:scale(1)}50%{transform:scale(1.05)}100%{transform:scale(1)}}
    #status{display:flex;align-items:center;font-size:14px}
    #status svg{height:18px;margin-right:6px;fill:var(--accent)}
  </style>
</head>
<body>
  <div id="map"></div>

  <div id="bottom">
    <div>
      <button id="currentBtn" onclick="getCurrent()">📍 Current</button>
      <button id="liveBtn" onclick="toggleLive()">🔴 Start Live</button>
    </div>
    <div id="status">
      <svg viewBox="0 0 24 24"><path d="M12 2C8 2 4 5 4 9c0 5.25 7 13 8 13s8-7.75 8-13c0-4-4-7-8-7zm0 10.5c-1.93 0-3.5-1.57-3.5-3.5S10.07 5.5 12 5.5s3.5 1.57 3.5 3.5S13.93 12.5 12 12.5z"/></svg>
      <span id="satInfo">Waiting for fix…</span>
    </div>
  </div>

  <script src="https://cdn.jsdelivr.net/npm/ol@v8.2.0/ol.js"></script>
  <script>
    /* ---------- Map ---------- */
    const map = new ol.Map({
      target: 'map',
      layers: [
        new ol.layer.Tile({
          source: new ol.source.OSM()
        })
      ],
      view: new ol.View({
        center: ol.proj.fromLonLat([0, 0]),
        zoom: 2
      })
    });

    /* ---------- Marker ---------- */
    let markerLayer = null;

    function toast(msg){
      const t=document.createElement('div');
      t.textContent=msg;Object.assign(t.style,{position:'fixed',bottom:80,left:'50%',transform:'translateX(-50%)',background:'rgba(0,0,0,.8)',color:'#fff',padding:'8px 16px',borderRadius:'6px'});
      document.body.appendChild(t);
      setTimeout(()=>t.remove(),2000);
    }

    function updateMap(lat,lng,sats){
      const coord = ol.proj.fromLonLat([lng, lat]);
      if(!markerLayer){
        const feature = new ol.Feature(new ol.geom.Point(coord));
        markerLayer = new ol.layer.Vector({
          source: new ol.source.Vector({ features: [feature] }),
          style: new ol.style.Style({
            image: new ol.style.Icon({
              anchor: [0.5, 1],
              src: 'https://unpkg.com/leaflet@1.9.4/dist/images/marker-icon.png'
            })
          })
        });
        map.addLayer(markerLayer);
      }else{
        markerLayer.getSource().getFeatures()[0].getGeometry().setCoordinates(coord);
      }
      map.getView().animate({center: coord, zoom: 17});
      document.getElementById('satInfo').textContent=`${sats} satellites`;
    }

    async function getCurrent(){
      try{
        const j=await (await fetch('/current')).json();
        updateMap(j.lat,j.lng,j.sat);
        toast("📍 Current location shown");
      }catch{toast("❌ No GPS fix");}
    }

    let liveInterval = null;
    async function toggleLive(){
      const wasActive = document.getElementById('liveBtn').classList.contains('active');
      await fetch('/toggle');
      if(!wasActive){
        document.getElementById('liveBtn').classList.add('active');
        document.getElementById('liveBtn').textContent="⏹ Stop Live";
        toast("🟢 Live tracking ON");
        liveInterval=setInterval(async()=>{
          try{
            const j=await (await fetch('/live')).json();
            updateMap(j.lat,j.lng,j.sat);
          }catch{}
        },1000);
      }else{
        document.getElementById('liveBtn').classList.remove('active');
        document.getElementById('liveBtn').textContent="🔴 Start Live";
        clearInterval(liveInterval);
        toast("⏹ Live tracking OFF");
      }
    }

    // resume state on reload
    fetch('/status').then(r=>r.text()).then(t=>{
      if(t==='1') toggleLive();
    });
  </script>
</body>
</html>
)rawliteral";

/* ---------- GPS helpers ---------- */
bool feedGPS(){
  while (gpsSerial.available()) gps.encode(gpsSerial.read());
  return gps.location.isValid();
}

String buildJson(){
  DynamicJsonDocument doc(256);
  doc["lat"] = gps.location.lat();
  doc["lng"] = gps.location.lng();
  doc["sat"] = gps.satellites.value();
  String s; serializeJson(doc, s); return s;
}

/* ---------- Web ---------- */
void setupWeb(){
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *r){
    r->send_P(200,"text/html",index_html);
  });

  server.on("/current", HTTP_GET, [](AsyncWebServerRequest *r){
    if (!feedGPS()) { r->send(503,"text/plain","No fix"); return; }
    r->send(200,"application/json", buildJson());
  });

  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *r){
    liveMode = !liveMode;
    r->send(200,"text/plain", liveMode?"1":"0");
  });

  server.on("/live", HTTP_GET, [](AsyncWebServerRequest *r){
    if (!liveMode) { r->send(204); return; }
    if (!feedGPS()) { r->send(503,"text/plain","No fix"); return; }
    r->send(200,"application/json", buildJson());
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *r){
    r->send(200,"text/plain", liveMode?"1":"0");
  });

  server.begin();
}

/* ---------- Arduino ---------- */
void setup(){
  Serial.begin(115200);
  gpsSerial.begin(9600,SERIAL_8N1,16,17);

  WiFi.softAP(ssid,pass);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());

  setupWeb();
}

void loop(){
  /* nothing – AsyncWebServer handles everything */
}
