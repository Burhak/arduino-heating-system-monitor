#include <NewPing.h>

#define PIN_TRIGGER 3
#define PIN_ECHO 4

#define MAX_DIST 450
#define MAX_ERROR_COUNT 10

NewPing sonar(PIN_TRIGGER, PIN_ECHO, MAX_DIST);

void setup() {
  Serial.begin(9600);
}

void loop() {
  int distance = sonar.ping_cm();
  int errorCount = 0;
  while((distance == 0) && (errorCount < MAX_ERROR_COUNT)) {
    errorCount++;
    distance = sonar.ping_cm();
  }
  Serial.println(distance);
  delay(5000);
}
