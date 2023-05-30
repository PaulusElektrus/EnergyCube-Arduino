// Relais Pins
const int Relais_AC       = 2;
const int Relais_AC_Boot  = 4; // Low = On
const int Relais_AC_to_NT = 6;
const int Relais_NT_to_BT = 7;   
const int Relais_BT_to_DC = 8;
const int Relais_DC_to_WR = 9;
const int Relais_WR_to_AC = 10;
const int Relais_BT       = 11; // Low = On

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
const float rFactor = 0.0111;

// Status
bool ntReady    = false;
bool ntSynced   = false;
bool dcReady    = false;
bool bsFull     = false;
bool bsEmpty    = false;

// Control
int pwm             = 255;
int controlTarget   = 0;
int pwmDelay        = 100;

// Safety Parameters
const float maxUBatt            = 50;
const float minUBatt            = 44.4;
const float maxIBattCharging    = -2.9;
const float maxIBattDischarging = 2.2;
const int maxbsPowerCharging    = -150;
const int maxbsPowerDischarging = 100;
const float deltaU              = 0.4;
const float deltaI              = 0.2;
const float deltaP              = 10;


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
        if (powerFromESP < 0 - deltaP || bsPower < 0 - deltaP) {
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
        if (powerFromESP > 0 + deltaP || bsPower > 0 + deltaP) {
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
    if (iBatt < maxIBattCharging - deltaI) {
        off();
        status = 5;
    }
    else if (iBatt > maxIBattDischarging + deltaI) {
        off();
        status = 5;
    }
    if (bsPower < maxbsPowerCharging - deltaP) {
        off();
        status = 5;
    }
    else if (bsPower > maxbsPowerDischarging + deltaP) {
        off();
        status = 5;
    }
    if (status >= 5) {
        while(true) {
            off();
            measurement();
            returnData();
            delay(5000); 
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
    pwm = 255;
    controlTarget = 0;
    analogWrite(PWM_NT, pwm);
    analogWrite(PWM_DC, pwm);
}


void activateNT() {
    off();
    digitalWrite(Relais_AC, HIGH);
    digitalWrite(Relais_AC_to_NT, HIGH);
    delay(100);
    ntReady = true;
}


void syncNT() {
    while (uNT <= uBatt) {
        safetyCheck();
        pwmDecreaseNT();
    }
    ntSynced = true;
}


void charge() {
    digitalWrite(Relais_NT_to_BT, HIGH);
    controlTarget = controlTarget + powerFromESP;
    if (controlTarget < maxbsPowerCharging) {controlTarget = maxbsPowerCharging;}
    while (controlTarget <= bsPower && iBatt >= maxIBattCharging && uBatt <= maxUBatt) {
        safetyCheck();
        pwmDecreaseNT();
        if (pwm == 0) return;
    }
    while (controlTarget >= bsPower) {
        safetyCheck();
        pwmIncreaseNT();
        if (pwm == 255) return;
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
    digitalWrite(Relais_DC_to_WR, HIGH);
    controlTarget = controlTarget + powerFromESP;
    if (controlTarget > maxbsPowerDischarging) {controlTarget = maxbsPowerDischarging;}
    while (controlTarget >= bsPower && bsPower <= maxbsPowerDischarging && uBatt >= minUBatt) {
        safetyCheck();
        pwmDecreaseDC();
        if (pwm == 0) return;
    }
    while (controlTarget <= bsPower) {
        safetyCheck();
        pwmIncreaseDC();
        if (pwm == 255) return;
    }
}


void pwmIncreaseNT() {
    if (pwm <= 254) {
        pwm = ++pwm;
        analogWrite(PWM_NT, pwm);
        delay(pwmDelay);
    }
}


void pwmDecreaseNT() {
    if (pwm >= 1) {
        pwm = --pwm;
        analogWrite(PWM_NT, pwm);
        delay(pwmDelay);
    }
}


void pwmIncreaseDC() {
    if (pwm <= 254) {
        pwm = ++pwm;
        analogWrite(PWM_NT, pwm);
        delay(pwmDelay);
    }
}


void pwmDecreaseDC() {
    if (pwm >= 1) {
        pwm = --pwm;
        analogWrite(PWM_NT, pwm);
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
