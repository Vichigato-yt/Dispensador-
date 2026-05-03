#include "supabase_api.h"

// ===== HELPERS =====

// Escribe el header Authorization directo al HTTPClient, sin construir un String
static void addAuthHeader(HTTPClient& http) {
  char buf[72];
  snprintf(buf, sizeof(buf), "Bearer %s", SUPABASE_ANON_KEY);
  http.addHeader("Authorization", buf);
}

int jsonToInt(JsonVariantConst value, int defaultValue) {
  if (value.is<int>()) return value.as<int>();
  if (value.is<const char*>()) {
    const char* t = value.as<const char*>();
    if (t && t[0] != '\0') return atoi(t);
  }
  return defaultValue;
}

int fieldInt(JsonObjectConst item, const char* key1, const char* key2, int defaultValue) {
  if (!item[key1].isNull()) return jsonToInt(item[key1], defaultValue);
  if (!item[key2].isNull()) return jsonToInt(item[key2], defaultValue);
  return defaultValue;
}

const char* fieldStr(JsonObjectConst item, const char* key1, const char* key2, const char* defaultValue) {
  if (!item[key1].isNull()) return item[key1] | defaultValue;
  if (!item[key2].isNull()) return item[key2] | defaultValue;
  return defaultValue;
}

void addSupabaseHeaders(HTTPClient& http, bool withJsonBody) {
  addAuthHeader(http);
  http.addHeader("apikey",     SUPABASE_ANON_KEY);
  http.addHeader("Accept",     "application/json");
  http.addHeader("Connection", "close");
  if (withJsonBody) {
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Prefer",       "return=minimal");
  }
}

void diagnosticoConexionSupabase() {
  if (!wifiOk()) return;
  IPAddress ip;
  if (!WiFi.hostByName(SUPABASE_HOST, ip)) {
    Serial.println(F("[NET] DNS fallo para Supabase"));
    return;
  }
  Serial.print(F("[NET] Supabase: "));
  Serial.println(ip);

  WiFiClientSecure c;
  c.setInsecure();
  c.setTimeout(5);
  Serial.print(F("[NET] TCP 443: "));
  Serial.println(c.connect(SUPABASE_HOST, 443) ? F("OK") : F("FALLO"));
  c.stop();
}

// ===== EMAIL (SMTP manual con mbedTLS base64) =====

#if ENABLE_EMAIL_NOTIFICATIONS

// Codifica en base64 usando mbedTLS (ya incluido en ESP32 Arduino core)
static bool b64Encode(const char* input, char* outBuf, size_t outSize) {
  size_t needed = 0;
  mbedtls_base64_encode(nullptr, 0, &needed,
    reinterpret_cast<const unsigned char*>(input), strlen(input));
  if (needed == 0 || needed >= outSize) return false;
  size_t written = 0;
  return mbedtls_base64_encode(
    reinterpret_cast<unsigned char*>(outBuf), outSize, &written,
    reinterpret_cast<const unsigned char*>(input), strlen(input)) == 0;
}

// Lee la respuesta SMTP y devuelve el código numérico (3 dígitos)
static int smtpReadCode(WiFiClientSecure& c) {
  unsigned long t = millis();
  while (!c.available() && millis() - t < 5000UL) delay(5);
  if (!c.available()) return -1;
  int code = 0;
  while (c.available()) {
    String line = c.readStringUntil('\n');
    line.trim();
    if (line.length() >= 3) code = line.substring(0, 3).toInt();
    if (line.length() < 4 || line[3] == ' ') break;
  }
  return code;
}

static bool smtpSend(WiFiClientSecure& c, const char* cmd, int expect) {
  c.print(cmd); c.print(F("\r\n"));
  return smtpReadCode(c) == expect;
}

