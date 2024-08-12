
#if defined(ARDUINO_ARCH_ESP32)
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <AutoConnect.h>
using WiFiWebServer = WebServer;
#else
#error Only for ESP32
#endif

class Connect {
public:
  Connect() : _config("", ""),
              _portal(_server),
              _httpPort(80) {
    _content = String(R"(
            <!DOCTYPE html>
            <html>
            <head>
              <meta charset="UTF-8" name="viewport" content="width=device-width, initial-scale=1">
            </head>
            <body>
            設定はこちら　&ensp;
            __AC_LINK__
            </body>
            </html>)");
  }

  void begin(void) {
    begin("", "");
  }

  void begin(const char* SSID, const char* PASSWORD) {
    // Responder of root page and apply page handled directly from WebServer class.
    _server.on("/", [&]() {
      _content.replace("__AC_LINK__", String(AUTOCONNECT_LINK(COG_16)));
      _server.send(200, "text/html", _content);
    });

    _config.autoReconnect     = true;
    _config.reconnectInterval = 1;

    _config.psk     = "0123456789";
    _config.apid    = "ESP-G-" + String((uint32_t)(ESP.getEfuseMac() >> 32));
    _config.channel = 3;
    _config.minRSSI = -40;

    _portal.config(_config);

    bool result = false;

    // printCredential();
    if (String(SSID).isEmpty() || String(PASSWORD).isEmpty()) {
      result = _portal.begin();
    } else {
      result = _portal.begin(SSID, PASSWORD);
    }

    if (result) {
      log_i("WiFi connected: %s", WiFi.localIP().toString().c_str());

      if (MDNS.begin(_hostName.c_str())) {
        MDNS.addService("http", "tcp", _httpPort);
        log_i("HTTP Server ready! Open http://%s.local/ in your browser\n", _hostName.c_str());
      } else
        log_e("Error setting up MDNS responder");
    } else {
      log_e("ESP32 can't connect to AP.");
      ESP.restart();
    }
  }

  void update(void) {
    _portal.handleClient();
  }

  void printCredential(void) {
    log_w("here1");
    AutoConnectCredential ac;
    station_config_t      entry;

    uint8_t count = ac.entries();
    if (count > 0) {
      for (int8_t i = 0; i < count; i++) {
        ac.load(i, &entry);
        log_i("\nSSID:%.*s\n", sizeof(entry.ssid), (const char*)entry.ssid);
        log_i("BSSID:");
        for (uint8_t b = 0; b < sizeof(entry.bssid); b++)
          log_i("%02x", entry.bssid[b]);
        log_i("\nPassword:%.*s\n", sizeof(entry.password), (const char*)entry.password);
        log_i("DHCP:%d\n", entry.dhcp);
        log_i("IP:0x%08" PRIx32 "\n", entry.config.sta.ip);
        log_i("Gateway:0x%08" PRIx32 "\n", entry.config.sta.gateway);
        log_i("Netmask:0x%08" PRIx32 "\n", entry.config.sta.netmask);
        log_i("DNS1:0x%08" PRIx32 "\n", entry.config.sta.dns1);
        log_i("DNS2:0x%08" PRIx32 "\n", entry.config.sta.dns2);
      }
      log_w("---");
    } else {
      log_w("No entries");
    }
  }

private:
  WiFiWebServer     _server;
  AutoConnectConfig _config;
  AutoConnect       _portal;

  String   _content;
  String   _hostName;
  String   _apName;
  uint16_t _httpPort;
};
