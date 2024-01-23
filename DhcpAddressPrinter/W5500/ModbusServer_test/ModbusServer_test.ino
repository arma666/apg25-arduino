/*
  WiFi Modbus TCP Server LED

  This sketch creates a Modbus TCP Server with a simulated coil.
  The value of the simulated coil is set on the LED

  Circuit:
   - MKR1000 or MKR WiFi 1010 board

  created 16 July 2018
  by Sandeep Mistry
*/

#include <SPI.h>


#include <ArduinoRS485.h> // ArduinoModbus depends on the ArduinoRS485 library
#include <ArduinoModbus.h>
#include <Ethernet2.h>
bool _1s;
unsigned long TimeAct, TimePrev, HoldingResult, InputResult, HeartBeat, i, StartingAddr;
long Cmd;

EthernetServer EthServer(502);
ModbusTCPServer modbusTCPServer;

void setup() {
  // Ethernet Settings
  byte mac[] = { 0x4E, 0xA0, 0xBE, 0x3F, 0xFE, 0x0F };  // Define MAc address
  byte ip[] = { 192, 168, 2, 107 };                      // Define IP address
  byte subnet[] = { 255, 255, 255, 0 };                 // Define SubNEt mask

  // initialize the ethernet device
  Ethernet.begin(mac, ip, subnet);                      // Assign MAC, IP, and subnet mask
  Serial.begin(9600);
  EthServer.begin();          // start listening for clients
  modbusTCPServer.begin();    // start listening for clients

  // Define Holding register:
  HoldingResult = modbusTCPServer.configureHoldingRegisters(0, 100);
  InputResult = modbusTCPServer.configureInputRegisters(0, 100);

  Serial.print("Holding Reg init result =");
  Serial.print(HoldingResult);
  Serial.print("\n");

  Serial.print("Input Reg init result =");
  Serial.print(InputResult);
  Serial.print("\n");

  Serial.print("Modbus server address=");
  Serial.println(Ethernet.localIP());
  Serial.print("\n");
}

void loop() {
  // Modbus server accept incoming connections
  EthernetClient client = EthServer.available();
   
  if (client.connected()) {
    modbusTCPServer.accept(client);
    // poll for Modbus TCP requests, while client connected
    modbusTCPServer.poll();
     Serial.print("poll");
  }

  //---------------------------------------------------------------------------------
  // TIME  clock 1s
  TimeAct = millis();     // Millis value between  0 and  655535

  _1s = false;
  if (  ( (TimeAct > TimePrev) and (TimeAct - TimePrev) > 1000) or ((TimeAct < TimePrev) and (65535 - TimePrev + TimeAct) > 1000 )  ) {
    _1s = true;
    TimePrev = TimeAct;
  }
  //---------------------------------------------------------------------------------
  // HeartBeat
  if (_1s) {
    HeartBeat++;
    if (HeartBeat > 255) {
      HeartBeat = 0;
    }
    Serial.print("HeartBeat=");
    Serial.println(HeartBeat);
    Serial.print("\n");
  }
  //---------------------------------------------------------------------------------
  // Modbus server  :

  // holding register 40001: heartbeat (FC3)
  modbusTCPServer.holdingRegisterWrite(0x00, HeartBeat);

  // holding register 40500: Command Word (FC6)
  Cmd = modbusTCPServer.holdingRegisterRead(90);


  if (_1s) {
    Serial.print("cmd");  // printout the Received holding register 90
    Serial.print(Cmd);
    Serial.print("\n");
  }
}
