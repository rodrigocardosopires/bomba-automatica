#pragma once
#include <Arduino.h>
#include "calibracao.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Monitoramento pelo Home Assistant via MQTT. O unico comando aceito e o ajuste
// da calibracao das caixas; a bomba nao pode ser acionada pelo MQTT.
//
// Roda numa tarefa propria no nucleo 0: Wi-Fi ou broker fora do ar nunca
// atrasam o controle da bomba, que continua no loop() do nucleo 1.
// Sem o arquivo secrets.h o MQTT fica desligado e o resto funciona igual.
// ---------------------------------------------------------------------------

#if __has_include("secrets.h")
#include "secrets.h"
#define MQTT_HABILITADO 1
#include <PubSubClient.h>
#include <WiFi.h>
#else
#define MQTT_HABILITADO 0
#endif

struct Telemetria {
  int nivel1000 = -1;       // -1 = sensor com falha
  int nivel500 = -1;
  float dist1000 = -1.0f;   // cm; -1 = sensor com falha
  float dist500 = -1.0f;
  const char* estado = "INICIANDO";
  bool bomba = false;
  bool manual = false;
  bool emergencia = false;
  bool falha = false;
};

namespace mqttha {

#if MQTT_HABILITADO

WiFiClient clienteWifi;
PubSubClient mqtt(clienteWifi);
portMUX_TYPE trava = portMUX_INITIALIZER_UNLOCKED;

Telemetria atual;     // escrita pelo loop(), lida pela tarefa
Telemetria publicada;
const char* motivoReinicio = "?";

char idDispositivo[24];
char topicoEstado[64];
char topicoStatus[64];
char topicoCalibracao[64];
char prefixoComandoCal[64];  // "<base>/calibracao/"; comandos em "<base>/calibracao/<campo>/set"
char dispositivo[200];

bool wifiConectado = false;
uint32_t wifiTentativaEm = 0;
bool mqttConectado = false;
uint32_t mqttTentativaEm = 0;
uint32_t publicadoEm = 0;
bool forcarPublicacao = true;
volatile bool reenviarDescoberta = false;
volatile bool publicarCal = true;

const char* textoOuNulo(const char* s) { return (s && s[0]) ? s : nullptr; }

// estado: topico JSON de onde sai o valor (campo = objeto); nullptr para botoes.
void publicarConfig(const char* componente, const char* objeto, const char* nome, const char* extras,
                    const char* estado) {
  char topico[128];
  char leitura[160] = "";
  char payload[MQTT_TAMANHO_BUFFER];
  snprintf(topico, sizeof(topico), "homeassistant/%s/%s/%s/config", componente, idDispositivo, objeto);
  if (estado) {
    snprintf(leitura, sizeof(leitura), "\"stat_t\":\"%s\",\"val_tpl\":\"{{ value_json.%s }}\",", estado, objeto);
  }
  snprintf(payload, sizeof(payload),
           "{\"name\":\"%s\",\"uniq_id\":\"%s_%s\",\"avty_t\":\"%s\",%s%s\"dev\":%s}",
           nome, idDispositivo, objeto, topicoStatus, leitura, extras, dispositivo);
  mqtt.publish(topico, payload, true);
}

void publicarConfig(const char* componente, const char* objeto, const char* nome, const char* extras) {
  publicarConfig(componente, objeto, nome, extras, topicoEstado);
}

void publicarConfigCalibracao(uint8_t campo, const char* nome) {
  char extras[256];
  snprintf(extras, sizeof(extras),
           "\"cmd_t\":\"%s%s/set\",\"min\":%.0f,\"max\":%.0f,\"step\":0.5,\"unit_of_meas\":\"cm\","
           "\"mode\":\"box\",\"ent_cat\":\"config\",\"ic\":\"mdi:arrow-expand-vertical\",",
           prefixoComandoCal, calibracao::NOMES[campo], CAL_MIN_CM, CAL_MAX_CM);
  publicarConfig("number", calibracao::NOMES[campo], nome, extras, topicoCalibracao);
}

void publicarDescoberta() {
  publicarConfig("sensor", "nivel_1000", "Nível CX1000L",
                 "\"unit_of_meas\":\"%\",\"stat_cla\":\"measurement\",\"ic\":\"mdi:water-percent\",");
  publicarConfig("sensor", "nivel_500", "Nível CX500L",
                 "\"unit_of_meas\":\"%\",\"stat_cla\":\"measurement\",\"ic\":\"mdi:water-percent\",");
  publicarConfig("sensor", "estado", "Estado",
                 "\"dev_cla\":\"enum\",\"ops\":[\"INICIANDO\",\"OCIOSO\",\"AUTO\",\"MANUAL\",\"EMERGENCIA\","
                 "\"FALHA_SENSOR\",\"FALHA_TEMPO\"],\"ic\":\"mdi:state-machine\",");
  publicarConfig("binary_sensor", "bomba", "Bomba", "\"dev_cla\":\"running\",\"ic\":\"mdi:water-pump\",");
  publicarConfig("binary_sensor", "manual", "Modo manual", "\"ic\":\"mdi:hand-back-right\",");
  publicarConfig("binary_sensor", "emergencia", "Emergência", "\"dev_cla\":\"safety\",");
  publicarConfig("binary_sensor", "falha", "Falha", "\"dev_cla\":\"problem\",");
  publicarConfig("sensor", "dist_1000", "Distância sensor CX1000L",
                 "\"unit_of_meas\":\"cm\",\"dev_cla\":\"distance\",\"stat_cla\":\"measurement\","
                 "\"ent_cat\":\"diagnostic\",");
  publicarConfig("sensor", "dist_500", "Distância sensor CX500L",
                 "\"unit_of_meas\":\"cm\",\"dev_cla\":\"distance\",\"stat_cla\":\"measurement\","
                 "\"ent_cat\":\"diagnostic\",");
  publicarConfig("sensor", "rssi", "Sinal Wi-Fi",
                 "\"unit_of_meas\":\"dBm\",\"dev_cla\":\"signal_strength\",\"stat_cla\":\"measurement\","
                 "\"ent_cat\":\"diagnostic\",");
  publicarConfig("sensor", "uptime", "Tempo ligado",
                 "\"unit_of_meas\":\"s\",\"dev_cla\":\"duration\",\"ent_cat\":\"diagnostic\",");
  publicarConfig("sensor", "reinicio", "Motivo do último reinício",
                 "\"ent_cat\":\"diagnostic\",\"ic\":\"mdi:restart\",");

  publicarConfigCalibracao(calibracao::CHEIO_1000, "CX1000L distância cheia");
  publicarConfigCalibracao(calibracao::VAZIO_1000, "CX1000L distância vazia");
  publicarConfigCalibracao(calibracao::CHEIO_500, "CX500L distância cheia");
  publicarConfigCalibracao(calibracao::VAZIO_500, "CX500L distância vazia");
  char extras[160];
  snprintf(extras, sizeof(extras), "\"cmd_t\":\"%spadrao/set\",\"ent_cat\":\"config\",\"ic\":\"mdi:restore\",",
           prefixoComandoCal);
  publicarConfig("button", "restaurar_calibracao", "Restaurar calibração padrão", extras, nullptr);
}

void publicarCalibracao() {
  calibracao::Valores v = calibracao::obter();
  char payload[160];
  snprintf(payload, sizeof(payload),
           "{\"cheio_1000\":%.1f,\"vazio_1000\":%.1f,\"cheio_500\":%.1f,\"vazio_500\":%.1f}",
           v.c1000.distCheio, v.c1000.distVazio, v.c500.distCheio, v.c500.distVazio);
  if (mqtt.publish(topicoCalibracao, payload, true)) publicarCal = false;
}

void tratarComandoCalibracao(const char* campo, const char* valor) {
  if (strcmp(campo, "padrao") == 0) {
    calibracao::restaurarPadrao();
    Serial.println("CALIBRACAO: restaurada para os valores de config.h");
  } else {
    uint8_t i = 0;
    while (i < calibracao::TOTAL_CAMPOS && strcmp(campo, calibracao::NOMES[i]) != 0) i++;
    char* fim = nullptr;
    float v = strtof(valor, &fim);
    if (fim == valor || !calibracao::alterar(i, v)) {
      Serial.printf("CALIBRACAO: valor recusado %s=%s (cheio/vazio incoerentes ou fora de %.0f-%.0f cm)\n",
                    campo, valor, CAL_MIN_CM, CAL_MAX_CM);
    } else {
      Serial.printf("CALIBRACAO: %s=%.1f cm\n", campo, v);
    }
  }
  // Publica sempre: se o valor foi recusado, o Home Assistant volta a mostrar o valor em uso.
  publicarCal = true;
}

void aoReceber(char* topico, byte* payload, unsigned int tamanho) {
  // O Home Assistant publica "online" ao reiniciar; as configuracoes precisam ser reenviadas.
  if (strcmp(topico, "homeassistant/status") == 0 && tamanho == 6 && memcmp(payload, "online", 6) == 0) {
    reenviarDescoberta = true;
    return;
  }

  size_t prefixo = strlen(prefixoComandoCal);
  size_t total = strlen(topico);
  if (total > prefixo + 4 && strncmp(topico, prefixoComandoCal, prefixo) == 0 &&
      strcmp(topico + total - 4, "/set") == 0) {
    char campo[24];
    char valor[24];
    size_t n = min(total - prefixo - 4, sizeof(campo) - 1);
    memcpy(campo, topico + prefixo, n);
    campo[n] = '\0';
    n = min((size_t)tamanho, sizeof(valor) - 1);
    memcpy(valor, payload, n);
    valor[n] = '\0';
    tratarComandoCalibracao(campo, valor);
  }
}

void manterWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!wifiConectado) {
      wifiConectado = true;
      Serial.printf("WIFI: conectado, IP %s\n", WiFi.localIP().toString().c_str());
    }
    return;
  }
  if (wifiConectado) {
    wifiConectado = false;
    wifiTentativaEm = millis();
    Serial.println("WIFI: desconectado");
  }
  if (millis() - wifiTentativaEm >= WIFI_RECONEXAO_MS) {
    wifiTentativaEm = millis();
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_SENHA);
  }
}

