#include <M5Unified.h>

#include "wifi_setup.h"
#include <Preferences.h>

#include "avatar.h"

#define usesoftAP

#ifndef usesoftAP
  #define useOTA
#endif

#define DEBUG

/********************************
* for Wifi Connection
*********************************/
#include "WebPage.h"
#include "CalPage.h"
char OTAHostPrefix[]= "ESP32OTA-";
char WiFiAPPrefix[] = "ESP32-";

char WiFiAPname[256];
char OTAHostname[256];

const char passwd[] = "esp32spider";   // softAP needs 8 or more characters

WiFiServer server(80);
IPAddress ipadr;

String header;

/********************************
* for Servo control
*********************************/
// Hat 8Servos (https://docs.m5stack.com/en/hat/Hat_8Servos)
#define SERVO_ADDR      0x38
#define SERVO_ANGLE_REG 0x00    // 0x00-0x07 : CH1-CH8 angle (0-180)
#define SERVO_PULSE_REG 0x10    // 0x10-0x1F : CH1-CH8 pulse width (us, high byte first)
#define SERVO_RGB_REG   0x20    // 0x20-0x22 : G, R, B

// HAT port of M5StickC
#define HAT_SDA 0
#define HAT_SCL 26

// Published values for SG90 servos; 0-180 deg
const int minUs = 500;
const int maxUs = 2400;

// Servo assign to each channel (CH1-CH8 = 0-7)
// Hat 8Servos has only 8 channels, so TILT servo is not used.
#define FR0 0   // knees
#define FL0 2
#define RR0 4
#define RL0 6

#define FR1 1   // leg
#define FL1 3
#define RR1 5
#define RL1 7

// center position calibration(0-180 deg)
#define OFST 20

#define CENT_FR0 (86 + OFST)   // knees UP ++
#define CENT_FL0 (77 - OFST)   // knees UP --
#define CENT_RR0 (90 - OFST)   // knees UP --
#define CENT_RL0 (97 + OFST)   // knees UP ++

#define CENT_FR1 80   // leg forward ++
#define CENT_FL1 95   // leg forward --
#define CENT_RR1 95   // leg backword ++
#define CENT_RL1 82   // leg backword --

#define SERVO_NUM 8

// default center of each channel (CH1-CH8)
const int centDefault[SERVO_NUM] = {
  CENT_FR0, CENT_FR1, CENT_FL0, CENT_FL1,
  CENT_RR0, CENT_RR1, CENT_RL0, CENT_RL1
};
// +1: angle increases for knee up / leg forward, -1: decreases
const int servoDir[SERVO_NUM] = {
  1, 1, -1, -1,
  -1, -1, 1, 1
};
// current center (calibrated from Web, saved to NVS)
int cent[SERVO_NUM];
bool centDirty = false;

Preferences prefs;

const int stp = 3;
const int smooth = 5;

const int stepInterval = 50;
const int stepIntervalTurn = 100;

//String Face = "/GALLERY/KumaMCN.jpg";

Avatar *avatar;
uint32_t count = 0;
uint32_t lastAvatarMs = 0;

void servoDemo();
void servoHome();
void walkStop();
void walkForward(int steps);
void stepForward();
void walkBackward(int steps);
void stepBackward();
void walkRight(int steps);
void stepRight();
void walkLeft(int steps);
void stepLeft();
void turnRight(int steps);
void stepTurnRight();
void turnLeft(int steps);
void stepTurnLeft();
void servoKneesAll(int ang);
void servoLegFront(int ang);
void servoLegRear(int ang);
void servoStandUp(int ang);
void servoLayDown(int ang);
void servoLegOpen(int ang);
void servoLegClose(int ang);
void moveServo(int n, int deltaAngle);
void setServoPulse(uint8_t n, uint16_t pulse_us);
void setServoAngle(uint8_t n, int ang);
void setServoLed(uint8_t r, uint8_t g, uint8_t b);
void adjMode();
void loadCenter();
void saveCenter();
void handleCalApi(WiFiClient &client, const String &req);
String getParam(const String &req, const char *name);
void avatarUpdate();

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  Serial.begin(115200);
  M5.Display.setRotation(1);   // landscape 160x80

  Serial.println("M5StickC Q-Spider Webserver example");

