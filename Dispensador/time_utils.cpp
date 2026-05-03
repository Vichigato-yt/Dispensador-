#include "time_utils.h"

bool obtenerFechaHoraLocal(
  char* fecha,
  char* hora,
  int16_t* anio,
  int16_t* diaAnio,
  int16_t* minutoActual
) {
  time_t now = time(nullptr);
  if (now < 24 * 3600) {
    return false;
  }

  struct tm localNow;
  localtime_r(&now, &localNow);

  if (fecha) strftime(fecha, 11, "%Y-%m-%d", &localNow);
  if (hora) strftime(hora, 9, "%H:%M:%S", &localNow);

  if (anio) *anio = static_cast<int16_t>(localNow.tm_year + 1900);
  if (diaAnio) *diaAnio = static_cast<int16_t>(localNow.tm_yday);
  if (minutoActual) *minutoActual = static_cast<int16_t>(localNow.tm_hour * 60 + localNow.tm_min);

  return true;
}

bool parseHoraProgramada(const char* horaDispenso, int16_t& minutos) {
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

  minutos = static_cast<int16_t>(hh * 60 + mm);
  return true;
}

bool horaEnVentana(const char* horaDispenso) {
  int16_t targetMinutes = 0;
  if (!parseHoraProgramada(horaDispenso, targetMinutes)) {
    return false;
  }

  time_t now = time(nullptr);
  if (now < 24 * 3600) {
    return false;
  }

  struct tm localNow;
  localtime_r(&now, &localNow);

  int16_t currentMinutes = static_cast<int16_t>(localNow.tm_hour * 60 + localNow.tm_min);
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

bool slotYaEjecutado(int id, int16_t anio, int16_t diaAnio, int16_t minutoProgramado) {
  for (int i = 0; i < SLOTS_MAX; i++) {
    if (slotsEjecutados[i].id == id &&
        slotsEjecutados[i].anio == anio &&
        slotsEjecutados[i].diaAnio == diaAnio &&
        slotsEjecutados[i].minutoProgramado == minutoProgramado) {
      return true;
    }
  }
  return false;
}

void marcarSlotEjecutado(int id, int16_t anio, int16_t diaAnio, int16_t minutoProgramado) {
  for (int i = 0; i < SLOTS_MAX; i++) {
    if (slotsEjecutados[i].id == 0) {
      slotsEjecutados[i].id = id;
      slotsEjecutados[i].anio = anio;
      slotsEjecutados[i].diaAnio = diaAnio;
      slotsEjecutados[i].minutoProgramado = minutoProgramado;
      return;
    }
  }
}
