#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetClient.h>
#include <EthernetServer.h>
#include <EthernetUdp.h>
#include <util.h>

#include <SPI.h>
#include <OneWire.h>

// OneWire DS18S20, DS18B20, DS1822 Temperature Example
//
// http://www.pjrc.com/teensy/td_libs_OneWire.html
//
// The DallasTemperature library can do all this work for you!
// http://milesburton.com/Dallas_Temperature_Control_Library

OneWire  ds(23);  // on pin 10 (a 4.7K resistor is necessary)

int requestID = 1;

byte mac[] = { 0xDE, 0xAD, 0xBE, 0x5c, 0x7d, 0x0c };

char packetBuffer[512];

PROGMEM prog_char *loopPacket1 = "{\"path\":\"/api/v1/thing/reporting\",\"requestID\":\"";
PROGMEM prog_char *loopPacket2 = "\",\"things\": {\"/device/climate/1wire/temperature\": {\"prototype\": {\"device\": {\"name\": \"DS1820\",\"maker\": \"Dallas Semiconductor\"},\"name\": \"true\",\"status\": [\"present\",\"absent\",\"recent\"],\"properties\": {\"temperature\": \"celsius\"}},\"instances\": [{\"name\": \"";
PROGMEM prog_char *loopPacket3 = "\",\"status\": \"present\",\"unit\": {\"serial\": \"";
PROGMEM prog_char *loopPacket4 = "\",\"udn\": \"1wire:";
PROGMEM prog_char *loopPacket5 = "\"},\"info\": {\"temperature\":";
PROGMEM prog_char *loopPacket6 = "}}]}}}";

EthernetUDP udp;
IPAddress ip(224,0,9,1);
unsigned int port = 22601;   

void setup() {
  Serial.begin(9600);
  Serial.println("Starting...");

  Serial.println("Waiting for DHCP address.");
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Error: Failed to configure Ethernet using DHCP");
    while(1) {  }
  } 

  udp.beginMulti(ip,port);

}

  byte data[12];
  byte addr[8];

void loop() {
    requestID = requestID + 1;

    char buffer[12];
  byte i;
  byte present = 0;
  byte type_s;
  float celsius, fahrenheit;
  
  if ( !ds.search(addr)) {
    Serial.println("No more addresses.");
    Serial.println();
    ds.reset_search();
    delay(250);
    return;
  }
  
  Serial.print("ROM =");
  for( i = 0; i < 8; i++) {
    Serial.write(' ');
    Serial.print(addr[i], HEX);
  }

  if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
      return;
  }
  Serial.println();
 
  // the first ROM byte indicates which chip
  switch (addr[0]) {
    case 0x10:
      Serial.println("  Chip = DS18S20");  // or old DS1820
      type_s = 1;
      break;
    case 0x28:
      Serial.println("  Chip = DS18B20");
      type_s = 0;
      break;
    case 0x22:
      Serial.println("  Chip = DS1822");
      type_s = 0;
      break;
    default:
      Serial.println("Device is not a DS18x20 family device.");
      return;
  } 

  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);        // start conversion, with parasite power on at the end
  
  delay(1000);     // maybe 750ms is enough, maybe not
  // we might do a ds.depower() here, but the reset will take care of it.
  
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE);         // Read Scratchpad

  Serial.print("  Data = ");
  Serial.print(present, HEX);
  Serial.print(" ");
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.print(" CRC=");
  Serial.print(OneWire::crc8(data, 8), HEX);
  Serial.println();

  // Convert the data to actual temperature
  // because the result is a 16 bit signed integer, it should
  // be stored to an "int16_t" type, which is always 16 bits
  // even when compiled on a 32 bit processor.
  int16_t raw = (data[1] << 8) | data[0];
  if (type_s) {
    raw = raw << 3; // 9 bit resolution default
    if (data[7] == 0x10) {
      // "count remain" gives full 12 bit resolution
      raw = (raw & 0xFFF0) + 12 - data[6];
    }
  } else {
    byte cfg = (data[4] & 0x60);
    // at lower res, the low bits are undefined, so let's zero them
    if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
    else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
    else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
    //// default is 12 bit resolution, 750 ms conversion time
  }
  celsius = (float)raw / 16.0;
  fahrenheit = celsius * 1.8 + 32.0;
  Serial.print("  Temperature = ");
  Serial.print(celsius);
  Serial.print(" Celsius, ");
  Serial.print(fahrenheit);
  Serial.println(" Fahrenheit");

    strcpy(packetBuffer,(char*)pgm_read_word(&loopPacket1) );
    strcat(packetBuffer, itoa( requestID, buffer, 10) );
    strcat(packetBuffer,(char*)pgm_read_word(&loopPacket2) );
    if (addr[0] == 0x10) strcat(packetBuffer, "ds1820");
    else if (addr[0] == 0x28) strcat(packetBuffer, "ds18b20");
    else if (addr[0] == 0x22) strcat(packetBuffer, "ds1822");
    else strcat(packetBuffer, "ds????");
    strcat(packetBuffer,(char*)pgm_read_word(&loopPacket3) );
    for (byte thisByte = 0; thisByte < 7; thisByte++) {
      sprintf(buffer, "%02x", addr[thisByte] );
      strcat(packetBuffer, buffer);
    }
    strcat(packetBuffer,(char*)pgm_read_word(&loopPacket4) );
    for (byte thisByte = 0; thisByte < 7; thisByte++) {
      sprintf(buffer, "%02x", addr[thisByte] );
      strcat(packetBuffer, buffer); 
    }
    strcat(packetBuffer,(char*)pgm_read_word(&loopPacket5) );
    strcat(packetBuffer, dtostrf(celsius,4,2,buffer));
    //sprintf(packetBuffer + strlen(packetBuffer), "%4.2f", celsius);
    strcat(packetBuffer,(char*)pgm_read_word(&loopPacket6) );


    Serial.println(packetBuffer); 
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write(packetBuffer);
    udp.endPacket();

  delay(2000);
}

