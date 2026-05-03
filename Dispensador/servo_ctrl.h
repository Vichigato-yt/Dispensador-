#pragma once

#include "config.h"

void setPCA(int servoIdx, int angle);
void updateServoSmooth();
bool moverServoHasta(int servoIdx, int target, unsigned long timeoutMs);
void cicloServoDispenso(int servoIdx);
void ejecutarDispensado(int compartimento, int cantidad);
