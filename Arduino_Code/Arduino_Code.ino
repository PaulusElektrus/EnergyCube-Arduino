// Relais Pins
const int Relais_AC       = 11;
const int Relais_AC_Boot  = 10; // Low = On
const int Relais_AC_to_NT = 9;
const int Relais_NT_to_BT = 8;   
const int Relais_BT_to_DC = 7;
const int Relais_DC_to_WR = 6;
const int Relais_WR_to_AC = 4;
const int Relais_BT       = 2; // Low = On

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
int commandFromESP  = 0;
int powerFromESP    = 0;

// UART Timing
unsigned long startMillis;
unsigned long currentMillis;
const unsigned long updateInterval = 1000;

// Measurements
int status          = 0;
float uNT           = 0.0;
float uBatt         = 0.0;
float uKal          = 0.0;
float iBatt         = 0.0;
int bsPower         = 0.0;
const float rFactor = 0.01085;

// Status
bool ntReady    = false;
bool ntSynced   = false;
bool dcReady    = false;
bool bsFull     = false;
bool bsEmpty    = false;

// Control
int pwmNT               = 255;
int pwmDC               = 255;
int controlTarget       = 0;
const int pwmDelay      = 100;
const int pwmDelaySync  = 20;

// Safety Parameters
const float maxUBatt            = 50;
const float minUBatt            = 44.4;
const float maxIBattCharging    = -5;
const float maxIBattDischarging = 2.5;
const int bsPowerCharging       = -150;
const int maxbsPowerCharging    = -300;
const int bsPowerDischarging    = 100;
const int maxbsPowerDischarging = 150;
const float deltaU              = 0.4;
const int deltaPMax             = 30;
const int deltaPMin             = 10;


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


void loop(){
    safetyCheck();
    getCommand();
    if (commandFromESP == 0) {
        status = 0;
        off();
    }
    else if (commandFromESP == 1 && bsFull == false) {
        if (powerFromESP <= 0 - deltaPMin || controlTarget <= 0 - deltaPMin) {
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
            status = 0;
            off();
        }
    }
    else if (commandFromESP == 2 && bsEmpty == false) {
        if (powerFromESP >= 0 + deltaPMin || controlTarget >= 0 + deltaPMin) {
            if (dcReady == false) {
                activateDC();
            }
            status = 2;
            discharge();
        }   
        else {
            status = 0;
            off();
        }
    }
}


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
    controlTarget = 0;
    analogWrite(PWM_NT, pwmNT);
    analogWrite(PWM_DC, pwmDC);
}


void activateNT() {
    off();
    digitalWrite(Relais_AC, HIGH);
    digitalWrite(Relais_AC_to_NT, HIGH);
    delay(100);
    ntReady = true;
}


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


void charge() {
    controlTarget = controlTarget + powerFromESP;
    if (controlTarget < bsPowerCharging) {controlTarget = bsPowerCharging;}
    digitalWrite(Relais_NT_to_BT, HIGH);
    while (controlTarget <= bsPower - deltaPMax) {
        pwmDecreaseNT();
        safetyCheck();
        debugPC();
        if (pwmNT == 0) return;
    }
    while (controlTarget >= bsPower + deltaPMin) {
        pwmIncreaseNT();
        safetyCheck();
        debugPC();
        if (pwmNT == 255) return;
    }
}


void activateDC() {
    off();
    digitalWrite(Relais_AC, HIGH);
    digitalWrite(Relais_BT_to_DC, HIGH);
    delay(4000);
    dcReady = true;
}


void discharge() {
    controlTarget = controlTarget + powerFromESP;
    if (controlTarget > bsPowerDischarging) {controlTarget = bsPowerDischarging;}
    digitalWrite(Relais_DC_to_WR, HIGH);
    while (controlTarget >= bsPower) {
        pwmDecreaseDC();
        safetyCheck();
        if (pwmDC == 0) return;
    }
    while (controlTarget <= bsPower) {
        pwmIncreaseDC();
        safetyCheck();
        if (pwmDC == 255) return;
    }
}


void pwmIncreaseNT() {
    if (pwmNT <= 254) {
        pwmNT = ++pwmNT;
        analogWrite(PWM_NT, pwmNT);
        delay(pwmDelay);
    }
}


void pwmDecreaseNT() {
    if (pwmNT >= 1) {
        pwmNT = --pwmNT;
        analogWrite(PWM_NT, pwmNT);
        delay(pwmDelay);
    }
}


void pwmDecreaseNTSync() {
    if (pwmNT >= 1) {
        pwmNT = --pwmNT;
        analogWrite(PWM_NT, pwmNT);
        delay(pwmDelaySync);
    }
}


void pwmIncreaseDC() {
    if (pwmDC <= 254) {
        pwmDC = ++pwmDC;
        analogWrite(PWM_NT, pwmDC);
        delay(pwmDelay);
    }
}


void pwmDecreaseDC() {
    if (pwmDC >= 1) {
        pwmDC = --pwmDC;
        analogWrite(PWM_NT, pwmDC);
        delay(pwmDelay);
    }
}


void getCommand() {
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
    else powerFromESP = 0;
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
    debugPC();
}


void measurement() {
    uNT = readChannel(ADS1115_COMP_0_GND);
    uBatt = readChannel(ADS1115_COMP_1_GND);
    uKal = readChannel(ADS1115_COMP_2_GND);
    iBatt = readChannel(ADS1115_COMP_3_GND);
    uNT = uNT * rFactor;
    uBatt = uBatt * rFactor;
    iBatt = -((iBatt - (uKal / 2)) / 185) + 0.2;
    bsPower = uBatt * iBatt;
}


float readChannel(ADS1115_MUX channel) {
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


void debugPC() {
    Serial.println("uNT: " + String(uNT) + ", uBatt: " + String(uBatt) + ", uKal: " + String(uKal) + ", iBatt: " + String(iBatt));
}
