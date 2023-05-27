// Relais Pins
const int Relais_AC       = 6;
const int Relais_AC_to_NT = 7;
const int Relais_NT_to_BT = 8;   
const int Relais_BT_to_DC = 9;
const int Relais_DC_to_WR = 10;
const int Relais_WR_to_AC = 11;

// PWM Pins
const int PWM_NT = 3;
const int PWM_DC = 5;

// ADS1115
#include<ADS1115_WE.h> 
#include<Wire.h>
ADS1115_WE adc = ADS1115_WE(0x48);

// Incoming Communication
boolean newData = false;
const byte numChars = 32;
char receivedChars[numChars];
char tempChars[numChars];

// Incoming Data
int commandFromESP = 0;
int powerFromESP = 0;

// UART Timing
unsigned long startMillis;
unsigned long currentMillis;
const unsigned long updateInterval = 1000;

// Measurements
int status = 0;
float uNT = 0.0;
float uBatt = 0.0;
float uKal = 0.0;
float iBatt = 0.0;
int bsPower = 0.0;
const float rFactor = 0.0111;

// Status
bool ntReady = false;
bool ntSynced = false;
bool dcReady = false;
bool bsFull = false;
bool bsEmpty = false;

// Control
int pwm = 255;
int controlTarget = 0;
int pwmDelay = 50;

// Safety Parameters
const float maxUBatt = 49.2;
const float minUBatt = 44.4;
const float maxIBattCharging = -3.0;
const float maxbsPowerCharging = -150;
const int maxbsPowerDischarging = 100;


void setup(){
    pinMode(Relais_AC      , OUTPUT);
    pinMode(Relais_AC_to_NT, OUTPUT);
    pinMode(Relais_NT_to_BT, OUTPUT);   
    pinMode(Relais_BT_to_DC, OUTPUT);
    pinMode(Relais_DC_to_WR, OUTPUT);
    pinMode(Relais_WR_to_AC, OUTPUT);
    Wire.begin();
    adc.init();
    adc.setVoltageRange_mV(ADS1115_RANGE_6144);
    Serial.begin(115200);
}


void loop(){
    getCommand();
    safetyCheck();
    if (commandFromESP == 0) {
        off();
    }
    else if (commandFromESP == 1 && bsFull == false) {
        if (powerFromESP < 0 || bsPower < 0) {
            if (ntReady == false) {
                activateNT();
            }
            if (ntSynced == false) { 
                syncNT();
            }
            status = 1;
            charge();
        }   
        else {
            off();
        }
    }
    else if (commandFromESP == 2 && bsEmpty == false) {
        if (powerFromESP > 0 || bsPower > 0) {
            if (dcReady == false) {
                activateDC();
            }
            status = 2;
            discharge();
        }   
        else {
            off();
        }
    }
}


void safetyCheck() {
    measurement();
    if (uBatt > maxUBatt) {
        bsFull = true;
        off();
    }
    if (uBatt < minUBatt) {
        bsEmpty = true;
        off();
    }
    if (iBatt < maxIBattCharging) {
        off();
        status = 3;
    }
    if (bsPower > maxbsPowerDischarging) {
        off();
        status = 3;
    }
}


void off() {
    digitalWrite(Relais_AC, LOW);
    digitalWrite(Relais_AC_to_NT, LOW);
    digitalWrite(Relais_NT_to_BT, LOW);
    digitalWrite(Relais_BT_to_DC, LOW);
    digitalWrite(Relais_DC_to_WR, LOW);
    digitalWrite(Relais_WR_to_AC, LOW);
    status = 0;
    ntReady = false;
    ntSynced = false;
    dcReady = false;
    pwm = 255;
    int controlTarget = 0;
    analogWrite(PWM_NT, pwm);
    analogWrite(PWM_DC, pwm);
}


void activateNT() {
    off();
    digitalWrite(Relais_AC, HIGH);
    digitalWrite(Relais_AC_to_NT, HIGH);
    ntReady = true;
}


