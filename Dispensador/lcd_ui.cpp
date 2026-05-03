#include "lcd_ui.h"
#include "time_utils.h"
#include "wifi_utils.h"

void setLcdState(LcdMode mode, int compartimento, int cantidad, const char* medicamento) {
  lcdMode = mode;
  lcdCompartimento = compartimento;
  lcdCantidad = cantidad;
  if (medicamento && strlen(medicamento) > 0) {
    strncpy(lcdMedicamento, medicamento, LCD_MEDICAMENTO_MAX);
    lcdMedicamento[LCD_MEDICAMENTO_MAX] = '\0';
  } else {
    lcdMedicamento[0] = '\0';
  }
}

void updateLCD() {
  unsigned long now = millis();
  if (now - lastLCDUpdate < LCD_UPDATE_MS) return;
  lastLCDUpdate = now;

  String fecha, hora;
  bool ok = obtenerFechaHoraLocal(fecha, hora, nullptr, nullptr, nullptr);

  lcd.clear();
  if (!ok) {
    lcd.setCursor(0, 0);
    lcd.print("Hora no sincronizada");
    lcd.setCursor(0, 1);
    lcd.print("Esperando NTP...");
    return;
  }

  if (lcdMode == LCD_MODE_DISPENSO) {
    lcd.setCursor(0, 0);
    lcd.print("Dispensando...");
    lcd.setCursor(0, 1);
    lcd.print("Comp: ");
    lcd.print(lcdCompartimento + 1);
    lcd.setCursor(0, 2);
    lcd.print("Cant: ");
    lcd.print(lcdCantidad);
    lcd.setCursor(0, 3);
    if (wifiOk()) {
      lcd.print("WiFi: ");
      lcd.print(WiFi.SSID());
    } else {
      lcd.print("WiFi: desconectado");
    }
    return;
  }

  if (lcdMode == LCD_MODE_CONFIRM) {
    lcd.setCursor(0, 0);
    lcd.print("Tomar pastilla");
    lcd.setCursor(0, 1);
    lcd.print("Comp: ");
    lcd.print(lcdCompartimento + 1);
    lcd.setCursor(0, 2);
    if (strlen(lcdMedicamento) > 0) {
      lcd.print("Med: ");
      lcd.print(lcdMedicamento);
    } else {
      lcd.print("Recoja medicamento");
    }
    lcd.setCursor(0, 3);
    lcd.print("Presione boton");
    return;
  }

  lcd.setCursor(0, 0);
  lcd.print(saludoPorHora());
  lcd.setCursor(0, 1);
  lcd.print("Listo para dispensar");
  lcd.setCursor(0, 2);
  lcd.print("Hora: ");
  if (hora.length() >= 5) {
    lcd.print(hora.substring(0, 5));
  } else {
    lcd.print(hora);
  }
  lcd.setCursor(0, 3);
  if (wifiOk()) {
    lcd.print("WiFi: ");
    lcd.print(WiFi.SSID());
  } else {
    lcd.print("WiFi: desconectado");
  }
}
