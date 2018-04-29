#include <TimeLib.h>
#include <Timezone.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define BME_ADDRESS (0x76)
#define PIN_ETHERNET 10

#define UDP_PORT 8888
#define PORT 80

#define TIME_CHANGE_RULE_EEPROM_ADDRESS 100

#define VALUE_COUNT 3

#define REFRESH_PERIOD 10

Adafruit_BME280 bme;

EthernetUDP udp;
EthernetServer server(PORT);
EthernetClient client;

Timezone timeZone(TIME_CHANGE_RULE_EEPROM_ADDRESS);
TimeChangeRule *tcr;

//popisy jednotlivych nameranych hodnot
//zobrazovane na displeji a webovom serveri
String captions[] = {
  "Teplota",
  "Vlhkost",
  "Tlak"
};

String urlCaptions[] = {
  "t_izba_j",
  "v_izba_j",
  "tlak"
};



//pole pre uskladnenie nameranych hodnot
long data[VALUE_COUNT];


boolean written = false;

void setup() {
  Serial.begin(9600);
  delay(250);

  pinMode(PIN_ETHERNET, OUTPUT);
  digitalWrite(PIN_ETHERNET, HIGH);

  
  
  initBME();
  initEthernet();
  getTime();
}



void loop() {
  //ak si klient vyziada stranku, tak ju vytvorime
  if (server.available()) {
    createServer(server.available());
  }


  //zapis na SD kartu
  if (minute() % REFRESH_PERIOD == 0) {   //minuta |10
    if (!written) {                     //a este sme v nej nezapisali
      sendData();
      written = true;                   //a oznacime ako zapisane
    }
  } else {
    written  = false;                   //na inej minute resetujeme stav
  }


  
}


//nacitanie hodnot zo senzorov
//+vytvorenie retazca na zapis na SD kartu
String getData() {
  String line = "";

  data[0] = bme.readTemperature() * 100;
  data[1] = bme.readHumidity() * 100;
  data[2] = bme.readPressure();

  for (int i = 0; i < VALUE_COUNT; i++) {
    line += '|';
    line += tempToString(data[i]);
  }
    
  return line;
}



//vytvori z celeho cisla retazec s pozadovanym poctom cifier (uvodne nuly)
String toString(long value, int places) {
  String s = String(value);
  while (s.length() < places) {
    s = '0' + s;
  }
  return s;
}


String tempToString(long temp) {
 String s = "";
 if (temp < 0) {
   s += '-';
 }
 
 s += String(abs(temp / 100));
 s += ".";
 s += toString(abs(temp % 100), 2);
 return s;
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



//inicializacia ethernet shield-u
void initEthernet() {
  digitalWrite(PIN_ETHERNET, LOW);
  byte mac[] = {0x54,0x55,0x58,0x10,0x00,0x25};   // 54:55:58:10:00:24
  while (!Ethernet.begin(mac)) {
    Serial.println("Failed to configure Ethernet using DHCP");
  }
  Serial.print("IP address assigned by DHCP is ");
  Serial.println(Ethernet.localIP());
  digitalWrite(PIN_ETHERNET, HIGH);
}

void initBME() {
  while (!bme.begin(BME_ADDRESS)) {
    Serial.println("BME280 sensor not found");
  }
  Serial.println("BME sensor initialized");
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
        
        for (int i = 0; i < VALUE_COUNT; i++) {
          client.println("<tr>");
          
          client.print("<td>");
          client.print(captions[i]);
          client.println("</td>");
          
          client.print("<td>");
          client.print(tempToString(data[i]));
          if (i == 0) {
            client.print(" C");
          } else if (i == 1) {
            client.print(" %");
          } else {
            client.print(" hPa");
          }
          
          client.println("</td>");
          
          client.println("</tr>");
        }

        

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
  getData();
  if (client.connect("192.168.100.3", 80)) {
    delay(20);
    client.print("GET http://192.168.100.3/insert2.php/?datum=");
    client.print(timeToString());

    for (int i = 0; i < VALUE_COUNT; i++) {

      client.print("&");
      client.print(urlCaptions[i]);
      client.print("=");
      client.print(tempToString(data[i]));
         
    }

    client.println();
    client.println("Host: 192.168.100.3");
    client.println("Connection: close");
    client.println();
    client.stop();
  }
}