void syncNT() {
    while (uNT < uBatt) {
        pwmDecreaseNT();
    }
    ntSynced = true;
}


void charge() {
    digitalWrite(Relais_NT_to_BT, HIGH);
    int target = controlTarget + powerFromESP;
    if (target < maxbsPowerCharging) {target = maxbsPowerCharging;}
    while (target <= bsPower && iBatt >= maxIBattCharging) {
        pwmDecreaseNT();
    }
    while (target >= bsPower) {
        pwmIncreaseNT();
    }
}


void activateDC() {
    off();
    digitalWrite(Relais_AC, HIGH);
    digitalWrite(Relais_BT_to_DC, HIGH);
    delay(5000);
    dcReady = true;
}


void discharge() {
    digitalWrite(Relais_DC_to_WR, HIGH);
    int target = controlTarget + powerFromESP;
    if (target > maxbsPowerDischarging) {target = maxbsPowerDischarging;}
    while (target >= bsPower && bsPower <= maxbsPowerDischarging) {
        pwmDecreaseDC();
    }
    while (target <= bsPower) {
        pwmIncreaseDC();
    }
}


void pwmIncreaseNT() {
    if (pwm <= 254) {
        pwm = ++pwm;
        analogWrite(PWM_NT, pwm);
        delay(pwmDelay);
        measurement();
    }
}


void pwmDecreaseNT() {
    if (pwm >= 1) {
        pwm = --pwm;
        analogWrite(PWM_NT, pwm);
        delay(pwmDelay);
        measurement();
    }
}


void pwmIncreaseDC() {
    if (pwm <= 254) {
        pwm = ++pwm;
        analogWrite(PWM_NT, pwm);
        delay(pwmDelay);
        measurement();
    }
}


void pwmDecreaseDC() {
    if (pwm >= 1) {
        pwm = --pwm;
        analogWrite(PWM_NT, pwm);
        delay(pwmDelay);
        measurement();
    }
}


void getCommand() {
    currentMillis = millis();
    if (currentMillis - startMillis >= updateInterval)
    {
    measurement();
    returnData();
    delay(100);
    recvWithStartEndMarkers();
    if (newData == true) {
        strcpy(tempChars, receivedChars);
        parseData();
        newData = false;
    }
    startMillis = currentMillis;
    }
}


void recvWithStartEndMarkers() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;

    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();
        if (recvInProgress == true) {
            if (rc != endMarker) {
                receivedChars[ndx] = rc;
                ndx++;
                if (ndx >= numChars) {
                    ndx = numChars - 1;
                }
            }
            else {
                receivedChars[ndx] = '\0';
                recvInProgress = false;
                ndx = 0;
                newData = true;
            }
        }
        else if (rc == startMarker) {
            recvInProgress = true;
        }
    }
}


void parseData() {

    char * strtokIndx;

    strtokIndx = strtok(tempChars, ",");
    commandFromESP = atoi(strtokIndx);     

    strtokIndx = strtok(NULL, ",");
    powerFromESP = atof(strtokIndx);
}


void returnData() {
    String outgoingData = "<" + String(status) + "," + uBatt + "," + iBatt + ">";
    Serial.print(outgoingData);
}


void measurement() {
    uNT = readChannel(ADS1115_COMP_0_GND);
    uBatt = readChannel(ADS1115_COMP_1_GND);
    uKal = readChannel(ADS1115_COMP_2_GND);
    iBatt = readChannel(ADS1115_COMP_3_GND);
    uNT = uNT * rFactor;
    uBatt = uBatt * rFactor;
    iBatt = (iBatt - (uKal / 2)) / 100;
    bsPower = uBatt * iBatt;
}


float readChannel(ADS1115_MUX channel) {
  float voltage = 0.0;
  adc.setCompareChannels(channel);
  adc.startSingleMeasurement();
  while(adc.isBusy()){}
  voltage = adc.getResult_mV();
  return voltage;
}
