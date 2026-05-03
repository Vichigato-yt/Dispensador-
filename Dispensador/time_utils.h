#pragma once

#include "config.h"

bool obtenerFechaHoraLocal(
  char* fecha,
  char* hora,
  int16_t* anio,
  int16_t* diaAnio,
  int16_t* minutoActual
);

bool parseHoraProgramada(const char* horaDispenso, int16_t& minutos);
bool horaEnVentana(const char* horaDispenso);
const char* saludoPorHora();
void sincronizarHora();

// ===== SLOT TRACKING (Prevent duplicate executions per day) =====
bool slotYaEjecutado(int id, int16_t anio, int16_t diaAnio, int16_t minutoProgramado);
void marcarSlotEjecutado(int id, int16_t anio, int16_t diaAnio, int16_t minutoProgramado);
