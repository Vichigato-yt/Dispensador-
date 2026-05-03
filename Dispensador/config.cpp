#include "config.h"

#if ENABLE_EMAIL_NOTIFICATIONS
const char* ALERT_RECIPIENT_EMAIL = "YOUR_ALERT_EMAIL";
#endif

const char* WIFI_SSID = "RedMen";
const char* WIFI_PASSWORD = "mcjd2024VF";

const char* SUPABASE_URL = "https://dnruwqzysjjfjpvdnfvf.supabase.co";
const char* SUPABASE_HOST = "dnruwqzysjjfjpvdnfvf.supabase.co";
const char* SUPABASE_ANON_KEY = "sb_publishable_2kI2ZxbsRILh-jnptALpxQ_aVV-3tM0";

const char* ENDPOINT_PROGRAMACION =
  "/rest/v1/programacion?select=*,usuario:usuarios(correo,email,usuario,nombre,rol)&estado=eq.activo&order=hora_dispenso.asc&limit=50";
const char* ENDPOINT_REGISTRO = "/rest/v1/historial_dispenso";
const char* ENDPOINT_ESTADO = "/rest/v1/configuracion_dispositivo?id=eq.1";

const char* NTP_SERVER = "pool.ntp.org";
const char* TZ_INFO = "ECT5";

const uint8_t SERVO_CHANNELS[TOTAL_SERVOS] = {
  12, 14, 15,
  11, 9, 7,
  5, 4, 3
};

int servo_pos[TOTAL_SERVOS];
int servo_target[TOTAL_SERVOS];
unsigned long lastServoUpdate = 0;

Adafruit_PWMServoDriver pca = Adafruit_PWMServoDriver(PCA_ADDR);

LiquidCrystal_I2C lcd(LCD_ADDR, 20, 4);
unsigned long lastLCDUpdate = 0;

unsigned long lastProgramacionCheck = 0;
unsigned long lastEstadoPing = 0;
unsigned long lastNtpSync = 0;
unsigned long lastDispenseAt[NUM_COMPARTIMENTOS] = {0, 0, 0};
unsigned long lastWifiRetry = 0;

LcdMode lcdMode = LCD_MODE_IDLE;
int lcdCompartimento = -1;
int lcdCantidad = 0;
char lcdMedicamento[LCD_MEDICAMENTO_MAX + 1] = {0};

RegistroReciente recientes[20];
int recientesCount = 0;

EjecucionSlot slotsEjecutados[80];
int slotsEjecutadosCount = 0;
