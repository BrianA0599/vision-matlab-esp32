#include <Arduino.h>
#include <string.h>

// ========================================
// PINES: ARDUINO UNO R3
// ========================================

const byte PIN_VELOCIDAD_MOTOR = 9;
const byte PIN_GIRO_DERECHA = 7;
const byte PIN_GIRO_IZQUIERDA = 8;
const byte PIN_LED_INDICADOR = 12;

// ========================================
// PARAMETROS DE LA PRACTICA
// ========================================

// Valores PWM de 0 a 255.
// No representan RPM medidas.
const byte VELOCIDAD_NORMAL = 150;
const byte VELOCIDAD_RAPIDA = 230;
const byte VELOCIDAD_LENTA = 85;

const unsigned long INTERVALO_LED = 400;

// Ajustar según la inercia del motor real.
const unsigned long PAUSA_INVERSION = 700;

const int8_t DETENIDO = 0;
const int8_t DERECHA = 1;
const int8_t IZQUIERDA = -1;

// ========================================
// VARIABLES DEL MOTOR
// ========================================

int8_t sentidoActual = DETENIDO;
int8_t sentidoSolicitado = DETENIDO;

byte velocidadSolicitada = 0;

bool inversionPendiente = false;
unsigned long inicioInversion = 0;

// ========================================
// VARIABLES DEL LED
// ========================================

bool parpadeoActivo = false;
bool estadoLED = false;

unsigned long ultimoCambioLED = 0;

// ========================================
// RECEPCION SERIAL
// ========================================

char comandoSerial[40];
byte longitudComando = 0;
bool descartarLinea = false;


// ========================================
// APAGAR SALIDAS DEL MOTOR
// ========================================

void apagarMotor() {

  analogWrite(PIN_VELOCIDAD_MOTOR, 0);

  digitalWrite(PIN_GIRO_DERECHA, LOW);
  digitalWrite(PIN_GIRO_IZQUIERDA, LOW);

  sentidoActual = DETENIDO;
}


// ========================================
// APLICAR MOVIMIENTO SOLICITADO
// ========================================

void aplicarMovimiento() {

  // Deshabilitar potencia antes de cambiar entradas.
  analogWrite(PIN_VELOCIDAD_MOTOR, 0);

  if (sentidoSolicitado == DERECHA) {

    digitalWrite(PIN_GIRO_DERECHA, HIGH);
    digitalWrite(PIN_GIRO_IZQUIERDA, LOW);

  } else if (sentidoSolicitado == IZQUIERDA) {

    digitalWrite(PIN_GIRO_DERECHA, LOW);
    digitalWrite(PIN_GIRO_IZQUIERDA, HIGH);

  } else {

    apagarMotor();
    return;
  }

  sentidoActual = sentidoSolicitado;

  analogWrite(
    PIN_VELOCIDAD_MOTOR,
    velocidadSolicitada
  );
}


// ========================================
// CONFIGURAR INDICADOR
// ========================================

void configurarLED(bool activarParpadeo) {

  if (activarParpadeo && !parpadeoActivo) {

    estadoLED = true;
    ultimoCambioLED = millis();

    digitalWrite(PIN_LED_INDICADOR, HIGH);
  }

  parpadeoActivo = activarParpadeo;

  if (!parpadeoActivo) {

    estadoLED = false;
    digitalWrite(PIN_LED_INDICADOR, LOW);
  }
}


// ========================================
// SOLICITAR MOVIMIENTO
// ========================================

void solicitarMovimiento(
  int8_t nuevoSentido,
  byte nuevaVelocidad,
  bool activarParpadeo
) {

  sentidoSolicitado = nuevoSentido;
  velocidadSolicitada = nuevaVelocidad;

  configurarLED(activarParpadeo);

  // Si ya estamos esperando, actualizar la solicitud
  // sin cancelar la pausa.
  if (inversionPendiente) {
    return;
  }

  bool requiereInversion =
    sentidoActual != DETENIDO &&
    sentidoActual != sentidoSolicitado;

  if (requiereInversion) {

    apagarMotor();

    inversionPendiente = true;
    inicioInversion = millis();

  } else {

    aplicarMovimiento();
  }
}


// ========================================
// PARO COMPLETO
// ========================================

void ejecutarParo() {

  inversionPendiente = false;

  sentidoSolicitado = DETENIDO;
  velocidadSolicitada = 0;

  apagarMotor();
  configurarLED(false);
}


// ========================================
// PROCESAR COMANDO DE MATLAB
// ========================================

void procesarComando(const char* comando) {

  if (strcmp(comando, "DERECHA_NORMAL") == 0) {

    solicitarMovimiento(
      DERECHA,
      VELOCIDAD_NORMAL,
      false
    );

  } else if (strcmp(comando, "DERECHA_RAPIDA") == 0) {

    solicitarMovimiento(
      DERECHA,
      VELOCIDAD_RAPIDA,
      true
    );

  } else if (strcmp(comando, "IZQUIERDA_LENTA") == 0) {

    solicitarMovimiento(
      IZQUIERDA,
      VELOCIDAD_LENTA,
      true
    );

  } else if (strcmp(comando, "PARO") == 0) {

    ejecutarParo();

  } else {

    Serial.print(F("ERROR:COMANDO_DESCONOCIDO:"));
    Serial.println(comando);
    return;
  }

  // Confirma recepción, no movimiento físico.
  Serial.print(F("OK:"));
  Serial.println(comando);
}


// ========================================
// LEER SERIAL SIN BLOQUEAR
// ========================================

void recibirComandos() {

  while (Serial.available() > 0) {

    char caracter = Serial.read();

    if (caracter == '\n') {

      if (!descartarLinea && longitudComando > 0) {

        comandoSerial[longitudComando] = '\0';
        procesarComando(comandoSerial);
      }

      longitudComando = 0;
      descartarLinea = false;

    } else if (caracter != '\r' && !descartarLinea) {

      if (longitudComando < sizeof(comandoSerial) - 1) {

        comandoSerial[longitudComando++] = caracter;

      } else {

        // Descartar líneas demasiado largas.
        descartarLinea = true;
        longitudComando = 0;
      }
    }
  }
}


// ========================================
// TERMINAR PAUSA DE INVERSION
// ========================================

void actualizarMotor() {

  if (!inversionPendiente) {
    return;
  }

  if (millis() - inicioInversion >= PAUSA_INVERSION) {

    inversionPendiente = false;
    aplicarMovimiento();
  }
}


// ========================================
// PARPADEO SIN DELAY
// ========================================

void actualizarLED() {

  if (!parpadeoActivo) {
    return;
  }

  unsigned long ahora = millis();

  if (ahora - ultimoCambioLED >= INTERVALO_LED) {

    ultimoCambioLED = ahora;
    estadoLED = !estadoLED;

    digitalWrite(PIN_LED_INDICADOR, estadoLED);
  }
}


// ========================================
// CONFIGURACION INICIAL
// ========================================

void setup() {

  pinMode(PIN_VELOCIDAD_MOTOR, OUTPUT);
  pinMode(PIN_GIRO_DERECHA, OUTPUT);
  pinMode(PIN_GIRO_IZQUIERDA, OUTPUT);
  pinMode(PIN_LED_INDICADOR, OUTPUT);

  ejecutarParo();

  Serial.begin(115200);
}


// ========================================
// PROGRAMA PRINCIPAL
// ========================================

void loop() {

  recibirComandos();
  actualizarMotor();
  actualizarLED();
}