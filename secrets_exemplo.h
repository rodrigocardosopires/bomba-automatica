#pragma once
// Copie este arquivo para secrets.h e preencha com os dados da sua rede.
// O secrets.h nao deve ir para o controle de versao (esta no .gitignore).
// Sem o secrets.h, o firmware compila com o MQTT desligado.

#define WIFI_SSID "nome-da-rede"
#define WIFI_SENHA "senha-da-rede"

#define MQTT_SERVIDOR "192.168.1.10"  // IP do Home Assistant (add-on Mosquitto)
#define MQTT_PORTA 1883
#define MQTT_USUARIO "bombaautom"     // usuario criado no Home Assistant; "" se o broker nao exigir
#define MQTT_SENHA "senha-do-usuario"
