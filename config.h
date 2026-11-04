#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Pinos (ESP32 DevKit V1, 30 pinos)
// ---------------------------------------------------------------------------
constexpr uint8_t PIN_SDA = 21;
constexpr uint8_t PIN_SCL = 22;

constexpr uint8_t PIN_TRIG_1000 = 25;
constexpr uint8_t PIN_ECHO_1000 = 34;  // somente entrada
constexpr uint8_t PIN_TRIG_500 = 26;
constexpr uint8_t PIN_ECHO_500 = 35;   // somente entrada

constexpr uint8_t PIN_RELE = 23;
constexpr uint8_t PIN_LED_VERMELHO = 18;
constexpr uint8_t PIN_LED_VERDE = 19;

// Botao de emergencia NF ligado ao GND: aberto (HIGH) = emergencia.
// Fio rompido tambem e lido como emergencia.
constexpr uint8_t PIN_EMERGENCIA = 32;
// Contato NA do rele RL1C7A127 ligado ao GND: fechado (LOW) = modo manual.
constexpr uint8_t PIN_MANUAL = 33;

// ---------------------------------------------------------------------------
// Rele da bomba
// ---------------------------------------------------------------------------
// A placa vendida aciona o modulo rele com nivel HIGH.
// Nao mude para true: o rele deixaria de desligar.
constexpr bool RELE_ATIVO_EM_LOW = false;

// ---------------------------------------------------------------------------
// Display LCD 16x2 I2C
// ---------------------------------------------------------------------------
constexpr uint8_t LCD_ENDERECO = 0x27;  // alguns modulos usam 0x3F
constexpr uint8_t LCD_COLUNAS = 16;
constexpr uint8_t LCD_LINHAS = 2;

// ---------------------------------------------------------------------------
// Regras de nivel (%)
// ---------------------------------------------------------------------------
constexpr int NIVEL_LIGA = 20;     // liga quando qualquer caixa estiver <= este valor
constexpr int NIVEL_DESLIGA = 98;  // desliga quando as duas estiverem >= este valor ("100%")

// ---------------------------------------------------------------------------
// Calibracao das caixas (cm, distancia da face do sensor ate a agua)
// Use o sketch teste_sensores para medir os valores reais.
// ---------------------------------------------------------------------------
struct CalibracaoCaixa {
  float distCheio;  // distancia com a caixa em 100%
  float distVazio;  // distancia com a caixa em 0%
};

// Valores padrao. Depois de ajustados pelo Home Assistant, os novos ficam gravados
// na flash da ESP32 e passam a valer no lugar destes (ate "Restaurar calibracao").
constexpr CalibracaoCaixa CAL_1000 = {25.0f, 105.0f};
constexpr CalibracaoCaixa CAL_500 = {25.0f, 85.0f};

// Limites aceitos para a calibracao enviada pelo Home Assistant.
constexpr float CAL_MIN_CM = 2.0f;
constexpr float CAL_MAX_CM = 450.0f;
constexpr float CAL_FAIXA_MIN_CM = 10.0f;  // vazio precisa ficar pelo menos 10 cm alem de cheio

// Leituras fora desta margem em relacao a faixa calibrada sao descartadas.
constexpr float MARGEM_DESCARTE_CM = 15.0f;

// ---------------------------------------------------------------------------
// Sensores ultrassonicos (JSN-SR04T / HC-SR04)
// ---------------------------------------------------------------------------
constexpr uint16_t TRIG_PULSO_US = 20;          // JSN-SR04T precisa de >= 10 us
constexpr uint32_t ECHO_TIMEOUT_US = 30000;     // ~5 m ida e volta
constexpr uint32_t INTERVALO_DISPARO_MS = 70;   // entre disparos (alterna os sensores)
constexpr uint8_t AMOSTRAS_MEDIANA = 5;
constexpr uint8_t LEITURAS_INVALIDAS_FALHA = 15;  // consecutivas, por sensor

// ---------------------------------------------------------------------------
// Tempos
// ---------------------------------------------------------------------------
constexpr uint32_t DEBOUNCE_MS = 50;
constexpr uint32_t WATCHDOG_TIMEOUT_MS = 60UL * 1000UL;  // reinicia se o loop travar por 60 s
constexpr uint32_t TEMPO_MAX_BOMBA_MS = 60UL * 60UL * 1000UL;  // 60 min ligada sem encher
constexpr uint32_t TEMPO_MIN_DESLIGADA_MS = 30UL * 1000UL;     // evita religar em sequencia
constexpr uint32_t PISCA_EMERGENCIA_MS = 500;
constexpr uint32_t ALTERNA_TELA_MS = 3000;
constexpr uint32_t LOG_NIVEIS_MS = 5000;

// ---------------------------------------------------------------------------
// Home Assistant via MQTT (credenciais em secrets.h)
// ---------------------------------------------------------------------------
#define FIRMWARE_VERSAO "1.1.1"
#define MQTT_TOPICO_BASE "bombaautom"                    // tambem e o hostname na rede
constexpr uint32_t MQTT_INTERVALO_PUBLICACAO_MS = 60UL * 1000UL;  // publica no minimo a cada 60 s
constexpr uint32_t MQTT_INTERVALO_NIVEIS_MS = 5000;     // mudanca de nivel: no maximo a cada 5 s
constexpr uint32_t MQTT_RECONEXAO_MS = 5000;
constexpr uint32_t WIFI_RECONEXAO_MS = 30UL * 1000UL;
constexpr uint16_t MQTT_TAMANHO_BUFFER = 1024;
