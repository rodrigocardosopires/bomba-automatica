// BombaAutom - controle automatico de bomba para duas caixas d'agua (1000L e 500L)
// Placa: ESP32 DevKit V1 | Biblioteca: LiquidCrystal_I2C (Frank de Brabander)

#include <Wire.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include "calibracao.h"
#include "config.h"
#include "sensores.h"
#include "display.h"
#include "mqtt_ha.h"

enum class Estado : uint8_t {
  INICIANDO,
  OCIOSO,
  BOMBANDO,
  MANUAL,
  EMERGENCIA,
  FALHA_SENSOR,
  FALHA_TEMPO,
};

constexpr uint32_t TEMPO_INICIALIZACAO_MS = 3000;

SensorNivel sensor1000(PIN_TRIG_1000, PIN_ECHO_1000, CAL_1000);
SensorNivel sensor500(PIN_TRIG_500, PIN_ECHO_500, CAL_500);
EntradaDigital emergencia(PIN_EMERGENCIA, HIGH);
EntradaDigital manual(PIN_MANUAL, LOW);
Display display;

Estado estado = Estado::INICIANDO;
uint32_t estadoDesde = 0;

// Enchimento automatico em andamento. Sobrevive a emergencia e a falha de sensor,
// para a bomba retomar ate 100% quando a condicao for normalizada.
bool cicloAtivo = false;
bool falhaTempo = false;
bool emergenciaAcionada = false;
uint32_t bombaLigadaEm = 0;
// Tempo ligada nas etapas anteriores do ciclo. Pausas por falha de sensor nao zeram
// o limite de TEMPO_MAX_BOMBA_MS; so um novo ciclo ou destravar a emergencia zeram.
uint32_t tempoBombaCicloMs = 0;
uint32_t bombaDesligadaEm = 0;
bool bombaJaDesligou = false;

uint32_t ultimoDisparo = 0;
bool vezDo1000 = true;
uint32_t ultimoLog = 0;

const char* nomeEstado(Estado e) {
  switch (e) {
    case Estado::INICIANDO: return "INICIANDO";
    case Estado::OCIOSO: return "OCIOSO";
    case Estado::BOMBANDO: return "AUTO";
    case Estado::MANUAL: return "MANUAL";
    case Estado::EMERGENCIA: return "EMERGENCIA";
    case Estado::FALHA_SENSOR: return "FALHA_SENSOR";
    case Estado::FALHA_TEMPO: return "FALHA_TEMPO";
  }
  return "?";
}

void acionarRele(bool ligar) {
  digitalWrite(PIN_RELE, (ligar != RELE_ATIVO_EM_LOW) ? HIGH : LOW);
}

void amostrarSensores() {
  if (millis() - ultimoDisparo < INTERVALO_DISPARO_MS) return;
  ultimoDisparo = millis();
  if (vezDo1000) {
    sensor1000.medir();
  } else {
    sensor500.medir();
  }
  vezDo1000 = !vezDo1000;
}

bool podeReligar() {
  return !bombaJaDesligou || millis() - bombaDesligadaEm >= TEMPO_MIN_DESLIGADA_MS;
}

