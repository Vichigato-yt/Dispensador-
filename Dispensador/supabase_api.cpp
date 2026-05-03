#include "supabase_api.h"

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

  SMTPSession smtp;
  smtp.debug(0);

  ESP_Mail_Session session;
  session.server.host_name = SMTP_HOST;
  session.server.port = SMTP_PORT;
  session.login.email = AUTHOR_EMAIL;
  String smtpPassword = String(AUTHOR_PASSWORD);
  smtpPassword.replace(" ", "");
  session.login.password = smtpPassword.c_str();
  session.login.user_domain = "";

  SMTP_Message message;
  message.sender.name = F("ESP32 Dispensador");
  message.sender.email = AUTHOR_EMAIL;
  message.subject = F("Atención - Medicamento no consumido");

  bool added = false;
  if (correoValido(correoPaciente)) {
    message.addRecipient(F("Paciente"), correoPaciente);
    added = true;
  }
  if (correoValido(correoCuidador)) {
    if (!correoValido(correoPaciente) || strcmp(correoPaciente, correoCuidador) != 0) {
      message.addRecipient(F("Cuidador"), correoCuidador);
      added = true;
    }
  }

  if (!added) {
    if (!correoValido(ALERT_RECIPIENT_EMAIL)) return false;
    message.addRecipient(F("Cuidador"), ALERT_RECIPIENT_EMAIL);
  }

  // Simplified email body
  String texto = F("Medicamento no confirmado en compartimento ");
  texto += String(comp + 1);
  if (medicamentoNombre && strlen(medicamentoNombre) > 0) {
    texto += F(" - ");
    texto += medicamentoNombre;
  }

  message.text.content = texto.c_str();
  message.text.charSet = "utf-8";
  message.text.transfer_encoding = Content_Transfer_Encoding::enc_7bit;
  message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_high;
  message.response.notify = esp_mail_smtp_notify_success |
                            esp_mail_smtp_notify_failure |
                            esp_mail_smtp_notify_delay;

  if (!smtp.connect(&session)) return false;
  bool ok = MailClient.sendMail(&smtp, &message, true);
  smtp.closeSession();
  return ok;
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

  String fecha;
  String hora;
  int anio = 0;
  int diaAnio = 0;
  int minutoActual = 0;
  bool tiempoOk = obtenerFechaHoraLocal(fecha, hora, &anio, &diaAnio, &minutoActual);

  if (!tiempoOk) {
    fecha = "1970-01-01";
    hora = "00:00:00";
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

    String fecha;
    String hora;
    int anio = 0;
    int diaAnio = 0;
    int minutoActual = 0;
    if (!obtenerFechaHoraLocal(fecha, hora, &anio, &diaAnio, &minutoActual)) continue;

    int minutoProgramado = 0;
    if (!parseHoraProgramada(horaDispenso, minutoProgramado)) continue;

    if (slotYaEjecutado(id, anio, diaAnio, minutoProgramado)) continue;

    if (id <= 0 || compartimento <= 0 || compartimento > NUM_COMPARTIMENTOS) continue;

    compartimento--;

    bool found = false;
    for (int i = 0; i < recientesCount; i++) {
      if (recientes[i].id == id && millis() - recientes[i].ts < DUPLICATE_WINDOW_MS) {
        found = true;
        break;
      }
    }
    if (found) continue;

    if (millis() - lastDispenseAt[compartimento] < DISPENSE_COOLDOWN_MS) continue;

    setLcdState(LCD_MODE_DISPENSO, compartimento, cantidad, medicamentoNombre);
    updateLCD();
    ejecutarDispensado(compartimento, cantidad);
    lastDispenseAt[compartimento] = millis();

    marcarSlotEjecutado(id, anio, diaAnio, minutoProgramado);

    setLcdState(LCD_MODE_CONFIRM, compartimento, cantidad, medicamentoNombre);
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

    if (recientesCount < 20) {
      recientes[recientesCount].id = id;
      recientes[recientesCount].ts = millis();
      recientesCount++;
    }
  }

  return true;
}
