#pragma once

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>
#include <LiquidCrystal_I2C.h>
#include <esp_sntp.h>
#include <time.h>

#define ENABLE_EMAIL_NOTIFICATIONS 1
#if ENABLE_EMAIL_NOTIFICATIONS
#include <ESP_Mail_Client.h>
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "pillhouruets@gmail.com"
#define AUTHOR_PASSWORD "motr hcqk dcge umqs"
extern const char* ALERT_RECIPIENT_EMAIL;
#endif

// ======= CONFIG WIFI =======
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;

// ======= CONFIG SUPABASE =======
extern const char* SUPABASE_URL;
extern const char* SUPABASE_HOST;
extern const char* SUPABASE_ANON_KEY;

// ======= ENDPOINTS =======
extern const char* ENDPOINT_PROGRAMACION;
extern const char* ENDPOINT_REGISTRO;
extern const char* ENDPOINT_ESTADO;

// ======= NTP =======
extern const char* NTP_SERVER;
extern const char* TZ_INFO;

// ======= HARDWARE - 3 COMPARTIMENTOS, 9 SERVOS =======
constexpr int NUM_COMPARTIMENTOS = 3;
constexpr int SERVOS_POR_COMPARTIMENTO = 3;
constexpr int TOTAL_SERVOS = NUM_COMPARTIMENTOS * SERVOS_POR_COMPARTIMENTO;
extern const uint8_t SERVO_CHANNELS[TOTAL_SERVOS];

constexpr int SERVO_FREQ = 50;
constexpr int SERVO_OPEN_ANGLE = 0;
constexpr int SERVO_CLOSED_ANGLE = 90;
constexpr int I2C_SDA_PIN = 26;
constexpr int I2C_SCL_PIN = 14;
constexpr int CONFIRM_BUTTON_PIN = 19;
constexpr int BUZZER_PIN = 21;

constexpr unsigned long BUZZER_BEEP_ON_MS = 150;
constexpr unsigned long BUZZER_BEEP_OFF_MS = 1500;

// ======= TIMING =======
constexpr unsigned long PROGRAMACION_INTERVAL_MS = 25000;
constexpr unsigned long ESTADO_INTERVAL_MS = 60000;
constexpr unsigned long NTP_SYNC_INTERVAL_MS = 600000;
constexpr unsigned long DISPENSE_COOLDOWN_MS = 15000;
constexpr unsigned long DUPLICATE_WINDOW_MS = 70000;
constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 15000;
constexpr unsigned long HTTP_CONNECT_TIMEOUT = 5000;
constexpr unsigned long HTTP_READ_TIMEOUT = 6000;
constexpr int HTTP_RETRIES = 2;
constexpr int DISPENSE_WINDOW_MINUTES = 1;
constexpr unsigned long CONFIRM_TIMEOUT_MS = 120000;
constexpr unsigned long BUTTON_DEBOUNCE_MS = 40;

// ======= SERVO SMOOTH =======
constexpr int SERVO_SMOOTH_INTERVAL = 20;
constexpr int SERVO_SMOOTH_SPEED = 4;

extern int servo_pos[TOTAL_SERVOS];
extern int servo_target[TOTAL_SERVOS];
extern unsigned long lastServoUpdate;

// PCA9685 (Adafruit) configuration
constexpr uint8_t PCA_ADDR = 0x40;
extern Adafruit_PWMServoDriver pca;
constexpr int PCA_POS0 = 172;
constexpr int PCA_POS180 = 565;

// ======= LCD I2C 20x4 =======
constexpr uint8_t LCD_ADDR = 0x27;
extern LiquidCrystal_I2C lcd;
extern unsigned long lastLCDUpdate;
constexpr unsigned long LCD_UPDATE_MS = 1000;
constexpr size_t LCD_MEDICAMENTO_MAX = 20;

// ======= STATE =======
extern unsigned long lastProgramacionCheck;
extern unsigned long lastEstadoPing;
extern unsigned long lastNtpSync;
extern unsigned long lastDispenseAt[NUM_COMPARTIMENTOS];
extern unsigned long lastWifiRetry;

enum LcdMode {
  LCD_MODE_IDLE,
  LCD_MODE_DISPENSO,
  LCD_MODE_CONFIRM
};

extern LcdMode lcdMode;
extern int lcdCompartimento;
extern int lcdCantidad;
extern char lcdMedicamento[LCD_MEDICAMENTO_MAX + 1];

struct RegistroReciente {
  int id;
  unsigned long ts;
};

extern RegistroReciente recientes[20];
extern int recientesCount;

struct EjecucionSlot {
  int id;
  int anio;
  int diaAnio;
  int minutoProgramado;
};

extern EjecucionSlot slotsEjecutados[80];
extern int slotsEjecutadosCount;