/*
 * Servo setup
 */
  Wire.begin(HAT_SDA, HAT_SCL, 100000);
  Wire.beginTransmission(SERVO_ADDR);
  if(Wire.endTransmission() != 0){
    M5.Display.setTextColor(RED, BLACK);
    M5.Display.println("8Servos not found");
    Serial.println("Hat 8Servos not found");
  }
  setServoLed(0, 32, 0);
  loadCenter();
  delay(10);

  // Wifi and OTA setup
  uint64_t Chipid = GetChipid();
  sprintf(WiFiAPname, "%s%04X", WiFiAPPrefix, (uint16_t)Chipid);

  M5.Display.fillScreen(BLACK);
  M5.Display.setTextColor(WHITE, BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(0, 0);
  M5.Display.println((String)WiFiAPname);
  M5.Display.println("192.168.4.1");
  #ifdef usesoftAP
    M5.Display.setTextColor(CYAN, BLACK);
    M5.Display.println("softAP mode");
    WiFi.softAP(WiFiAPname, passwd);
    delay(100);
    IPAddress ip(192,168,4,1);
    IPAddress subnet(255,255,255,0);
    WiFi.softAPConfig(ip, ip, subnet);
    Serial.print("AP name: ");
    Serial.println(WiFiAPname);
  #else
    WiFiMgrSetup(WiFiAPname);
    ipadr = WiFi.localIP();
    M5.Display.setTextColor(GREEN, BLACK);
    M5.Display.println("connected(^^)");
    M5.Display.println((String)WiFi.SSID());
    M5.Display.printf("%3d.%3d.%3d.%3d", ipadr[0], ipadr[1], ipadr[2], ipadr[3]);
  #endif

  delay(1000);

  #ifdef useOTA
    sprintf(OTAHostname, "%s%04X", OTAHostPrefix, (uint16_t)Chipid);
    OTASetup(OTAHostname);
  #endif

  Serial.print("IP address: ");
  #ifdef usesoftAP
    Serial.println(ip);
  #else
    Serial.println(WiFi.localIP());
  #endif

  Serial.println("Ready");

  server.begin();

  avatar = new Avatar();
  M5.Display.fillScreen(SECONDARY_COLOR);
  avatar->init();

/*
 * Demo action
 */
//  servoDemo();
  servoHome();
}

void loop(){
  M5.update();

  uint32_t now = millis();
  if(now - lastAvatarMs >= 200){
    lastAvatarMs = now;
    avatarUpdate();
  }

  #ifdef DEBUG
    if(M5.BtnA.wasPressed()){
      i2c_scan();
    }
  #endif

  if(M5.BtnB.wasPressed()){
    adjMode();
  }

  #ifdef useOTA
    ArduinoOTA.handle();
  #endif

  WiFiClient client = server.available();   // Listen for incoming clients

  int lineNum;

  if (client) {                             // If a new client connects,
    header = "";                            // Clear header at start of each request
    #ifdef DEBUG
      Serial.println("New Client.");          // print a message out in the serial port
    #endif
    String currentLine = "";                // make a String to hold incoming data from the client
    const uint32_t CLIENT_TIMEOUT_MS = 2000;
    uint32_t clientConnectedAt = millis();
    while (client.connected()){             // loop while the client's connected
      if (millis() - clientConnectedAt > CLIENT_TIMEOUT_MS) {
        #ifdef DEBUG
          Serial.println("Client timeout.");
        #endif
        break;
      }
      if (client.available()){              // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        #ifdef DEBUG
          Serial.write(c);                    // print it out the serial monitor
        #endif
        header += c;
        if (c == '\n'){                     // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0){
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            // request line (e.g. "GET /api/cal?adj=0&d=1 HTTP/1.1")
            String req = header.substring(0, header.indexOf('\n'));
            if(req.startsWith("GET /api/cal")){
              handleCalApi(client, req);
              break;
            }
            if(req.startsWith("GET /cal")){
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html; charset=utf-8");
              client.println("Connection: close");
              client.println();
              client.print(CAL_Page);
              break;
            }

            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            // Display the web page of WebPage.h
            lineNum = 0;
            do{
              client.println(HTTP_Page[lineNum]);
              if(HTTP_Page[lineNum].indexOf("</html>")>= 0){
                break;
              }
              lineNum++;
            }while(true);

            // Get Motor value
            if(header.indexOf("GET /?MOTOR=STOP")>=0) {
              walkStop();
            }
            if(header.indexOf("GET /?MOTOR=FWD")>=0) {
              walkForward(5);
            }
            if(header.indexOf("GET /?MOTOR=BACK")>=0) {
              walkBackward(5);
            }
            if(header.indexOf("GET /?MOTOR=ACT1")>=0) {
              servoHome();
              delay(500);
              walkStop();
            }
            if(header.indexOf("GET /?MOTOR=ACT2")>=0) {
              servoDemo();
            }
            if(header.indexOf("GET /?MOTOR=LEFT")>=0) {
              walkLeft(5);
            }
            if(header.indexOf("GET /?MOTOR=RIGHT")>=0) {
              walkRight(5);
            }
            if(header.indexOf("GET /?MOTOR=SPIN_L")>=0) {
              turnLeft(6);
            }
            if(header.indexOf("GET /?MOTOR=SPIN_R")>=0) {
              turnRight(6);
            }

            client.println();
            // Break out of the while loop
            break;
          }else{
            currentLine = "";
          }
        }else if(c != '\r') {
          currentLine += c;
        }
      }
    }
    // Clear the header variable
    header = "";
    client.stop();
    #ifdef DEBUG
      Serial.println("Client disconnected.");
      Serial.println("");
    #endif
  }
}

