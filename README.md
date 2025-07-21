# ESP32_AND_Neo_6M_based_tracking

# 🚀 ESP32 GPS Tracker with Live Map Web Interface

This project turns your ESP32 into a compact GPS tracking hub using a NEO-6M module, with real-time location updates displayed via a sleek Leaflet-based map hosted directly on the device. It's ideal for standalone setups—no internet or external servers required.

# 🧠 Key Features

- Local Wi-Fi Access Point: ESP32 acts as a hotspot with SSID ESP32-GPS and password 12345678
- GPS Integration: UART2 (RX2=16, TX2=17) used to interface with the NEO-6M module via TinyGPS++
- 
# Interactive Map UI:
- 
- 📍 Show current location instantly
- 🔴 Toggle live tracking with dynamic updates every second
- 🌐 Map rendered using OpenStreetMap + Leaflet.js
- Toolbar Controls:
- Bottom navigation bar with buttons and satellite status display
- Responsive design with dark mode support
# 🌍 How It Works
- On startup, ESP32 boots into AP mode and starts a web server
- Page includes controls to:
- Request current GPS location
- Toggle live updates (handled by /toggle, /live, /status endpoints)
- GPS data is encoded and served in JSON format via buildJson()
- Web app fetches data and smoothly pans the map to new coordinates
# 📦 Noteworthy UI Elements
- Modern button styling and animations
- Satellite fix status icon
- Toast-style notifications on interaction
Wanna take it further, Electro? We could integrate motion trails, store paths in SPIFFS, or even add OTA updates and mobile-friendly dashboards. Just say the word. 😎
