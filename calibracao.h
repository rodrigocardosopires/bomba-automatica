#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

// ---------------------------------------------------------------------------
// Calibracao das caixas (cheia/vazia), ajustavel pelo Home Assistant.
// Gravada na flash (NVS). Alterada pela tarefa do MQTT e aplicada pelo loop().
// ---------------------------------------------------------------------------
namespace calibracao {

enum Campo : uint8_t { CHEIO_1000, VAZIO_1000, CHEIO_500, VAZIO_500, TOTAL_CAMPOS };

// Nomes usados nas chaves da flash e nos topicos MQTT.
const char* const NOMES[TOTAL_CAMPOS] = {"cheio_1000", "vazio_1000", "cheio_500", "vazio_500"};

struct Valores {
  CalibracaoCaixa c1000;
  CalibracaoCaixa c500;

  float campo(uint8_t i) const {
    switch (i) {
      case CHEIO_1000: return c1000.distCheio;
      case VAZIO_1000: return c1000.distVazio;
      case CHEIO_500: return c500.distCheio;
      default: return c500.distVazio;
    }
  }
  void definir(uint8_t i, float v) {
    switch (i) {
      case CHEIO_1000: c1000.distCheio = v; break;
      case VAZIO_1000: c1000.distVazio = v; break;
      case CHEIO_500: c500.distCheio = v; break;
      default: c500.distVazio = v; break;
    }
  }
};

portMUX_TYPE trava = portMUX_INITIALIZER_UNLOCKED;
Valores atuais = {CAL_1000, CAL_500};
bool pendente = false;

bool valida(const CalibracaoCaixa& c) {
  return c.distCheio >= CAL_MIN_CM && c.distVazio <= CAL_MAX_CM &&
         c.distVazio - c.distCheio >= CAL_FAIXA_MIN_CM;
}

Valores obter() {
  portENTER_CRITICAL(&trava);
  Valores v = atuais;
  portEXIT_CRITICAL(&trava);
  return v;
}

void guardar(const Valores& v) {
  portENTER_CRITICAL(&trava);
  atuais = v;
  pendente = true;
  portEXIT_CRITICAL(&trava);

  Preferences prefs;
  prefs.begin("calibracao", false);
  for (uint8_t i = 0; i < TOTAL_CAMPOS; i++) prefs.putFloat(NOMES[i], v.campo(i));
  prefs.end();
}

void carregar() {
  Valores padrao = {CAL_1000, CAL_500};
  Valores v = padrao;
  Preferences prefs;
  if (prefs.begin("calibracao", true)) {
    for (uint8_t i = 0; i < TOTAL_CAMPOS; i++) v.definir(i, prefs.getFloat(NOMES[i], padrao.campo(i)));
    prefs.end();
  }
  if (!valida(v.c1000)) v.c1000 = CAL_1000;
  if (!valida(v.c500)) v.c500 = CAL_500;
  portENTER_CRITICAL(&trava);
  atuais = v;
  pendente = true;
  portEXIT_CRITICAL(&trava);
}

// Retorna false (e nao altera nada) se o valor deixar a caixa com cheio/vazio incoerentes.
bool alterar(uint8_t campo, float valor) {
  if (campo >= TOTAL_CAMPOS || isnan(valor)) return false;
  Valores v = obter();
  v.definir(campo, valor);
  if (!valida(v.c1000) || !valida(v.c500)) return false;
  guardar(v);
  return true;
}

void restaurarPadrao() { guardar({CAL_1000, CAL_500}); }

// Usado pelo loop(): true uma vez a cada alteracao.
bool consumirAlteracao(Valores& v) {
  portENTER_CRITICAL(&trava);
  bool havia = pendente;
  pendente = false;
  v = atuais;
  portEXIT_CRITICAL(&trava);
  return havia;
}

}  // namespace calibracao
