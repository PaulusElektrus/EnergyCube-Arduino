# EnergyCube Arduino Part

### -–> For the ESP Part see [here](https://github.com/PaulusElektrus/EnergyCube-ESP)

This is a code documentation for a battery storage system. The system is designed to control the charging and discharging of a battery based on commands received from an ESP device. It utilizes relays, PWM signals, and an ADS1115 analog-to-digital converter for measurements.

## Hardware Requirements

- Arduino board
- ESP device
- ADS1115 analog-to-digital converter
- Relays for controlling power flow
- Resistors and capacitors as required for circuitry

## Libraries Used

- ADS1115_WE: This library provides an interface for the ADS1115 analog-to-digital converter.
- Wire: This library is used for I2C communication with the ADS1115 and other devices.

## Functions

- Relais Pins: These pins define the digital pins used for controlling the relays that control power flow.
- PWM Pins: These pins define the PWM pins used for controlling the charging and discharging power.
- ADS1115: This section includes the library and configuration for the ADS1115 analog-to-digital converter.
- Incoming Communication: These variables and functions handle communication with the ESP device.
- Incoming Data: These variables store the command and power data received from the ESP device.
- UART Timing: These variables control the timing of UART communication with the ESP device.
- Measurements: These variables store the measured values of voltage and current.
- Status: These variables track the status of the battery system.
- Control: These variables and functions handle the control of charging and discharging.
- Safety Parameters: These variables define the safety thresholds for various parameters.
- Setup: This function is called once at boot to initialize the system.
- Loop: This function is the main loop of the system, responsible for safety checks, receiving commands, and control.
- Safety Check: This function checks the safety parameters and takes appropriate actions if thresholds are exceeded.
- Get Command: This function receives commands and power data from the ESP device.
- Return Data: This function sends data back to the ESP device.
- Receive with Start/End Markers: This function receives data from the ESP device with start and end markers.
- Parse Data: This function parses the received data into command and power values.
- Control: This function determines the control action based on the received command and power values.
- Off: This function turns off all relays and resets system variables.
- Activate NT: This function activates the charger (NT) by turning on the necessary relays.
- Sync NT: This function syncs the charger with the battery voltage.
- Charge: This function controls the charging power based on the control target.
- Activate DC: This function activates the discharging (DC) by turning on the necessary relays.
- Discharge: This function controls the discharging power based on the control target.
- PWM Helper Functions: These functions help increase or decrease the PWM signals for controlling power.
- Measurement: This function reads the voltage and current measurements from the ADS1115.
- Read Voltage: This function reads the voltage measurement from the ADS1115.
- Read Current: This function reads the current measurement from the ADS1115.
- Debug PC: This function can be used for debugging by printing relevant system variables to the serial monitor.

## Usage

1. Connect the required hardware components as per the pin configuration.
2. Install the required libraries (ADS1115_WE and Wire) in your Arduino IDE.
3. Upload the code to your Arduino board.
4. Connect the ESP device for communication.
5. Monitor the system status and measurements through the serial monitor.

## Acknowledgments

- ADS1115_WE library by Wolle Wald (https://github.com/wollewald/ADS1115_WE)
- Wire library by Arduino (https://www.arduino.cc/en/reference/wire)