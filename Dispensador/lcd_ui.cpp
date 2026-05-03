#include "lcd_ui.h"
#include "time_utils.h"
#include "wifi_utils.h"

namespace {
// Solo escribe una fila si cambió, evita parpadeo y tráfico I2C innecesario
void writeLine20(uint8_t row, const char* text) {
  char padded[21];
  snprintf(padded, sizeof(padded), "%-20.20s", text ? text : "");
  lcd.setCursor(0, row);
  lcd.print(padded);
}
}  // namespace

void setLcdState(LcdMode mode, int8_t compartimento, uint8_t cantidad, const char* medicamento) {
  lcdMode          = mode;
  lcdCompartimento = compartimento;
  lcdCantidad      = cantidad;
  if (medicamento && medicamento[0] != '\0') {
    strncpy(lcdMedicamento, medicamento, LCD_MEDICAMENTO_MAX);
    lcdMedicamento[LCD_MEDICAMENTO_MAX] = '\0';
  } else {
    lcdMedicamento[0] = '\0';
  }
}

void updateLCD() {
  // Buffer con las últimas líneas mostradas; solo escribe si cambian
  static char lastLines[4][21] = {};

  unsigned long now = millis();
  if (now - lastLCDUpdate < LCD_UPDATE_MS) return;
  lastLCDUpdate = now;

  lcd.backlight();

  char fecha[11] = {};
  char hora[9]   = {};
  bool ok = obtenerFechaHoraLocal(fecha, hora, nullptr, nullptr, nullptr);

  char lines[4][21] = {};

  if (!ok) {
    snprintf(lines[0], sizeof(lines[0]), "Hora no sincronizada");
    snprintf(lines[1], sizeof(lines[1]), "Esperando NTP...");
  } else if (lcdMode == LCD_MODE_DISPENSO) {
    snprintf(lines[0], sizeof(lines[0]), "Dispensando...");
    snprintf(lines[1], sizeof(lines[1]), "Comp: %d", lcdCompartimento + 1);
    snprintf(lines[2], sizeof(lines[2]), "Cant: %u", lcdCantidad);
    snprintf(lines[3], sizeof(lines[3]), wifiOk() ? "WiFi: %.14s" : "WiFi: desconectado",
             wifiOk() ? WiFi.SSID().c_str() : "");
  } else if (lcdMode == LCD_MODE_CONFIRM) {
    snprintf(lines[0], sizeof(lines[0]), "Tomar pastilla");
    snprintf(lines[1], sizeof(lines[1]), "Comp: %d", lcdCompartimento + 1);
    if (lcdMedicamento[0] != '\0') {
      snprintf(lines[2], sizeof(lines[2]), "Med: %.15s", lcdMedicamento);
    } else {
      snprintf(lines[2], sizeof(lines[2]), "Recoja medicamento");
    }
    snprintf(lines[3], sizeof(lines[3]), "Presione boton");
  } else {
    // LCD_MODE_IDLE
    char hhmm[6] = "--:--";
    if (hora[0] != '\0') { strncpy(hhmm, hora, 5); hhmm[5] = '\0'; }
    snprintf(lines[0], sizeof(lines[0]), "%.20s", saludoPorHora());
    snprintf(lines[1], sizeof(lines[1]), "Listo para dispensar");
    snprintf(lines[2], sizeof(lines[2]), "Hora: %s", hhmm);
    snprintf(lines[3], sizeof(lines[3]), wifiOk() ? "WiFi: %.14s" : "WiFi: desconectado",
             wifiOk() ? WiFi.SSID().c_str() : "");
  }

  for (uint8_t row = 0; row < 4; row++) {
    if (strncmp(lastLines[row], lines[row], 20) != 0) {
      writeLine20(row, lines[row]);
      strncpy(lastLines[row], lines[row], 20);
      lastLines[row][20] = '\0';
    }
  }
}