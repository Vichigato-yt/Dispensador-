#include "supabase_api.h"
#include <mbedtls/base64.h>

// ===== HELPER FUNCTIONS =====

String buildAuthHeader() {
  return "Bearer " + String(SUPABASE_ANON_KEY);
}

int jsonToInt(JsonVariantConst value, int defaultValue) {
  if (value.is<int>() || value.is<long>() || value.is<unsigned long>()) {
    return value.as<int>();
  }
  if (value.is<const char*>()) {
    const char* text = value.as<const char*>();
    if (text != nullptr && strlen(text) > 0) {
      return atoi(text);
    }
  }
  return defaultValue;
}

int fieldInt(JsonObjectConst item, const char* key1, const char* key2, int defaultValue) {
  if (!item[key1].isNull()) {
    return jsonToInt(item[key1], defaultValue);
  }
  if (!item[key2].isNull()) {
    return jsonToInt(item[key2], defaultValue);
  }
  return defaultValue;
}

const char* fieldStr(JsonObjectConst item, const char* key1, const char* key2, const char* defaultValue) {
  if (!item[key1].isNull()) {
    const char* v = item[key1] | defaultValue;
    return v;
  }
  if (!item[key2].isNull()) {
    const char* v = item[key2] | defaultValue;
    return v;
  }
  return defaultValue;
}

void addSupabaseHeaders(HTTPClient& http, bool withJsonBody) {
  http.addHeader("Authorization", buildAuthHeader());
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("Accept", "application/json");
  http.addHeader("Connection", "close");

  if (withJsonBody) {
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Prefer", "return=minimal");
  }
}

void diagnosticoConexionSupabase() {
  if (!wifiOk()) return;

  IPAddress ip;
  if (!WiFi.hostByName(SUPABASE_HOST, ip)) {
    Serial.println("[NET] DNS fallo para Supabase");
    return;
  }

  Serial.print("[NET] Supabase DNS: ");
  Serial.println(ip);

  WiFiClientSecure testClient;
  testClient.setInsecure();
  testClient.setTimeout(5);

  bool ok = testClient.connect(SUPABASE_HOST, 443);
  Serial.print("[NET] TCP 443: ");
  Serial.println(ok ? "OK" : "FALLO");
  testClient.stop();
}

// ===== EMAIL FUNCTIONS =====

#if ENABLE_EMAIL_NOTIFICATIONS

static String smtpBase64(const String& input) {
  size_t needed = 0;
  mbedtls_base64_encode(nullptr, 0, &needed,
                        reinterpret_cast<const unsigned char*>(input.c_str()),
                        input.length());
  if (needed == 0) return "";

  String out;
  out.reserve(needed + 2);
  unsigned char encoded[256];
  if (needed > sizeof(encoded)) return "";

  if (mbedtls_base64_encode(encoded, sizeof(encoded), &needed,
                            reinterpret_cast<const unsigned char*>(input.c_str()),
                            input.length()) != 0) {
    return "";
  }

  for (size_t i = 0; i < needed; i++) out += static_cast<char>(encoded[i]);
  return out;
}

static bool smtpExpect(WiFiClientSecure& client, int expectedCode) {
  unsigned long start = millis();
  while (!client.available() && millis() - start < 5000) delay(5);
  if (!client.available()) return false;

  int code = 0;
  while (client.available()) {
    String line = client.readStringUntil('\n');
    line.trim();
    if (line.length() >= 3 && isDigit(line[0]) && isDigit(line[1]) && isDigit(line[2])) {
      code = line.substring(0, 3).toInt();
      if (line.length() >= 4 && line[3] == ' ') break;
    }
  }

  return code == expectedCode;
}

static bool smtpCommand(WiFiClientSecure& client, const String& cmd, int expectedCode) {
  client.print(cmd);
  client.print("\r\n");
  return smtpExpect(client, expectedCode);
}

static String passwordSinEspacios() {
  String pass = String(AUTHOR_PASSWORD);
  pass.replace(" ", "");
  return pass;
}

