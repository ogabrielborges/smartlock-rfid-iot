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
#define WIFI_SSID         "GAMINGROOM-2.4GHz"    // [PROJETO] nome da rede wifi
#define WIFI_PASS         "entrega@newba"    // [PROJETO] senha da rede wifi
#define APP_KEY           "4e940f2c-1789-48d1-86af-1feaab6e00b7"      // [PROJETO] Se parece com isto --> "de0bxxxx-1x3x-4x3x-ax2x-5dabxxxxxxxx"
#define APP_SECRET        "43d50cea-92a7-4613-a283-a5917e86e46e-24cbd0e5-aa26-4194-b312-c406e7176f25"   // [PROJETO] Se parece com isto --> "5f36xxxx-x3x7-4x3x-xexe-e86724a9xxxx-4c4axxxx-3x3x-x5xe-x9x3-333d65xxxxxx"
#define LOCK_ID           "63bda09942545d8b75a7acee"    // [PROJETO] Se parece com isto --> "5dc1564130xxxxxxxxxxxxxx"


// Setar a taxa de transmissão
#define BAUD_RATE         9600                // [PROJETO] troca o baud rate

// Cria uma instancia para o MFRC522
MFRC522 mfrc522(SS_PIN, RST_PIN); 

// Variável de tempo para a funcao milis
unsigned long lock_timer = 0;

void lock() { // Tranca a fechadura inteligente
    digitalWrite(LOCK_PIN, LOW);
}

void unlock() { // Destranca a fechadura inteligente
    digitalWrite(LOCK_PIN, HIGH);
}

void start_lock_timer() { // Inicia a contagem do tempo na variavel lock_time
    lock_timer = millis();
}

void clear_lock_timer() { // Reseta a variavel lock_time
    lock_timer = 0;
}

bool lock_timer_has_expired() { // Informa que a variavel lock_time chegou no seu valor maximo de X tempo
    if (lock_timer == 0) return false;

    unsigned long passed_time = millis() - lock_timer;
    return passed_time >= TEMP_AUTOLOCK;
}

bool RFID_card_is_present() { // Verifica e le o cartao no modulo
    return (mfrc522.PICC_IsNewCardPresent() == true && mfrc522.PICC_ReadCardSerial() == true);
}

bool RFID_card_is_valid() { // Verifica se o cartao esta cadastrado
    char hex_buf[11]{0};

    for (byte i = 0; i < mfrc522.uid.size; i++) {
        sprintf(&hex_buf[i * 2], "%02X", mfrc522.uid.uidByte[i]);
    }
    String conteudo(hex_buf);

    return conteudo == "3706D73A" || conteudo == "031CA0F6";
}

void handleRFID() {
    if (RFID_card_is_present()) {
        if (RFID_card_is_valid()) {
            unlock();
            start_lock_timer();
            send_lock_state(false);
        }
      }
}
void auto_lock() {
    if (lock_timer_has_expired() == true) {
        clear_lock_timer();
        lock();
        send_lock_state(true);
    }
}

void send_lock_state(bool state) { // Informa ao sinricpro o estado da fechadura
        SinricProLock& myLock = SinricPro[LOCK_ID];
        myLock.sendLockStateEvent(state);
}

bool onLockState(String deviceId, bool &lockState) { // Recebe o estado da fechadura na api e executa a proxima linha
    if (lockState == false)
  {
    Serial.printf("A fechadura inteligente foi %s\r\n", lockState?"travada":"destravada"); // informa no monitor serial o estado da fechadura
    unlock();
    start_lock_timer();
// Descomente esse código se sua fechadura vai ser acionada várias vezes em um curto período de tempo para não sobrecarregar a api do SinricPro
//    lockState = true;
    } else {
        lock();
  } 
  return true;
}

void setupWiFi() { // Funcao de setup do Wifi
    Serial.printf("\r\n[Wifi]: Conectando");
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    while (WiFi.status() != WL_CONNECTED) {
        Serial.printf(".");
        delay(250);
    }
    Serial.printf("\r\n[CONECTADO] Endereco IP: %s\r\n", WiFi.localIP().toString().c_str());
}

void setupRFID() { // Funcao de setup do MFRC522
    SPI.begin();
    mfrc522.PCD_Init();
}

void setupLock() { // Define o pino  da fechadura como SAIDA
    pinMode(LOCK_PIN, OUTPUT);
}

void setupSinricPro() { // Funcao de setup do SinricPro
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