// Servo Demo
void servoDemo(){
  servoHome();
  delay(500);

  servoLegOpen(45);
  delay(500);
  servoStandUp(70);
  delay(500);
  servoLegClose(45);
  delay(500);
  servoLayDown(70);
  delay(500);

  walkStop();
}

// Home position
void servoHome(){
  moveServo(FR1, 0);
  moveServo(FL1, 0);
  moveServo(RR1, 0);
  moveServo(RL1, 0);

  moveServo(FR0, 0);
  moveServo(FL0, 0);
  moveServo(RR0, 0);
  moveServo(RL0, 0);
}

// STOP position
void walkStop(){
  servoLegOpen(45);
  delay(500);
  servoStandUp(70);
}

// Walk Forward
void walkForward(int steps){
  servoLegFront(45);
  servoLegRear(45);
  servoKneesAll(70);
  delay(stepInterval);

  // initial position for walk Forward
  moveServo(RL0, -50);
  moveServo(RL1, 0);
  delay(stepInterval);
  moveServo(RL0, -70);

  for(int i = 0; i < steps; i++){
    stepForward();
  }

  servoLegFront(45);
  servoLegRear(45);
  servoKneesAll(70);
}

void stepForward(){
  moveServo(FL0, -50);
  moveServo(RR0, -50);
  delay(stepInterval);

  moveServo(FL1, 45);
  delay(stepInterval);

  moveServo(FR1, 0);
  moveServo(RL1, 45);
  delay(stepInterval);

  moveServo(RR1, 0);
  delay(stepInterval);

  moveServo(FL0, -70);
  moveServo(RR0, -70);
  delay(stepInterval);

  moveServo(FR0, -50);
  moveServo(RL0, -50);
  delay(stepInterval);

  moveServo(FR1, 45);
  delay(stepInterval);

  moveServo(FL1, 0);
  moveServo(RR1, 45);
  delay(stepInterval);

  moveServo(RL1, 0);
  delay(stepInterval);

  moveServo(FR0, -70);
  moveServo(RL0, -70);
  delay(stepInterval);
}

// Walk Backward
void walkBackward(int steps){
  servoLegFront(45);
  servoLegRear(45);
  servoKneesAll(70);
  delay(stepInterval);

  // initial position for walk Back
  moveServo(FR0, -50);
  moveServo(FR1, 0);
  delay(stepInterval);
  moveServo(FR0, -70);

  for(int i = 0; i < steps; i++){
    stepBackward();
  }

  servoLegFront(45);
  servoLegRear(45);
  servoKneesAll(70);
}

