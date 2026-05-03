#pragma once

#include "config.h"

void setLcdState(LcdMode mode, int8_t compartimento, uint8_t cantidad, const char* medicamento);
void updateLCD();