bool credencialesEmailConfiguradas() {
  String author = String(AUTHOR_EMAIL);
  String pass = String(AUTHOR_PASSWORD);
  if (!author.endsWith("@gmail.com")) return false;
  if (author.startsWith("YOUR_")) return false;
  if (pass.startsWith("YOUR_")) return false;
  return true;
}

bool correoValido(const char* correo) {
  if (correo == nullptr) return false;
  String c = String(correo);
  if (c.length() < 6) return false;
  if (c.indexOf('@') < 1) return false;
  if (c.indexOf('.') < 3) return false;
  return true;
}

void detectarCorreosReceptores(
  JsonObjectConst item,
  const char** correoPaciente,
  const char** correoCuidador
) {
  if (correoPaciente) *correoPaciente = "";
  if (correoCuidador) *correoCuidador = "";

  JsonVariantConst usuario = item["usuario"];
  if (!usuario.isNull() && usuario.is<JsonObjectConst>()) {
    JsonObjectConst u = usuario.as<JsonObjectConst>();
    const char* e = fieldStr(u, "email", "correo", "");
    const char* rol = fieldStr(u, "rol", "role", "");
    if (strlen(e) > 0) {
      if (strcmp(rol, "paciente") == 0) {
        if (correoPaciente) *correoPaciente = e;
      } else if (strcmp(rol, "cuidador") == 0) {
        if (correoCuidador) *correoCuidador = e;
      } else if (correoCuidador && strlen(*correoCuidador) == 0) {
        *correoCuidador = e;
      }
    }
  }
}

bool enviarCorreoNoConfirmado(
  int id,
  int comp,
  int medicamentoId,
  const char* medicamentoNombre,
  const char* usuarioNombre,
  const char* correoPaciente,
  const char* correoCuidador
) {
  if (!credencialesEmailConfiguradas()) return false;
  if (!wifiOk()) {
    conectarWiFi();
    if (!wifiOk()) return false;
  }

  const char* recipients[2];
  int recipientCount = 0;
  if (correoValido(correoPaciente)) {
    recipients[recipientCount++] = correoPaciente;
  }
  if (correoValido(correoCuidador)) {
    if (!correoValido(correoPaciente) || strcmp(correoPaciente, correoCuidador) != 0) {
      recipients[recipientCount++] = correoCuidador;
    }
  }

  if (recipientCount == 0) {
    if (!correoValido(ALERT_RECIPIENT_EMAIL)) return false;
    recipients[recipientCount++] = ALERT_RECIPIENT_EMAIL;
  }

  String texto = F("Medicamento no confirmado en compartimento ");
  texto += String(comp + 1);
  if (medicamentoNombre && strlen(medicamentoNombre) > 0) {
    texto += F(" - ");
    texto += medicamentoNombre;
  }

  WiFiClientSecure smtp;
  smtp.setInsecure();
  smtp.setTimeout(10);
  if (!smtp.connect(SMTP_HOST, SMTP_PORT)) return false;
  if (!smtpExpect(smtp, 220)) return false;

  if (!smtpCommand(smtp, "EHLO esp32.local", 250)) return false;
  if (!smtpCommand(smtp, "AUTH LOGIN", 334)) return false;

  String b64User = smtpBase64(String(AUTHOR_EMAIL));
  String b64Pass = smtpBase64(passwordSinEspacios());
  if (b64User.length() == 0 || b64Pass.length() == 0) return false;

  if (!smtpCommand(smtp, b64User, 334)) return false;
  if (!smtpCommand(smtp, b64Pass, 235)) return false;
  if (!smtpCommand(smtp, "MAIL FROM:<" + String(AUTHOR_EMAIL) + ">", 250)) return false;

  for (int i = 0; i < recipientCount; i++) {
    if (!smtpCommand(smtp, "RCPT TO:<" + String(recipients[i]) + ">", 250)) return false;
  }

  if (!smtpCommand(smtp, "DATA", 354)) return false;
  smtp.print(F("From: ESP32 Dispensador <"));
  smtp.print(AUTHOR_EMAIL);
  smtp.print(F(">\r\nSubject: Atencion - Medicamento no consumido\r\n"));
  smtp.print(F("Content-Type: text/plain; charset=utf-8\r\n\r\n"));
  smtp.print(texto);
  smtp.print(F("\r\n.\r\n"));
  if (!smtpExpect(smtp, 250)) return false;

  smtpCommand(smtp, "QUIT", 221);
  smtp.stop();
  return true;
}

