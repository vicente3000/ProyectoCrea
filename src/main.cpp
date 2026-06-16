#include <Arduino.h>
#include <WiFi.h>

String inputLine = "";

String jsonValue(const String& json, const String& key) {
  String marker = "\"" + key + "\"";
  int keyIndex = json.indexOf(marker);
  if (keyIndex < 0) return "";

  int colonIndex = json.indexOf(':', keyIndex + marker.length());
  if (colonIndex < 0) return "";

  int firstQuote = json.indexOf('"', colonIndex + 1);
  if (firstQuote < 0) return "";

  String value = "";
  bool escaped = false;

  for (int i = firstQuote + 1; i < json.length(); i++) {
    char current = json.charAt(i);

    if (escaped) {
      value += current;
      escaped = false;
      continue;
    }

    if (current == '\\') {
      escaped = true;
      continue;
    }

    if (current == '"') break;
    value += current;
  }

  return value;
}

String wifiStatusName(wl_status_t status) {
  switch (status) {
    case WL_CONNECTED:
      return "WL_CONNECTED";
    case WL_NO_SSID_AVAIL:
      return "WL_NO_SSID_AVAIL";
    case WL_CONNECT_FAILED:
      return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST:
      return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED:
      return "WL_DISCONNECTED";
    case WL_IDLE_STATUS:
      return "WL_IDLE_STATUS";
    default:
      return "UNKNOWN";
  }
}

void sendJson(const String& type, const String& message) {
  Serial.print("{\"type\":\"");
  Serial.print(type);
  Serial.print("\",\"message\":\"");
  Serial.print(message);
  Serial.println("\"}");
}

void connectWifi(const String& ssid, const String& password) {
  if (ssid.length() == 0) {
    sendJson("wifi_error", "SSID vacio");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(300);

  sendJson("wifi", "Conectando a " + ssid);
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long startedAt = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 20000) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    sendJson("wifi_connected", WiFi.localIP().toString());
  } else {
    sendJson("wifi_error", wifiStatusName(WiFi.status()));
  }
}

void handleLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  Serial.print("{\"type\":\"serial_received\",\"message\":\"");
  Serial.print(line);
  Serial.println("\"}");

  String type = jsonValue(line, "type");

  if (type == "wifi") {
    String ssid = jsonValue(line, "ssid");
    String password = jsonValue(line, "password");
    connectWifi(ssid, password);
    return;
  }

  if (type == "ping") {
    sendJson("pong", "ESP32 activo");
    return;
  }

  sendJson("info", "Comando no reconocido");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  sendJson("ready", "ESP32 listo. Envia WiFi desde el frontend.");
}

void loop() {
  while (Serial.available()) {
    char current = Serial.read();

    if (current == '\n') {
      handleLine(inputLine);
      inputLine = "";
    } else if (current != '\r') {
      inputLine += current;
    }
  }

  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > 5000) {
    lastHeartbeat = millis();

    if (WiFi.status() == WL_CONNECTED) {
      sendJson("heartbeat", "IP " + WiFi.localIP().toString());
    }
  }
}
