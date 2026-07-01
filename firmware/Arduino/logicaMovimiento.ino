#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// =================================================================
// 0. CONFIGURACIÓN DFPLAYER MINI
// =================================================================
HardwareSerial dfSerial(2); // UART2: RX = 16, TX = 17
DFRobotDFPlayerMini dfPlayer;

// =================================================================
// 1. DEFINICIÓN DE PINES
// =================================================================
const int pinRojo = 25;
const int pinVerde = 26;
const int pinAzul = 27;
const int pinBuzzer = 32; 

const int pinRojoSecundario = 33;
const int pinBotonAmarillo_Alarma = 13; 

// --- Ajustes ---
const int brilloVerde = 40; 
const int brilloRojoAlarma = 255; 

// =================================================================
// 2. CONFIGURACIÓN MPU6050
// =================================================================
Adafruit_MPU6050 mpu;
const float umbralMovimiento = 15.0; 

enum Estados {
  ESTADO_APAGADO,
  ESTADO_VERDE,
  ESTADO_AMARILLO,
  ESTADO_ROJO_PARPADEO,
  ESTADO_ROJO_FIJO,
  ESTADO_PENALIZACION 
};

Estados estadoActual = ESTADO_APAGADO; 

// --- Variables Sistema ---
bool estadoAlarmaSecundaria = false; 
int estadoAnteriorBotonAmarillo = HIGH;

unsigned long tiempoInicioEstado = 0; 
unsigned long tiempoUltimoEstimulo = 0; 
const unsigned long tiempoInactividadReinicio = 5000; 

const unsigned long tiempoVerde = 5000;        
const unsigned long tiempoAmarillo = 8000;      
const unsigned long tiempoLimiteApagar = 10000; 

bool requiereReposo = false; 
unsigned long tiempoInicioReposo = 0;
const unsigned long tiempoReposoObligatorio = 5000;

unsigned long tiempoUltimoParpadeo = 0;
const unsigned long intervaloParpadeo = 500; 
bool estadoParpadeo = false; 

unsigned long tiempoUltimoBuzzer = 0;
const unsigned long intervaloBuzzer = 250; 
bool estadoSonidoBuzzer = false;

// Prototipos
void cambiarEstado(Estados nuevoEstado);
void apagarLEDPrincipal();
void intentarEncender();

void setup() {
  Serial.begin(9600);
  
  // Iniciar puerto serie para DFPlayer en pines 16 (RX) y 17 (TX)
  dfSerial.begin(9600, SERIAL_8N1, 16, 17);
  
  // Muchos DFPlayer "clones" tardan un par de segundos en iniciar tras recibir corriente.
  // Damos un tiempo de espera y desactivamos el ACK (isACK = false) para evitar errores falsos.
  Serial.println("Iniciando DFPlayer...");
  delay(1500); 
  
  if (!dfPlayer.begin(dfSerial, false, true)) {
    Serial.println("Error: DFPlayer no encontrado o SD sin formato.");
  } else {
    Serial.println("DFPlayer OK.");
  }
  
  dfPlayer.volume(30);  // Ajusta el volumen al MAXIMO (0 a 30)
  delay(100);
  
  pinMode(pinRojo, OUTPUT);
  pinMode(pinVerde, OUTPUT);
  pinMode(pinAzul, OUTPUT);
  pinMode(pinBuzzer, OUTPUT);
  pinMode(pinRojoSecundario, OUTPUT);
  pinMode(pinBotonAmarillo_Alarma, INPUT_PULLUP);
  
  if (!mpu.begin()) {
    while (1) { delay(10); } 
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);

  cambiarEstado(ESTADO_APAGADO);
}

