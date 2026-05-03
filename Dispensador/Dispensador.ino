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
  lcd.backlight();
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
