// Relais Pins
#define Relais_AC           6
#define Relais_AC_to_NT     7
#define Relais_NT_to_BT     8   
#define Relais_BT_to_DC     9
#define Relais_DC_to_WR     10
#define Relais_WR_to_AC     11

// PWM Pins
#define PWM_NT 3
#define PWM_DC 5

// ADS1115
#include<ADS1115_WE.h> 
#include<Wire.h>
#define I2C_ADDRESS 0x48
ADS1115_WE adc = ADS1115_WE(I2C_ADDRESS);

// Incoming Communication
boolean newData = false;
const byte numChars = 32;
char receivedChars[numChars];
char tempChars[numChars];

// Incoming Data
int commandFromESP = 0;
int powerFromESP = 0;

// Measurements
int status = 0;
float uNT = 0.0;
float uBatt = 0.0;
float uKal = 0.0;
float iBatt = 0.0;
int bsPower = 0.0;

// Status
bool ntReady = false;
bool ntSynced = false;
bool dcReady = false;
bool bsFull = false;
bool bsEmpty = false;

// PWM Status
int pwmNt = 255;
int pwmDc = 255;

// Safety
float maxUBatt = 49.2;
float minUBatt = 44.4;
float maxIBattCharging = -3.0;
float maxbsPowerDischarging = 100;


void setup(){
    pinMode(Relais_AC, OUTPUT);
    pinMode(Relais_AC_to_NT, OUTPUT);
    pinMode(Relais_NT_to_BT, OUTPUT);
    pinMode(Relais_BT_to_DC, OUTPUT);
    pinMode(Relais_DC_to_WR, OUTPUT);
    pinMode(Relais_WR_to_AC, OUTPUT);
    pinMode(PWM_NT, OUTPUT);
    pinMode(PWM_DC, OUTPUT);
    Wire.begin();
    adc.init();
    adc.setVoltageRange_mV(ADS1115_RANGE_6144);
    Serial.begin(115200);
}


void loop(){
    safetyCheck();
    control();
    Serial.print("N");
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


void control() {
    getCommand();
    if (commandFromESP == 0) {
        off();
    }
    else if (commandFromESP == 1 && bsFull == false) {
        if (powerFromESP < 0) {
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
        if (powerFromESP > 0) {
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
    pwmNt = 255;
    pwmDc = 255;
}


void activateNT() {
    off();
    digitalWrite(Relais_AC, HIGH);
    digitalWrite(Relais_AC_to_NT, HIGH);
    ntReady = true;
}


void syncNT() {
    do {
        analogWrite(PWM_NT, pwmNt);
        delay(50);
        measurement();
        pwmNt = --pwmNt;
    } while (uNT < uBatt);
    ntSynced = true;
}


void charge() {
    digitalWrite(Relais_NT_to_BT, HIGH);
    while (powerFromESP < 0 && iBatt >= maxIBattCharging) {
        pwmNt = --pwmNt;
        analogWrite(PWM_NT, pwmNt);
        delay(50);
        getCommand();
    }
    while (powerFromESP > 0) {
        pwmNt = ++pwmNt;
        analogWrite(PWM_NT, pwmNt);
        delay(50);
        getCommand();
    }
}


void activateDC() {
    off();
    digitalWrite(Relais_AC, HIGH);
    digitalWrite(Relais_BT_to_DC, HIGH);
    analogWrite(PWM_DC, pwmDc);
    delay(5000);
    dcReady = true;
}


void discharge() {
    digitalWrite(Relais_DC_to_WR, HIGH);
    while (powerFromESP > 0 && bsPower <= maxbsPowerDischarging) {
        pwmNt = --pwmNt;
        analogWrite(PWM_NT, pwmNt);
        delay(50);
        getCommand();
    }
    while (powerFromESP < 0) {
        pwmNt = ++pwmNt;
        analogWrite(PWM_NT, pwmNt);
        delay(50);
        getCommand();
    }
}


void getCommand() {
    measurement();
    returnData();
    recvWithStartEndMarkers();
    if (newData == true) {
        strcpy(tempChars, receivedChars);
        parseData();
        newData = false;
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
    uNT = 45;
    uBatt = 45;
    uKal = 5;
    iBatt = 1;
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
