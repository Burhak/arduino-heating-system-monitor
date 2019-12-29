#include <OneWire.h>

#define PIN_SENSOR_DS 8

OneWire ds(PIN_SENSOR_DS);

void setup(void) {
  Serial.begin(9600);
}

void getDeviceAddress(void) {
  byte i;
  byte addr[8];
  
  Serial.println("Getting the address...");
  Serial.println();
  
  while(ds.search(addr)) {
    
    for( i = 0; i < 8; i++) {
      Serial.print(addr[i]);
      if (i < 7) {
        Serial.print(", ");
      }
    }
    Serial.println();

    if (OneWire::crc8( addr, 7) != addr[7]) {
      Serial.println("CRC is not valid!");
    }
  }

  ds.reset_search();
  Serial.println("Done.");
  Serial.println();
}

void loop() {
  getDeviceAddress();
  delay(2000);
}
