#include "input_utils.h"
#include "lcd_ui.h"
#include "wifi_utils.h"

void buzzerOn()  { tone(BUZZER_PIN, 2400); }
void buzzerOff() { noTone(BUZZER_PIN); digitalWrite(BUZZER_PIN, LOW); }

void pruebaBuzzerInicio() {
  for (uint8_t i = 0; i < 2; i++) {
    buzzerOn();  delay(90);
    buzzerOff(); delay(110);
  }
  Serial.println(F("[BUZZER] OK"));
}

bool esperarConfirmacionUsuario(unsigned long timeoutMs) {
  Serial.println(F("[CONFIRM] Esperando confirmacion..."));

  buzzerOn();
  unsigned long lastBeepChange = millis();
  bool          buzzerActivo   = true;

  unsigned long start      = millis();
  bool          lastRaw    = digitalRead(CONFIRM_BUTTON_PIN);
  unsigned long lastChange = millis();

  while (millis() - start < timeoutMs) {
    unsigned long now = millis();

    // Beep intermitente
    if (buzzerActivo) {
      if (now - lastBeepChange >= BUZZER_BEEP_ON_MS) {
        buzzerOff();
        buzzerActivo   = false;
        lastBeepChange = now;
      }
    } else if (now - lastBeepChange >= BUZZER_BEEP_OFF_MS) {
      buzzerOn();
      buzzerActivo   = true;
      lastBeepChange = now;
    }

    // Confirmación por Serial (enviar 'S' o 's')
    if (Serial.available()) {
      char c = (char)Serial.read();
      while (Serial.available()) Serial.read();  // descartar resto del buffer para evitar comandos obsoletos
      if (c == 'S' || c == 's') {
        Serial.println(F("[CONFIRM] Confirmado via Serial (S)"));
        buzzerOff();
        return true;
      }
    }

    bool raw = digitalRead(CONFIRM_BUTTON_PIN);
    if (raw != lastRaw) {
      lastRaw    = raw;
      lastChange = now;
    }

    // Botón presionado y debounce superado
    if ((now - lastChange) >= BUTTON_DEBOUNCE_MS && raw == LOW) {
      Serial.println(F("[CONFIRM] Pastilla confirmada"));
      buzzerOff();
      while (digitalRead(CONFIRM_BUTTON_PIN) == LOW) delay(10);
      return true;
    }

    // Reintento WiFi si cayó durante la espera
    if (!wifiOk() && (now - lastWifiRetry > WIFI_RETRY_INTERVAL_MS)) {
      conectarWiFi();
      lastWifiRetry = now;
    }

    updateLCD();
    delay(10);
  }

  buzzerOff();
  Serial.println(F("[CONFIRM] Timeout - no confirmado"));
  return false;
}