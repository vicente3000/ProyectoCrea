# Panel ESP32 de alertas

Este proyecto tiene una pagina web para configurar y monitorear un ESP32, mas un firmware hecho con PlatformIO/Arduino. La idea principal es:

- Conectar el ESP32 al computador por USB.
- Abrir la pagina `index.html` desde el navegador.
- Enviar la red WiFi al ESP32 usando Web Serial.
- Dejar al ESP32 levantando su propia red WiFi de respaldo y una pagina interna de configuracion.
- Controlar un sistema de alertas con MPU6050, LED RGB, botones y buzzer.

## Estructura del proyecto

```txt
.
+-- index.html
+-- styles.css
+-- app.js
+-- platformio.ini
+-- src/
|   +-- main.cpp
+-- firmware/
    +-- esp32_wifi_serial/
        +-- esp32_wifi_serial.ino
        +-- logicaArduino.ino
```

- `index.html`, `styles.css` y `app.js`: pagina externa que se abre desde el PC. Usa Web Serial para hablar con el ESP32 por USB.
- `src/main.cpp`: firmware principal para PlatformIO. Recibe comandos por serial, configura WiFi, levanta un access point y sirve una pagina interna.
- `platformio.ini`: configuracion de compilacion para placa `esp32dev`.
- `firmware/esp32_wifi_serial/`: version de apoyo para Arduino IDE y logica original del sistema de alertas.

## Requisitos

- ESP32 DevKit o placa compatible con `esp32dev`.
- Cable USB con datos.
- Visual Studio Code con PlatformIO, o PlatformIO CLI instalado.
- Navegador Chrome o Edge para usar Web Serial.
- Librerias del firmware:
  - `Adafruit MPU6050`
  - `Adafruit Unified Sensor`

PlatformIO las instala automaticamente porque estan declaradas en `platformio.ini`.

## Hardware usado

El firmware principal usa estos pines:

| Componente | Pin ESP32 |
| --- | --- |
| LED RGB rojo | GPIO 25 |
| LED RGB verde | GPIO 26 |
| LED RGB azul | GPIO 27 |
| Boton rojo / simulacion de movimiento | GPIO 14 |
| LED rojo secundario | GPIO 33 |
| Boton amarillo / alarma manual | GPIO 13 |
| Buzzer | GPIO 32 |
| MPU6050 SDA | GPIO 21 |
| MPU6050 SCL | GPIO 22 |

Recomendaciones:

- Conecta los botones entre el pin y GND, porque el codigo usa `INPUT_PULLUP`.
- Usa resistencias para los LEDs.
- Alimenta el MPU6050 con 3.3 V si tu modulo lo permite, o revisa la especificacion del modulo.
- Une siempre GND de todos los componentes con GND del ESP32.

## Como subir el firmware al ESP32

Desde la raiz del proyecto:

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

Si usas la extension de PlatformIO en VS Code:

1. Abre esta carpeta como proyecto.
2. Conecta el ESP32 por USB.
3. Presiona `Build`.
4. Presiona `Upload`.
5. Abre el monitor serial a `115200`.

Al iniciar correctamente, el ESP32 envia por serial algo parecido a:

```json
{"type":"ready","message":"ESP32 listo. Pagina: http://192.168.4.1"}
```

## Como usar la pagina externa del PC

La pagina externa esta en `index.html`. Como Web Serial funciona mejor en un origen local, abre un servidor simple:

```bash
python -m http.server 8080
```

Luego abre:

```txt
http://localhost:8080
```

Pasos de uso:

1. Conecta el ESP32 por USB.
2. Abre la pagina en Chrome o Edge.
3. Presiona `Conectar USB`.
4. Elige el puerto serial del ESP32.
5. Deja la velocidad en `115200`.
6. Escribe el nombre de la red WiFi y la clave.
7. Presiona `Enviar WiFi al ESP32`.
8. Revisa el monitor de mensajes de la pagina.

La pagina envia este JSON al ESP32:

```json
{"type":"wifi","ssid":"NOMBRE_DE_TU_RED","password":"CLAVE_DE_TU_RED"}
```

El ESP32 intenta conectarse durante 20 segundos y responde por serial con mensajes JSON.

## Comandos por USB/Serial

Puedes enviar comandos desde la pagina o desde el monitor serial. Cada comando debe ir en una linea.

