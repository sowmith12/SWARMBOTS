ESP32 UGV Control System
This project implements a remote control system for an Unmanned Ground Vehicle (UGV) using two ESP32 boards. One ESP32 acts as the master (with a web server for control), and the other acts as the slave (receiving commands via ESP-NOW). Both boards control motors via an L293D motor driver to move the UGV.
Hardware Requirements

2x ESP32 Development Boards (e.g., ESP32-WROOM-32)
L293D Motor Driver (or dual H-bridge motor driver)
2x DC Motors (for each ESP32, controlling a differential drive UGV)
Power Supply:
3.3V/5V for ESP32 boards (via USB or external power)
5-12V for motors (depending on motor specifications)


WiFi Router (for master ESP32 to connect to the web server)
Jumper Wires and Breadboard or custom PCB
Computer with Arduino IDE installed
USB Cables for programming ESP32 boards

Hardware Connections
ESP32 (Master and Slave) to L293D Motor Driver
Both ESP32 boards use the same motor control pin configuration to drive two DC motors via the L293D motor driver.



ESP32 Pin
L293D Pin
Description



GPIO 25
IN1
Left Motor Forward


GPIO 26
IN2
Left Motor Backward


GPIO 27
IN3
Right Motor Forward


GPIO 14
IN4
Right Motor Backward


GND
GND
Common Ground


VIN/5V
VCC1
L293D Logic Power (5V)


External
VCC2
Motor Power (5-12V, per specs)



Enable Pins (ENA, ENB on L293D): Connect to VCC or set to HIGH for full speed (not controlled in this code).
Motors: Connect to OUT1/OUT2 (left motor) and OUT3/OUT4 (right motor) on the L293D.
Power:
ESP32: Power via USB or 5V VIN pin.
Motors: Use an external battery (e.g., 7.4V LiPo) connected to L293D VCC2 and GND. Ensure common ground with ESP32.


Note: Repeat the same connections for both master and slave ESP32 boards if both control motors.

Additional Notes

Ensure the motor power supply matches your DC motors' voltage requirements.
Use decoupling capacitors (e.g., 100µF) across motor power lines to reduce noise.
Verify all connections before powering on to avoid short circuits.

Software Requirements

Arduino IDE (version 2.x or later recommended)
ESP32 Board Support: Install via Arduino IDE Boards Manager (search for "ESP32" by Espressif Systems).
Libraries:
WiFi.h (included with ESP32 core)
esp_now.h (included with ESP32 core)
WebServer.h (included with ESP32 core)


Files:
esp_now_motor_control.ino: Slave ESP32 code for receiving ESP-NOW commands and controlling motors.
esp_now_web_control.ino: Master ESP32 code for hosting a web server and sending ESP-NOW commands.



Setup Instructions

Install Arduino IDE and ESP32 Support:

Download and install Arduino IDE from arduino.cc.
In Arduino IDE, go to File > Preferences, add the following URL to Additional Boards Manager URLs:https://raw.githubusercontent.com/espressif/arduino-esp32/master/package_esp32_index.json


Go to Tools > Board > Boards Manager, search for "ESP32", and install the ESP32 package.


Configure WiFi Credentials (Master only):

In esp_now_web_control.ino, update the WiFi credentials:const char* ssid = "Infinix"; // Replace with your WiFi SSID
const char* password = "12345678"; // Replace with your WiFi password




Set Slave MAC Address (Master only):

In esp_now_web_control.ino, update the slaveMacAddress array with the MAC address of the slave ESP32:uint8_t slaveMacAddress[] = {0xEC, 0x94, 0xCB, 0x52, 0x99, 0x78}; // Replace with slave's MAC


To find the slave's MAC address:
Upload esp_now_motor_control.ino to the slave ESP32.
Open the Serial Monitor (Tools > Serial Monitor, 115200 baud).
The slave will print its MAC address (e.g., Slave ready. MAC Address: XX:XX:XX:XX:XX:XX).




Upload Code:

