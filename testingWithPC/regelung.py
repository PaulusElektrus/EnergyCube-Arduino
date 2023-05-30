from time import sleep
import serial

if __name__ == "__main__":
    port = "/dev/ttyUSB0"
    baudrate = 115200
    serial_port = serial.Serial(port, baudrate)
    print(f"The Port name is {serial_port.name}")
    
    while True:
        try:
            if serial_port.inWaiting() > 0:
                line = serial_port.read(serial_port.inWaiting())
                line = line.decode()
                print(line)
        except KeyboardInterrupt:
            user_input = input("Enter data to send or enter nothing to send standard: ")
            if user_input == "":
                data = "<0,0>"
            else:
                data = user_input
                serial_port.write(data.encode(encoding = 'ascii', errors = 'strict'))