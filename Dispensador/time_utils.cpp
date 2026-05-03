#include "time_utils.h"

bool obtenerFechaHoraLocal(
  String& fecha,
  String& hora,
  int* anio,
  int* diaAnio,
  int* minutoActual
) {
  time_t now = time(nullptr);
  if (now < 24 * 3600) {
    return false;
  }

  struct tm localNow;
  localtime_r(&now, &localNow);

  char fechaBuff[11];
  char horaBuff[9];
  strftime(fechaBuff, sizeof(fechaBuff), "%Y-%m-%d", &localNow);
  strftime(horaBuff, sizeof(horaBuff), "%H:%M:%S", &localNow);
  fecha = fechaBuff;
  hora = horaBuff;

  if (anio) *anio = localNow.tm_year + 1900;
  if (diaAnio) *diaAnio = localNow.tm_yday;
  if (minutoActual) *minutoActual = localNow.tm_hour * 60 + localNow.tm_min;

  return true;
}

bool parseHoraProgramada(const char* horaDispenso, int& minutos) {
  if (horaDispenso == nullptr || strlen(horaDispenso) < 5) {
    return false;
  }

  int hh = -1;
  int mm = -1;
  if (sscanf(horaDispenso, "%d:%d", &hh, &mm) != 2) {
    return false;
  }

  if (hh < 0 || hh > 23 || mm < 0 || mm > 59) {
    return false;
  }

  minutos = hh * 60 + mm;
  return true;
}

bool horaEnVentana(const char* horaDispenso) {
  int targetMinutes = 0;
  if (!parseHoraProgramada(horaDispenso, targetMinutes)) {
    return false;
  }

  time_t now = time(nullptr);
  if (now < 24 * 3600) {
    return false;
  }

  struct tm localNow;
  localtime_r(&now, &localNow);

  int currentMinutes = localNow.tm_hour * 60 + localNow.tm_min;
  int diff = abs(currentMinutes - targetMinutes);
  if (diff > 720) {
    diff = 1440 - diff;
  }

  return diff <= DISPENSE_WINDOW_MINUTES;
}

const char* saludoPorHora() {
  time_t now = time(nullptr);
  if (now < 24 * 3600) return "Bienvenido";

  struct tm localNow;
  localtime_r(&now, &localNow);
  int h = localNow.tm_hour;
  if (h < 12) return "Buenos dias";
  if (h < 19) return "Buenas tardes";
  return "Buenas noches";
}

void sincronizarHora() {
  setenv("TZ", TZ_INFO, 1);
  tzset();

  sntp_set_sync_interval(NTP_SYNC_INTERVAL_MS);
  configTzTime(TZ_INFO, NTP_SERVER);

  int intentos = 0;
  time_t ahora = time(nullptr);

  while (ahora < 24 * 3600 && intentos < 10) {
    delay(500);
    ahora = time(nullptr);
    intentos++;
  }

  if (ahora > 24 * 3600) {
    struct tm localNow;
    localtime_r(&ahora, &localNow);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &localNow);
    Serial.println(F("[NTP] OK"));
    lastNtpSync = millis();
  } else {
  }
}

// ===== SLOT TRACKING =====

bool slotYaEjecutado(int id, int anio, int diaAnio, int minutoProgramado) {
  for (int i = 0; i < 80; i++) {
    if (slotsEjecutados[i].id == id &&
        slotsEjecutados[i].anio == anio &&
        slotsEjecutados[i].diaAnio == diaAnio &&
        slotsEjecutados[i].minutoProgramado == minutoProgramado) {
      return true;
    }
  }
  return false;
}

void marcarSlotEjecutado(int id, int anio, int diaAnio, int minutoProgramado) {
  for (int i = 0; i < 80; i++) {
    if (slotsEjecutados[i].id == 0) {
      slotsEjecutados[i].id = id;
      slotsEjecutados[i].anio = anio;
      slotsEjecutados[i].diaAnio = diaAnio;
      slotsEjecutados[i].minutoProgramado = minutoProgramado;
      return;
    }
  }
}