Connect each ESP32 to your computer via USB.
In Arduino IDE, select Tools > Board > ESP32 Arduino > ESP32 Dev Module.
Upload esp_now_motor_control.ino to the slave ESP32.
Upload esp_now_web_control.ino to the master ESP32.


Verify Connections:

Ensure both ESP32 boards are powered and connected to the L293D and motors as described.
Confirm the master ESP32 is in range of your WiFi router.



Running the System

Power On:

Power both ESP32 boards (via USB or external power).
Ensure the motor power supply is connected to the L293D.


Slave ESP32:

After uploading esp_now_motor_control.ino, open the Serial Monitor (115200 baud) to confirm it starts and prints its MAC address.
The slave listens for ESP-NOW commands and controls its motors accordingly.


Master ESP32:

After uploading esp_now_web_control.ino, open the Serial Monitor (115200 baud).
The master connects to the specified WiFi network and prints its IP address (e.g., 192.168.x.x).
If WiFi connection fails after 20 attempts, the master halts (check WiFi credentials).


Access the Web Interface:

On a device connected to the same WiFi network, open a web browser.
Enter the master ESP32’s IP address (from Serial Monitor) in the browser (e.g., http://192.168.x.x).
The web interface displays buttons: Forward, Backward, Left, Right, and Stop.


Control the UGV:

Click a button on the web interface to send a command.
The master ESP32:
Controls its own motors based on the command.
Sends the command to the slave ESP32 via ESP-NOW.


The slave ESP32 receives the command and controls its motors.
Both UGVs (if both have motors) move synchronously (Forward, Backward, Left, Right, or Stop).
Serial Monitor on both ESP32s shows command status and ESP-NOW send/receive logs.



How It Works

Master ESP32:

Hosts a web server on port 80 using WebServer.h.
Serves a simple HTML page with buttons for UGV control.
When a button is clicked, the browser sends a GET request to /command?dir=<command>.
The handleCommand function processes the command, controls the master’s motors, and sends the command to the slave via ESP-NOW.
The onDataSent callback logs whether ESP-NOW transmission was successful.


Slave ESP32:

Listens for ESP-NOW commands using onDataRecv callback.
Controls its motors based on received commands (FORWARD, BACKWARD, LEFT, RIGHT, or STOP).
Logs received commands to Serial Monitor.


Motor Control:

Both ESP32s use the L293D motor driver to control two DC motors in a differential drive configuration.
Commands translate to motor states:
Forward: Both motors forward.
Backward: Both motors backward.
Left: Left motor backward, right motor forward.
Right: Left motor forward, right motor backward.
Stop: All motors off.





Troubleshooting

WiFi Connection Fails (Master):

Verify ssid and password in esp_now_web_control.ino.
Ensure the router is in range and supports 2.4GHz (ESP32 does not support 5GHz).
Restart the master ESP32 and check Serial Monitor.


ESP-NOW Fails:

Confirm the slave’s MAC address in esp_now_web_control.ino is correct.
Ensure both ESP32s are within ~100m (line-of-sight) for ESP-NOW communication.
Check Serial Monitor for ESP-NOW initialization or peer errors.


Motors Don’t Move:

Verify L293D connections and power supply voltage.
Ensure ESP32 GPIOs are correctly wired to L293D input pins.
Check Serial Monitor for command logs to confirm commands are received.


Web Interface Inaccessible:

Confirm the device is on the same WiFi network as the master ESP32.
Use the correct IP address from the master’s Serial Monitor.
Ensure the browser supports JavaScript (for fetch API).



Notes

Security: The web server and ESP-NOW communication are unencrypted. Use in a trusted network or add encryption for production.
Scalability: The system can support multiple slaves by adding their MAC addresses to the master’s peer list (modify esp_now_web_control.ino).
Power: Ensure sufficient current for motors to avoid brownouts on the ESP32.
Range: ESP-NOW range depends on environment; obstacles may reduce effective range.

License
This project is provided as-is for educational purposes. Use at your own risk.
