#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <SPI.h>
#include <SD.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <limits.h>
#include <max6675.h>
#include <PWM.hpp>


#define PIN_SENSOR_DHT 2
#define PIN_PUMP_PWM 3
#define PIN_SD 4
#define PIN_BTN_LEFT 6
#define PIN_BTN_RIGHT 7
#define PIN_SENSOR_DS 8
#define PIN_ETHERNET 10

#define PIN_MAX6675_GND 31
#define PIN_MAX6675_VCC 33
#define PIN_MAX6675_SCK 35
#define PIN_MAX6675_CS 37
#define PIN_MAX6675_SO 39

#define UDP_PORT 8888
#define PORT 80

#define TIME_CHANGE_RULE_EEPROM_ADDRESS 100

#define SENSOR_DS_COUNT 16                                          //pocet DS senzorov
#define SENSOR_OTHER_COUNT 4                                        //pocet ostatnych senzorov
#define TOTAL_SENSOR_COUNT (SENSOR_DS_COUNT + SENSOR_OTHER_COUNT)     //pocet vsetkych senzorov

#define DHT_TYPE DHT22                        //typ DHT senzora

#define LCD_ADDRESS 0x3F
#define LCD_COLS 16
#define LCD_ROWS 2
#define LCD_TURN_OFF_IN 1*60*1000L            // 1 minuta v ms

#define BTN_HOLD_TIME 15                      //cas ako dlho musi byt stlacene tlacidlo (ms)
#define REFRESH_PERIOD 10                     //perioda zapisu na SD kartu (min)

#define PUMP_OFF_THRESHOLD 9000

//vytvori z celeho cisla retazec s pozadovanym poctom cifier (uvodne nuly)
String toString(int value, int places) {
  String s = String(value);
  while (s.length() < places) {
    s = '0' + s;
  }
  return s;
}


String tempToString(int temp) {
 String s = "";
 if (temp < 0) {
   s += '-';
 }
 
 s += String(abs(temp / 100));
 s += ".";
 s += toString(abs(temp % 100), 2);
 return s;
}

class Sensor {
public:
  String caption;
  String urlCaption;
  
  virtual String getDisplayData() = 0;
  virtual String getRawData() = 0;
  
  Sensor(String c, String urlC) {
    caption = c;
    urlCaption = urlC;
  }
};

class TempSensor: public Sensor {
  public:
    int value;
    TempSensor(String c, String urlC): Sensor(c, urlC) {};
    String getDisplayData() {
      return tempToString(value) + " C";
    }
    String getRawData() {
      return tempToString(value);
    }
};

class HumSensor: public Sensor {
  public:
    int value;
    HumSensor(String c, String urlC): Sensor(c, urlC) {};
    String getDisplayData() {
      return tempToString(value) + " %";
    }
    String getRawData() {
      return tempToString(value);
    }
};

class PwmSensor: public Sensor {
  public:
    unsigned int value;
    PwmSensor(String c, String urlC): Sensor(c, urlC) {};
    String getDisplayData() {
      if (value > PUMP_OFF_THRESHOLD) return "OFF - " + String(value);
      return "ON - " + String(value);
    }
    String getRawData() {
      if (value > PUMP_OFF_THRESHOLD) return "0";
      return "1";
    }
};

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);

EthernetUDP udp;
EthernetServer server(PORT);
EthernetClient client;

Timezone timeZone(TIME_CHANGE_RULE_EEPROM_ADDRESS);
TimeChangeRule *tcr;

OneWire oneWire(PIN_SENSOR_DS);
DallasTemperature sensorsDS(&oneWire);
DHT dht(PIN_SENSOR_DHT, DHT_TYPE);
MAX6675 max_6675(PIN_MAX6675_SCK, PIN_MAX6675_CS, PIN_MAX6675_SO);

PWM pwm(PIN_PUMP_PWM);


