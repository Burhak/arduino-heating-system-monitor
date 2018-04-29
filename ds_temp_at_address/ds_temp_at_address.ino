#include <OneWire.h>
#include <DallasTemperature.h>

#define PIN_SENSOR_DS 8

OneWire oneWire(PIN_SENSOR_DS);
DallasTemperature sensorsDS(&oneWire);

byte ds_adress[][8] {
  {40, 255, 113, 231, 192, 23, 5, 201},//aku stred
  {40, 255, 27, 224, 192, 23, 5, 227}, //kotol spiat

  {40, 255, 132, 149, 162, 22, 4, 98}, //izba sever
  {40, 255, 242, 123, 163, 22, 5, 58}, //vonku sever
  {40, 255, 10, 172, 162, 22, 4, 208}, //vyvod kotol
  {40, 255, 74, 230, 192, 23, 5, 178}, //bez oznacenia
  {40, 255, 90, 82, 163, 22, 3, 9},    //cislo 3
  {40, 255, 126, 177, 162, 22, 4, 7},  //riadotory privod, cislo 4
  {40, 255, 251, 221, 162, 22, 4, 156}, //bojler tuv, cislo 5
  {40, 255, 215, 171, 162, 22, 4, 228}, //cislo 6
  {40, 255, 191, 174, 162, 22, 4, 74}   //cislo 9
};

void setup() {
  Serial.begin(9600);
  sensorsDS.begin();
  delay(500);
}

void loop() {
  sensorsDS.requestTemperatures();

  Serial.println(sensorsDS.getTempC(ds_adress[0]));
  Serial.println(sensorsDS.getTempC(ds_adress[1]));
  
  Serial.println();
  

  delay(3000);

}
