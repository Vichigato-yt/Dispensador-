#ifndef SUPABASE_API_H
#define SUPABASE_API_H

#include "config.h"
#include "wifi_utils.h"
#include "time_utils.h"
#include "lcd_ui.h"
#include "input_utils.h"
#include "servo_ctrl.h"

// ===== HELPER FUNCTIONS =====
String buildAuthHeader();
int jsonToInt(JsonVariantConst value, int defaultValue = 0);
int fieldInt(JsonObjectConst item, const char* key1, const char* key2, int defaultValue = 0);
const char* fieldStr(JsonObjectConst item, const char* key1, const char* key2, const char* defaultValue = "");
const char* httpErrorToText(int code);
void addSupabaseHeaders(HTTPClient& http, bool withJsonBody = false);
void diagnosticoConexionSupabase();

// ===== EMAIL FUNCTIONS =====
#if ENABLE_EMAIL_NOTIFICATIONS
bool credencialesEmailConfiguradas();
bool correoValido(const char* correo);
void detectarCorreosReceptores(JsonObjectConst item, const char** correoPaciente, const char** correoCuidador);
bool enviarCorreoNoConfirmado(
  int id,
  int comp,
  int medicamentoId,
  const char* medicamentoNombre,
  const char* usuarioNombre,
  const char* correoPaciente,
  const char* correoCuidador
);
#endif

// ===== SUPABASE API CALLS =====
int requestSupabase(
  const char* endpoint,
  const char* method,
  const String* requestBody,
  String* responseBody
);

bool registrarDispenso(
  int id,
  int comp,
  int medicamentoId,
  const char* medicamentoNombre,
  const char* resultado,
  const char* observaciones = "registro_esp32"
);

void enviarEstado();

bool procesarProgramacion();

#endif
