// =====================================================
// DISPENSADOR MEDICINAS ESP32 v3 - MODULAR
// Basado en Supabase + 3 Compartimentos + PCA9685
// =====================================================

#include "config.h"
#include "wifi_utils.h"
#include "time_utils.h"
#include "lcd_ui.h"
#include "input_utils.h"
#include "servo_ctrl.h"
#include "supabase_api.h"

namespace {

bool          botonManualPrev      = HIGH;
unsigned long botonManualDownAt    = 0;
bool          botonManualDisparado = false;

// Escanea el bus I2C e imprime qué direcciones responden
void diagnosticoI2CLcd() {
  Serial.println(F("[I2C] Escaneando bus..."));
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("[I2C] Dispositivo en 0x"));
      if (addr < 16) Serial.print('0');
      Serial.println(addr, HEX);
    }
  }
  Wire.beginTransmission(LCD_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println(F("[LCD] ADVERTENCIA: LCD no responde. Revisar SDA/SCL/VCC"));
  }
}

// Pulsación larga (>1.5 s) dispara prueba manual de dispensado
void manejarPruebaManualBoton() {
  bool raw = digitalRead(CONFIRM_BUTTON_PIN);

  if (raw != botonManualPrev) {
    botonManualPrev = raw;
    if (raw == LOW) {
      botonManualDownAt    = millis();
      botonManualDisparado = false;
    }
  }

  if (raw == LOW && !botonManualDisparado &&
      (millis() - botonManualDownAt) > 1500UL) {
    botonManualDisparado = true;
    Serial.println(F("[BTN] Pulsacion larga -> prueba manual"));

    setLcdState(LCD_MODE_CONFIRM, 0, 1, "PRUEBA");
    updateLCD();
    bool ok = esperarConfirmacionUsuario(15000UL);
    Serial.print(F("[BTN] Prueba manual: "));
    Serial.println(ok ? F("CONFIRMADO") : F("TIMEOUT"));
    setLcdState(LCD_MODE_IDLE, -1, 0, "");
    updateLCD();
  }
}

}  // namespace

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println(F("\n===================================="));
  Serial.println(F(" ESP32 DISPENSADOR 3 COMP v3"));
  Serial.println(F("===================================="));

  pinMode(CONFIRM_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  buzzerOff();
  pruebaBuzzerInicio();

  // I2C + PCA9685
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  diagnosticoI2CLcd();
  pca.begin();
  pca.setPWMFreq(SERVO_FREQ);

  for (uint8_t i = 0; i < TOTAL_SERVOS; i++) {
    servo_pos[i]    = SERVO_CLOSED_ANGLE;
    servo_target[i] = SERVO_CLOSED_ANGLE;
    setPCA(i, SERVO_CLOSED_ANGLE);
  }
  Serial.println(F("[INIT] PCA9685 OK - 9 servos"));

  // LCD
  lcd.init();
  delay(200);
  lcd.display();
  delay(100);
  lcd.backlight();
  delay(100);
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(F("Inicializando..."));
  lcd.setCursor(0, 1); lcd.print(F("Dispensador ESP32"));
  Serial.println(F("[INIT] LCD OK"));

  // Red y hora
  conectarWiFi();
  diagnosticoConexionSupabase();
  sincronizarHora();

  setLcdState(LCD_MODE_IDLE, -1, 0, "");
  Serial.println(F("[INIT] LISTO\n"));
}

// ===== LOOP =====
void loop() {
  unsigned long now = millis();

  updateServoSmooth();

  if (wifiOk() && (now - lastProgramacionCheck > PROGRAMACION_INTERVAL_MS)) {
    procesarProgramacion();
    lastProgramacionCheck = now;
  }

  updateLCD();
  manejarPruebaManualBoton();

  if (wifiOk() && (now - lastEstadoPing > ESTADO_INTERVAL_MS)) {
    enviarEstado();
    lastEstadoPing = now;
  }

  if (!wifiOk() && (now - lastWifiRetry > WIFI_RETRY_INTERVAL_MS)) {
    conectarWiFi();
    lastWifiRetry = now;
  }

  if (now - lastNtpSync > NTP_SYNC_INTERVAL_MS) {
    sincronizarHora();
  }

  delay(100);
}