byte ds_adress[][8] {
  {40, 255, 132, 149, 162, 22, 4, 98},  // 0 : izba S
  {40, 255, 242, 123, 163, 22, 5, 58},  // 1 : vonku S
  {40, 255, 10, 172, 162, 22, 4, 208},  // 2 : kotol vyvod
  {40, 255, 27, 224, 192, 23, 5, 227},  // - : kotol spiat
  {40, 255, 90, 82, 163, 22, 3, 9},     // 3 : radiatory spiat
  {40, 255, 126, 177, 162, 22, 4, 7},   // 4 : radiatory privod
  {40, 255, 251, 221, 162, 22, 4, 156}, // 5 : bojler
  {40, 255, 215, 171, 162, 22, 4, 228}, // 6 : aku TUV
  {40, 255, 74, 230, 192, 23, 5, 178},  // - : aku hore
  {40, 255, 113, 231, 192, 23, 5, 201}, // - : aku stred
  {40, 255, 191, 174, 162, 22, 4, 74},  // 9 : aku dole
  {40, 255, 239, 177, 162, 22, 4, 125}, // - : plyn 1
  {40, 255, 17, 230, 192, 23, 5, 87},   // - : plyn 2
  {40, 92, 122, 69, 146, 7, 2, 73},     // - : strecha 1
  {40, 191, 5, 69, 146, 9, 2, 231},     // - : strecha 2
  {40, 103, 155, 69, 146, 3, 2, 112}    // - : strecha 3
  
};


//pole pre uskladnenie nameranych hodnot
TempSensor dsSensors[] = {
  TempSensor("Izba S", "t_izba_s"),
  TempSensor("Vonku S", "t_vonku_s"),
  TempSensor("Kotol vyvod", "t_kotol"),
  TempSensor("Kotol spiat.", "t_kotol_sp"),
  TempSensor("Radiatory spiat.", "t_rad_sp"),
  TempSensor("Radiatory privod", "t_rad_pr"),
  TempSensor("Bojler TUV", "t_bojler"),
  TempSensor("Aku TUV", "t_aku_tuv"),
  TempSensor("Aku hore", "t_aku_hore"),
  TempSensor("Aku stred", "t_aku_stred"),
  TempSensor("Aku dole", "t_aku_dole"),
  TempSensor("Plyn 1", "t_plyn_1"),
  TempSensor("Plyn 2", "t_plyn_2"),
  TempSensor("Strecha 1", "t_strecha_1"),
  TempSensor("Strecha 2", "t_strecha_2"),
  TempSensor("Strecha 3", "t_strecha_3")
};

HumSensor humDdhtSouth("Vonku J vlhkost","v_vonku_j");
TempSensor tempDdhtSouth("Vonku J teplota", "t_vonku_j");

TempSensor exhaustGasMax("Spaliny", "t_spaliny");

PwmSensor pumpPwm("Cerpadlo", "cerpadlo");

Sensor* data[TOTAL_SENSOR_COUNT];

boolean written = false;
unsigned long turnOffLCDin = ULONG_MAX;

//index aktualne zobrazovanej hodnoty na displeji
int displayIndex = 0;


//stavy stlacenia tlacidiel
boolean btnLeftPressed = false;
boolean btnRightPressed = false;

//casy stlacenia tlacidiel
unsigned long btnLeftPressedAt = 0;
unsigned long btnRightPressedAt = 0;


void setup() {
  for (int i = 0; i < SENSOR_DS_COUNT + 4; i++) {
    data[i] = &dsSensors[i];
  }
  data[SENSOR_DS_COUNT + 0] = &humDdhtSouth;
  data[SENSOR_DS_COUNT + 1] = &tempDdhtSouth;
  data[SENSOR_DS_COUNT + 2] = &exhaustGasMax;
  data[SENSOR_DS_COUNT + 3] = &pumpPwm;
  
  Serial.begin(9600);
  delay(250);

  //inicializacia pinov
  pinMode(PIN_SD, OUTPUT);
  pinMode(PIN_ETHERNET, OUTPUT);
  digitalWrite(PIN_SD, HIGH);
  digitalWrite(PIN_ETHERNET, HIGH);

  pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
  pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);


  //inicializacia LCD, modulov a senzorov
  initMaxSensor();
  initEthernet();
  getTime();
  initSD();
  sensorsDS.begin();
  dht.begin();
  pwm.begin(true);
  initLCD();
}









