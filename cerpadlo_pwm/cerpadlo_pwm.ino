volatile int pwm_value = 0;
volatile int prev_time = 0;
 
void setup() {
  Serial.begin(9600);
  // when pin D2 goes high, call the rising function
  attachInterrupt(digitalPinToInterrupt(2), rising, RISING);
}
 
void loop() { }
 
void rising() {
  attachInterrupt(digitalPinToInterrupt(2), falling, FALLING);
  prev_time = micros();
}
 
void falling() {
  attachInterrupt(digitalPinToInterrupt(2), rising, RISING);
  pwm_value = micros()-prev_time;
  Serial.println(pwm_value);
}