bool credencialesEmailConfiguradas() {
  if (strstr(AUTHOR_EMAIL, "@gmail.com") == nullptr)  return false;
  if (strncmp(AUTHOR_EMAIL,    "YOUR_", 5) == 0)       return false;
  if (strncmp(AUTHOR_PASSWORD, "YOUR_", 5) == 0)       return false;
  return true;
}

bool correoValido(const char* c) {
  if (!c || c[0] == '\0' || strlen(c) < 6) return false;
  return strchr(c, '@') != nullptr && strchr(c, '.') != nullptr;
}

void detectarCorreosReceptores(JsonObjectConst item,
                               const char** correoPaciente,
                               const char** correoCuidador) {
  if (correoPaciente)  *correoPaciente  = "";
  if (correoCuidador)  *correoCuidador  = "";

  JsonVariantConst usr = item["usuario"];
  if (usr.isNull() || !usr.is<JsonObjectConst>()) return;
  JsonObjectConst u   = usr.as<JsonObjectConst>();
  const char*     e   = fieldStr(u, "email", "correo", "");
  const char*     rol = fieldStr(u, "rol",   "role",   "");
  if (e[0] == '\0') return;

  if      (strcmp(rol, "paciente")  == 0) { if (correoPaciente) *correoPaciente = e; }
  else if (strcmp(rol, "cuidador")  == 0) { if (correoCuidador) *correoCuidador = e; }
  else if (correoCuidador && (*correoCuidador)[0] == '\0') { *correoCuidador = e; }
}

// Envía email al destinatario fijo ALERT_EMAIL (aaronmachuca19@gmail.com)
// más cualquier correo de paciente/cuidador que devuelva la BD.
bool enviarCorreoNoConfirmado(
  int         id,
  uint8_t     comp,
  int         medicamentoId,
  const char* medicamentoNombre,
  const char* usuarioNombre,
  const char* correoPaciente,
  const char* correoCuidador
) {
  if (!credencialesEmailConfiguradas()) {
    Serial.println(F("[EMAIL] Credenciales no configuradas"));
    return false;
  }
  if (!wifiOk()) {
    conectarWiFi();
    if (!wifiOk()) { Serial.println(F("[EMAIL] Sin WiFi")); return false; }
  }

  // Limpia la contraseña de espacios en un buffer de stack
  char smtpPass[32] = {};
  uint8_t j = 0;
  for (uint8_t i = 0; AUTHOR_PASSWORD[i] && j < sizeof(smtpPass) - 1; i++) {
    if (AUTHOR_PASSWORD[i] != ' ') smtpPass[j++] = AUTHOR_PASSWORD[i];
  }

  // Codifica credenciales en base64
  char b64User[64] = {}, b64Pass[64] = {};
  if (!b64Encode(AUTHOR_EMAIL, b64User, sizeof(b64User)) ||
      !b64Encode(smtpPass,     b64Pass, sizeof(b64Pass))) {
    Serial.println(F("[EMAIL] Error base64"));
    return false;
  }

  // Lista de destinatarios: siempre incluye ALERT_EMAIL, más los de la BD si difieren
  const char* rcpts[3];
  uint8_t     rcptCount = 0;
  rcpts[rcptCount++] = ALERT_EMAIL;   // aaronmachuca19@gmail.com siempre

  if (correoValido(correoPaciente) && strcmp(correoPaciente, ALERT_EMAIL) != 0)
    rcpts[rcptCount++] = correoPaciente;
  if (correoValido(correoCuidador) &&
      strcmp(correoCuidador, ALERT_EMAIL) != 0 &&
      (!correoValido(correoPaciente) || strcmp(correoCuidador, correoPaciente) != 0))
    rcpts[rcptCount++] = correoCuidador;

  // Cuerpo del email
  char body[100] = {};
  if (medicamentoNombre && medicamentoNombre[0] != '\0') {
    snprintf(body, sizeof(body),
             "Medicamento no confirmado - Compartimento %u: %s",
             (unsigned)(comp + 1), medicamentoNombre);
  } else {
    snprintf(body, sizeof(body),
             "Medicamento no confirmado - Compartimento %u",
             (unsigned)(comp + 1));
  }

  // Conexión SMTP TLS
  WiFiClientSecure smtp;
  smtp.setInsecure();
  smtp.setTimeout(10);
  if (!smtp.connect(SMTP_HOST, SMTP_PORT)) {
    Serial.println(F("[EMAIL] No se pudo conectar al SMTP"));
    return false;
  }

  if (smtpReadCode(smtp) != 220)                      { smtp.stop(); return false; }
  if (!smtpSend(smtp, "EHLO esp32.local",        250)) { smtp.stop(); return false; }
  if (!smtpSend(smtp, "AUTH LOGIN",              334)) { smtp.stop(); return false; }
  if (!smtpSend(smtp, b64User,                  334)) { smtp.stop(); return false; }
  if (!smtpSend(smtp, b64Pass,                  235)) { smtp.stop(); return false; }

  // MAIL FROM
  char cmd[80] = {};
  snprintf(cmd, sizeof(cmd), "MAIL FROM:<%s>", AUTHOR_EMAIL);
  if (!smtpSend(smtp, cmd, 250)) { smtp.stop(); return false; }

  // RCPT TO para cada destinatario
  for (uint8_t i = 0; i < rcptCount; i++) {
    snprintf(cmd, sizeof(cmd), "RCPT TO:<%s>", rcpts[i]);
    if (!smtpSend(smtp, cmd, 250)) { smtp.stop(); return false; }
  }

  if (!smtpSend(smtp, "DATA", 354)) { smtp.stop(); return false; }

  smtp.print(F("From: ESP32 Dispensador <")); smtp.print(AUTHOR_EMAIL); smtp.print(F(">\r\n"));
  smtp.print(F("Subject: ALERTA - Medicamento no tomado\r\n"));
  smtp.print(F("Content-Type: text/plain; charset=utf-8\r\n\r\n"));
  smtp.print(body);
  smtp.print(F("\r\n.\r\n"));

  bool ok = (smtpReadCode(smtp) == 250);
  smtpSend(smtp, "QUIT", 221);
  smtp.stop();

  Serial.print(F("[EMAIL] Envio "));
  Serial.println(ok ? F("OK") : F("FALLO"));
  return ok;
}

