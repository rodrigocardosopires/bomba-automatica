#pragma once
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "config.h"

class Display {
 public:
  Display() : lcd_(LCD_ENDERECO, LCD_COLUNAS, LCD_LINHAS) {}

  void iniciar() {
    lcd_.init();
    lcd_.backlight();
    lcd_.clear();
    linhas_[0][0] = '\0';
    linhas_[1][0] = '\0';
  }

  // Tela principal: cada caixa ocupa meia tela (8 colunas), com o texto centralizado.
  void mostrarNiveis(int pct1000, int pct500) {
    char l1[LCD_COLUNAS + 1];
    char l2[LCD_COLUNAS + 1];
    char esq[9];
    char dir[9];

    centralizar("CX1000L", esq, 8);
    centralizar("CX500L", dir, 8);
    snprintf(l1, sizeof(l1), "%s%s", esq, dir);

    char txt[8];
    formatarPct(pct1000, txt, sizeof(txt));
    centralizar(txt, esq, 8);
    formatarPct(pct500, txt, sizeof(txt));
    centralizar(txt, dir, 8);
    snprintf(l2, sizeof(l2), "%s%s", esq, dir);

    escrever(l1, l2);
  }

  // Tela de status: duas mensagens centralizadas na largura total.
  void mostrarMensagem(const char* linha1, const char* linha2) {
    char l1[LCD_COLUNAS + 1];
    char l2[LCD_COLUNAS + 1];
    centralizar(linha1, l1, LCD_COLUNAS);
    centralizar(linha2, l2, LCD_COLUNAS);
    escrever(l1, l2);
  }

 private:
  static void formatarPct(int pct, char* destino, size_t tamanho) {
    if (pct < 0) {
      snprintf(destino, tamanho, "ERRO");
    } else {
      snprintf(destino, tamanho, "%d%%", pct);
    }
  }

  static void centralizar(const char* texto, char* destino, uint8_t largura) {
    uint8_t len = strlen(texto);
    if (len > largura) len = largura;
    uint8_t antes = (largura - len) / 2;
    memset(destino, ' ', largura);
    memcpy(destino + antes, texto, len);
    destino[largura] = '\0';
  }

  // So reescreve a linha que mudou, para o display nao piscar.
  void escrever(const char* l1, const char* l2) {
    const char* novas[2] = {l1, l2};
    for (uint8_t i = 0; i < 2; i++) {
      if (strcmp(linhas_[i], novas[i]) != 0) {
        lcd_.setCursor(0, i);
        lcd_.print(novas[i]);
        strncpy(linhas_[i], novas[i], LCD_COLUNAS);
        linhas_[i][LCD_COLUNAS] = '\0';
      }
    }
  }

  LiquidCrystal_I2C lcd_;
  char linhas_[2][LCD_COLUNAS + 1];
};
