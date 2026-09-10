#include <TFT_eSPI.h>
#include <SD.h>
#include <SPI.h>
#include <EEPROM.h>

#include <HX711_ADC.h>
#include <ESP32Servo.h>

#include <HardwareSerial.h>

#if defined(ESP32)
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <time.h>
#endif

WiFiUDP udp;

File logFile;

const char* ssid = "ThrustStand";
const char* password = "risl123";

const int UDP_PORT = 4210;

// change this to your DISPLAY ESP32 IP after it connects
IPAddress displayIP(192,168,4,1);

// ================= PIN MAP =================
const int HX711_dout = 36;
const int HX711_sck  = 26;

const int currentPin = 34;
const int voltagePin = 35;
const int potPin     = 32;

const int switchPin  = 27;
const int chipSelect = 5; // change if your SD CS differs

const int escPin     = 25;

// UART

// ================= OBJECTS =================
HX711_ADC LoadCell(HX711_dout, HX711_sck);
Servo ESC;
File logfile;

// ================= STATE =================
bool runningTest = false;
bool previousSwitchState = false;
bool sdFailed = false;

int testLength = 5;
float testTimer = 0;
int testSpeed = 0;
int motorSpeed = 0;

float startMillis = 0;

unsigned long lastFlush = 0;

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid);
  Serial.print("Connecting WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());

  udp.begin(UDP_PORT);

  analogSetPinAttenuation(currentPin, ADC_11db);
  // UART link to other ESP32
  pinMode(switchPin, INPUT_PULLUP);

  // ADC setup (ESP32)
  analogReadResolution(12);

  // Load cell
  LoadCell.begin();
  LoadCell.start(2000);
  LoadCell.setCalFactor(-196.3626);

  if (getMotorPot() > 5) {
    exit(0);
  }

  // ESC PWM (ESP32-safe)
  ESC.attach(escPin, 1000, 2000);
  ESC.writeMicroseconds(1000);
  delay(5000);

  // SD
  if (!SD.begin(chipSelect)) {
    Serial.println("SD failed");
    sdFailed = true;
  }
  else {
    Serial.println("SD Success");
  }

  previousSwitchState = digitalRead(switchPin);

  Serial.println("ESP32 Ready");
}

// ================= HELPERS =================
float getMotorPot() {
  int raw = analogRead(potPin);
  return constrain(
        map(raw, 0, 4095, 0, 100),
        0,
        100
    ); // scaled 0–100
}

bool getSwitch() {
  return digitalRead(switchPin) == LOW;
}

float getLoad(){
    static boolean newDataReady = 0;
    if (LoadCell.update()) newDataReady = true;
    if (newDataReady) {
        float i = LoadCell.getData();
        return i; //In grams
      }
    return 0.0;
}

void tareLoadCell(){
  LoadCell.tare();
}

void setMotorSpeed(int speed) {
  if (speed == 0) {
    ESC.writeMicroseconds(1000);
    motorSpeed = 0;
    return;
  }

  while (motorSpeed != speed) {
    if (motorSpeed < speed) motorSpeed++;
    else motorSpeed--;

    ESC.writeMicroseconds(map(motorSpeed, 0, 100, 1000, 2000));
    delay(5);
  }
}

float readCurrent() {
  int raw = analogRead(currentPin);
  return ((raw + 142.903)/36.9797) + 0.73;
}

float readVoltage() {

  int raw = analogRead(voltagePin);

  return (raw / 95.2);

}

// ================= LOGGING =================

bool startLogging() {
  logFile = SD.open("/log"+String(millis())+".csv", FILE_APPEND);

  if (!logFile) {
    Serial.println("Failed to open /log"+String(millis())+".csv");
    return false;
  }

  // Add header if file is empty
  if (logFile.size() == 0) {
    logFile.println("time_ms,current,voltage,load,motorSpeed");
    logFile.flush();
  }

  Serial.println("Logging started");
  return true;
}

void logData(float time,
             float current,
             float voltage,
             float load,
             float motorSpeed) {

  if (!logFile) {
    return;
  }

  logFile.print(time);
  logFile.print(",");

  logFile.print(current, 2);
  logFile.print(",");

  logFile.print(voltage, 2);
  logFile.print(",");

  logFile.print(load, 2);
  logFile.print(",");

  logFile.println(motorSpeed, 0);
}

void updateLogging() {
  if (logFile && millis() - lastFlush > 1000) {
    logFile.flush();
    lastFlush = millis();
  }
}

void stopLogging() {
  if (logFile) {
    logFile.flush();
    logFile.close();
    Serial.println("Logging stopped");
  }
}

// ================= COMM =================
void communicateData(float current, float voltage, float load, float motorSpeed) {
  float sentCurrent = current;
  if (current < 4.73){
    sentCurrent = 0;
  }
  String msg =
      String(sentCurrent, 2) + "," +
      String(voltage, 2) + "," +
      String(load, 2) + "," +
      String(motorSpeed, 0);

  // BROADCAST to entire network
  IPAddress broadcastIP(255, 255, 255, 255);

  udp.beginPacket(broadcastIP, UDP_PORT);
  udp.print(msg);
  udp.endPacket();
}

void readUDP() {
  int packetSize = udp.parsePacket();
  if (!packetSize) return;

  char packet[255];
  int len = udp.read(packet, 255);
  if (len > 0) packet[len] = 0;

  String cmd = String(packet);
  Serial.println("UDP CMD: " + cmd);

  if (cmd == "TARE") {
    tareLoadCell();
  }

  if (cmd.startsWith("TESTTIME:")) {
    testLength = cmd.substring(9).toInt();
  }
}

// ================= TEST =================
void runTest(int speed) {
  setMotorSpeed(speed);

  float currentV = readCurrent();
  float voltageV = readVoltage();
  float load = getLoad();

  Serial.print("T:");
  Serial.print(millis() - startMillis);
  Serial.print(" S:");
  Serial.print(speed);
  Serial.print(" I:");
  Serial.print(currentV);
  Serial.print(" V:");
  Serial.print(voltageV);
  Serial.print(" L:");
  Serial.println(load);

  logData(millis() - startMillis, currentV, voltageV, load, speed);
}

// ================= LOOP =================
void loop() {
  readUDP();

  testTimer += 0.1;

  bool sw = getSwitch();

  if (sw && !previousSwitchState) {
    runningTest = !runningTest;
    testSpeed = getMotorPot();
    startMillis = millis();
    testTimer = 0;

    if (runningTest){
      if (!sdFailed) {
        startLogging();
      }
    }
    else{
      if (!sdFailed){
        stopLogging();
      }
    }
  }

  communicateData(
    readCurrent(),
    readVoltage(),
    getLoad(),
    getMotorPot()
  );

  if (runningTest && testTimer > testLength) {
    runningTest = false;
  }

  if (runningTest) {
    runTest(testSpeed);
  } else {
    setMotorSpeed(0);
  }

  previousSwitchState = sw;
  delay(100);
}