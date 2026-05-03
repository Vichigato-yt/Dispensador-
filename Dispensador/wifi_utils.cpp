#include "wifi_utils.h"

bool wifiOk() {
  return WiFi.status() == WL_CONNECTED;
}

void conectarWiFi() {
  if (wifiOk()) return;

  Serial.print(F("[WIFI] Conectando..."));
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint8_t intentos = 0;
  while (!wifiOk() && intentos < 20) {
    delay(500);
    Serial.print('.');
    intentos++;
  }

  if (wifiOk()) {
    Serial.print(F(" OK | IP: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F(" FALLO"));
  }
}