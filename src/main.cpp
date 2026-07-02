#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

const char* apSsid = "ESP32-Alertas";
const char* apPassword = "12345678";

WebServer server(80);
Preferences prefs;
Adafruit_MPU6050 mpu;

String inputLine = "";
String alertEmail = "";
String lastEvent = "Sistema iniciado";
bool mpuReady = false;

const int pinRojo = 25;
const int pinVerde = 26;
const int pinAzul = 27;
const int pinBotonRojoTimer = 14;
const int pinRojoSecundario = 33;
const int pinBotonAmarilloAlarma = 13;
const int pinBuzzer = 32;

const int brilloVerde = 40;
const int brilloRojoAlarma = 255;
const float umbralMovimiento = 15.0;

enum Estados {
  ESTADO_APAGADO,
  ESTADO_VERDE,
  ESTADO_AMARILLO,
  ESTADO_ROJO_PARPADEO,
  ESTADO_ROJO_FIJO
};

Estados estadoActual = ESTADO_APAGADO;

int estadoAnteriorBotonRojo = HIGH;
int estadoAnteriorBotonAmarillo = HIGH;
bool simulacionMovimiento = false;
bool estadoAlarmaSecundaria = false;

unsigned long tiempoInicioEstado = 0;
unsigned long tiempoUltimoEstimulo = 0;
const unsigned long tiempoInactividadReinicio = 5000;
const unsigned long tiempoVerde = 5000;
const unsigned long tiempoAmarillo = 8000;
const unsigned long tiempoLimiteApagar = 10000;

unsigned long tiempoUltimoParpadeo = 0;
const unsigned long intervaloParpadeo = 500;
bool estadoParpadeo = false;

unsigned long tiempoUltimoBuzzer = 0;
const unsigned long intervaloBuzzer = 250;
bool estadoSonidoBuzzer = false;

