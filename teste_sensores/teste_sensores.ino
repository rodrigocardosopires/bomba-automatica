// Teste dos sensores de nivel com o cabo ja instalado.
// Abra o Monitor Serial em 115200. Cada linha mostra a distancia bruta (cm) de cada
// sensor e a variacao (max - min) das ultimas 10 leituras. Variacao de ate 1-2 cm = ok.
//
// Calibracao: anote a distancia com a caixa cheia (distCheio) e vazia (distVazio)
// e digite no Home Assistant (ou copie para CAL_1000 / CAL_500 em config.h).

constexpr uint8_t TRIG_1000 = 25;
constexpr uint8_t ECHO_1000 = 34;
constexpr uint8_t TRIG_500 = 26;
constexpr uint8_t ECHO_500 = 35;
constexpr uint8_t JANELA = 10;

float hist1000[JANELA];
float hist500[JANELA];
uint8_t pos = 0;
uint8_t cheios = 0;

float medir(uint8_t trig, uint8_t echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(4);
  digitalWrite(trig, HIGH);
  delayMicroseconds(20);
  digitalWrite(trig, LOW);
  unsigned long us = pulseIn(echo, HIGH, 30000);
  return us ? us / 58.3f : -1.0f;
}

float variacao(const float* h, uint8_t n) {
  float mn = 1e9, mx = -1e9;
  for (uint8_t i = 0; i < n; i++) {
    if (h[i] < 0) return -1;
    mn = min(mn, h[i]);
    mx = max(mx, h[i]);
  }
  return mx - mn;
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_1000, OUTPUT);
  pinMode(TRIG_500, OUTPUT);
  pinMode(ECHO_1000, INPUT);
  pinMode(ECHO_500, INPUT);
  Serial.println("Teste de sensores: 1000L(cm)  var  |  500L(cm)  var");
}

void loop() {
  float d1 = medir(TRIG_1000, ECHO_1000);
  delay(70);
  float d2 = medir(TRIG_500, ECHO_500);
  delay(70);

  hist1000[pos] = d1;
  hist500[pos] = d2;
  pos = (pos + 1) % JANELA;
  if (cheios < JANELA) cheios++;

  Serial.printf("1000L: %7.1f  var %5.1f  |  500L: %7.1f  var %5.1f\n",
                d1, variacao(hist1000, cheios), d2, variacao(hist500, cheios));
  delay(300);
}