void stepBackward(){
  moveServo(FL0, -50);
  moveServo(RR0, -50);
  delay(stepInterval);

  moveServo(RR1, 45);
  delay(stepInterval);

  moveServo(FR1, 45);
  moveServo(RL1, 0);
  delay(stepInterval);

  moveServo(FL1, 0);
  delay(stepInterval);

  moveServo(FL0, -70);
  moveServo(RR0, -70);
  delay(stepInterval);

  moveServo(FR0, -50);
  moveServo(RL0, -50);
  delay(stepInterval);

  moveServo(RL1, 45);
  delay(stepInterval);

  moveServo(FL1, 45);
  moveServo(RR1, 0);
  delay(stepInterval);

  moveServo(FR1, 0);
  delay(stepInterval);

  moveServo(FR0, -70);
  moveServo(RL0, -70);
  delay(stepInterval);
}

// Walk Right
void walkRight(int steps){
  servoLegFront(45);
  servoLegRear(45);
  servoKneesAll(70);
  delay(stepInterval);

  // initial position for walk Right
  moveServo(RL0, -50);
  moveServo(RL1, 90);
  delay(stepInterval);
  moveServo(RL0, -70);

  for(int i = 0; i < steps; i++){
    stepRight();
  }

  servoLegFront(45);
  servoLegRear(45);
  servoKneesAll(70);
}

void stepRight(){
  moveServo(FL0, -50);
  moveServo(RR0, -50);
  delay(stepInterval);

  moveServo(RR1, 45);
  delay(stepInterval);

  moveServo(RL1, 45);
  moveServo(FR1, 90);
  delay(stepInterval);

  moveServo(FL1, 90);
  delay(stepInterval);

  moveServo(FL0, -70);
  moveServo(RR0, -70);
  delay(stepInterval);

  moveServo(FR0, -50);
  moveServo(RL0, -50);
  delay(stepInterval);

  moveServo(FR1, 45);
  delay(stepInterval);

  moveServo(FL1, 45);
  moveServo(RR1, 90);
  delay(stepInterval);

  moveServo(RL1, 90);
  delay(stepInterval);

  moveServo(FR0, -70);
  moveServo(RL0, -70);
  delay(stepInterval);
}

// Walk Left
void walkLeft(int steps){
  servoLegFront(45);
  servoLegRear(45);
  servoKneesAll(70);
  delay(stepInterval);

  // initial position for walk Left
  moveServo(FR0, -50);
  moveServo(FR1, 90);
  delay(stepInterval);
  moveServo(FR0, -70);

  for(int i = 0; i < steps; i++){
    stepLeft();
  }

  servoLegFront(45);
  servoLegRear(45);
  servoKneesAll(70);
}

void stepLeft(){
  moveServo(FL0, -50);
  moveServo(RR0, -50);
  delay(stepInterval);

  moveServo(FL1, 45);
  delay(stepInterval);

  moveServo(FR1, 45);
  moveServo(RL1, 90);
  delay(stepInterval);

  moveServo(RR1, 90);
  delay(stepInterval);

  moveServo(FL0, -70);
  moveServo(RR0, -70);
  delay(stepInterval);

  moveServo(FR0, -50);
  moveServo(RL0, -50);
  delay(stepInterval);

  moveServo(RL1, 45);
  delay(stepInterval);

  moveServo(FL1, 90);
  moveServo(RR1, 45);
  delay(stepInterval);

  moveServo(FR1, 90);
  delay(stepInterval);

  moveServo(FR0, -70);
  moveServo(RL0, -70);
  delay(stepInterval);
}

// Turn Right
void turnRight(int steps){
  servoLegFront(45);
  servoLegRear(45);
  servoKneesAll(70);
  delay(stepIntervalTurn);

  for(int i = 0; i < steps; i++){
    stepTurnRight();
  }

  servoLegFront(45);
  servoLegRear(45);
  servoKneesAll(70);
}

void stepTurnRight(){
  moveServo(FL0, -50);
  moveServo(RR0, -50);
  moveServo(FR1, 90);
  moveServo(RL1, 90);
  moveServo(FL1, 90);
  moveServo(RR1, 90);
  delay(stepIntervalTurn);

  moveServo(FL0, -70);
  moveServo(RR0, -70);
  delay(stepIntervalTurn);

  moveServo(FR0, -50);
  moveServo(RL0, -50);
  moveServo(FL1, 45);
  moveServo(RR1, 45);
  moveServo(FR1, 45);
  moveServo(RL1, 45);
  delay(stepIntervalTurn);

  moveServo(FR0, -70);
  moveServo(RL0, -70);
  delay(stepIntervalTurn);
}

