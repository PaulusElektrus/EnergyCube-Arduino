// Relais Pins //
const int Relais_AC       = 11;
const int Relais_AC_Boot  = 10; // Low = On
const int Relais_AC_to_NT = 9;
const int Relais_NT_to_BT = 8;   
const int Relais_BT_to_DC = 7;
const int Relais_DC_to_WR = 6;
const int Relais_WR_to_AC = 4;
const int Relais_BT       = 2; // Low = On

// PWM Pins //
const int PWM_NT = 3;
const int PWM_DC = 5;

// ADS1115 //
#include<ADS1115_WE.h> 
#include<Wire.h>
ADS1115_WE adc = ADS1115_WE(0x48);

// Incoming Communication //
boolean newData = false;
const byte numChars = 32;
char receivedChars[numChars];
char tempChars[numChars];

// Incoming Data //
int commandFromESP  = 0;
int powerFromESP    = 0;

// UART Timing //
unsigned long startMillis;
unsigned long currentMillis;
const unsigned long updateInterval = 500;

// Measurements //
int status          = 0;
float uNT           = 0.0;
float uBatt         = 0.0;
float uWR           = 0.0;
float iBatt         = 0.0;
int bsPower         = 0.0;
const float rFactor = 0.01085;
const float iFactor = 0.13333333;

// Status //
bool ntReady        = false;
bool ntSynced       = false;
bool dcReady        = false;
bool bsFull         = false;
bool bsEmpty        = false;

// Control //
int pwmNT               = 255;
const int pwmNTStep     = 1;
int pwmDC               = 255;
const int pwmDCStep     = 3;
int controlTarget       = 0;
const int pwmDelayNT      = 100;
const int pwmDelayNTSync  = 1;
const int pwmDelayDC      = 1;

// Safety Parameters //
const float maxUBatt            = 50;
const float minUBatt            = 44.4;
const float maxIBattCharging    = -10;
const float maxIBattDischarging = 7.5;
const int bsPowerCharging       = -600;
const int maxbsPowerCharging    = -650;
const int bsPowerDischarging    = 300;
const int maxbsPowerDischarging = 320;
const float deltaU              = 0.2;
const int deltaPMax             = 15;
const int deltaPMin             = 10;


// Setup runs once at boot //
void setup(){
    pinMode(Relais_AC      , OUTPUT);
    pinMode(Relais_AC_Boot , OUTPUT);
    pinMode(Relais_AC_to_NT, OUTPUT);
    pinMode(Relais_NT_to_BT, OUTPUT);   
    pinMode(Relais_BT_to_DC, OUTPUT);
    pinMode(Relais_DC_to_WR, OUTPUT);
    pinMode(Relais_WR_to_AC, OUTPUT);
    pinMode(Relais_BT      , OUTPUT);
    Wire.begin();
    delay(5000);
    Serial.begin(115200);
}
///////////////////////////////////////////////////


// Main Loop //
void loop(){
    safetyCheck();
    getCommand();
    control();
}
///////////////////////////////////////////////////


// Checks safety parameters //
void safetyCheck() {
    measurement();
    if (uBatt > maxUBatt + deltaU) {
        bsFull = true;
        off();
        status = 3;
        }
    else if (uBatt < maxUBatt + deltaU && uBatt > minUBatt - deltaU) {
        bsFull = false;
        bsEmpty = false;
    }
    else if (uBatt < minUBatt - deltaU) {
        bsEmpty = true;
        off();
        status = 4;
    }
    if (iBatt < maxIBattCharging) {
        off();
        status = 5;
    }
    else if (iBatt > maxIBattDischarging) {
        off();
        status = 6;
    }
    if (bsPower < maxbsPowerCharging) {
        off();
        status = 7;
    }
    else if (bsPower > maxbsPowerDischarging) {
        off();
        status = 8;
    }
    if (status >= 5) {
        while(true) {
            off();
            returnData();
            delay(5000);
            measurement(); 
        }
    }
}
///////////////////////////////////////////////////


