#pragma once
#include <Arduino.h>
#include "config.h"

// ---------------------------------------------------------------------------
// Sensor ultrassonico de nivel (JSN-SR04T em modo 1, compativel com HC-SR04)
// ---------------------------------------------------------------------------
class SensorNivel {
 public:
  SensorNivel(uint8_t pinTrig, uint8_t pinEcho, const CalibracaoCaixa& cal)
      : pinTrig_(pinTrig), pinEcho_(pinEcho), cal_(cal) {}

  void calibrar(const CalibracaoCaixa& cal) { cal_ = cal; }
  const CalibracaoCaixa& calibracao() const { return cal_; }

  void iniciar() {
    pinMode(pinTrig_, OUTPUT);
    digitalWrite(pinTrig_, LOW);
    pinMode(pinEcho_, INPUT);
  }

  // Faz um disparo e guarda a amostra se for valida. Bloqueia no maximo ECHO_TIMEOUT_US.
  void medir() {
    float d = dispararCm();
    if (d >= distMinima() && d <= distMaxima()) {
      amostras_[proxima_] = d;
      proxima_ = (proxima_ + 1) % AMOSTRAS_MEDIANA;
      if (validas_ < AMOSTRAS_MEDIANA) validas_++;
      invalidasSeguidas_ = 0;
    } else if (invalidasSeguidas_ < 255) {
      invalidasSeguidas_++;
      // Em falha, descarta as amostras antigas: ao voltar, o sensor so e aceito
      // depois de novas leituras validas, e a mediana nao usa valores velhos.
      if (invalidasSeguidas_ >= LEITURAS_INVALIDAS_FALHA) validas_ = 0;
    }
  }

  bool ok() const {
    return validas_ >= 3 && invalidasSeguidas_ < LEITURAS_INVALIDAS_FALHA;
  }

  float distanciaCm() const {
    float ordenadas[AMOSTRAS_MEDIANA];
    for (uint8_t i = 0; i < validas_; i++) ordenadas[i] = amostras_[i];
    for (uint8_t i = 1; i < validas_; i++) {
      float v = ordenadas[i];
      int8_t j = i - 1;
      while (j >= 0 && ordenadas[j] > v) {
        ordenadas[j + 1] = ordenadas[j];
        j--;
      }
      ordenadas[j + 1] = v;
    }
    return validas_ ? ordenadas[validas_ / 2] : -1.0f;
  }

  int percentual() const {
    if (!ok()) return -1;
    float pct = (cal_.distVazio - distanciaCm()) * 100.0f / (cal_.distVazio - cal_.distCheio);
    return constrain((int)lroundf(pct), 0, 100);
  }

 private:
  float dispararCm() {
    digitalWrite(pinTrig_, LOW);
    delayMicroseconds(4);
    digitalWrite(pinTrig_, HIGH);
    delayMicroseconds(TRIG_PULSO_US);
    digitalWrite(pinTrig_, LOW);
    unsigned long us = pulseIn(pinEcho_, HIGH, ECHO_TIMEOUT_US);
    if (us == 0) return -1.0f;
    return us / 58.3f;
  }

  float distMinima() const { return cal_.distCheio - MARGEM_DESCARTE_CM; }
  float distMaxima() const { return cal_.distVazio + MARGEM_DESCARTE_CM; }

  uint8_t pinTrig_;
  uint8_t pinEcho_;
  CalibracaoCaixa cal_;
  float amostras_[AMOSTRAS_MEDIANA] = {};
  uint8_t proxima_ = 0;
  uint8_t validas_ = 0;
  uint8_t invalidasSeguidas_ = 0;
};

// ---------------------------------------------------------------------------
// Entrada digital com debounce
// ---------------------------------------------------------------------------
class EntradaDigital {
 public:
  EntradaDigital(uint8_t pino, bool nivelAtivo) : pino_(pino), nivelAtivo_(nivelAtivo) {}

  void iniciar() {
    pinMode(pino_, INPUT_PULLUP);
    bruto_ = digitalRead(pino_);
    estavel_ = bruto_;
    mudouEm_ = millis();
  }

  void atualizar() {
    bool leitura = digitalRead(pino_);
    if (leitura != bruto_) {
      bruto_ = leitura;
      mudouEm_ = millis();
    } else if (estavel_ != bruto_ && millis() - mudouEm_ >= DEBOUNCE_MS) {
      estavel_ = bruto_;
    }
  }

  bool ativa() const { return estavel_ == nivelAtivo_; }

 private:
  uint8_t pino_;
  bool nivelAtivo_;
  bool bruto_ = HIGH;
  bool estavel_ = HIGH;
  uint32_t mudouEm_ = 0;
};
