#include "lcd_ui.h"
#include "time_utils.h"
#include "wifi_utils.h"

namespace {
void writeLine20(uint8_t row, const char* text) {
  char padded[21] = {0};
  snprintf(padded, sizeof(padded), "%-20.20s", text ? text : "");
  lcd.setCursor(0, row);
  lcd.print(padded);
}
}

void setLcdState(LcdMode mode, int8_t compartimento, uint8_t cantidad, const char* medicamento) {
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
  static char lastLines[4][21] = {{0}};

  unsigned long now = millis();
  if (now - lastLCDUpdate < LCD_UPDATE_MS) return;
  lastLCDUpdate = now;

  lcd.backlight();

  char fecha[11] = {0};
  char hora[9] = {0};
  bool ok = obtenerFechaHoraLocal(fecha, hora, nullptr, nullptr, nullptr);

  char lines[4][21] = {{0}};

  if (!ok) {
    snprintf(lines[0], sizeof(lines[0]), "%s", "Hora no sincronizada");
    snprintf(lines[1], sizeof(lines[1]), "%s", "Esperando NTP...");
  } else if (lcdMode == LCD_MODE_DISPENSO) {
    snprintf(lines[0], sizeof(lines[0]), "%s", "Dispensando...");
    snprintf(lines[1], sizeof(lines[1]), "Comp: %d", lcdCompartimento + 1);
    snprintf(lines[2], sizeof(lines[2]), "Cant: %u", lcdCantidad);
    if (wifiOk()) {
      snprintf(lines[3], sizeof(lines[3]), "WiFi: %.14s", WiFi.SSID().c_str());
    } else {
      snprintf(lines[3], sizeof(lines[3]), "%s", "WiFi: desconectado");
    }
  } else if (lcdMode == LCD_MODE_CONFIRM) {
    snprintf(lines[0], sizeof(lines[0]), "%s", "Tomar pastilla");
    snprintf(lines[1], sizeof(lines[1]), "Comp: %d", lcdCompartimento + 1);
    if (strlen(lcdMedicamento) > 0) {
      snprintf(lines[2], sizeof(lines[2]), "Med: %.15s", lcdMedicamento);
    } else {
      snprintf(lines[2], sizeof(lines[2]), "%s", "Recoja medicamento");
    }
    snprintf(lines[3], sizeof(lines[3]), "%s", "Presione boton");
  } else {
    char hhmm[6] = "--:--";
    if (hora[0] != '\0') {
      strncpy(hhmm, hora, 5);
      hhmm[5] = '\0';
    }

    snprintf(lines[0], sizeof(lines[0]), "%.20s", saludoPorHora());
    snprintf(lines[1], sizeof(lines[1]), "%s", "Listo para dispensar");
    snprintf(lines[2], sizeof(lines[2]), "Hora: %s", hhmm);

    if (wifiOk()) {
      snprintf(lines[3], sizeof(lines[3]), "WiFi: %.14s", WiFi.SSID().c_str());
    } else {
      snprintf(lines[3], sizeof(lines[3]), "%s", "WiFi: desconectado");
    }
  }

  for (uint8_t row = 0; row < 4; row++) {
    if (strncmp(lastLines[row], lines[row], 20) != 0) {
      writeLine20(row, lines[row]);
      strncpy(lastLines[row], lines[row], 20);
      lastLines[row][20] = '\0';
    }
  }
}