void loop() {
  // --- FLUJO 1: BOTÓN AMARILLO (Alarma Manual) ---
  int estadoActualBotonAmarillo = digitalRead(pinBotonAmarillo_Alarma);
  if (estadoAnteriorBotonAmarillo == HIGH && estadoActualBotonAmarillo == LOW) {
    delay(50);
    estadoAlarmaSecundaria = !estadoAlarmaSecundaria; 
    if (estadoAlarmaSecundaria) {
      Serial.println("\n[BOTON AMARILLO] Alarma Secundaria ACTIVADA");
    } else {
      Serial.println("\n[BOTON AMARILLO] Alarma Secundaria DESACTIVADA");
    }
    analogWrite(pinRojoSecundario, estadoAlarmaSecundaria ? brilloRojoAlarma : 0);
  }
  estadoAnteriorBotonAmarillo = estadoActualBotonAmarillo;

  // --- FLUJO 2: MPU6050 (Detector de Movimiento) ---
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float magnitudAccel = sqrt(pow(a.acceleration.x, 2) + pow(a.acceleration.y, 2) + pow(a.acceleration.z, 2));
  bool hayMovimiento = (magnitudAccel > umbralMovimiento);

  // Lógica de encendido/apagado automático
  if (hayMovimiento) {
    tiempoUltimoEstimulo = millis();
    if (estadoActual == ESTADO_APAGADO) {
      Serial.println("\n[MOVIMIENTO REAL DETECTADO] Iniciando secuencia de fases...");
      intentarEncender();
    }
  } else if (estadoActual != ESTADO_APAGADO) {
    // Si dejamos de movernos por 5 segundos, apagamos
    if (millis() - tiempoUltimoEstimulo >= tiempoInactividadReinicio) {
      Serial.println("\n[DESCANSO] 5 segundos sin movimiento. Sistema apagado y reseteado.");
      cambiarEstado(ESTADO_APAGADO);
    }
  }

  // --- FLUJO 3: TRANSICIONES DE FASE ---
  if (estadoActual != ESTADO_APAGADO) {
    unsigned long tiempoTranscurrido = millis() - tiempoInicioEstado;
    switch (estadoActual) {
      case ESTADO_VERDE: if (tiempoTranscurrido >= tiempoVerde) cambiarEstado(ESTADO_AMARILLO); break;
      case ESTADO_AMARILLO: if (tiempoTranscurrido >= tiempoAmarillo) cambiarEstado(ESTADO_ROJO_PARPADEO); break;
      case ESTADO_ROJO_PARPADEO: 
        if (tiempoTranscurrido >= tiempoLimiteApagar) {
          Serial.println("-> ¡TIEMPO AGOTADO POR MOVIMIENTO CONTINUO!");
          cambiarEstado(ESTADO_ROJO_FIJO); 
        }
        break;
      default: break;
    }
  }

  // --- MANTENER PARPADEO LED ---
  if (estadoActual == ESTADO_AMARILLO || estadoActual == ESTADO_ROJO_PARPADEO) {
    if (millis() - tiempoUltimoParpadeo >= intervaloParpadeo) {
      tiempoUltimoParpadeo = millis(); 
      estadoParpadeo = !estadoParpadeo; 
      if (estadoParpadeo) {
        if (estadoActual == ESTADO_AMARILLO) { analogWrite(pinRojo, brilloVerde); analogWrite(pinVerde, brilloVerde); }
        else { analogWrite(pinRojo, brilloVerde); analogWrite(pinVerde, 0); }
        analogWrite(pinAzul, 0);
      } else apagarLEDPrincipal();
    }
  }

  // --- GESTIÓN BUZZER ---
  // Damos un tiempo de 2500 ms para que el audio se escuche primero, y luego empiece a sonar el buzzer
  unsigned long tiempoEnFaseBuzzer = 0;
  if (estadoActual != ESTADO_APAGADO) {
    tiempoEnFaseBuzzer = millis() - tiempoInicioEstado;
  }

  if (estadoAlarmaSecundaria) {
    digitalWrite(pinBuzzer, HIGH);
  } else if (estadoActual == ESTADO_PENALIZACION && tiempoEnFaseBuzzer > 2500) {
    digitalWrite(pinBuzzer, HIGH);
  } else if (estadoActual == ESTADO_ROJO_FIJO && tiempoEnFaseBuzzer > 2500) {
    if (millis() - tiempoUltimoBuzzer >= intervaloBuzzer) {
      tiempoUltimoBuzzer = millis();
      estadoSonidoBuzzer = !estadoSonidoBuzzer;
      digitalWrite(pinBuzzer, estadoSonidoBuzzer ? HIGH : LOW);
    }
  } else {
    digitalWrite(pinBuzzer, LOW);
  }
}

// --- FUNCIONES ---
void intentarEncender() {
  if (requiereReposo) {
    if (millis() - tiempoInicioReposo < tiempoReposoObligatorio) {
      cambiarEstado(ESTADO_PENALIZACION);
      return; 
    } else {
      requiereReposo = false; 
    }
  }
  cambiarEstado(ESTADO_VERDE);
}

void cambiarEstado(Estados nuevoEstado) {
  if (nuevoEstado == ESTADO_APAGADO) {
    if (estadoActual == ESTADO_ROJO_PARPADEO || estadoActual == ESTADO_ROJO_FIJO || estadoActual == ESTADO_PENALIZACION) {
      requiereReposo = true;
      tiempoInicioReposo = millis();
    } else {
      requiereReposo = false;
    }
  }

  estadoActual = nuevoEstado;
  tiempoInicioEstado = millis(); 
  
  switch (estadoActual) {
    case ESTADO_APAGADO: 
      apagarLEDPrincipal(); 
      break;
    case ESTADO_VERDE: 
      Serial.println("[FASE 1 - VERDE] En movimiento...");
      analogWrite(pinVerde, brilloVerde); 
      break;
    case ESTADO_AMARILLO: 
      Serial.println("[FASE 2 - AMARILLO] Precaucion, sigue en movimiento.");
      dfPlayer.play(1); // Reproduce el 1er archivo (0001.mp3)
      break;
    case ESTADO_ROJO_PARPADEO: 
      Serial.println("[FASE 3 - ROJO PARPADEANTE] Advertencia critica.");
      dfPlayer.play(2); // Reproduce el 2do archivo (0002.mp3)
      break;
    case ESTADO_ROJO_FIJO: 
      Serial.println("[FASE 4 - ALERTA] Alarma acustica activada. Detengase para reiniciar.");
      analogWrite(pinRojo, brilloVerde); 
      dfPlayer.play(2); // Reproduce el 2do archivo (0002.mp3)
      break;
    case ESTADO_PENALIZACION: 
      Serial.println("[FASE 5 - PENALIZACION] Reposo insuficiente. Penalizacion activada.");
      analogWrite(pinRojo, brilloVerde); 
      dfPlayer.play(2); // Reproduce el 2do archivo (0002.mp3)
      break;
  }
}

void apagarLEDPrincipal() {
  analogWrite(pinRojo, 0);
  analogWrite(pinVerde, 0);
  analogWrite(pinAzul, 0);
}