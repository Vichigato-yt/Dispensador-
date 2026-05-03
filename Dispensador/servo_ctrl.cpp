#include "servo_ctrl.h"

void setPCA(uint8_t servoIdx, uint8_t angle) {
  if (servoIdx >= TOTAL_SERVOS) return;
  uint16_t duty    = (uint16_t)map(angle, 0, 180, PCA_POS0, PCA_POS180);
  uint8_t  channel = SERVO_CHANNELS[servoIdx];
  pca.setPWM(channel, 0, duty);
}

void updateServoSmooth() {
  unsigned long now = millis();
  if (now - lastServoUpdate < SERVO_SMOOTH_INTERVAL) return;
  lastServoUpdate = now;

  for (uint8_t i = 0; i < TOTAL_SERVOS; i++) {
    int16_t pos    = servo_pos[i];
    int16_t target = servo_target[i];
    if      (pos < target) pos = (int16_t)min((int)pos + SERVO_SMOOTH_SPEED, (int)target);
    else if (pos > target) pos = (int16_t)max((int)pos - SERVO_SMOOTH_SPEED, (int)target);
    servo_pos[i] = pos;
    setPCA(i, (uint8_t)pos);
  }
}

bool moverServoHasta(uint8_t servoIdx, uint8_t target, unsigned long timeoutMs) {
  if (servoIdx >= TOTAL_SERVOS) return false;
  servo_target[servoIdx] = target;
  unsigned long start = millis();
  while (servo_pos[servoIdx] != target && millis() - start < timeoutMs) {
    updateServoSmooth();
    delay(10);
  }
  return servo_pos[servoIdx] == target;
}

// Ciclo completo de un servo: abre → espera → cierra → espera
// openAngle/closedAngle permiten invertir la dirección según el lado del servo.
void cicloServoDispenso(uint8_t servoIdx, uint8_t openAngle, uint8_t closedAngle) {
  moverServoHasta(servoIdx, openAngle,   4000UL);
  delay(450);
  moverServoHasta(servoIdx, closedAngle, 4000UL);
  delay(350);
}

// Dispensa 'cantidad' pastillas del compartimento indicado (0-based).
// Secuencia por repetición:
//   1. [base+0] abre tapa          (lado derecho → 0° abre, 90° cierra)
//   2. [base+1] empuja pastilla    (lado izquierdo → invertido: 90° abre, 0° cierra)
//   3. [base+2] retorna empujador  (mismo lado que base+0 → 0° abre, 90° cierra)
void ejecutarDispensado(uint8_t compartimento, uint8_t cantidad) {
  if (compartimento >= NUM_COMPARTIMENTOS) return;

  uint8_t base = (uint8_t)(compartimento * SERVOS_POR_COMPARTIMENTO);
  // base+0 → canal PCA SERVO_CHANNELS[base+0]
  // base+1 → canal PCA SERVO_CHANNELS[base+1]
  // base+2 → canal PCA SERVO_CHANNELS[base+2]

  Serial.print(F("[SERVO] Comp "));
  Serial.print(compartimento + 1);
  Serial.print(F(" canales: "));
  Serial.print(SERVO_CHANNELS[base]);   Serial.print(',');
  Serial.print(SERVO_CHANNELS[base+1]); Serial.print(',');
  Serial.println(SERVO_CHANNELS[base+2]);

  for (uint8_t i = 0; i < cantidad; i++) {
    cicloServoDispenso(base,     SERVO_OPEN_ANGLE,   SERVO_CLOSED_ANGLE);  // tapa (derecho)
    cicloServoDispenso(base + 1, SERVO_CLOSED_ANGLE, SERVO_OPEN_ANGLE);    // empujador (izquierdo, invertido)
    cicloServoDispenso(base + 2, SERVO_OPEN_ANGLE,   SERVO_CLOSED_ANGLE);  // retorno (derecho)
  }
}