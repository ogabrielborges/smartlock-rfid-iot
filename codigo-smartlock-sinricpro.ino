// Descomente essa linha para habilitar o debug serial em modo saída
//#define ENABLE_DEBUG
#ifdef ENABLE_DEBUG
       #define DEBUG_ESP_PORT Serial
       #define NODEBUG_WEBSOCKETS
       #define NDEBUG
#endif 

// Algumas bibliotecas importantes
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include "SinricPro.h"
#include "SinricProLock.h"

// Define os pinos da fechadura inteligente e dos leds de resposta
#define LED_TRUE D0
#define LED_FALSE D4
#define LOCK_PIN D1

// Delay para a fechadura inteligente se trancar
#define TEMP_AUTOLOCK 10000 

// Define os pinos do MFRC522-RFID
#define SS_PIN D8
#define RST_PIN D3

// Define as credenciais do wifi e de api
#define WIFI_SSID         "YOUR-WIFI-SSID"    // nome da rede wifi
#define WIFI_PASS         "YOUR-WIFI-PASSWORD"    // senha da rede wifi
#define APP_KEY           "YOUR-APP-KEY-SINRICPRO"      // se parece com isto --> "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET        "YOUR-APP-SECRET-SINRICPRO"   // se parece com isto --> "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define LOCK_ID           "YOUR-DEVICEID-SINRICPRO"    // se parece com isto --> "5dc1564130xxxxxxxxxxxxxx"

// Setar a taxa de transmissão
#define BAUD_RATE         9600

// Cria uma instancia para o MFRC522
MFRC522 mfrc522(SS_PIN, RST_PIN); 

// Variável de tempo para a funcao milis
unsigned long lock_timer = 0;

String valid_card_ids[] = { // string para registrar os cartões válidos
    "3706D73A",
    "031CA0F6"
};

void lock() { // tranca a fechadura inteligente
    digitalWrite(LOCK_PIN, LOW);
    Serial.println("A fechadura foi trancada");
}

void unlock() { // destranca a fechadura inteligente
    digitalWrite(LOCK_PIN, HIGH);
    Serial.println("A fechadura foi destrancada");
}

void start_lock_timer() { // inicia a contagem do tempo na variavel lock_time
    lock_timer = millis();
    Serial.println("Contagem para auto-lock iniciada");
}

void clear_lock_timer() { // reseta a variavel lock_time
    lock_timer = 0;
}

bool lock_timer_has_expired() { // informa que a variavel lock_time chegou no seu valor maximo de X tempo
    if (lock_timer == 0) return false;

    unsigned long passed_time = millis() - lock_timer;
    return passed_time >= TEMP_AUTOLOCK;
}

void unlock_with_auto_relock() { // funcao que inicia o processo de destrancar e trancar a fechadura
    unlock();
    start_lock_timer();
}

void send_lock_state(bool state) {
    SinricProLock &myLock = SinricPro[LOCK_ID];
    myLock.sendLockStateEvent(state);
}

void auto_lock() {
    if (lock_timer_has_expired() == true) {
        Serial.println("Tempo para auto-lock expirado");

        clear_lock_timer();
        lock();
        send_lock_state(true);
    }
}

bool RFID_card_is_present() {
    return (mfrc522.PICC_IsNewCardPresent() == true && mfrc522.PICC_ReadCardSerial() == true);
}

bool RFID_card_is_not_present() {
    return !RFID_card_is_present();
}

String get_RFID_card_ID() {
    char hex_buf[11]{0};

    for (byte i = 0; i < mfrc522.uid.size; i++) {
        sprintf(&hex_buf[i * 2], "%02X", mfrc522.uid.uidByte[i]);
    }

    return String(hex_buf);
}

bool validate_RFID_card(String card_id) {
    for (auto &valid_id : valid_card_ids) {
        if (card_id == valid_id) return true;
    }

    return false;
}

void handleRFID() {
    if (RFID_card_is_not_present()) return;

    String card_id            = get_RFID_card_ID();
    bool   RFID_card_is_valid = validate_RFID_card(card_id);

    if (RFID_card_is_valid) {
        Serial.printf("O cartao RFID \"%s\" e valido.\r\n", card_id.c_str());

        unlock_with_auto_relock();
        send_lock_state(false);
    } else {
        Serial.printf("O cartao RFID \"%s\" nao e valido.\r\n", card_id.c_str());
    }
}

bool onLockState(String deviceId, bool &lockState) { // recebe o estado da fechadura na api e executa a proxima linha
    if (lockState == false)
  {
        unlock_with_auto_relock();
    } else {
        lock();
  } 
  return true;
}

void setupWiFi() { // funcao de setup do Wifi
    Serial.printf("[WIFI] Conectando\r\n");
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    while (WiFi.status() != WL_CONNECTED) {
        Serial.printf("-");
        delay(250);
    }
    Serial.printf("\r\n[CONECTADO] Endereco IP: %s\r\n", WiFi.localIP().toString().c_str());
}

void setupRFID() { // funcao de setup do MFRC522
    SPI.begin();
    mfrc522.PCD_Init();
}

void setupLock() { // define o pino  da fechadura como SAIDA
    pinMode(LOCK_PIN, OUTPUT);
}

void setupSinricPro() { // funcao de setup do SinricPro
    SinricProLock &myLock = SinricPro[LOCK_ID];
    myLock.onLockState(onLockState);
    SinricPro.onConnected([]() { Serial.printf("Conectado ao SinricPro\r\n"); });
    SinricPro.onDisconnected([]() { Serial.printf("Desconectado do SinricPro\r\n"); });
    SinricPro.begin(APP_KEY, APP_SECRET);
}

void setup(){
  Serial.begin(BAUD_RATE);
  Serial.printf("\r\n\r\n");
  setupLock();
  setupRFID();
  setupWiFi();
  setupSinricPro();
//  pinMode(LedVerde, OUTPUT); // define o pino do led verde como saida - encaixar comentario no lugar correto
//  pinMode(LedVermelho, OUTPUT);  // define o pino do led vermelho como saida - encaixar comentario no lugar correto
}

void loop() {

  SinricPro.handle();
  handleRFID();
  auto_lock();
}