void manterMqtt() {
  if (mqtt.connected()) return;
  if (mqttConectado) {
    mqttConectado = false;
    Serial.println("MQTT: desconectado");
  }
  if (!wifiConectado) return;
  if (mqttTentativaEm != 0 && millis() - mqttTentativaEm < MQTT_RECONEXAO_MS) return;
  mqttTentativaEm = millis();

  if (!mqtt.connect(idDispositivo, textoOuNulo(MQTT_USUARIO), textoOuNulo(MQTT_SENHA),
                    topicoStatus, 1, true, "offline")) {
    Serial.printf("MQTT: falha ao conectar em %s:%u (rc=%d)\n", MQTT_SERVIDOR, MQTT_PORTA, mqtt.state());
    return;
  }
  mqttConectado = true;
  Serial.println("MQTT: conectado");
  mqtt.publish(topicoStatus, "online", true);
  mqtt.subscribe("homeassistant/status");
  char comandos[80];
  snprintf(comandos, sizeof(comandos), "%s+/set", prefixoComandoCal);
  mqtt.subscribe(comandos);
  publicarDescoberta();
  forcarPublicacao = true;
  publicarCal = true;
}

void numeroOuNulo(char* dst, size_t n, float v, uint8_t casas) {
  if (v < 0) {
    snprintf(dst, n, "null");
  } else {
    snprintf(dst, n, "%.*f", casas, v);
  }
}

