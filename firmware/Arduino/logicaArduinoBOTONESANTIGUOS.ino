#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

// =================================================================
// 1. DEFINICIÓN DE PINES: SISTEMA PRINCIPAL - BOTÓN ROJO
// =================================================================
const int pinRojo = 25;
const int pinVerde = 26;
const int pinAzul = 27;
const int pinBotonRojo_Timer = 14; 

const int pinBuzzer = 32; 

// =================================================================
// 2. DEFINICIÓN DE PINES: SISTEMA SECUNDARIO (ALARMA) - BOTÓN AMARILLO
// =================================================================
const int pinRojoSecundario = 33;
const int pinBotonAmarillo_Alarma = 13; 

// --- Ajustes de intensidad visual ---
const int brilloVerde = 40; 
const int brilloRojoAlarma = 255; 

// =================================================================
// 3. CONFIGURACIÓN MPU6050 (Detector de Movimiento)
// =================================================================
Adafruit_MPU6050 mpu;
const float umbralMovimiento = 15.0; // Umbral de fuerza. Ajustar según sensibilidad deseada.

// --- MÁQUINA DE ESTADOS PRINCIPAL ---
enum Estados {
  ESTADO_APAGADO,
  ESTADO_VERDE,
  ESTADO_AMARILLO,
  ESTADO_ROJO_PARPADEO,
  ESTADO_ROJO_FIJO
};

Estados estadoActual = ESTADO_APAGADO; 

// --- VARIABLES SISTEMA PRINCIPAL (Botón Rojo) ---
int estadoAnteriorBotonRojo = HIGH;
bool simulacionMovimiento = false; // Funciona como interruptor ON/OFF para el movimiento

// --- VARIABLES SISTEMA SECUNDARIO (Botón Amarillo) ---
int estadoAnteriorBotonAmarillo = HIGH;
bool estadoAlarmaSecundaria = false; 

// --- CRONÓMETROS DEL SISTEMA PRINCIPAL ---
unsigned long tiempoInicioEstado = 0; 
unsigned long tiempoUltimoEstimulo = 0; // Registra el último instante en que hubo movimiento
const unsigned long tiempoInactividadReinicio = 5000; // 5 segundos quieto para apagar

// Duración de cada fase mientras te mantienes en movimiento
const unsigned long tiempoVerde = 5000;         
const unsigned long tiempoAmarillo = 8000;      
const unsigned long tiempoLimiteApagar = 10000; 

// --- VARIABLES DE PARPADEO (LED 3 Colores) ---
unsigned long tiempoUltimoParpadeo = 0;
const unsigned long intervaloParpadeo = 500; 
bool estadoParpadeo = false; 

// --- VARIABLES DE PARPADEO (BUZZER) ---
unsigned long tiempoUltimoBuzzer = 0;
const unsigned long intervaloBuzzer = 250; 
bool estadoSonidoBuzzer = false;