#endif

// ===== SUPABASE API CALLS =====

int requestSupabase(
  const char* endpoint,
  const char* method,
  const String* requestBody,
  String* responseBody
) {
  int code = -1;

  for (int intento = 1; intento <= HTTP_RETRIES; intento++) {
    if (!wifiOk()) {
      conectarWiFi();
      if (!wifiOk()) break;
    }

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(HTTP_READ_TIMEOUT / 1000);

    HTTPClient http;
    bool beginOk = http.begin(client, SUPABASE_HOST, 443, endpoint, true);

    if (!beginOk) {
      code = -1;
      delay(250);
      continue;
    }

    http.setConnectTimeout(HTTP_CONNECT_TIMEOUT);
    http.setTimeout(HTTP_READ_TIMEOUT);
    http.setReuse(false);
    addSupabaseHeaders(http, requestBody != nullptr);

    if (strcmp(method, "GET") == 0) {
      code = http.GET();
    } else {
      code = http.sendRequest(method, requestBody ? *requestBody : "");
    }

    if (code > 0 && responseBody != nullptr) {
      *responseBody = http.getString();
    }

    http.end();

    if (code > 0) return code;
    delay(250);
  }

  return code;
}

bool registrarDispenso(
  int id,
  int comp,
  int medicamentoId,
  const char* medicamentoNombre,
  const char* resultado,
  const char* observaciones
) {
  if (!wifiOk()) return false;

  char fecha[11] = {0};
  char hora[9] = {0};
  int16_t anio = 0;
  int16_t diaAnio = 0;
  int16_t minutoActual = 0;
  bool tiempoOk = obtenerFechaHoraLocal(fecha, hora, &anio, &diaAnio, &minutoActual);

  if (!tiempoOk) {
    strncpy(fecha, "1970-01-01", sizeof(fecha) - 1);
    strncpy(hora, "00:00:00", sizeof(hora) - 1);
  }

  StaticJsonDocument<256> doc;
  doc["id_programacion"] = id;
  doc["id_compartimento"] = comp + 1;
  doc["fecha"] = fecha;
  doc["hora"] = hora;
  doc["id_medicamento"] = medicamentoId;
  doc["medicamento"] = medicamentoNombre ? medicamentoNombre : "";
  doc["resultado"] = resultado;
  doc["observaciones"] = observaciones ? observaciones : "registro_esp32";

  String json;
  serializeJson(doc, json);

  int code = requestSupabase(ENDPOINT_REGISTRO, "POST", &json, nullptr);

  if (!(code == 200 || code == 201)) {
    StaticJsonDocument<256> fallback;
    fallback["id_programacion"] = id;
    fallback["fecha"] = fecha;
    fallback["hora"] = hora;
    fallback["resultado"] = resultado;

    String obs = "comp=" + String(comp + 1) +
                 ",med_id=" + String(medicamentoId) +
                 ",med=" + String(medicamentoNombre ? medicamentoNombre : "") +
                 ",obs=" + String(observaciones ? observaciones : "");
    fallback["observaciones"] = obs;

    String fallbackJson;
    serializeJson(fallback, fallbackJson);
    code = requestSupabase(ENDPOINT_REGISTRO, "POST", &fallbackJson, nullptr);
  }

  return (code == 200 || code == 201);
}

void enviarEstado() {
  if (!wifiOk()) return;

  StaticJsonDocument<128> doc;
  doc["estado"] = "conectado";
  doc["ultimo_ping"] = (unsigned long)time(nullptr);

  String json;
  serializeJson(doc, json);

  requestSupabase(ENDPOINT_ESTADO, "PATCH", &json, nullptr);
}