const char* onOff(bool v) { return v ? "ON" : "OFF"; }

void publicarEstado() {
  if (!mqtt.connected()) return;

  Telemetria t;
  portENTER_CRITICAL(&trava);
  t = atual;
  portEXIT_CRITICAL(&trava);

  bool mudouEstado = strcmp(t.estado, publicada.estado) != 0 || t.bomba != publicada.bomba ||
                     t.manual != publicada.manual || t.emergencia != publicada.emergencia ||
                     t.falha != publicada.falha;
  bool mudouNivel = t.nivel1000 != publicada.nivel1000 || t.nivel500 != publicada.nivel500;
  uint32_t desde = millis() - publicadoEm;
  if (!forcarPublicacao && !mudouEstado && !(mudouNivel && desde >= MQTT_INTERVALO_NIVEIS_MS) &&
      desde < MQTT_INTERVALO_PUBLICACAO_MS) {
    return;
  }

  char n1000[8], n500[8], d1000[12], d500[12];
  numeroOuNulo(n1000, sizeof(n1000), t.nivel1000, 0);
  numeroOuNulo(n500, sizeof(n500), t.nivel500, 0);
  numeroOuNulo(d1000, sizeof(d1000), t.dist1000, 1);
  numeroOuNulo(d500, sizeof(d500), t.dist500, 1);

  char payload[320];
  snprintf(payload, sizeof(payload),
           "{\"nivel_1000\":%s,\"nivel_500\":%s,\"dist_1000\":%s,\"dist_500\":%s,\"estado\":\"%s\","
           "\"bomba\":\"%s\",\"manual\":\"%s\",\"emergencia\":\"%s\",\"falha\":\"%s\","
           "\"rssi\":%d,\"uptime\":%lu,\"reinicio\":\"%s\"}",
           n1000, n500, d1000, d500, t.estado, onOff(t.bomba), onOff(t.manual), onOff(t.emergencia),
           onOff(t.falha), WiFi.RSSI(), (unsigned long)(esp_timer_get_time() / 1000000ULL), motivoReinicio);
  if (mqtt.publish(topicoEstado, payload, true)) {
    publicada = t;
    publicadoEm = millis();
    forcarPublicacao = false;
  }
}

