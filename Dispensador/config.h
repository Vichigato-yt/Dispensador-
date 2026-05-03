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
#include <mbedtls/base64.h>
#define SMTP_HOST       "smtp.gmail.com"
#define SMTP_PORT       465
#define AUTHOR_EMAIL    "pillhouruets@gmail.com"
#define AUTHOR_PASSWORD "motr hcqk dcge umqs"
// Destinatario fijo de alertas de no-confirmación
#define ALERT_EMAIL     "aaronmachuca19@gmail.com"
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
extern const char* ENDPOINT_PROGRAMACION_FALLBACK;
extern const char* ENDPOINT_REGISTRO;
extern const char* ENDPOINT_ESTADO;

// ======= NTP =======
extern const char* NTP_SERVER;
extern const char* TZ_INFO;

// ======= HARDWARE =======
// Servo layout por compartimento (3 servos c/u, índice base = comp * 3):
//   [base+0] → abre/cierra tapa
//   [base+1] → empuja pastilla
//   [base+2] → retorna empujador
// Compartimento 0: canales PCA 12, 14, 15
// Compartimento 1: canales PCA 11,  9,  7
// Compartimento 2: canales PCA  5,  4,  3
constexpr uint8_t NUM_COMPARTIMENTOS       = 3;
constexpr uint8_t SERVOS_POR_COMPARTIMENTO = 3;
constexpr uint8_t TOTAL_SERVOS             = NUM_COMPARTIMENTOS * SERVOS_POR_COMPARTIMENTO;
extern const uint8_t SERVO_CHANNELS[TOTAL_SERVOS];

constexpr uint8_t  SERVO_FREQ         = 50;
constexpr uint8_t  SERVO_OPEN_ANGLE   = 0;
constexpr uint8_t  SERVO_CLOSED_ANGLE = 90;
constexpr uint8_t  I2C_SDA_PIN        = 26;
constexpr uint8_t  I2C_SCL_PIN        = 14;
constexpr uint8_t  CONFIRM_BUTTON_PIN = 19;
constexpr uint8_t  BUZZER_PIN         = 21;

constexpr uint16_t BUZZER_BEEP_ON_MS  = 150;
constexpr uint16_t BUZZER_BEEP_OFF_MS = 1500;

// ======= TIMING =======
constexpr unsigned long PROGRAMACION_INTERVAL_MS = 25000UL;
constexpr unsigned long ESTADO_INTERVAL_MS        = 60000UL;
constexpr unsigned long NTP_SYNC_INTERVAL_MS      = 600000UL;
constexpr unsigned long DISPENSE_COOLDOWN_MS      = 15000UL;
constexpr unsigned long DUPLICATE_WINDOW_MS       = 70000UL;
constexpr unsigned long WIFI_RETRY_INTERVAL_MS    = 15000UL;
constexpr unsigned long HTTP_CONNECT_TIMEOUT      = 5000UL;
constexpr unsigned long HTTP_READ_TIMEOUT         = 6000UL;
// 60 s: si el usuario no confirma en este tiempo → se registra error y se envía email
constexpr unsigned long CONFIRM_TIMEOUT_MS        = 60000UL;
constexpr unsigned long BUTTON_DEBOUNCE_MS        = 40UL;

constexpr uint8_t HTTP_RETRIES            = 2;
constexpr uint8_t DISPENSE_WINDOW_MINUTES = 1;

// ======= SERVO SMOOTH =======
constexpr uint8_t SERVO_SMOOTH_INTERVAL = 20;
constexpr uint8_t SERVO_SMOOTH_SPEED    = 4;

extern int16_t       servo_pos[TOTAL_SERVOS];
extern int16_t       servo_target[TOTAL_SERVOS];
extern unsigned long lastServoUpdate;

// PCA9685
constexpr uint8_t  PCA_ADDR   = 0x40;
extern Adafruit_PWMServoDriver pca;
constexpr uint16_t PCA_POS0   = 172;
constexpr uint16_t PCA_POS180 = 565;

// ======= LCD I2C 20x4 =======
constexpr uint8_t  LCD_ADDR           = 0x27;
extern LiquidCrystal_I2C lcd;
extern unsigned long     lastLCDUpdate;
constexpr uint16_t LCD_UPDATE_MS      = 1000;
constexpr uint8_t  LCD_MEDICAMENTO_MAX = 20;

// ======= STATE =======
extern unsigned long lastProgramacionCheck;
extern unsigned long lastEstadoPing;
extern unsigned long lastNtpSync;
extern unsigned long lastDispenseAt[NUM_COMPARTIMENTOS];
extern unsigned long lastWifiRetry;

enum LcdMode : uint8_t {
  LCD_MODE_IDLE,
  LCD_MODE_DISPENSO,
  LCD_MODE_CONFIRM
};

extern LcdMode  lcdMode;
extern int8_t   lcdCompartimento;
extern uint8_t  lcdCantidad;
extern char     lcdMedicamento[LCD_MEDICAMENTO_MAX + 1];

// Tamaños ajustados al uso real:
//   recientes: dedup de 70 s, máximo 3 disparos simultáneos → 10 es amplio
//   slots: 3 comp × ~13 dosis/día = 39 máx → 40
constexpr uint8_t RECIENTES_MAX = 10;
constexpr uint8_t SLOTS_MAX     = 40;

struct RegistroReciente {
  int          id;
  unsigned long ts;
};
extern RegistroReciente recientes[RECIENTES_MAX];
extern uint8_t          recientesCount;

struct EjecucionSlot {
  int     id;
  int16_t anio;
  int16_t diaAnio;
  int16_t minutoProgramado;
};
extern EjecucionSlot slotsEjecutados[SLOTS_MAX];
extern uint8_t       slotsEjecutadosCount;