// Function to receive and send data to ESP //
void getCommand() {
    powerFromESP = 0;
    currentMillis = millis();
    if (currentMillis - startMillis >= updateInterval)
    {
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


// Sending data to ESP
void returnData() {
    String outgoingData = "<" + String(status) + "," + uBatt + "," + iBatt + ">";
    Serial.print(outgoingData);
}


// Receiving data from ESP
void recvWithStartEndMarkers() {
// Code from: https://forum.arduino.cc/t/serial-input-basics-updated/382007/3
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


// Parsing Data received from ESP
void parseData() {

    char * strtokIndx;

    strtokIndx = strtok(tempChars, ",");
    commandFromESP = atoi(strtokIndx);     

    strtokIndx = strtok(NULL, ",");
    powerFromESP = atof(strtokIndx);
}
///////////////////////////////////////////////////


// Main control function, determines if charging or discharging //
void control() {
    controlTarget = controlTarget + powerFromESP;
    if (commandFromESP == 0) {
            status = 0;
            controlTarget = 0;
            off();
        }
    else if (commandFromESP == 1) {
        if (controlTarget <= 0 - deltaPMin) {
            if (bsFull == false) {
                if (ntReady == false) {
                    activateNT();
                }
                if (ntSynced == false) { 
                    syncNT();
                }
                if (controlTarget < bsPowerCharging) {controlTarget = bsPowerCharging;}
                status = 1;
                charge();
            }
        }
        else if (controlTarget >= 0 + deltaPMin) {
            if (bsEmpty == false) {
                if (dcReady == false) {
                    activateDC();
                }
                if (controlTarget > bsPowerDischarging) {controlTarget = bsPowerDischarging;}
                status = 2;
                discharge();
            }
        }
        else {
            status = 0;
            off();
        }
    }
}
///////////////////////////////////////////////////


// Will be called when battery storage should turn off //
void off() {
    digitalWrite(Relais_AC, LOW);
    digitalWrite(Relais_AC_to_NT, LOW);
    digitalWrite(Relais_NT_to_BT, LOW);
    digitalWrite(Relais_BT_to_DC, LOW);
    digitalWrite(Relais_DC_to_WR, LOW);
    digitalWrite(Relais_WR_to_AC, LOW);
    ntReady = false;
    ntSynced = false;
    dcReady = false;
    pwmNT = 255;
    pwmDC = 255;
    analogWrite(PWM_NT, pwmNT);
    analogWrite(PWM_DC, pwmDC);
}
///////////////////////////////////////////////////


// Activates the Charger //
void activateNT() {
    off();
    digitalWrite(Relais_AC, HIGH);
    digitalWrite(Relais_AC_to_NT, HIGH);
    ntReady = true;
}


// Synces the charger with battery voltage
void syncNT() {
    while (uNT <= uBatt - deltaU) {
        safetyCheck();
        pwmDecreaseNTSync();
        debugPC();
        if (pwmNT == 0) {status = 9;}
    }
    ntSynced = true;
    safetyCheck();
}


// Control function for charging power
void charge() {
    digitalWrite(Relais_NT_to_BT, HIGH);
    while (controlTarget <= bsPower - deltaPMin) {
        pwmDecreaseNT();
        safetyCheck();
        debugPC();
        if (pwmNT == 0) return;
    }
    while (controlTarget > bsPower + deltaPMin) {
        pwmIncreaseNT();
        safetyCheck();
        debugPC();
        if (pwmNT == 255) return;
    }
}
///////////////////////////////////////////////////


// Activates Discharging //
void activateDC() {
    off();
    digitalWrite(Relais_AC, HIGH);
    digitalWrite(Relais_WR_to_AC, HIGH);
    digitalWrite(Relais_BT_to_DC, HIGH);
    digitalWrite(Relais_DC_to_WR, HIGH);
    delay(2000);
    dcReady = true;
}


// Control function for discharging power
void discharge() {
    while (controlTarget >= bsPower + deltaPMax) {
        pwmDecreaseDC();
        safetyCheck();
        debugPC();
        if (pwmDC == 0) return;
    }
    while (controlTarget < bsPower - deltaPMin) {
        pwmIncreaseDC();
        safetyCheck();
        debugPC();
        if (pwmDC == 255) return;
    }
}
///////////////////////////////////////////////////


// PWM helper functions //
void pwmIncreaseNT() {
    if (pwmNT <= 255 - pwmNTStep) {
        pwmNT = pwmNT + pwmNTStep;
        analogWrite(PWM_NT, pwmNT);
        delay(pwmDelayNT);
    }
}


void pwmDecreaseNT() {
    if (pwmNT >= 0 + pwmNTStep) {
        pwmNT = pwmNT - pwmNTStep;
        analogWrite(PWM_NT, pwmNT);
        delay(pwmDelayNT);
    }
}


void pwmDecreaseNTSync() {
    if (pwmNT >= 10) {
        pwmNT = pwmNT - 10;
        analogWrite(PWM_NT, pwmNT);
        delay(pwmDelayNTSync);
    }
}


void pwmIncreaseDC() {
    if (pwmDC <= 255 - pwmDCStep) {
        pwmDC = pwmDC + pwmDCStep;
        analogWrite(PWM_DC, pwmDC);
        delay(pwmDelayDC);
    }
}


void pwmDecreaseDC() {
    if (pwmDC >= 0 + pwmDCStep) {
        pwmDC = pwmDC - pwmDCStep;
        analogWrite(PWM_DC, pwmDC);
        delay(pwmDelayDC);
    }
}
///////////////////////////////////////////////////


// Measurement Section //
void measurement() {
    uBatt = readVoltage(ADS1115_COMP_0_GND);
    uNT = readVoltage(ADS1115_COMP_1_GND);
    uWR = readVoltage(ADS1115_COMP_2_GND);
    iBatt = readCurrent(ADS1115_COMP_3_GND);
    uNT = uNT * rFactor;
    uWR = uWR * rFactor;
    uBatt = uBatt * rFactor;
    iBatt = -iBatt * iFactor;
    bsPower = uBatt * iBatt;
}


float readVoltage(ADS1115_MUX channel) {
    if (adc.init() == true){
        adc.setVoltageRange_mV(ADS1115_RANGE_6144);
        float voltage = 0.0;
        adc.setCompareChannels(channel);
        adc.startSingleMeasurement();
        while (adc.isBusy()) {}
        voltage = adc.getResult_mV();
        return voltage;
    }
    else {
        off();
        return 0.0;
        status = 7;
    }
}


float readCurrent(ADS1115_MUX channel) {
    if (adc.init() == true){
        adc.setVoltageRange_mV(ADS1115_RANGE_0256);
        float voltage = 0.0;
        adc.setCompareChannels(channel);
        adc.startSingleMeasurement();
        while (adc.isBusy()) {}
        voltage = adc.getResult_mV();
        return voltage;
    }
    else {
        off();
        return 0.0;
        status = 7;
    }
}
///////////////////////////////////////////////////


// Function can be called when a debug information to PC is needed //
void debugPC() {
    Serial.println("uNT: " + String(uNT) + ", uBatt: " + String(uBatt) + ", uWR>: " + String(uWR) + ", iBatt: " + String(iBatt) + ", controlTarget: " + String(controlTarget));
}
///////////////////////////////////////////////////
