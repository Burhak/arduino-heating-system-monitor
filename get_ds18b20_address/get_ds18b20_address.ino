#include <OneWire.h>

#define PIN_SENSOR_DS 8

OneWire  ds(PIN_SENSOR_DS);

void setup(void) {
  Serial.begin(9600);
  getDeviceAddress();
}

void getDeviceAddress(void) {
  byte i;
  byte addr[8];
  
  Serial.println("Getting the address...\n\r");
  
  while(ds.search(addr)) {
    for( i = 0; i < 8; i++) {
      
      Serial.print(addr[i]);
      if (i < 7) {
        Serial.print(", ");
      }
    }
    if ( OneWire::crc8( addr, 7) != addr[7]) {
        Serial.print("CRC is not valid!\n");
        return;
    }
    Serial.println();
  }
  ds.reset_search();
  return;
}

void loop() {
  // do nothing
}
