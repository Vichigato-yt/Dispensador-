#include "input_utils.h"
#include "lcd_ui.h"
#include "wifi_utils.h"

void buzzerOn() {
  tone(BUZZER_PIN, 2400);
}

void buzzerOff() {
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
}

void pruebaBuzzerInicio() {
  for (int i = 0; i < 2; i++) {
    buzzerOn();
    delay(90);
    buzzerOff();
    delay(110);
  }
  Serial.println("[BUZZER] Prueba de inicio completada");
}

bool esperarConfirmacionUsuario(unsigned long timeoutMs) {
  Serial.println("[CONFIRM] Esperando confirmacion de usuario...");
  Serial.print("[CONFIRM] Presione boton en GPIO ");
  Serial.println(CONFIRM_BUTTON_PIN);

  buzzerOn();
  unsigned long lastBeepChange = millis();
  bool buzzerActivo = true;

  unsigned long start = millis();
  bool lastRaw = digitalRead(CONFIRM_BUTTON_PIN);
  unsigned long lastChange = millis();

  while (millis() - start < timeoutMs) {
    unsigned long now = millis();

    if (buzzerActivo) {
      if (now - lastBeepChange >= BUZZER_BEEP_ON_MS) {
        buzzerOff();
        buzzerActivo = false;
        lastBeepChange = now;
      }
    } else if (now - lastBeepChange >= BUZZER_BEEP_OFF_MS) {
      buzzerOn();
      buzzerActivo = true;
      lastBeepChange = now;
    }

    bool raw = digitalRead(CONFIRM_BUTTON_PIN);
    if (raw != lastRaw) {
      lastRaw = raw;
      lastChange = millis();
    }

    if ((millis() - lastChange) >= BUTTON_DEBOUNCE_MS && raw == LOW) {
      Serial.println("[CONFIRM] Pastilla recogida (boton confirmado)");
      buzzerOff();
      while (digitalRead(CONFIRM_BUTTON_PIN) == LOW) {
        delay(10);
      }
      return true;
    }

    if (!wifiOk() && (millis() - lastWifiRetry > WIFI_RETRY_INTERVAL_MS)) {
      conectarWiFi();
      lastWifiRetry = millis();
    }

    updateLCD();
    delay(10);
  }

  buzzerOff();
  Serial.println("[CONFIRM] Timeout: usuario no confirmo toma");
  return false;
}
