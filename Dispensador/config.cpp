#include "config.h"

#if ENABLE_EMAIL_NOTIFICATIONS
// Destinatario de alertas: se puede sobreescribir en runtime si la BD devuelve un correo válido
const char* ALERT_RECIPIENT_EMAIL = ALERT_EMAIL;
#endif

const char* WIFI_SSID     = "RedMen";
const char* WIFI_PASSWORD = "mcjd2024VF";

const char* SUPABASE_URL      = "https://dnruwqzysjjfjpvdnfvf.supabase.co";
const char* SUPABASE_HOST     = "dnruwqzysjjfjpvdnfvf.supabase.co";
const char* SUPABASE_ANON_KEY = "sb_publishable_2kI2ZxbsRILh-jnptALpxQ_aVV-3tM0";

const char* ENDPOINT_PROGRAMACION =
  "/rest/v1/programacion"
  "?select=*,usuario:usuarios(correo,email,usuario,nombre,rol)"
  "&estado=eq.activo&order=hora_dispenso.asc&limit=50";
const char* ENDPOINT_PROGRAMACION_FALLBACK =
  "/rest/v1/programacion?select=*&estado=eq.activo&order=hora_dispenso.asc&limit=50";
const char* ENDPOINT_REGISTRO = "/rest/v1/historial_dispenso";
const char* ENDPOINT_ESTADO   = "/rest/v1/configuracion_dispositivo?id=eq.1";

const char* NTP_SERVER = "pool.ntp.org";
const char* TZ_INFO    = "ECT5";

// Servo channels ordered by compartment:
//   Comp 0 → índices 0,1,2  → canales PCA 12,15,14
//   Comp 1 → índices 3,4,5  → canales PCA 11, 9, 7
//   Comp 2 → índices 6,7,8  → canales PCA  5, 4, 3
// Secuencia dentro de cada grupo: abrir(0) → empujar(1) → cerrar(2)
const uint8_t SERVO_CHANNELS[TOTAL_SERVOS] = {
  12, 15, 14,   // Compartimento 0
  11,  9,  7,   // Compartimento 1
   5,  4,  3    // Compartimento 2
};

int16_t       servo_pos[TOTAL_SERVOS]    = {};
int16_t       servo_target[TOTAL_SERVOS] = {};
unsigned long lastServoUpdate            = 0;

Adafruit_PWMServoDriver pca = Adafruit_PWMServoDriver(PCA_ADDR);
LiquidCrystal_I2C       lcd(LCD_ADDR, 20, 4);
unsigned long           lastLCDUpdate = 0;

unsigned long lastProgramacionCheck             = 0;
unsigned long lastEstadoPing                    = 0;
unsigned long lastNtpSync                       = 0;
unsigned long lastDispenseAt[NUM_COMPARTIMENTOS] = {};
unsigned long lastWifiRetry                     = 0;

LcdMode lcdMode          = LCD_MODE_IDLE;
int8_t  lcdCompartimento = -1;
uint8_t lcdCantidad      =  0;
char    lcdMedicamento[LCD_MEDICAMENTO_MAX + 1] = {};

RegistroReciente recientes[RECIENTES_MAX]     = {};
uint8_t          recientesCount               = 0;

EjecucionSlot slotsEjecutados[SLOTS_MAX] = {};
uint8_t       slotsEjecutadosCount       = 0;