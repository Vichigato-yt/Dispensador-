#pragma once
#include "config.h"

void setPCA(uint8_t servoIdx, uint8_t angle);
void updateServoSmooth();
bool moverServoHasta(uint8_t servoIdx, uint8_t target, unsigned long timeoutMs);
void cicloServoDispenso(uint8_t servoIdx);
void ejecutarDispensado(uint8_t compartimento, uint8_t cantidad);