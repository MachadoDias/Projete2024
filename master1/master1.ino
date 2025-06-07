#include <esp_now.h>
#include "WiFi.h"
#include <LiquidCrystal_I2C.h>
#define CHANNEL 1
#define id 1

LiquidCrystal_I2C lcd(0x27, 16, 2); 
int show = 0;

struct Slave {
  int pins[5];
  uint8_t data[10];
};

const int ldr = 18;
const int accelerate = 13, brake = 23, turnRight = 14, turnLeft = 27, usePower = 26;

Slave slave = { { accelerate, brake, turnRight, turnLeft, usePower }, { id } };

uint8_t macSlaves[][6] = {
  { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }
};

bool hasSpeedBoost1 = false, hasSlowdown1 = true, hasIc1 = false, hasSpeedBoost2 = false, hasSlowdown2 = false, hasIc2 = false;
bool slowdownOpp = false, invertControlsOpp = false;
bool canMove = true;
static unsigned long lapStart, timep1, timep2;
int lapPlayer1 = 0, lapPlayer2 = 0;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  Serial.print("Mac Address int station: ");
  Serial.print(WiFi.macAddress());
  initESPNow();

  int nslaves = sizeof(macSlaves) / 6 / sizeof(uint8_t);
  for (int i = 0; i < nslaves; i++) {
    esp_now_peer_info_t slave = {};
    memcpy(slave.peer_addr, macSlaves[i], 6);
    slave.channel = CHANNEL;
    slave.encrypt = false;
    slave.ifidx = WIFI_IF_STA;
    if (esp_now_add_peer(&slave) != ESP_OK) {
      Serial.println("Failed to add peer");
    } 
    else {
      Serial.println("Peer added successfully");
    }
  }
  for (int i = 0; i < 5; i++) {
    pinMode(slave.pins[i], INPUT_PULLUP);
  }
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  esp_now_register_send_cb(OnDataSent);
  send();
  lcd.init();
  lcd.backlight();
}

void initESPNow() {
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow unit Sucess");
  } else {
    Serial.println("ESPNow unit Failed");
    ESP.restart();
  }
}

void send() {
  for (int i = 1; i < 6; i++) {
    slave.data[i] = digitalRead(slave.pins[i - 1]);
    if(!canMove) slave.data[i] = 1;
  }
  slave.data[7] = slowdownOpp;
  slave.data[8] = invertControlsOpp;
  slave.data[9] = canMove;
  esp_err_t result = esp_now_send(macSlaves[0], slave.data, sizeof(slave.data));
  Serial.println("Send Status: ");
  if (result == ESP_OK) {
    Serial.println("sucess");
  } else {
    Serial.println("Error");
  }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Sent to: ");
  Serial.println(macStr);
  Serial.print("Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
  send();
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  Serial.println(digitalRead(22));
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Received from: ");
  Serial.println(macStr);
  Serial.println("");
  if (data[0] == 3) {
    if (data[1] == 1) {
      hasSpeedBoost1 = true;
    } else if (data[2] == 1) {
      hasSlowdown1 = true;
    } else if (data[3] == 1) {
      hasIc1 = true;
    }
    if (data[4] == 1) {
      hasSpeedBoost2 = true;
    } else if (data[5] == 1) {
      hasSlowdown2 = true;
    } else if (data[6] == 1) {
      hasIc2 = true;
    }
    if (data[8] == 1 && analogRead(ldr) > 900) {
      lapPlayer1++;
      timep1 = millis() - lapStart;
    }
  }
  else if(data[0] == 2 && (show == 2)){
    show++;
    lcd.setCursor(0, 1);
    lcd.print("Pronto(a)");
  }
  else if(data[0] == 4){
    if(data[8] == 1 && analogRead(ldr) > 900) {
      lapPlayer2++;
      timep2 = millis() - lapStart;
    }
  }
}

void loop() {
  if (digitalRead(usePower) == 0 && hasSlowdown1) {
    slowdownOpp = true;
    hasSlowdown1 = false;
  } else if (digitalRead(usePower) == 0 && hasIc1) {
    invertControlsOpp = true;
    hasIc1 = false;
  } else if (digitalRead(usePower) == 0 && hasSlowdown2) {
    slowdownOpp = true;
    hasSlowdown2 = false;
  } else if (digitalRead(usePower) == 0 && hasIc2) {
    invertControlsOpp = true;
    hasIc2 = false;
  }

  if (show == 0) {
    lcd.setCursor(3, 0);
    lcd.print("Crazy Race");
    lcd.setCursor(4, 1);
    if (digitalRead(usePower) == 0) {
      show++;
      lcd.clear();
    }
  } else if (show == 1) {
    lcd.setCursor(0, 0);
    lcd.print("Iniciar");
    lcd.setCursor(0, 1);
    lcd.print("Iniciar");
    if (usePower == 0) {
      show++;
      lcd.setCursor(0, 0);
      lcd.print("Pronto(a)");
    }
  } else if (show == 3) {
    canMove = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("1");
    unsigned long counter = millis();
    while (millis() - counter <= 1500) {}
    show++;
  } else if (show == 4) {
    lcd.setCursor(7, 0);
    lcd.print("2");
    unsigned long counter = millis();
    while (millis() - counter <= 1500) {}
    show++;
  } else if (show == 5) {
    lcd.setCursor(14, 0);
    lcd.print("3");
    unsigned long counter = millis();
    while (millis() - counter <= 500) {}
    show++;
  } else if (show == 6) {
    lcd.setCursor(0, 1);
    lcd.print("Ja!");
    delay(100);
    lapStart = millis();
    lcd.clear();
    show++;
  } else if (show == 7) {
    lcd.setCursor(0, 0);
    lcd.print("Player 1 - ");
    if(lapPlayer1>=1) lcd.print(timep1/1000);
    lcd.setCursor(0, 1);
    lcd.print("Player 2 - ");
    if(lapPlayer2>=1) lcd.print(timep2/1000);
    if(lapPlayer1 == 2){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Fim de jogo");
      lcd.setCursor(0, 1);
      lcd.print("Player 1 - ");
      lcd.print(timep1);
      show++;
    }
    if(lapPlayer2 == 2){
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Fim de jogo");
      lcd.setCursor(0, 1);
      lcd.print("Player 2 - ");
      lcd.print(timep2);
      show++;
    }
  }
  else if(show == 8){
    show = 0;
  }
  Serial.println(digitalRead(13));
}