Configurar WiFi:

```json
{"type":"wifi","ssid":"MiRed","password":"MiClave"}
```

Probar conexion:

```json
{"type":"ping"}
```

Guardar correo de alertas:

```json
{"type":"config","email":"persona@correo.com"}
```

Respuestas esperadas:

```json
{"type":"pong","message":"ESP32 activo"}
{"type":"wifi_connected","message":"192.168.1.50"}
{"type":"wifi_error","message":"WL_CONNECT_FAILED"}
{"type":"config","message":"Correo guardado: persona@correo.com"}
```

## Pagina interna del ESP32

El firmware principal tambien crea una red WiFi propia:

```txt
SSID: ESP32-Alertas
Clave: 12345678
IP: 192.168.4.1
```

Para usarla:

1. Enciende el ESP32.
2. Desde el celular o computador, conectate a la red `ESP32-Alertas`.
3. Abre `http://192.168.4.1`.
4. Configura el correo de alertas.
5. Revisa el estado del WiFi, la IP y el ultimo evento.

Endpoints internos disponibles:

| Metodo | Ruta | Funcion |
| --- | --- | --- |
| `GET` | `/` | Muestra la pagina interna del ESP32 |
| `GET` | `/api/status` | Devuelve estado, IP, correo y ultimo evento |
| `POST` | `/api/config` | Guarda el correo de alertas |

Ejemplo de `/api/status`:

```json
{
  "email": "persona@correo.com",
  "wifiConnected": true,
  "ip": "192.168.1.50",
  "lastEvent": "WiFi conectado"
}
```

## Como esta hecha la conexion pagina-ESP32

La pagina externa usa la API Web Serial del navegador:

1. `navigator.serial.requestPort()` pide permiso para usar el puerto USB.
2. `port.open({ baudRate: 115200 })` abre la comunicacion.
3. Los comandos se mandan como texto JSON terminado en salto de linea.
4. El ESP32 acumula caracteres hasta recibir `\n`.
5. El ESP32 interpreta el campo `type`.
6. El ESP32 responde tambien con JSON por serial.

En el firmware, la funcion importante es `handleLine(String line)`. Ahi se decide que hacer con cada comando.

## Sistema de alertas

El firmware tambien ejecuta la logica fisica del proyecto:

- El MPU6050 detecta movimiento.
- El boton rojo activa o desactiva una simulacion de movimiento.
- El sistema pasa por fases:
  - apagado
  - verde
  - amarillo
  - rojo parpadeante
  - rojo fijo con buzzer
- El boton amarillo activa una alarma manual independiente.
- Cuando hay una alerta critica, el ESP32 registra el evento y lo muestra en la pagina interna.

Nota: en este momento el envio real de correo SMTP no esta implementado. El codigo guarda el correo y registra el evento, pero deja comentado el punto donde se conectaria el envio real.

## Importante sobre WebSocket

La pagina externa tiene campos para conectar por WebSocket a `ws://192.168.4.1/ws`, pero el firmware actual no implementa todavia un servidor WebSocket en esa ruta. La configuracion que si funciona actualmente es por USB/Serial y por HTTP interno.

Para habilitar WebSocket de verdad, hay que agregar una libreria como `ESPAsyncWebServer` junto con `AsyncTCP`, crear la ruta `/ws` y mandar eventos desde el ESP32 hacia los clientes conectados.

## Como hacer una pagina parecida desde cero

1. Crear una interfaz HTML con:
   - Estado de USB.
   - Estado de red.
   - Formulario de WiFi.
   - Formulario de correo/configuracion.
   - Monitor de mensajes.
2. En JavaScript, usar Web Serial para abrir el puerto del ESP32.
3. Enviar comandos JSON terminados en `\n`.
4. Leer respuestas del ESP32 y mostrarlas en pantalla.
5. En el firmware, usar `Serial.read()` para recibir lineas completas.
6. Parsear el JSON minimo buscando campos como `type`, `ssid`, `password` y `email`.
7. Usar `WiFi.begin()` para conectar a una red.
8. Usar `Preferences` para guardar configuraciones permanentes.
9. Levantar un `WebServer` en el ESP32 para mostrar una pagina interna.
10. Exponer endpoints HTTP simples para consultar estado y guardar datos.

