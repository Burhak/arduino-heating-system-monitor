#include <TimeLib.h>
#include <Timezone.h>

//stredoeuropsky letny cas
//posledna marcova nedela 2->3
//UTC +2
TimeChangeRule cest = {"CEST", Last, Sun, Mar, 2, +120};

//stredoeuropsky zimny cas
//posledna oktobrova nedela 3->2
//UTC +1
TimeChangeRule cet = {"CET", Last, Sun, Oct, 3, +60};


Timezone tz(cest, cet);

void setup(void) {
    pinMode(13, OUTPUT);
    //zapiseme pravidla do EEPROM na adresu 100
    tz.writeRules(100);
}

void loop(void) {
    //rozblikame LEDku, zapis do EEPROM je ukonceny
    digitalWrite(13, HIGH);
    delay(100);    
    digitalWrite(13, LOW);
    delay(100);    
}