void loop() {
  //ak si klient vyziada stranku, tak ju vytvorime
  if (server.available()) {
    createServer(server.available());
  }


  //zapis na SD kartu
  if (minute() % REFRESH_PERIOD == 0) {   //minuta |10
    if (!written) {                     //a este sme v nej nezapisali
      writeLine();                        //zapiseme riadok na kartu
      sendData();
      written = true;                   //a oznacime ako zapisane
    }
  } else {
    written  = false;                   //na inej minute resetujeme stav
  }


  //stlacenie laveho tlacidla
  if (digitalRead(PIN_BTN_LEFT) == HIGH) {
    //tlacidlo je uvolnene a bolo pred tym stlacene dlhsie ako BTN_HOLD_TIME ms
    if (btnLeftPressed && (millis() - btnLeftPressedAt > BTN_HOLD_TIME)) {
      //zmenime stav na uvolnene
      btnLeftPressed = false;
      //ak bol displej zhasnuty
      if (!lcd.getBacklight()) {
        //nacitame hodnoty zo senzorov
        getData();
        //zobrazime ich
        lcdPrint();
        //a displej rozsvietime
        lcd.backlight();
      } else {
        //inak sa vratime o index spat
        displayIndex--;
        //ak sme na zaciatku tak sa posunieme na koniec
        if (displayIndex < 0) {
          displayIndex = TOTAL_SENSOR_COUNT - 1;
        }
        //zobrazime hodnoty na novom indexe
        lcdPrint();
      }
    }
  } else {
    //ak je tlacidlo stlacene a pred tym bolo uvolnenne
    if (!btnLeftPressed) {
      //zapiseme cas stlacenia
      btnLeftPressedAt = millis();
      //a zmenime stav na stlacene
      btnLeftPressed = true;
    }
  }


  //stlacenie praveho tlacidla
  if (digitalRead(PIN_BTN_RIGHT) == HIGH) {
    if (btnRightPressed && (millis() - btnRightPressedAt > BTN_HOLD_TIME)) {
      btnRightPressed = false;
      if (!lcd.getBacklight()) {
        getData();
        lcdPrint();
        lcd.backlight();
      } else {
        displayIndex++;
        if (displayIndex > TOTAL_SENSOR_COUNT - 1) {
          displayIndex = 0;
        }
        lcdPrint();
      }
    }
  } else {
    if (!btnRightPressed) {
      btnRightPressedAt = millis();
      btnRightPressed = true;
    }
  }



  //ak sme presiahli cas zhasnutia displeja
  if (millis() >= turnOffLCDin) {
    lcd.clear();
    //tak ho zhasneme
    lcd.noBacklight();
  }
}


//zobrazenie popisu a hodnoty na LCD displej
void lcdPrint() {
  //vymazeme displej a nastavime kurzor na 0,0
  lcd.clear();
  //zobrazime popis
  lcd.print(data[displayIndex]->caption);
  //posunieme kurzor na zaciatok 2.riadku
  lcd.setCursor(0,1);
  
  //vypiseme hodnotu
  lcd.print(data[displayIndex]->getDisplayData());

  //predlzime cas zhasnutia displeja o LCD_TURN_OFF_IN
  turnOffLCDin = millis() + LCD_TURN_OFF_IN;
}