Estado calcularEstado() {
  if (manual.ativa()) {
    cicloAtivo = false;
    return Estado::MANUAL;
  }

  if (emergencia.ativa()) {
    emergenciaAcionada = true;
    return Estado::EMERGENCIA;
  }
  if (emergenciaAcionada) {
    // Destravar a emergencia tambem rearma a falha por tempo maximo.
    emergenciaAcionada = false;
    falhaTempo = false;
    tempoBombaCicloMs = 0;
  }

  if (estado == Estado::INICIANDO && millis() < TEMPO_INICIALIZACAO_MS) {
    return Estado::INICIANDO;
  }

  if (falhaTempo) return Estado::FALHA_TEMPO;
  if (!sensor1000.ok() || !sensor500.ok()) return Estado::FALHA_SENSOR;

  int p1000 = sensor1000.percentual();
  int p500 = sensor500.percentual();

  if (cicloAtivo) {
    if (p1000 >= NIVEL_DESLIGA && p500 >= NIVEL_DESLIGA) {
      cicloAtivo = false;
      return Estado::OCIOSO;
    }
    if (estado == Estado::BOMBANDO && tempoBombaCicloMs + (millis() - bombaLigadaEm) >= TEMPO_MAX_BOMBA_MS) {
      cicloAtivo = false;
      falhaTempo = true;
      return Estado::FALHA_TEMPO;
    }
    if (estado != Estado::BOMBANDO && !podeReligar()) return Estado::OCIOSO;
    return Estado::BOMBANDO;
  }

  if ((p1000 <= NIVEL_LIGA || p500 <= NIVEL_LIGA) && podeReligar()) {
    cicloAtivo = true;
    tempoBombaCicloMs = 0;
    return Estado::BOMBANDO;
  }
  return Estado::OCIOSO;
}

void mudarEstado(Estado novo) {
  if (novo == estado) return;
  if (novo == Estado::BOMBANDO) {
    bombaLigadaEm = millis();
  } else if (estado == Estado::BOMBANDO) {
    tempoBombaCicloMs += millis() - bombaLigadaEm;
    bombaDesligadaEm = millis();
    bombaJaDesligou = true;
  }
  Serial.printf("ESTADO: %s -> %s\n", nomeEstado(estado), nomeEstado(novo));
  estado = novo;
  estadoDesde = millis();
}

void atualizarSaidas() {
  acionarRele(estado == Estado::BOMBANDO);

  bool vermelho = false;
  if (estado == Estado::BOMBANDO) {
    vermelho = true;
  } else if (estado == Estado::EMERGENCIA) {
    vermelho = ((millis() - estadoDesde) / PISCA_EMERGENCIA_MS) % 2 == 0;
  }
  digitalWrite(PIN_LED_VERMELHO, vermelho ? HIGH : LOW);
  digitalWrite(PIN_LED_VERDE, estado == Estado::MANUAL ? HIGH : LOW);
}

void atualizarDisplay() {
  int p1000 = sensor1000.percentual();
  int p500 = sensor500.percentual();

  if (estado == Estado::INICIANDO) {
    display.mostrarMensagem("BombaAutom", "Iniciando...");
    return;
  }
  if (estado == Estado::OCIOSO || estado == Estado::BOMBANDO) {
    display.mostrarNiveis(p1000, p500);
    return;
  }

  // Estados especiais alternam entre a mensagem e a tela de niveis.
  bool mostrarMensagem = ((millis() - estadoDesde) / ALTERNA_TELA_MS) % 2 == 0;
  if (!mostrarMensagem) {
    display.mostrarNiveis(p1000, p500);
    return;
  }

  switch (estado) {
    case Estado::MANUAL:
      display.mostrarMensagem("MODO MANUAL", "Auto bloqueado");
      break;
    case Estado::EMERGENCIA:
      display.mostrarMensagem("EMERGENCIA!", "Bomba bloqueada");
      break;
    case Estado::FALHA_SENSOR:
      if (!sensor1000.ok() && !sensor500.ok()) {
        display.mostrarMensagem("ERRO SENSOR", "CX1000L e CX500L");
      } else if (!sensor1000.ok()) {
        display.mostrarMensagem("ERRO SENSOR", "CX1000L");
      } else {
        display.mostrarMensagem("ERRO SENSOR", "CX500L");
      }
      break;
    case Estado::FALHA_TEMPO:
      display.mostrarMensagem("TEMPO MAX BOMBA", "Rearme: emerg.");
      break;
    default:
      break;
  }
}