bool procesarProgramacion() {
  if (!wifiOk()) return false;

  String payload;
  int code = requestSupabase(ENDPOINT_PROGRAMACION, "GET", nullptr, &payload);

  if (code != 200) {
    return false;
  }

  StaticJsonDocument<3072> doc;
  if (deserializeJson(doc, payload)) return false;
  JsonArray arr = doc.as<JsonArray>();

  for (JsonObject item : arr) {
    int id = fieldInt(item, "id_programacion", "id", 0);
    int compartimento = fieldInt(item, "id_compartimento", "compartimento", 0);
    int cantidad = fieldInt(item, "cantidad", "cantidad_dosis", 1);
    int medicamentoId = fieldInt(item, "id_medicamento", "medicamento_id", 0);
    const char* medicamentoNombre = fieldStr(item, "medicamento", "nombre_medicamento", "");
    const char* estado = item["estado"] | "activo";
    const char* horaDispenso = fieldStr(item, "hora_dispenso", "hora", "");

    if (strcmp(estado, "inactivo") == 0) continue;

    if (!horaEnVentana(horaDispenso)) continue;

    char fecha[11] = {0};
    char hora[9] = {0};
    int16_t anio = 0;
    int16_t diaAnio = 0;
    int16_t minutoActual = 0;
    if (!obtenerFechaHoraLocal(fecha, hora, &anio, &diaAnio, &minutoActual)) continue;

    int16_t minutoProgramado = 0;
    if (!parseHoraProgramada(horaDispenso, minutoProgramado)) continue;

    if (slotYaEjecutado(id, anio, diaAnio, minutoProgramado)) continue;

    if (id <= 0 || compartimento <= 0 || compartimento > NUM_COMPARTIMENTOS) continue;

    compartimento--;

    bool found = false;
    for (uint8_t i = 0; i < recientesCount; i++) {
      if (recientes[i].id == id && millis() - recientes[i].ts < DUPLICATE_WINDOW_MS) {
        found = true;
        break;
      }
    }
    if (found) continue;

    if (millis() - lastDispenseAt[compartimento] < DISPENSE_COOLDOWN_MS) continue;

    setLcdState(LCD_MODE_DISPENSO, static_cast<int8_t>(compartimento), static_cast<uint8_t>(cantidad), medicamentoNombre);
    updateLCD();
    ejecutarDispensado(compartimento, cantidad);
    lastDispenseAt[compartimento] = millis();

    marcarSlotEjecutado(id, anio, diaAnio, minutoProgramado);

    setLcdState(LCD_MODE_CONFIRM, static_cast<int8_t>(compartimento), static_cast<uint8_t>(cantidad), medicamentoNombre);
    updateLCD();
    bool confirmado = esperarConfirmacionUsuario(CONFIRM_TIMEOUT_MS);
    setLcdState(LCD_MODE_IDLE, -1, 0, "");
    updateLCD();

    if (confirmado) {
      registrarDispenso(
        id,
        compartimento,
        medicamentoId,
        medicamentoNombre,
        "exitoso",
        "confirmado_usuario"
      );
    } else {
      registrarDispenso(
        id,
        compartimento,
        medicamentoId,
        medicamentoNombre,
        "error",
        "no_confirmado_usuario"
      );
#if ENABLE_EMAIL_NOTIFICATIONS
      const char* usuarioNombre = "";
      JsonVariantConst usuarioVar = item["usuario"];
      if (!usuarioVar.isNull() && usuarioVar.is<JsonObjectConst>()) {
        JsonObjectConst u = usuarioVar.as<JsonObjectConst>();
        usuarioNombre = fieldStr(u, "nombre", "name", "");
      }

      const char* correoPaciente = "";
      const char* correoCuidador = "";
      detectarCorreosReceptores(item, &correoPaciente, &correoCuidador);

      enviarCorreoNoConfirmado(
        id,
        compartimento,
        medicamentoId,
        medicamentoNombre,
        usuarioNombre,
        correoPaciente,
        correoCuidador
      );
#endif
    }

    if (recientesCount < RECIENTES_MAX) {
      recientes[recientesCount].id = id;
      recientes[recientesCount].ts = millis();
      recientesCount++;
    }
  }

  return true;
}