#endif  // ENABLE_EMAIL_NOTIFICATIONS

// ===== SUPABASE API =====

int requestSupabase(const char* endpoint, const char* method,
                    const String* requestBody, String* responseBody) {
  int code = -1;
  for (uint8_t intento = 1; intento <= HTTP_RETRIES; intento++) {
    if (!wifiOk()) { conectarWiFi(); if (!wifiOk()) break; }

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(HTTP_READ_TIMEOUT / 1000);

    HTTPClient http;
    if (!http.begin(client, SUPABASE_HOST, 443, endpoint, true)) {
      delay(250); continue;
    }
    http.setConnectTimeout(HTTP_CONNECT_TIMEOUT);
    http.setTimeout(HTTP_READ_TIMEOUT);
    http.setReuse(false);
    addSupabaseHeaders(http, requestBody != nullptr);

    code = (strcmp(method, "GET") == 0)
           ? http.GET()
           : http.sendRequest(method, requestBody ? *requestBody : "");

    if (code > 0 && responseBody) *responseBody = http.getString();
    http.end();
    if (code > 0) return code;
    delay(250);
  }
  return code;
}

bool registrarDispenso(int id, uint8_t comp, int medicamentoId,
                       const char* medicamentoNombre,
                       const char* resultado,
                       const char* observaciones) {
  if (!wifiOk()) return false;

  char    fechaBuf[11] = {}, horaBuf[9] = {};
  int16_t anio = 0, diaAnio = 0, minutoActual = 0;
  if (!obtenerFechaHoraLocal(fechaBuf, horaBuf, &anio, &diaAnio, &minutoActual)) {
    strncpy(fechaBuf, "1970-01-01", sizeof(fechaBuf) - 1);
    strncpy(horaBuf,  "00:00:00",   sizeof(horaBuf)  - 1);
  }

  StaticJsonDocument<256> doc;
  doc["id_programacion"]  = id;
  doc["id_compartimento"] = (int)(comp + 1);
  doc["fecha"]            = fechaBuf;
  doc["hora"]             = horaBuf;
  doc["id_medicamento"]   = medicamentoId;
  doc["medicamento"]      = medicamentoNombre ? medicamentoNombre : "";
  doc["resultado"]        = resultado;
  doc["observaciones"]    = observaciones ? observaciones : "registro_esp32";

  String json;
  serializeJson(doc, json);
  int code = requestSupabase(ENDPOINT_REGISTRO, "POST", &json, nullptr);
  if (code == 200 || code == 201) return true;

  // Fallback mínimo con observaciones compactas
  char obs[96] = {};
  snprintf(obs, sizeof(obs), "comp=%u,med_id=%d,med=%s,obs=%s",
           (unsigned)(comp + 1), medicamentoId,
           medicamentoNombre ? medicamentoNombre : "",
           observaciones     ? observaciones     : "");

  StaticJsonDocument<200> fb;
  fb["id_programacion"] = id;
  fb["fecha"]           = fechaBuf;
  fb["hora"]            = horaBuf;
  fb["resultado"]       = resultado;
  fb["observaciones"]   = obs;

  String fbJson;
  serializeJson(fb, fbJson);
  code = requestSupabase(ENDPOINT_REGISTRO, "POST", &fbJson, nullptr);
  return (code == 200 || code == 201);
}