## Prompt para pedirle a una IA que haga la misma pagina

Copia y pega este prompt en la IA que quieras usar:

```txt
Necesito que construyas una pagina web y un firmware para ESP32 con PlatformIO/Arduino.

Objetivo:
Crear un panel web en HTML, CSS y JavaScript puro para configurar y monitorear un ESP32. La pagina debe conectarse al ESP32 por USB usando Web Serial desde Chrome/Edge, enviar credenciales WiFi y mostrar las respuestas del ESP32 en un monitor de mensajes.

Frontend:
- Crear index.html, styles.css y app.js.
- La interfaz debe estar en espanol.
- Debe tener estado de conexion USB, boton conectar/desconectar USB, selector de baud rate, formulario de SSID y password, formulario para guardar correo de alertas, area para enviar comandos manuales y monitor de mensajes.
- Debe usar Web Serial con navigator.serial.requestPort().
- Debe enviar cada comando como JSON terminado en salto de linea.
- Debe guardar en localStorage el ultimo SSID y configuraciones utiles.
- Debe mostrar JSON recibido de forma legible.
- Debe funcionar desde http://localhost:8080.

Protocolo serial:
- Para WiFi debe enviar:
  {"type":"wifi","ssid":"NOMBRE_RED","password":"CLAVE"}
- Para ping debe enviar:
  {"type":"ping"}
- Para guardar correo debe enviar:
  {"type":"config","email":"persona@correo.com"}
- El ESP32 debe responder con JSON del tipo:
  {"type":"ready","message":"ESP32 listo"}
  {"type":"wifi_connected","message":"192.168.1.50"}
  {"type":"wifi_error","message":"WL_CONNECT_FAILED"}
  {"type":"pong","message":"ESP32 activo"}

Firmware ESP32:
- Usar framework Arduino en PlatformIO.
- Board: esp32dev.
- Baud rate: 115200.
- Usar WiFi.h, WebServer.h y Preferences.h.
- Recibir comandos por Serial linea por linea.
- Parsear JSON simple para extraer type, ssid, password y email.
- Conectar el ESP32 a WiFi con WiFi.begin().
- Guardar el correo con Preferences.
- Crear un access point de respaldo:
  SSID: ESP32-Alertas
  Password: 12345678
  IP: 192.168.4.1
- Servir una pagina interna en http://192.168.4.1.
- Crear endpoints:
  GET /api/status para devolver email, wifiConnected, ip y lastEvent.
  POST /api/config para guardar email.

Hardware:
- LED RGB: rojo GPIO 25, verde GPIO 26, azul GPIO 27.
- Boton rojo en GPIO 14 con INPUT_PULLUP.
- LED rojo secundario en GPIO 33.
- Boton amarillo en GPIO 13 con INPUT_PULLUP.
- Buzzer en GPIO 32.
- MPU6050 por I2C: SDA GPIO 21 y SCL GPIO 22.
- Usar Adafruit_MPU6050 y Adafruit Unified Sensor.

Logica:
- Detectar movimiento con MPU6050.
- Si hay movimiento, pasar por fases: verde, amarillo, rojo parpadeante y rojo fijo.
- En rojo fijo activar buzzer y registrar alerta.
- El boton rojo debe simular movimiento.
- El boton amarillo debe activar/desactivar una alarma manual.
- Mostrar el ultimo evento en la pagina interna.

Entregables:
- platformio.ini completo.
- src/main.cpp completo.
- index.html, styles.css y app.js completos.
- README con instrucciones para compilar, subir firmware y usar la pagina.

Importante:
- Si agregas WebSocket, implementa tambien el endpoint real /ws en el ESP32. Si no lo implementas, no pongas controles de WebSocket en la pagina.
```

## Problemas comunes

- No aparece el puerto USB: revisa el cable, drivers CP210x/CH340 y que otro programa no tenga abierto el monitor serial.
- Web Serial no funciona: usa Chrome o Edge y abre la pagina desde `localhost`.
- El ESP32 no conecta al WiFi: revisa SSID, clave y que sea una red 2.4 GHz.
- No aparece el MPU6050: revisa SDA, SCL, 3.3 V y GND.
- La pagina `ws://192.168.4.1/ws` falla: el firmware actual no trae WebSocket implementado.