// Turn Left
void turnLeft(int steps){
  servoLegFront(45);
  servoLegRear(45);
  servoKneesAll(70);
  delay(stepIntervalTurn);

  for(int i = 0; i < steps; i++){
    stepTurnLeft();
  }

  servoLegFront(45);
  servoLegRear(45);
  servoKneesAll(70);
}

void stepTurnLeft(){
  moveServo(FR0, -50);
  moveServo(RL0, -50);
  moveServo(FR1, 90);
  moveServo(RL1, 90);
  moveServo(FL1, 90);
  moveServo(RR1, 90);
  delay(stepIntervalTurn);

  moveServo(FR0, -70);
  moveServo(RL0, -70);
  delay(stepIntervalTurn);

  moveServo(FL0, -50);
  moveServo(RR0, -50);
  moveServo(FL1, 45);
  moveServo(RR1, 45);
  moveServo(FR1, 45);
  moveServo(RL1, 45);
  delay(stepIntervalTurn);

  moveServo(FL0, -70);
  moveServo(RR0, -70);
  delay(stepIntervalTurn);
}

// all knees angle
void servoKneesAll(int ang){
  int i = (-1 * ang);
  moveServo(FR0, i);
  moveServo(FL0, i);
  moveServo(RR0, i);
  moveServo(RL0, i);
}

// front leg angle
void servoLegFront(int ang){
  moveServo(FR1, ang);
  moveServo(FL1, ang);
}

// Rear leg angle
void servoLegRear(int ang){
  moveServo(RR1, ang);
  moveServo(RL1, ang);
}

// Stand Up
void servoStandUp(int ang){
  int i = 0;
  while(i < ang){
    servoKneesAll(i);
    i = i + stp;
    delay(smooth);
  }
}

// Lay Down
void servoLayDown(int ang){
  // lay down
  int i = ang;
  while(i > 0){
    servoKneesAll(i);
    i = i - stp;
    delay(smooth);
  }
}

// Leg Open
void servoLegOpen(int ang){
  int i = 0;
  while(i < ang){
    servoLegFront(i);
    servoLegRear(i);
    i = i + stp;
    delay(smooth);
  }
}

// Leg Close
void servoLegClose(int ang){
  int i = ang;
  // leg close
  while(i > 0){
    servoLegFront(i);
    servoLegRear(i);
    i = i - stp;
    delay(smooth);
  }
}

/************************************
 * Servo position by angle
 ************************************/
void moveServo(int n, int deltaAngle){
  if(n < 0 || n >= SERVO_NUM){
    return;
  }
  setServoAngle(n, cent[n] + servoDir[n] * deltaAngle);
}

/************************************
 * Center calibration (saved to NVS)
 ************************************/
void loadCenter(){
  uint8_t buf[SERVO_NUM];
  prefs.begin("spider", false);   // read-only open fails before first save
  size_t len = prefs.isKey("cent") ? prefs.getBytes("cent", buf, sizeof(buf)) : 0;
  prefs.end();
  for(int i = 0; i < SERVO_NUM; i++){
    cent[i] = (len == sizeof(buf)) ? buf[i] : centDefault[i];
  }
  centDirty = false;
}

void saveCenter(){
  uint8_t buf[SERVO_NUM];
  for(int i = 0; i < SERVO_NUM; i++){
    buf[i] = cent[i];
  }
  prefs.begin("spider", false);
  prefs.putBytes("cent", buf, sizeof(buf));
  prefs.end();
  centDirty = false;
}

String getParam(const String &req, const char *name){
  String key = String(name) + "=";
  int start = req.indexOf("?" + key);
  if(start < 0){
    start = req.indexOf("&" + key);
  }
  if(start < 0){
    return "";
  }
  start += key.length() + 1;
  int end = start;
  while(end < (int)req.length() && req[end] != '&' && req[end] != ' '){
    end++;
  }
  return req.substring(start, end);
}

