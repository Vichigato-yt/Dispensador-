// =====================================================
// DISPENSADOR MEDICINAS ESP32 v3 - MODULAR
// Optimizado para Arduino IDE con estructura modular
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
bool botonManualPrev = HIGH;
unsigned long botonManualDownAt = 0;
bool botonManualDisparado = false;

void diagnosticoI2CLcd() {
  Serial.println("[I2C] Escaneando bus...");
  bool found = false;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      found = true;
      Serial.print("[I2C] Dispositivo en 0x");
      if (addr < 16) Serial.print('0');
      Serial.println(addr, HEX);
    }
  }

  Wire.beginTransmission(LCD_ADDR);
  uint8_t lcdErr = Wire.endTransmission();
  if (lcdErr == 0) {
    Serial.print("[LCD] Detectado en 0x");
    if (LCD_ADDR < 16) Serial.print('0');
    Serial.println(LCD_ADDR, HEX);
  } else {
    Serial.print("[LCD] No responde en 0x");
    if (LCD_ADDR < 16) Serial.print('0');
    Serial.println(LCD_ADDR, HEX);
    if (!found) {
      Serial.println("[LCD] No se detectaron dispositivos I2C. Revisar cableado SDA/SCL y VCC/GND");
    }
  }
}

void manejarPruebaManualBoton() {
  bool raw = digitalRead(CONFIRM_BUTTON_PIN);

  if (raw != botonManualPrev) {
    botonManualPrev = raw;
    if (raw == LOW) {
      botonManualDownAt = millis();
      botonManualDisparado = false;
      Serial.println("[BTN] Boton presionado");
    } else {
      Serial.println("[BTN] Boton liberado");
    }
  }

  if (raw == LOW && !botonManualDisparado && (millis() - botonManualDownAt) > 1500) {
    botonManualDisparado = true;
    Serial.println("[BTN] Pulsacion larga detectada -> prueba manual");

    setLcdState(LCD_MODE_CONFIRM, 0, 1, "PRUEBA");
    updateLCD();

    bool ok = esperarConfirmacionUsuario(15000);
    Serial.print("[BTN] Resultado prueba manual: ");
    Serial.println(ok ? "CONFIRMADO" : "TIMEOUT");

    setLcdState(LCD_MODE_IDLE, -1, 0, "");
    updateLCD();
  }
}
}  // namespace

// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n\n====================================");
  Serial.println(" ESP32 DISPENSADOR 3 COMP v3");
  Serial.println(" Modular + Supabase");
  Serial.println("====================================\n");

  // Initialize GPIO pins
  pinMode(CONFIRM_BUTTON_PIN, INPUT_PULLUP);
  Serial.print("[INIT] Boton confirmacion GPIO ");
  Serial.println(CONFIRM_BUTTON_PIN);

  pinMode(BUZZER_PIN, OUTPUT);
  buzzerOff();
  Serial.print("[INIT] Buzzer GPIO ");
  Serial.println(BUZZER_PIN);
  pruebaBuzzerInicio();

  // Initialize I2C bus for PCA9685 and LCD
  Serial.println("[INIT] Inicializando 9 servos...");
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  diagnosticoI2CLcd();
  pca.begin();
  pca.setPWMFreq(SERVO_FREQ);

  // Initialize servo positions to closed angle
  for (int i = 0; i < TOTAL_SERVOS; i++) {
    servo_pos[i] = SERVO_CLOSED_ANGLE;
    servo_target[i] = SERVO_CLOSED_ANGLE;
    setPCA(i, SERVO_CLOSED_ANGLE);
  }
  Serial.println("[INIT] PCA9685 servos OK\n");

  // Initialize LCD
  Serial.println("[INIT] Inicializando LCD I2C 20x4...");
  lcd.init();
  delay(200);
  lcd.display();
  delay(100);
  lcd.backlight();
  delay(100);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Inicializando...");
  lcd.setCursor(0, 1);
  lcd.print("Dispensador ESP32");
  Serial.println("[INIT] LCD OK\n");

  // Connect to WiFi and sync time
  conectarWiFi();
  diagnosticoConexionSupabase();
  sincronizarHora();

  // Initialize LCD state machine
  setLcdState(LCD_MODE_IDLE, -1, 0, "");

  Serial.println("\n[INIT] ✓ LISTO - 3 COMPARTIMENTOS\n");
}

// ===== LOOP =====
void loop() {
  unsigned long now = millis();

  // Update servo smoothing/movement
  updateServoSmooth();

  // Check and process scheduled medications (every ~25 seconds)
  if (wifiOk() && (now - lastProgramacionCheck > PROGRAMACION_INTERVAL_MS)) {
    procesarProgramacion();
    lastProgramacionCheck = now;
  }

  // Update LCD display (every ~1 second)
  updateLCD();

  // Manual hardware test (long-press button >1.5s)
  manejarPruebaManualBoton();

  // Send device status ping (every ~5 minutes)
  if (wifiOk() && (now - lastEstadoPing > ESTADO_INTERVAL_MS)) {
    enviarEstado();
    lastEstadoPing = now;
  }

  // Retry WiFi connection if disconnected
  if (!wifiOk() && (now - lastWifiRetry > WIFI_RETRY_INTERVAL_MS)) {
    conectarWiFi();
    lastWifiRetry = now;
  }

  // Re-sync time with NTP server (every ~10 minutes)
  if (now - lastNtpSync > NTP_SYNC_INTERVAL_MS) {
    sincronizarHora();
  }

  delay(100);
}
