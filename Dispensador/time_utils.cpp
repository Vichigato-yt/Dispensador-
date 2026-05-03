#include "time_utils.h"

bool obtenerFechaHoraLocal(
  char*    fecha,
  char*    hora,
  int16_t* anio,
  int16_t* diaAnio,
  int16_t* minutoActual
) {
  time_t now = time(nullptr);
  if (now < 86400L) return false;

  struct tm localNow;
  localtime_r(&now, &localNow);

  if (fecha) strftime(fecha, 11, "%Y-%m-%d", &localNow);
  if (hora)  strftime(hora,   9, "%H:%M:%S", &localNow);

  if (anio)         *anio         = (int16_t)(localNow.tm_year + 1900);
  if (diaAnio)      *diaAnio      = (int16_t) localNow.tm_yday;
  if (minutoActual) *minutoActual = (int16_t)(localNow.tm_hour * 60 + localNow.tm_min);
  return true;
}

// Parse manual: evita sscanf para ahorrar código flash
bool parseHoraProgramada(const char* h, int16_t& minutos) {
  if (!h || h[0] < '0' || h[0] > '9') return false;
  int hh = (h[0] - '0') * 10 + (h[1] - '0');
  if (h[2] != ':') return false;
  int mm = (h[3] - '0') * 10 + (h[4] - '0');
  if (hh > 23 || mm > 59) return false;
  minutos = (int16_t)(hh * 60 + mm);
  return true;
}

bool horaEnVentana(const char* horaDispenso) {
  int16_t target = 0;
  if (!parseHoraProgramada(horaDispenso, target)) return false;

  time_t now = time(nullptr);
  if (now < 86400L) return false;

  struct tm localNow;
  localtime_r(&now, &localNow);

  int16_t current = (int16_t)(localNow.tm_hour * 60 + localNow.tm_min);
  int16_t diff    = current - target;
  if (diff < 0) diff = -diff;
  if (diff > 720) diff = 1440 - diff;
  return diff <= DISPENSE_WINDOW_MINUTES;
}

const char* saludoPorHora() {
  time_t now = time(nullptr);
  if (now < 86400L) return "Bienvenido";
  struct tm localNow;
  localtime_r(&now, &localNow);
  uint8_t h = (uint8_t)localNow.tm_hour;
  if (h < 12) return "Buenos dias";
  if (h < 19) return "Buenas tardes";
  return "Buenas noches";
}

void sincronizarHora() {
  setenv("TZ", TZ_INFO, 1);
  tzset();
  sntp_set_sync_interval(NTP_SYNC_INTERVAL_MS);
  configTzTime(TZ_INFO, NTP_SERVER);

  uint8_t intentos = 0;
  time_t  ahora    = time(nullptr);
  while (ahora < 86400L && intentos < 10) {
    delay(500);
    ahora = time(nullptr);
    intentos++;
  }

  if (ahora > 86400L) {
    Serial.println(F("[NTP] Hora sincronizada OK"));
    lastNtpSync = millis();
  } else {
    Serial.println(F("[NTP] ERROR: no se pudo sincronizar"));
  }
}

// ===== SLOT TRACKING =====

bool slotYaEjecutado(int id, int16_t anio, int16_t diaAnio, int16_t minutoProgramado) {
  for (uint8_t i = 0; i < slotsEjecutadosCount; i++) {
    const EjecucionSlot& s = slotsEjecutados[i];
    if (s.id == id && s.anio == anio &&
        s.diaAnio == diaAnio && s.minutoProgramado == minutoProgramado) {
      return true;
    }
  }
  return false;
}

void marcarSlotEjecutado(int id, int16_t anio, int16_t diaAnio, int16_t minutoProgramado) {
  if (slotsEjecutadosCount >= SLOTS_MAX) return;
  EjecucionSlot& s    = slotsEjecutados[slotsEjecutadosCount++];
  s.id                = id;
  s.anio              = anio;
  s.diaAnio           = diaAnio;
  s.minutoProgramado  = minutoProgramado;
}