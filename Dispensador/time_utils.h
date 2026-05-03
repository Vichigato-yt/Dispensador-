#pragma once

#include "config.h"

bool obtenerFechaHoraLocal(
  String& fecha,
  String& hora,
  int* anio,
  int* diaAnio,
  int* minutoActual
);

bool parseHoraProgramada(const char* horaDispenso, int& minutos);
bool horaEnVentana(const char* horaDispenso);
const char* saludoPorHora();
void sincronizarHora();

// ===== SLOT TRACKING (Prevent duplicate executions per day) =====
bool slotYaEjecutado(int id, int anio, int diaAnio, int minutoProgramado);
void marcarSlotEjecutado(int id, int anio, int diaAnio, int minutoProgramado);
