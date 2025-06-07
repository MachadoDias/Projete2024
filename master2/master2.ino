#include <esp_now.h>
#include "WiFi.h"
#include <LiquidCrystal_I2C.h>
#define CHANNEL 1
#define id 2

struct Slave {
  int pins[5];
  uint8_t data[9];
};

const int ldr = 18;
const int accelerate = 22, brake = 23, turnRight = 14, turnLeft = 27, usePower = 26;
const int green11 = 13, red11 = 12, green12 = 14, red12 = 27, green21 = 26, red21 = 25, green22 = 23, red22 = 25, Red = 21, Yellow = 19, Green = 18, driver = 5;
const int leds[12] = {green11, red11, green12, red12, green21, red21, green22, red22, Red, Yellow, Green, driver};

Slave slave ={{accelerate, brake, turnRight, turnLeft, usePower}, {id}};

uint8_t macSlaves[][6] = {
 {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}
};

bool hasSpeedBoost1 = false, hasSlowdown1 = true, hasIc1 = false, hasSpeedBoost2 = false, hasSlowdown2 = false, hasIc2 = false;
bool slowdownOpp = false, invertControlsOpp = false;
bool canMove = true;
int lap = 0;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  Serial.print("Mac Address int station: ");
  Serial.print(WiFi.macAddress());
  initESPNow();

  int nslaves = sizeof(macSlaves)/6/sizeof(uint8_t);
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
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  esp_now_register_send_cb(OnDataSent);
  for(int i=0; i<5; i++){
    pinMode(slave.pins[i], INPUT_PULLUP);
  }
  for(int i=0; i<12; i++){
    pinMode(leds[i], OUTPUT);
  }
  send();
}

void initESPNow() {
  if(esp_now_init() == ESP_OK){
    Serial.println("ESPNow unit Sucess");
  } else {
    Serial.println("ESPNow unit Failed");
    ESP.restart();
  }
}

void send(){
  for(int i=1; i<6; i++){
    slave.data[i] = digitalRead(slave.pins[i-1]);
    if(!canMove) slave.data[i] = 1;
  }
  slave.data[7] = slowdownOpp;
  slave.data[8] = invertControlsOpp;
  esp_err_t result = esp_now_send(macSlaves[0], slave.data, sizeof(slave.data));
  Serial.println("Send Status: ");
  if (result == ESP_OK){
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
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Received from: "); 
  Serial.println(macStr);
  Serial.println("");
  if(data[0] == 4) {
    if(data[1] == 1){
      hasSpeedBoost1 = true;
    }
    else if(data[2] == 1){
      hasSlowdown1 = true;
    }
    else if(data[3] == 1){
      hasIc1 = true;
    }
    if(data[4] == 1){
      hasSpeedBoost2 = true;
  }
    else if(data[5] == 1){
      hasSlowdown2 = true;
    }
    else if(data[6] == 1){
      hasIc2 = true;
    }
    if(data[8] == 1 && analogRead(ldr)>900){
      lap++;
    }
  }
  else if(data[0] == 1) {
    if(data[9] == 0) canMove = false;
    else canMove = true;
  }
}

void loop (){
  if(hasIc1){
    digitalWrite(green21, 1);
    digitalWrite(red21, 1);
  }
  else{
    digitalWrite(green21, hasSpeedBoost1);
    digitalWrite(red21, hasSlowdown1);
  }
  if(hasIc2){
    digitalWrite(green22, 1);
    digitalWrite(red21, 1);
  }
  else {
    digitalWrite(green22, hasSpeedBoost2);
    digitalWrite(red22, hasSlowdown2);
  }
  if (digitalRead(usePower) == 0 && hasSlowdown1) {
    slowdownOpp = true;
    hasSlowdown1 = false;
  }
  else if (digitalRead(usePower) == 0 && hasIc1) {
    invertControlsOpp = true;
    hasIc1 = false;
  }
  else if (digitalRead(usePower) == 0 && hasSlowdown2) {
    slowdownOpp = true;
    hasSlowdown2 = false;
  }
  else if (digitalRead(usePower) == 0 && hasIc2) {
    invertControlsOpp = true;
    hasIc2 = false;
  }
  if(!canMove){
    digitalWrite(Red, 1);
    delay(1500);
    digitalWrite(Red, 0);
    digitalWrite(Yellow, 1);
    delay(1500);
    digitalWrite(Yellow, 0);
    digitalWrite(Green, 1);
    delay(500);
    digitalWrite(Green, 0);
    digitalWrite(driver, 1);
    canMove = true;
  }
}