void registrarNiveis() {
  if (millis() - ultimoLog < LOG_NIVEIS_MS) return;
  ultimoLog = millis();
  Serial.printf("NIVEIS: 1000L=%d%% (%.1f cm) 500L=%d%% (%.1f cm) estado=%s\n",
                sensor1000.percentual(), sensor1000.distanciaCm(),
                sensor500.percentual(), sensor500.distanciaCm(), nomeEstado(estado));
}

void aplicarCalibracao() {
  calibracao::Valores v;
  if (!calibracao::consumirAlteracao(v)) return;
  sensor1000.calibrar(v.c1000);
  sensor500.calibrar(v.c500);
  Serial.printf("CALIBRACAO: 1000L cheio=%.1f vazio=%.1f cm | 500L cheio=%.1f vazio=%.1f cm\n",
                v.c1000.distCheio, v.c1000.distVazio, v.c500.distCheio, v.c500.distVazio);
}

void enviarTelemetria() {
  static uint32_t enviadoEm = 0;
  if (millis() - enviadoEm < 100) return;
  enviadoEm = millis();
  Telemetria t;
  t.nivel1000 = sensor1000.percentual();
  t.nivel500 = sensor500.percentual();
  t.dist1000 = sensor1000.ok() ? sensor1000.distanciaCm() : -1.0f;
  t.dist500 = sensor500.ok() ? sensor500.distanciaCm() : -1.0f;
  t.estado = nomeEstado(estado);
  t.bomba = estado == Estado::BOMBANDO;
  t.manual = estado == Estado::MANUAL;
  t.emergencia = estado == Estado::EMERGENCIA;
  t.falha = estado == Estado::FALHA_SENSOR || estado == Estado::FALHA_TEMPO;
  mqttha::atualizar(t);
}

const char* nomeMotivoReinicio(esp_reset_reason_t motivo) {
  switch (motivo) {
    case ESP_RST_POWERON: return "ENERGIA";
    case ESP_RST_TASK_WDT:
    case ESP_RST_INT_WDT:
    case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_BROWNOUT: return "QUEDA_TENSAO";
    case ESP_RST_PANIC: return "ERRO_FIRMWARE";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_EXT: return "BOTAO_EN";
    default: return "OUTRO";
  }
}

void iniciarWatchdog() {
  esp_task_wdt_config_t cfg = {
      .timeout_ms = WATCHDOG_TIMEOUT_MS,
      .idle_core_mask = 0,
      .trigger_panic = true,  // reinicia a ESP32 ao estourar
  };
  // O core Arduino ja inicializa o watchdog de tarefas; nesse caso so reconfigura.
  if (esp_task_wdt_init(&cfg) == ESP_ERR_INVALID_STATE) {
    esp_task_wdt_reconfigure(&cfg);
  }
  esp_task_wdt_add(NULL);
}

void setup() {
  acionarRele(false);
  pinMode(PIN_RELE, OUTPUT);
  acionarRele(false);
  pinMode(PIN_LED_VERMELHO, OUTPUT);
  pinMode(PIN_LED_VERDE, OUTPUT);

  Serial.begin(115200);
  esp_reset_reason_t motivo = esp_reset_reason();
  if (motivo == ESP_RST_TASK_WDT) {
    Serial.println("AVISO: reiniciado pelo watchdog (loop travado)");
  }
  iniciarWatchdog();
  Wire.begin(PIN_SDA, PIN_SCL);

  calibracao::carregar();
  aplicarCalibracao();
  sensor1000.iniciar();
  sensor500.iniciar();
  emergencia.iniciar();
  manual.iniciar();
  display.iniciar();

  Serial.println("BombaAutom iniciado");
  mqttha::iniciar(nomeMotivoReinicio(motivo));
}

void loop() {
  esp_task_wdt_reset();
  emergencia.atualizar();
  manual.atualizar();
  aplicarCalibracao();
  amostrarSensores();

  mudarEstado(calcularEstado());
  atualizarSaidas();
  atualizarDisplay();
  registrarNiveis();
  enviarTelemetria();
}