void setup() {
  Serial.begin(9600);
  
  // Configuración pines: Sistema Principal
  pinMode(pinRojo, OUTPUT);
  pinMode(pinVerde, OUTPUT);
  pinMode(pinAzul, OUTPUT);
  pinMode(pinBotonRojo_Timer, INPUT_PULLUP);
  
  // Configuración pines: Sistema Secundario
  pinMode(pinRojoSecundario, OUTPUT);
  pinMode(pinBotonAmarillo_Alarma, INPUT_PULLUP);
  pinMode(pinBuzzer, OUTPUT);
  
  // Iniciar MPU6050
  Serial.println("Iniciando MPU6050...");
  
  if (!mpu.begin()) {
    Serial.println("¡ERROR! No se encontro el chip MPU6050. Revisa las conexiones.");
    while (1) { delay(10); } 
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("MPU6050 configurado y listo.\n");

  cambiarEstado(ESTADO_APAGADO); 
  analogWrite(pinRojoSecundario, 0);
}

void loop() {
  // =================================================================
  // FLUJO 1: BOTÓN AMARILLO (Alarma Manual Independiente)
  // =================================================================
  int estadoActualBotonAmarillo = digitalRead(pinBotonAmarillo_Alarma);
  bool botonAmarilloPresionado = (estadoAnteriorBotonAmarillo == HIGH && estadoActualBotonAmarillo == LOW);

  if (botonAmarilloPresionado) {
    delay(50); // Antirrebote
    estadoAlarmaSecundaria = !estadoAlarmaSecundaria; 
    
    if (estadoAlarmaSecundaria) {
      Serial.println("\n[BOTON AMARILLO] Alarma Secundaria ACTIVADA");
      analogWrite(pinRojoSecundario, brilloRojoAlarma); 
    } else {
      Serial.println("\n[BOTON AMARILLO] Alarma Secundaria DESACTIVADA");
      analogWrite(pinRojoSecundario, 0); 
    }
  }
  estadoAnteriorBotonAmarillo = estadoActualBotonAmarillo;

  // =================================================================
  // FLUJO 2: SISTEMA PRINCIPAL (Movimiento Constante y Botón Rojo)
  // =================================================================
  
  // A. Leer el movimiento real del MPU6050
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float magnitudAccel = sqrt(pow(a.acceleration.x, 2) + pow(a.acceleration.y, 2) + pow(a.acceleration.z, 2));
  bool hayMovimiento = (magnitudAccel > umbralMovimiento);

  // B. Leer el botón físico rojo (Lógica de Interruptor ON/OFF)
  int estadoActualBotonRojo = digitalRead(pinBotonRojo_Timer);
  bool botonRojoPresionado = (estadoAnteriorBotonRojo == HIGH && estadoActualBotonRojo == LOW);
  
  if (botonRojoPresionado) {
    delay(50); // Antirrebote
    simulacionMovimiento = !simulacionMovimiento; // Alterna la simulación de movimiento
    
    if (simulacionMovimiento) {
      Serial.println("\n[BOTON ROJO] Simulacion ACTIVADA. Encendiendo fases...");
      tiempoUltimoEstimulo = millis(); // Actualiza el timer
      if (estadoActual == ESTADO_APAGADO) {
        cambiarEstado(ESTADO_VERDE);
      }
    } else {
      Serial.println("\n[BOTON ROJO] Simulacion DESACTIVADA. Apagado inmediato.");
      cambiarEstado(ESTADO_APAGADO); // FUERZA EL APAGADO AL INSTANTE
    }
  }
  estadoAnteriorBotonRojo = estadoActualBotonRojo;

  // C. Si hay movimiento real O la simulación está activada, mantenemos la actividad viva
  if (hayMovimiento || simulacionMovimiento) {
    tiempoUltimoEstimulo = millis(); 
    
    // Si el sistema estaba apagado y hay movimiento real, inicia las fases
    if (estadoActual == ESTADO_APAGADO && hayMovimiento) {
      Serial.println("\n[MOVIMIENTO REAL DETECTADO] Iniciando secuencia de fases...");
      cambiarEstado(ESTADO_VERDE);
    }
  }

  // D. Lógica de apagado por inactividad (Solo aplica si dejas de moverte naturalmente)
  // Si apagaste por botón, estadoActual ya es ESTADO_APAGADO, por lo que ignora esto.
  if (estadoActual != ESTADO_APAGADO) {
    if (millis() - tiempoUltimoEstimulo >= tiempoInactividadReinicio) {
      Serial.println("\n[DESCANSO] 5 segundos sin movimiento. Sistema apagado y reseteado.");
      cambiarEstado(ESTADO_APAGADO);
    }
  }

  // =================================================================
  // FLUJO 3: TRANSICIONES AUTOMÁTICAS DE LAS FASES
  // =================================================================
  // Las fases solo avanzan si el sistema NO está apagado
  if (estadoActual != ESTADO_APAGADO) {
    unsigned long tiempoTranscurrido = millis() - tiempoInicioEstado;

    switch (estadoActual) {
      case ESTADO_VERDE:
        if (tiempoTranscurrido >= tiempoVerde) {
          cambiarEstado(ESTADO_AMARILLO);
        }
        break;
        
      case ESTADO_AMARILLO:
        if (tiempoTranscurrido >= tiempoAmarillo) {
          cambiarEstado(ESTADO_ROJO_PARPADEO);
        }
        break;
        
      case ESTADO_ROJO_PARPADEO:
        if (tiempoTranscurrido >= tiempoLimiteApagar) {
          Serial.println("-> ¡TIEMPO AGOTADO POR MOVIMIENTO CONTINUO!");
          cambiarEstado(ESTADO_ROJO_FIJO); 
        }
        break;
        
      case ESTADO_ROJO_FIJO:
        // En esta fase se queda pitando y encendido en rojo hasta que te detengas real o manualmente.
        break;
    }
  }

  // MANTENER EL PARPADEO DEL LED RGB
  if (estadoActual == ESTADO_AMARILLO || estadoActual == ESTADO_ROJO_PARPADEO) {
    if (millis() - tiempoUltimoParpadeo >= intervaloParpadeo) {
      tiempoUltimoParpadeo = millis(); 
      estadoParpadeo = !estadoParpadeo; 
      
      if (estadoParpadeo) {
        if (estadoActual == ESTADO_AMARILLO) {
          analogWrite(pinRojo, brilloVerde); 
          analogWrite(pinVerde, brilloVerde);
          analogWrite(pinAzul, 0);
        } else if (estadoActual == ESTADO_ROJO_PARPADEO) {
          analogWrite(pinRojo, brilloVerde); 
          analogWrite(pinVerde, 0);
          analogWrite(pinAzul, 0);
        }
      } else {
        apagarLEDPrincipal();
      }
    }
  }

  // =================================================================
  // 4. GESTIÓN DEL BUZZER (Prioridades)
  // =================================================================
  if (estadoAlarmaSecundaria) {
    digitalWrite(pinBuzzer, HIGH);
  } 
  else if (estadoActual == ESTADO_ROJO_FIJO) {
    // Pitido intermitente en la fase final
    if (millis() - tiempoUltimoBuzzer >= intervaloBuzzer) {
      tiempoUltimoBuzzer = millis();
      estadoSonidoBuzzer = !estadoSonidoBuzzer; 
      digitalWrite(pinBuzzer, estadoSonidoBuzzer ? HIGH : LOW);
    }
  } 
  else {
    digitalWrite(pinBuzzer, LOW);
  }
}

// =================================================================
// FUNCIONES AUXILIARES
// =================================================================

void cambiarEstado(Estados nuevoEstado) {
  estadoActual = nuevoEstado;
  tiempoInicioEstado = millis(); 
  estadoSonidoBuzzer = false;
  
  switch (estadoActual) {
    case ESTADO_APAGADO:
      apagarLEDPrincipal();
      break;
      
    case ESTADO_VERDE:
      Serial.println("[FASE 1 - VERDE] En movimiento...");
      analogWrite(pinRojo, 0);
      analogWrite(pinVerde, brilloVerde);
      analogWrite(pinAzul, 0);
      break;
      
    case ESTADO_AMARILLO:
      Serial.println("[FASE 2 - AMARILLO] Precaucion, sigue en movimiento.");
      estadoParpadeo = true;
      tiempoUltimoParpadeo = millis();
      break;
      
    case ESTADO_ROJO_PARPADEO:
      Serial.println("[FASE 3 - ROJO PARPADEANTE] Advertencia critica.");
      estadoParpadeo = true;
      tiempoUltimoParpadeo = millis();
      break;
      
    case ESTADO_ROJO_FIJO:
      Serial.println("[FASE 4 - ALERTA] Alarma acustica activada. Detengase para reiniciar.");
      analogWrite(pinRojo, brilloVerde); 
      analogWrite(pinVerde, 0);
      analogWrite(pinAzul, 0);
      estadoSonidoBuzzer = true;
      tiempoUltimoBuzzer = millis();
      break;
  }
}

void apagarLEDPrincipal() {
  analogWrite(pinRojo, 0);
  analogWrite(pinVerde, 0);
  analogWrite(pinAzul, 0);
}