const char pageHtml[] PROGMEM = R"HTML(
<!doctype html>
<html lang="es">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESP32 Alertas</title>
  <style>
    :root{--bg:#eef2f0;--panel:#fff;--ink:#15211d;--muted:#63716c;--line:#d7dfda;--accent:#0d7c66;--bad:#b7472a}
    *{box-sizing:border-box}body{margin:0;min-height:100vh;background:var(--bg);color:var(--ink);font-family:system-ui,-apple-system,Segoe UI,sans-serif}
    main{width:min(860px,calc(100% - 28px));margin:0 auto;padding:28px 0}
    header{display:flex;justify-content:space-between;gap:18px;align-items:end;margin-bottom:18px}
    h1{font-size:clamp(2rem,5vw,3.8rem);line-height:1;margin:0}h2{margin:0 0 12px;font-size:1.1rem}.eyebrow{margin:0 0 8px;color:var(--accent);font-weight:800;text-transform:uppercase;font-size:.78rem}
    .grid{display:grid;grid-template-columns:1fr 1fr;gap:16px}.panel{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:18px}
    label{display:grid;gap:8px;color:var(--muted);font-weight:700;font-size:.9rem}input{width:100%;border:1px solid var(--line);border-radius:7px;padding:12px;font:inherit}
    button{min-height:42px;border:0;border-radius:7px;background:var(--accent);color:white;font-weight:800;padding:0 16px;cursor:pointer}.secondary{background:#e6ece9;color:var(--ink)}
    .row{display:flex;gap:10px;flex-wrap:wrap;margin-top:12px}.status{display:grid;gap:10px}.item{display:flex;justify-content:space-between;gap:12px;border-bottom:1px solid var(--line);padding:10px 0}.item:last-child{border-bottom:0}
    code{overflow-wrap:anywhere}.ok{color:var(--accent);font-weight:800}.bad{color:var(--bad);font-weight:800}
    @media(max-width:720px){header{display:block}.grid{grid-template-columns:1fr}}
  </style>
</head>
<body>
  <main>
    <header>
      <div>
        <p class="eyebrow">ESP32</p>
        <h1>Panel de alertas</h1>
      </div>
      <button class="secondary" id="refreshBtn">Actualizar</button>
    </header>

    <section class="grid">
      <form class="panel" id="emailForm">
        <h2>Correo de alertas</h2>
        <label>
          Destinatario
          <input id="email" type="email" placeholder="nombre@correo.com" required>
        </label>
        <div class="row">
          <button type="submit">Guardar correo</button>
        </div>
      </form>

      <section class="panel status">
        <h2>Estado</h2>
        <div class="item"><span>WiFi</span><code id="wifi">-</code></div>
        <div class="item"><span>IP local</span><code id="ip">-</code></div>
        <div class="item"><span>AP directo</span><code>ESP32-Alertas / 192.168.4.1</code></div>
        <div class="item"><span>Correo</span><code id="savedEmail">-</code></div>
        <div class="item"><span>Ultimo evento</span><code id="event">-</code></div>
      </section>
    </section>
  </main>

  <script>
    const email = document.querySelector("#email");
    const savedEmail = document.querySelector("#savedEmail");
    const wifi = document.querySelector("#wifi");
    const ip = document.querySelector("#ip");
    const eventBox = document.querySelector("#event");

    async function loadStatus(){
      const res = await fetch("/api/status");
      const data = await res.json();
      email.value = data.email || "";
      savedEmail.textContent = data.email || "Sin configurar";
      wifi.textContent = data.wifiConnected ? "Conectado" : "Desconectado";
      wifi.className = data.wifiConnected ? "ok" : "bad";
      ip.textContent = data.ip || "-";
      eventBox.textContent = data.lastEvent || "-";
    }

    document.querySelector("#emailForm").addEventListener("submit", async (event) => {
      event.preventDefault();
      await fetch("/api/config", {
        method: "POST",
        headers: {"Content-Type": "application/json"},
        body: JSON.stringify({email: email.value.trim()})
      });
      await loadStatus();
    });

    document.querySelector("#refreshBtn").addEventListener("click", loadStatus);
    loadStatus();
    setInterval(loadStatus, 4000);
  </script>
</body>
</html>
)HTML";

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
    } else if (current == '\\') {
      escaped = true;
    } else if (current == '"') {
      break;
    } else {
      value += current;
    }
  }
  return value;
}

String escapeJson(const String& value) {
  String output = "";
  for (int i = 0; i < value.length(); i++) {
    char current = value.charAt(i);
    if (current == '"' || current == '\\') output += '\\';
    output += current;
  }
  return output;
}

String wifiStatusName(wl_status_t status) {
  switch (status) {
    case WL_CONNECTED: return "WL_CONNECTED";
    case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
    case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED: return "WL_DISCONNECTED";
    case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
    default: return "UNKNOWN";
  }
}

void sendJson(const String& type, const String& message) {
  Serial.print("{\"type\":\"");
  Serial.print(type);
  Serial.print("\",\"message\":\"");
  Serial.print(escapeJson(message));
  Serial.println("\"}");
}

void registerAlert(const String& message) {
  lastEvent = message;
  sendJson("alert", message + " | correo: " + (alertEmail.length() ? alertEmail : "sin configurar"));
  // Aqui se conectaria el envio SMTP real usando alertEmail como destinatario.
}

void apagarLEDPrincipal() {
  analogWrite(pinRojo, 0);
  analogWrite(pinVerde, 0);
  analogWrite(pinAzul, 0);
}

void cambiarEstado(Estados nuevoEstado) {
  estadoActual = nuevoEstado;
  tiempoInicioEstado = millis();
  estadoSonidoBuzzer = false;

  switch (estadoActual) {
    case ESTADO_APAGADO:
      apagarLEDPrincipal();
      lastEvent = "Sistema apagado";
      break;
    case ESTADO_VERDE:
      lastEvent = "Fase verde: en movimiento";
      analogWrite(pinRojo, 0);
      analogWrite(pinVerde, brilloVerde);
      analogWrite(pinAzul, 0);
      break;
    case ESTADO_AMARILLO:
      lastEvent = "Fase amarilla: precaucion";
      estadoParpadeo = true;
      tiempoUltimoParpadeo = millis();
      break;
    case ESTADO_ROJO_PARPADEO:
      lastEvent = "Fase roja parpadeante";
      estadoParpadeo = true;
      tiempoUltimoParpadeo = millis();
      break;
    case ESTADO_ROJO_FIJO:
      analogWrite(pinRojo, brilloVerde);
      analogWrite(pinVerde, 0);
      analogWrite(pinAzul, 0);
      estadoSonidoBuzzer = true;
      tiempoUltimoBuzzer = millis();
      registerAlert("Alerta critica por movimiento continuo");
      break;
  }
}

void startWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", pageHtml);
  });

  server.on("/api/status", HTTP_GET, []() {
    String json = "{";
    json += "\"email\":\"" + escapeJson(alertEmail) + "\",";
    json += "\"wifiConnected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    json += "\"ip\":\"" + String(WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString()) + "\",";
    json += "\"lastEvent\":\"" + escapeJson(lastEvent) + "\"";
    json += "}";
    server.send(200, "application/json", json);
  });

  server.on("/api/config", HTTP_POST, []() {
    String body = server.arg("plain");
    String email = jsonValue(body, "email");
    email.trim();

    alertEmail = email;
    prefs.putString("alertEmail", alertEmail);
    lastEvent = "Correo de alertas actualizado";

    server.send(200, "application/json", "{\"ok\":true}");
    sendJson("config", "Correo guardado: " + alertEmail);
  });

  server.begin();
}

void connectWifi(const String& ssid, const String& password) {
  if (ssid.length() == 0) {
    sendJson("wifi_error", "SSID vacio");
    return;
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(false);
  delay(300);

  sendJson("wifi", "Conectando a " + ssid);
  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 20000) {
    delay(500);
    Serial.print(".");
    server.handleClient();
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    lastEvent = "WiFi conectado";
    sendJson("wifi_connected", WiFi.localIP().toString());
  } else {
    lastEvent = "Fallo WiFi: " + wifiStatusName(WiFi.status());
    sendJson("wifi_error", wifiStatusName(WiFi.status()));
  }
}

void handleLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  sendJson("serial_received", line);
  String type = jsonValue(line, "type");

  if (type == "wifi") {
    connectWifi(jsonValue(line, "ssid"), jsonValue(line, "password"));
  } else if (type == "config") {
    alertEmail = jsonValue(line, "email");
    alertEmail.trim();
    prefs.putString("alertEmail", alertEmail);
    sendJson("config", "Correo guardado: " + alertEmail);
  } else if (type == "ping") {
    sendJson("pong", "ESP32 activo");
  } else {
    sendJson("info", "Comando no reconocido");
  }
}

void setupPinsAndSensors() {
  pinMode(pinRojo, OUTPUT);
  pinMode(pinVerde, OUTPUT);
  pinMode(pinAzul, OUTPUT);
  pinMode(pinBotonRojoTimer, INPUT_PULLUP);
  pinMode(pinRojoSecundario, OUTPUT);
  pinMode(pinBotonAmarilloAlarma, INPUT_PULLUP);
  pinMode(pinBuzzer, OUTPUT);

  mpuReady = mpu.begin();
  if (mpuReady) {
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  }

  cambiarEstado(ESTADO_APAGADO);
  analogWrite(pinRojoSecundario, 0);
}

