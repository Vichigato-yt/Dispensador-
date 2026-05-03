#include "servo_ctrl.h"

void setPCA(int servoIdx, int angle) {
  if (servoIdx < 0 || servoIdx >= TOTAL_SERVOS) return;
  int duty = map(angle, 0, 180, PCA_POS0, PCA_POS180);
  uint8_t channel = SERVO_CHANNELS[servoIdx];
  pca.setPWM(channel, 0, duty);
}

void updateServoSmooth() {
  unsigned long now = millis();
  if (now - lastServoUpdate < SERVO_SMOOTH_INTERVAL) return;
  lastServoUpdate = now;

  for (int i = 0; i < TOTAL_SERVOS; i++) {
    if (servo_pos[i] < servo_target[i]) {
      servo_pos[i] = min(servo_pos[i] + SERVO_SMOOTH_SPEED, servo_target[i]);
    } else if (servo_pos[i] > servo_target[i]) {
      servo_pos[i] = max(servo_pos[i] - SERVO_SMOOTH_SPEED, servo_target[i]);
    }
    setPCA(i, servo_pos[i]);
  }
}

bool moverServoHasta(int servoIdx, int target, unsigned long timeoutMs) {
  if (servoIdx < 0 || servoIdx >= TOTAL_SERVOS) return false;

  servo_target[servoIdx] = target;
  unsigned long start = millis();
  while (servo_pos[servoIdx] != target && millis() - start < timeoutMs) {
    updateServoSmooth();
    delay(10);
  }

  return servo_pos[servoIdx] == target;
}

void cicloServoDispenso(int servoIdx) {
  moverServoHasta(servoIdx, SERVO_OPEN_ANGLE, 4000);
  delay(450);
  moverServoHasta(servoIdx, SERVO_CLOSED_ANGLE, 4000);
  delay(350);
}

void ejecutarDispensado(int compartimento, int cantidad) {
  if (compartimento < 0 || compartimento >= NUM_COMPARTIMENTOS) return;

  int base = compartimento * SERVOS_POR_COMPARTIMENTO;
  int servo1_idx = base;
  int servo2_idx = base + 1;
  int servo3_idx = base + 2;

  for (int i = 0; i < cantidad; i++) {
    cicloServoDispenso(servo1_idx);
    cicloServoDispenso(servo2_idx);
    cicloServoDispenso(servo3_idx);
  }
}
