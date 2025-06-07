#include <ESP32Servo.h>
#include <esp_now.h>
#include "WiFi.h"
#include <cmath>
#define CHANNEL 1
#define id 3

const int LDR_1 = 26, LDR_2 = 27;
bool speedBoost1 = false, slowdown1 = false, invertControl1 = false, speedBoost2 = false, slowdown2 = false, invertControl2 = false, isAffected = false, winnerVerifier = false;

uint8_t macSlaves[][6] = {
  { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }
};
int pincount;
const int AIN1 = 21, AIN2 = 22, PWM = 23;
int speed = 0, step = 0;
const int defaultSpeed = 190, maxSpeed = 255, lowerSpeed = 121;
float acceleration;
unsigned long timeCounter, lastStepInterval = 0, speedBoostCounter = 0, icCounter = 0, sdCounter = 0, servoCounter = 0, winnerCounter = 0;
bool re = false, speedBoostOn = false, slowdown = false, invertControl = false;
const int stepInterval = 1000 / 25;

const int verde1 = 13, verde2 = 12, vermelho1 = 14, vermelho2 = 15;
const int leds[4] = {verde1, verde2, vermelho1, vermelho2};

const int servoPin = 19;
Servo servo;
int posDeg = 90;

void setup() {
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWM, OUTPUT);
  digitalWrite(AIN1, 1);
  digitalWrite(AIN2, 0);
  ledcAttach(PWM, 5000, 8);
  for(int i=0; i<4; i++){
    pinMode(leds[i], OUTPUT);
  }

  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  Serial.print("Mac Address int station: ");
  Serial.print(WiFi.macAddress());
  initESPNow();
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
  int nslaves = sizeof(macSlaves) / 6 / sizeof(uint8_t);
  for (int i = 0; i < nslaves; i++) {
    esp_now_peer_info_t slave;
    slave.channel = CHANNEL;
    slave.encrypt = 0;
    memcpy(slave.peer_addr, macSlaves[i], sizeof(macSlaves[i]));
    esp_now_add_peer(&slave);
    esp_now_register_send_cb(OnDataSent);
    send();
  }

  randomSeed(analogRead(0));

  servo.attach(servoPin);
}

void initESPNow() {
  if (esp_now_init() == ESP_OK) {
    Serial.println("ESPNow init Sucess");
  } else {
    Serial.println("ESPNow init Failed");
    ESP.restart();
  }
}

void send() {
  uint8_t values[9];
  int dataEspRoad[9] = { id, speedBoost1, slowdown1, invertControl1, speedBoost2, slowdown2, invertControl2, isAffected, winnerVerifier };
  for (int i = 0; i < 9; i++) {
    values[i] = dataEspRoad[i];
  }
  esp_err_t result = esp_now_send(macSlaves[0], (uint8_t *)&values, sizeof(values));
  speedBoost1 = dataEspRoad[1];
  slowdown1 = dataEspRoad[2];
  invertControl1 = dataEspRoad[3];
  speedBoost2 = dataEspRoad[4];
  slowdown2 = dataEspRoad[5];
  invertControl2 = dataEspRoad[6];
  
  if(toDigital(LDR_1, 900) || toDigital(LDR_2, 900)){
    winnerVerifier = true;
    winnerCounter = millis();
    long rand = random(1,3);
    if(dataEspRoad[1] == 0 && dataEspRoad[2] == 0 && dataEspRoad[3] == 0) dataEspRoad[rand] = 1;
    else if(dataEspRoad[4] == 0 && dataEspRoad[5] == 0 && dataEspRoad[6] == 0) dataEspRoad[rand + 3] = 1;
  }
}

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  send();
}

void OnDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

  Serial.println(data[1]);
  if (data[0] == 1) {
    if (data[1] == 0) {
      if (!speedBoostOn && !slowdown) speed = Accelerate(step, 25, defaultSpeed);
      else if (speedBoostOn) speed = Accelerate(step, 25, maxSpeed);
      else speed = Accelerate(step, 25, lowerSpeed);
      digitalWrite(AIN1, 1);
      digitalWrite(AIN2, 0);
      if (step < 25 && millis() - lastStepInterval >= stepInterval) step++;
    } else if (data[2] == 0) {
      if (speed == 0) re = true;
      if (millis() - lastStepInterval >= stepInterval && step > 0) step--;
      if (speed == 0 || re) {
        digitalWrite(AIN1, 0);
        digitalWrite(AIN2, 1);
        if (step < 25 && millis() - lastStepInterval >= stepInterval) step++;
        speed = Accelerate(step, 25, 100);
      } else {
        Brake(100);
      }
    } else {
      Brake(500);
      re = false;
    }
    if (data[3] == 1 && data[4] == 1) {
      posDeg = 90;
    } else if (data[3] == 0) {
      if (posDeg > 45 && millis() - servoCounter >= stepInterval / 2) {
        if (!invertControl) posDeg--;
        else posDeg++;
        servoCounter = millis();
      }
    } else if (data[4] == 0) {
      if (posDeg < 135 && millis() - servoCounter >= stepInterval / 2) {
        if (!invertControl) posDeg++;
        else posDeg++;
        servoCounter = millis();
      }
    }
    if (data[5] == 1) {
      if (speedBoost1) {
        speedBoostOn = true;
        speedBoost1 = false;
        speedBoostCounter = millis();
      } 
      else if (slowdown1) slowdown1 = false;
      else if (invertControl1) invertControl1 = false;
      else if (speedBoost2) {
        speedBoostOn = true;
        speedBoost2 = false;
        speedBoostCounter = millis();
      } 
      else if (slowdown2) slowdown2 = false;
      else if (invertControl2) invertControl2 = false;
    }

    ledcWrite(PWM, speed);
    servo.write(posDeg);
  }
  else if(data[0] == 1){
     if (data[6] == 1){
      slowdown = true;
      sdCounter = millis();
    }
    if (data[7] == 1){
      invertControl = true;
      icCounter = millis();
    }
  }
}

void Brake(int time) {
  static unsigned long brakeStart;
  unsigned long timeCounter = millis() - brakeStart;
  acceleration = 255.0 / time;
  speed -= acceleration * timeCounter;
  if (speed < 0) speed = 0;
}

int Accelerate(int step, int totalSteps, int maxSpeed) {
  float normalizedStep = static_cast<float>(step) / static_cast<float>(totalSteps);
  float logValue = log1p(normalizedStep * (M_E - 1));
  int speed = static_cast<int>(logValue * maxSpeed);
  return speed;
}

bool toDigital(int pin, int reference){
  if (analogRead(pin)>reference) return 1;
  else return 0;
}

void loop() {
  if (millis() - speedBoostCounter >= 3000) speedBoostOn = false;
  if (millis() - sdCounter >= 3000) slowdown = false;
  if (millis() - icCounter >= 3000) invertControl = false;
  if(millis() - winnerCounter >= 500) winnerVerifier = false;
  if (slowdown || invertControl) isAffected = true;
  if(speedBoostOn){
    digitalWrite(verde1, 1);
    digitalWrite(verde2, 1);
  }
  else if(slowdown){
    digitalWrite(vermelho1, 1);
    digitalWrite(vermelho2, 1);
  }
  else if(invertControl){
    digitalWrite(verde1, 1);
    digitalWrite(verde2, 1);
    digitalWrite(vermelho1, 1);
    digitalWrite(vermelho2, 1);
  }
  else{
    digitalWrite(verde1, 0);
    digitalWrite(verde2, 0);
    digitalWrite(vermelho1, 0);
    digitalWrite(vermelho2, 0);
  }
}