//nacitanie hodnot zo senzorov
//+vytvorenie retazca na zapis na SD kartu
String getData() {
  //vyziadame si teploty od senzorov DS
  sensorsDS.requestTemperatures();
  //vytvorime si prazdny riadok
  String line = "";

  for (int i = 0; i < SENSOR_DS_COUNT; i++) {
    //kazdu teplotu ulozime do pola ako cele cislo  ( *100)
    //stacia nam 2 des. miesta
    dsSensors[i].value = sensorsDS.getTempC(ds_adress[i]) * 100;
  }

  //precitame teplotu a vlhkost z DHT senzora
  tempDdhtSouth.value = dht.readTemperature() * 100;
  humDdhtSouth.value = dht.readHumidity() * 100;

  //pwm hodnota z cerpadla
  pumpPwm.value = pwm.getValue();

  // teplota zo senzora MAX6675
  exhaustGasMax.value = max_6675.readCelsius() * 100;
  
  for (int i = 0; i < TOTAL_SENSOR_COUNT; i++) {
    //pridame oddelovac |
    line += '|';
    //zapiseme celu cast
    line += data[i]->getRawData();
  }
    
  return line;
}




//zapise riadok na SD kartu v tvare:
//RRRR-MM-DD|HH:MM|T1.T1|T2.T2|...|Tn.Tn
void writeLine() {
  digitalWrite(PIN_SD, LOW);
  File file = SD.open(getFileName(), FILE_WRITE);
  if (file) {
    String line = timeToString() + getData();
    file.println(line);
    file.close();
    Serial.println(line);
  } else {
    Serial.println("Error opening " + getFileName());
    digitalWrite(PIN_SD, HIGH);
  }
  digitalWrite(PIN_SD, HIGH);
}



//vytvori nazov suboru na SD karte v tvare:
//RRRR-MM.txt
String getFileName() {
  String fileName = toString(year(), 4);
  fileName += '-';
  fileName += toString(month(), 2);
  fileName += ".txt";
  return fileName;
}






//z aktualneho casu vytvori retazec v tvare:
//RRRR-MM-DD|HH:MM
//s ohladom na letny/zimny cas
String timeToString(){
  time_t t = timeZone.toLocal(now(), &tcr);
  String sTime = toString(year(t), 4);
  sTime += '-';
  sTime += toString(month(t), 2);
  sTime += '-';
  sTime += toString(day(t), 2);
  sTime += '|';
  sTime += toString(hour(t), 2);
  sTime += ':';
  sTime += toString(minute(t), 2);
  return sTime;
}

void initMaxSensor() {
  pinMode(PIN_MAX6675_VCC, OUTPUT);
  pinMode(PIN_MAX6675_GND, OUTPUT);
  
  digitalWrite(PIN_MAX6675_VCC, HIGH);
  digitalWrite(PIN_MAX6675_GND, LOW);
}


//inicializacia LCD displeja
void initLCD() {
  lcd.begin();
  lcd.noBacklight();
  lcd.clear();
}


//inicializacia ethernet shield-u
void initEthernet() {
  digitalWrite(PIN_ETHERNET, LOW);
  byte mac[] = {0x54,0x55,0x58,0x10,0x00,0x24};   // 54:55:58:10:00:24
  while (!Ethernet.begin(mac)) {
    Serial.println("Failed to configure Ethernet using DHCP");
  }
  Serial.print("IP address assigned by DHCP is ");
  Serial.println(Ethernet.localIP());
  digitalWrite(PIN_ETHERNET, HIGH);
}


//zisti aktualny cas z NTP servera
void getTime() {
  digitalWrite(PIN_ETHERNET, LOW);
  udp.begin(UDP_PORT);
  while (timeStatus() != timeSet) {
    setSyncProvider(getNtpTime);
  }
  Serial.println("Time has been set");
  digitalWrite(PIN_ETHERNET, HIGH);
}


//inicializacia SD karty
void initSD() {
  digitalWrite(PIN_SD, LOW);
  while (!SD.begin(PIN_SD)) {
    Serial.println("SD card error");
  }
  Serial.println("Card initialized");
  digitalWrite(PIN_SD, HIGH);
}


