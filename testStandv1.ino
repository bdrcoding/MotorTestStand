#include <XPT2046_Touchscreen.h>

#include <TFT_eSPI.h>

#include <SD.h>

#include <Servo.h>
#include <HX711_ADC.h>
#if defined(ESP8266)|| defined(ESP32) || defined(AVR)
#include <EEPROM.h>
#include <SPI.h>
#include <SD.h>
#endif

const int HX711_dout = 4; //mcu > HX711 dout pin
const int HX711_sck = 5; //mcu > HX711 sck pin

const int currentPin = A0;
const int voltagePin = A1;
const int potPin = A2;

const int switchPin = 22;

const int chipSelect = 53;

File logfile;

bool runningTest = false;
bool previousSwitchState = false;
bool sdFailed = false;

int testLength = 5;
float testTimer = 0;
int testSpeed = 0;

int motorSpeed = 0;

float startMillis = millis();

HX711_ADC LoadCell(HX711_dout, HX711_sck);

Servo ESC;

void setup() {
    Serial.begin(9600); 
    Serial1.begin(115200);
    pinMode(switchPin, INPUT_PULLUP);

    LoadCell.begin();

    LoadCell.start(2000);

    LoadCell.setCalFactor(-196.3626);

    ESC.attach(9, 1000, 2000);

    ESC.writeMicroseconds(1000);   // minimum throttle
    delay(5000);    // allow ESC to boot/arm

    if (!SD.begin(chipSelect)){
      Serial.println("SD failed");
      sdFailed = true;
    }
    previousSwitchState = getSwitch(switchPin);
    Serial.println("Arduino Ready");
}

float getLoad(){
    static boolean newDataReady = 0;
    if (LoadCell.update()) newDataReady = true;
    if (newDataReady) {
        float i = LoadCell.getData();
        return i;
      }
    return 0.0;
}

void logData(File file, float time, float current, float voltage, float load) {
    if (file){
      file.print(time);
      file.print(",");

      file.print(voltage);
      file.print(",");

      file.print(current);
      file.print(",");

      file.flush();
    }
}

void serializeData(float time, float current, float voltage, float load) {
    Serial.print("Time: ");
    Serial.println(time);
    Serial.print("Current: ");
    Serial.println(current);
    Serial.print("Voltage: ");
    Serial.println(voltage);
    Serial.print("Load: ");
    Serial.println(load);
}

void communicateData(float current, float voltage, float load, float motorSpeed) {
  Serial1.print(current);
  Serial1.print(",");
  Serial1.print(voltage);
  Serial1.print(",");
  Serial1.print(load);
  Serial1.print(",");
  Serial1.println(motorSpeed);
}

bool getButton(int switchPin){
    int buttonState = digitalRead(switchPin);

    if (buttonState == LOW){
      return true;
    }
    return false;
}

bool getSwitch(int switchPin){
    int switchState = digitalRead(switchPin);

    if (switchState == LOW){
      return true;
    }
    return false;
}

void setMotorSpeed(int speed){
    bool motorAtSpeed = false;
    // Serial.print("Speed: ");
    // Serial.println(speed);
    if (speed == 0){
      ESC.writeMicroseconds(1000);
      return;
    }
    while (!motorAtSpeed){
      if (motorSpeed < speed){
        motorSpeed += 1;
      }
      else if (motorSpeed > speed){
        motorSpeed = speed;
      }
      else{
        motorAtSpeed = true;
      }
      ESC.writeMicroseconds((motorSpeed*10)+1000);
      delay(5);
    }
}

float getMotorPot(){
    int raw = analogRead(potPin);
    return (raw*0.097751711);
}

float calculateCurrent(float signal){
    return (signal*.11) + 2.355;
}

void tareLoadCell(){
  LoadCell.tare();
}

void test(int motorSpeed){
    bool pressed = true;
    setMotorSpeed(motorSpeed);
    // if (getButton(switchPin)){
    //   if (!pressed){
    //     setMotorSpeed(0);
    //     return;
    //   }
    //   pressed = true;
    // }
    // else{
    //   pressed = false;
    // }
    int rawCurrent = analogRead(currentPin);
    int rawVoltage = analogRead(voltagePin);
    //Serial.println(rawCurrent);
    float currentV = (rawCurrent*.11) + 2.355;// * (5.0 / 1023.0);
    float voltageV = rawVoltage;// * (5.0 / 1023.0);

    float load = LoadCell.getData();

    //Serial.print("Current signal: ");
    //Serial.println(currentV);
    //Serial.println(getMotorPot());

    // logData(logFile, millis()-startMillis, currentV, voltageV, load);
    serializeData(millis()-startMillis, currentV, voltageV, load);
    //Serial.print(" V    Voltage signal: ");
    //Serial.println(voltageV);    

}

void loop() {
    testTimer += 0.1;
    if (getSwitch(switchPin) != previousSwitchState){
      runningTest = !runningTest;
      testSpeed = getMotorPot();
      if (!sdFailed){
        logfile = SD.open("log.csv", FILE_WRITE);
      }
      startMillis = millis();
      if (runningTest){
        testTimer = 0;
      }
    }

    communicateData(((analogRead(currentPin)*.11) + 2.355), analogRead(voltagePin), LoadCell.getData(), getMotorPot());

    LoadCell.update();

    //Serial.println(getSwitch(switchPin));
    if (runningTest && testTimer > testLength){
      runningTest = false;
    }
    
    if (runningTest){
      test(testSpeed);
    }
    else{
      setMotorSpeed(0);
    }
    previousSwitchState = getSwitch(switchPin);
    delay(100);
}