void runAlertLogic() {
  int estadoActualBotonAmarillo = digitalRead(pinBotonAmarilloAlarma);
  bool botonAmarilloPresionado = (estadoAnteriorBotonAmarillo == HIGH && estadoActualBotonAmarillo == LOW);

  if (botonAmarilloPresionado) {
    delay(50);
    estadoAlarmaSecundaria = !estadoAlarmaSecundaria;

    if (estadoAlarmaSecundaria) {
      analogWrite(pinRojoSecundario, brilloRojoAlarma);
      registerAlert("Alarma manual activada");
    } else {
      analogWrite(pinRojoSecundario, 0);
      lastEvent = "Alarma manual desactivada";
    }
  }
  estadoAnteriorBotonAmarillo = estadoActualBotonAmarillo;

  bool hayMovimiento = false;
  if (mpuReady) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);
    float magnitudAccel = sqrt(pow(a.acceleration.x, 2) + pow(a.acceleration.y, 2) + pow(a.acceleration.z, 2));
    hayMovimiento = (magnitudAccel > umbralMovimiento);
  }

  int estadoActualBotonRojo = digitalRead(pinBotonRojoTimer);
  bool botonRojoPresionado = (estadoAnteriorBotonRojo == HIGH && estadoActualBotonRojo == LOW);

  if (botonRojoPresionado) {
    delay(50);
    simulacionMovimiento = !simulacionMovimiento;

    if (simulacionMovimiento) {
      tiempoUltimoEstimulo = millis();
      if (estadoActual == ESTADO_APAGADO) cambiarEstado(ESTADO_VERDE);
    } else {
      cambiarEstado(ESTADO_APAGADO);
    }
  }
  estadoAnteriorBotonRojo = estadoActualBotonRojo;

  if (hayMovimiento || simulacionMovimiento) {
    tiempoUltimoEstimulo = millis();
    if (estadoActual == ESTADO_APAGADO && hayMovimiento) cambiarEstado(ESTADO_VERDE);
  }

  if (estadoActual != ESTADO_APAGADO && millis() - tiempoUltimoEstimulo >= tiempoInactividadReinicio) {
    cambiarEstado(ESTADO_APAGADO);
  }

  if (estadoActual != ESTADO_APAGADO) {
    unsigned long tiempoTranscurrido = millis() - tiempoInicioEstado;

    if (estadoActual == ESTADO_VERDE && tiempoTranscurrido >= tiempoVerde) {
      cambiarEstado(ESTADO_AMARILLO);
    } else if (estadoActual == ESTADO_AMARILLO && tiempoTranscurrido >= tiempoAmarillo) {
      cambiarEstado(ESTADO_ROJO_PARPADEO);
    } else if (estadoActual == ESTADO_ROJO_PARPADEO && tiempoTranscurrido >= tiempoLimiteApagar) {
      cambiarEstado(ESTADO_ROJO_FIJO);
    }
  }

  if (estadoActual == ESTADO_AMARILLO || estadoActual == ESTADO_ROJO_PARPADEO) {
    if (millis() - tiempoUltimoParpadeo >= intervaloParpadeo) {
      tiempoUltimoParpadeo = millis();
      estadoParpadeo = !estadoParpadeo;

      if (estadoParpadeo) {
        analogWrite(pinRojo, brilloVerde);
        analogWrite(pinVerde, estadoActual == ESTADO_AMARILLO ? brilloVerde : 0);
        analogWrite(pinAzul, 0);
      } else {
        apagarLEDPrincipal();
      }
    }
  }

  if (estadoAlarmaSecundaria) {
    digitalWrite(pinBuzzer, HIGH);
  } else if (estadoActual == ESTADO_ROJO_FIJO) {
    if (millis() - tiempoUltimoBuzzer >= intervaloBuzzer) {
      tiempoUltimoBuzzer = millis();
      estadoSonidoBuzzer = !estadoSonidoBuzzer;
      digitalWrite(pinBuzzer, estadoSonidoBuzzer ? HIGH : LOW);
    }
  } else {
    digitalWrite(pinBuzzer, LOW);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  prefs.begin("config", false);
  alertEmail = prefs.getString("alertEmail", "");

  setupPinsAndSensors();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(apSsid, apPassword);
  startWebServer();

  sendJson("ready", "ESP32 listo. Pagina: http://192.168.4.1");
  if (!mpuReady) sendJson("sensor_error", "No se encontro MPU6050");
}

void loop() {
  server.handleClient();

  while (Serial.available()) {
    char current = Serial.read();
    if (current == '\n') {
      handleLine(inputLine);
      inputLine = "";
    } else if (current != '\r') {
      inputLine += current;
    }
  }

  runAlertLogic();
}