void enviarEstado() {
  if (!wifiOk()) return;
  StaticJsonDocument<64> doc;
  doc["estado"]      = "conectado";
  doc["ultimo_ping"] = (unsigned long)time(nullptr);
  String json;
  serializeJson(doc, json);
  requestSupabase(ENDPOINT_ESTADO, "PATCH", &json, nullptr);
}

bool procesarProgramacion() {
  if (!wifiOk()) return false;

  String payload;
  int code = requestSupabase(ENDPOINT_PROGRAMACION, "GET", nullptr, &payload);

  // Fallback si el join devuelve 300
  if (code == 300) {
    Serial.println(F("[SCHED] HTTP 300 - usando fallback sin join"));
    String fbPayload;
    int fbCode = requestSupabase(ENDPOINT_PROGRAMACION_FALLBACK, "GET", nullptr, &fbPayload);
    if (fbCode == 200) { payload = fbPayload; code = 200; }
  }

  if (code != 200) {
    Serial.print(F("[SCHED] Error HTTP: ")); Serial.println(code);
    return false;
  }

  StaticJsonDocument<3072> doc;
  if (deserializeJson(doc, payload)) {
    Serial.println(F("[SCHED] JSON invalido"));
    return false;
  }

  JsonArray arr = doc.as<JsonArray>();
  Serial.print(F("[SCHED] Registros: ")); Serial.println(arr.size());

  uint8_t ejecutados = 0;

  for (JsonObject item : arr) {
    int         id                = fieldInt(item, "id_programacion",    "id",                 0);
    uint8_t     compartimento     = (uint8_t)fieldInt(item, "id_compartimento", "compartimento",     0);
    uint8_t     cantidad          = (uint8_t)fieldInt(item, "cantidad",          "cantidad_dosis",    1);
    int         medicamentoId     = fieldInt(item, "id_medicamento",     "medicamento_id",     0);
    const char* medicamentoNombre = fieldStr(item, "medicamento",        "nombre_medicamento", "");
    const char* estado            = item["estado"] | "activo";
    const char* horaDispenso      = fieldStr(item, "hora_dispenso",      "hora",               "");

    if (strcmp(estado, "inactivo") == 0)                            continue;
    if (!horaEnVentana(horaDispenso))                               continue;

    char    fechaBuf[11] = {}, horaBuf[9] = {};
    int16_t anio = 0, diaAnio = 0, minutoActual = 0;
    if (!obtenerFechaHoraLocal(fechaBuf, horaBuf, &anio, &diaAnio, &minutoActual)) continue;

    int16_t minutoProgramado = 0;
    if (!parseHoraProgramada(horaDispenso, minutoProgramado))       continue;
    if (slotYaEjecutado(id, anio, diaAnio, minutoProgramado))       continue;
    if (id <= 0 || compartimento == 0 || compartimento > NUM_COMPARTIMENTOS) continue;

    compartimento--;  // 0-based

    // Verificar duplicado reciente
    bool found = false;
    for (uint8_t i = 0; i < recientesCount; i++) {
      if (recientes[i].id == id && millis() - recientes[i].ts < DUPLICATE_WINDOW_MS) {
        found = true; break;
      }
    }
    if (found) continue;
    if (millis() - lastDispenseAt[compartimento] < DISPENSE_COOLDOWN_MS) continue;

    Serial.print(F("[SCHED] Dispensando id="));
    Serial.print(id);
    Serial.print(F(" comp="));
    Serial.print(compartimento + 1);
    Serial.print(F(" hora="));
    Serial.println(horaDispenso);

    // --- DISPENSADO ---
    setLcdState(LCD_MODE_DISPENSO, (int8_t)compartimento, cantidad, medicamentoNombre);
    updateLCD();
    ejecutarDispensado(compartimento, cantidad);
    lastDispenseAt[compartimento] = millis();
    marcarSlotEjecutado(id, anio, diaAnio, minutoProgramado);

    // --- ESPERA CONFIRMACIÓN (60 s) ---
    setLcdState(LCD_MODE_CONFIRM, (int8_t)compartimento, cantidad, medicamentoNombre);
    updateLCD();
    bool confirmado = esperarConfirmacionUsuario(CONFIRM_TIMEOUT_MS);
    setLcdState(LCD_MODE_IDLE, -1, 0, "");
    updateLCD();

    if (confirmado) {
      Serial.println(F("[SCHED] Toma confirmada"));
      registrarDispenso(id, compartimento, medicamentoId,
                        medicamentoNombre, "exitoso", "confirmado_usuario");
    } else {
      // --- NO CONFIRMADO → email a aaronmachuca19@gmail.com ---
      Serial.println(F("[SCHED] No confirmado - registrando y enviando email"));
      registrarDispenso(id, compartimento, medicamentoId,
                        medicamentoNombre, "error", "no_confirmado_usuario");

#if ENABLE_EMAIL_NOTIFICATIONS
      const char* usuarioNombre  = "";
      const char* correoPaciente = "";
      const char* correoCuidador = "";

      JsonVariantConst usuarioVar = item["usuario"];
      if (!usuarioVar.isNull() && usuarioVar.is<JsonObjectConst>()) {
        JsonObjectConst u = usuarioVar.as<JsonObjectConst>();
        usuarioNombre = fieldStr(u, "nombre", "name", "");
      }
      detectarCorreosReceptores(item, &correoPaciente, &correoCuidador);

      enviarCorreoNoConfirmado(id, compartimento, medicamentoId,
                               medicamentoNombre, usuarioNombre,
                               correoPaciente, correoCuidador);
#endif
    }

    // Registrar en recientes para dedup
    if (recientesCount < RECIENTES_MAX) {
      recientes[recientesCount].id = id;
      recientes[recientesCount].ts = millis();
      recientesCount++;
    }

    ejecutados++;
  }

  if (ejecutados > 0) {
    Serial.print(F("[SCHED] Ejecutados: "));
    Serial.println(ejecutados);
  }

  return true;
}