//vytvorenie weboveho serveru
void createServer(EthernetClient client) {
  //nacitame hodnoty zo senzorov
  getData();

  //http poziadavka zacina prazdnym riadkom
  boolean currentLineIsBlank = true;
  while (client.connected()) {
    if (client.available()) {
      char c = client.read();
      Serial.write(c);
      //pokail sme sa dostali na koniec riadku a zaroven tento riadok
      //bol prazdny, HTTP poziadavka skoncila a mozeme poslat odpoved
      if (c == '\n' && currentLineIsBlank) {
        //posleme standardnu HTTP hlavicku
        client.println("HTTP/1.1 200 OK");
        client.println("Content-Type: text/html");
        client.println("Connection: close");
        //samoobnovenie stranky kazdych 60 sekund
        client.println("Refresh: 60");
        client.println();
        client.println("<!DOCTYPE HTML>");
        client.println("<html>");



        //vyvtorime HTML stranku s nacitanymi hodnotami
        client.println("<table>");
        client.print("<caption>");
        client.print(timeToString());
        client.println("</caption>");
        
        for (int i = 0; i < TOTAL_SENSOR_COUNT; i++) {
          client.println("<tr>");
          
          client.print("<td>");
          client.print(data[i]->caption);
          client.println("</td>");
          
          client.print("<td>");
          client.print(data[i]->getDisplayData());

          client.println("</td>");
          
          client.println("</tr>");
        }

//        client.println("<tr>");
//          
//        client.print("<td>Cerpadlo</td>");
//        client.print("<td>");
//        if (pumpPwmValue > PUMP_OFF_THRESHOLD) {
//          client.print("OFF - ");
//        } else {
//          client.print("ON - ");
//        }
//        client.print(pumpPwmValue);
//        client.println("</td>");
//        
//        client.println("</tr>");
        

        client.println("</table>");





        
        client.println("</html>");
        break;
      }


     
      if (c == '\n') {
        //dosli sme na koniec riadku, zaciname novy
        currentLineIsBlank = true;
      } else if (c != '\r') {
        //na aktualnom riadku je nejaky znak
        currentLineIsBlank = false;
      }
    }
  }
  
  delay(1);
  // zatvorime spojenie
  client.stop();
  Serial.println("client disconnected");
}



//NTP cas je v prvych 48 bajtoch spravy
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

//vyziadanie paketu od NTP servera
//a precitanie casu zo ziskaneho UDP paketu
time_t getNtpTime() {
  //zahodime vsetky predchadzajuce pakety
  while (udp.parsePacket() > 0) ;
  Serial.println("Transmit NTP Request");
  //192.168.100.3 - NTP server bezi na sietovom disku v lokalnej sieti
  IPAddress timeServer(192, 168, 100, 3);
  //posleme serveru poziadavku
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      //precitame paket do zasobnika
      udp.read(packetBuffer, NTP_PACKET_SIZE);
      unsigned long secsSince1900;
      //prevedieme 4 bajty na pozicii 40 na long
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL;
    }
  }
  Serial.println("No NTP Response");
  return 0;
}



//posleme NTP poziadavku casovemu serveru na zadanej adrese
void sendNTPpacket(IPAddress &address) {
  //cely zasobnik nastavime na 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  //nastavime hodnoty potrebne na zostavenie NTP poziadavky
  packetBuffer[0] = 0b11100011;
  packetBuffer[1] = 0;
  packetBuffer[2] = 6;
  packetBuffer[3] = 0xEC;
  //8x0
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  //vsetky hodnoty nastavene, posleme poziadavku
  //NTP port - 123            
  udp.beginPacket(address, 123);
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();
}

void sendData() {
  if (client.connect("192.168.100.3", 80)) {
    delay(20);
    client.print("GET http://192.168.100.3/insert.php/?datum=");
    client.print(timeToString());

    for (int i = 0; i < TOTAL_SENSOR_COUNT; i++) {

      client.print("&");
      client.print(data[i]->urlCaption);
      client.print("=");
      client.print(data[i]->getRawData());
         
    }

    client.println();
    client.println("Host: 192.168.100.3");
    client.println("Connection: close");
    client.println();
    client.stop();
  }
}