void tarefa(void*) {
  WiFi.setHostname(MQTT_TOPICO_BASE);  // precisa vir antes de WiFi.mode()
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_SENHA);
  wifiTentativaEm = millis();

  mqtt.setServer(MQTT_SERVIDOR, MQTT_PORTA);
  mqtt.setBufferSize(MQTT_TAMANHO_BUFFER);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(5);
  mqtt.setCallback(aoReceber);

  for (;;) {
    manterWifi();
    manterMqtt();
    if (reenviarDescoberta && mqtt.connected()) {
      reenviarDescoberta = false;
      publicarDescoberta();
      forcarPublicacao = true;
      publicarCal = true;
    }
    if (publicarCal && mqtt.connected()) publicarCalibracao();
    publicarEstado();
    mqtt.loop();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void iniciar(const char* motivo) {
  motivoReinicio = motivo;
  snprintf(idDispositivo, sizeof(idDispositivo), "%s_%012llx", MQTT_TOPICO_BASE,
           (unsigned long long)ESP.getEfuseMac());
  snprintf(topicoEstado, sizeof(topicoEstado), "%s/estado", MQTT_TOPICO_BASE);
  snprintf(topicoStatus, sizeof(topicoStatus), "%s/status", MQTT_TOPICO_BASE);
  snprintf(topicoCalibracao, sizeof(topicoCalibracao), "%s/calibracao", MQTT_TOPICO_BASE);
  snprintf(prefixoComandoCal, sizeof(prefixoComandoCal), "%s/calibracao/", MQTT_TOPICO_BASE);
  snprintf(dispositivo, sizeof(dispositivo),
           "{\"ids\":[\"%s\"],\"name\":\"BombaAutom\",\"mf\":\"BombaAutom\",\"mdl\":\"ESP32 DevKit V1\","
           "\"sw\":\"%s\"}",
           idDispositivo, FIRMWARE_VERSAO);
  Serial.printf("MQTT: habilitado, broker %s:%u, topico %s\n", MQTT_SERVIDOR, MQTT_PORTA, topicoEstado);
  xTaskCreatePinnedToCore(tarefa, "mqtt", 8192, nullptr, 1, nullptr, 0);
}

void atualizar(const Telemetria& t) {
  portENTER_CRITICAL(&trava);
  atual = t;
  portEXIT_CRITICAL(&trava);
}

#else

void iniciar(const char*) { Serial.println("MQTT: desabilitado (crie secrets.h a partir de secrets_exemplo.h)"); }
void atualizar(const Telemetria&) {}

#endif

}  // namespace mqttha