// /api/cal?adj=<ch>&d=<+-deg> | home=1 | save=1 | load=1 | reset=1
// d is in physical direction (+ = knee up / leg forward)
void handleCalApi(WiFiClient &client, const String &req){
  String adj = getParam(req, "adj");
  if(adj.length() > 0){
    int ch = adj.toInt();
    int d = getParam(req, "d").toInt();
    if(ch >= 0 && ch < SERVO_NUM){
      cent[ch] = constrain(cent[ch] + servoDir[ch] * d, 0, 180);
      centDirty = true;
      moveServo(ch, 0);
    }
  }
  if(getParam(req, "save").length() > 0){
    saveCenter();
  }
  if(getParam(req, "load").length() > 0){
    loadCenter();
    servoHome();
  }
  if(getParam(req, "reset").length() > 0){
    for(int i = 0; i < SERVO_NUM; i++){
      cent[i] = centDefault[i];
    }
    centDirty = true;
    servoHome();
  }
  if(getParam(req, "home").length() > 0){
    servoHome();
  }

  String json = "{\"cent\":[";
  for(int i = 0; i < SERVO_NUM; i++){
    json += (i ? "," : "") + String(cent[i]);
  }
  json += "],\"def\":[";
  for(int i = 0; i < SERVO_NUM; i++){
    json += (i ? "," : "") + String(centDefault[i]);
  }
  json += "],\"dir\":[";
  for(int i = 0; i < SERVO_NUM; i++){
    json += (i ? "," : "") + String(servoDir[i]);
  }
  json += "],\"dirty\":";
  json += centDirty ? "true}" : "false}";

  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:application/json");
  client.println("Connection: close");
  client.println();
  client.print(json);
}

/*************************************************** 
 you can use this function if you'd like to set the pulse length in microsecond
***************************************************/
void setServoPulse(uint8_t n, uint16_t pulse_us){
  if(n > 7){
    return;
  }
  Wire.beginTransmission(SERVO_ADDR);
  Wire.write(SERVO_PULSE_REG + n * 2);
  Wire.write(pulse_us >> 8);
  Wire.write(pulse_us & 0xff);
  Wire.endTransmission();
}

/*************************************************** 
 you can use this function if you'd like to set the angle in degree
***************************************************/
void setServoAngle(uint8_t n, int ang){
  if (ang < 0){
    ang = 0;
  }
  if (ang > 180){
    ang = 180;
  }
  uint16_t pulse_us = map(ang, 0, 180, minUs, maxUs); //map angle(0～180) to pulse_us
  setServoPulse(n, pulse_us);
}

/***************************************************
 RGB LED on Hat 8Servos
***************************************************/
void setServoLed(uint8_t r, uint8_t g, uint8_t b){
  Wire.beginTransmission(SERVO_ADDR);
  Wire.write(SERVO_RGB_REG);
  Wire.write(g);
  Wire.write(r);
  Wire.write(b);
  Wire.endTransmission();
}

/*************************************************** 
 status of charging
 **************************************************/
void adjMode(){
    delay(300);
    M5.Display.fillScreen(BLACK);

    // Title
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.setCursor(2, 0);
    M5.Display.print("HOME POS.");

    // SSID / IP address
    M5.Display.setTextSize(1);
    M5.Display.setCursor(0, 30);
  #ifdef usesoftAP
    M5.Display.setTextColor(CYAN, BLACK);
    M5.Display.print("AP: ");
    M5.Display.println(WiFiAPname);
    M5.Display.setTextColor(GREEN, BLACK);
    M5.Display.setCursor(0, 50);
    M5.Display.print("IP: 192.168.4.1");
  #else
    M5.Display.setTextColor(CYAN, BLACK);
    M5.Display.print("SSID: ");
    M5.Display.println(WiFi.SSID());
    M5.Display.setTextColor(GREEN, BLACK);
    M5.Display.setCursor(0, 50);
    M5.Display.printf("IP: %d.%d.%d.%d",
      ipadr[0], ipadr[1], ipadr[2], ipadr[3]);
  #endif

    servoHome();

    while(1){
      M5.update();
      if(M5.BtnB.wasPressed()){
        M5.Display.fillScreen(SECONDARY_COLOR);
        avatar->init();
        count = 0;
        break;
      }
      delay(100);
    }
}

void avatarUpdate(){
  if(count == 0){
    //avatar->openMouth(50);
  }

  if (count % 100 == 97){
    avatar->openEye(false);
  }
  if (count % 100 == 0){
    avatar->openEye(true);
    count = random(100);
  }
  count++;
}
