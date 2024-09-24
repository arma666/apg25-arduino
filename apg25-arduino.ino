String VERSION="v0.11a";

#include <SSD1306Wire.h>
#include "fontsRus.h"
#include "fonts.h"
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <SPI.h>
#include <Ethernet2.h>
#include "esp_task_wdt.h"

byte rAddr= 0;
SSD1306Wire display(0x3c, SDA, SCL); // SDA - IO5 (D1), SCL - IO4 (D2) 

// Температура ------------------------->
#include <OneWire.h>
OneWire  ds(14);
byte addr[8];
float temperVal = 0; // Глобальная температура
//<---------------------------------

//Настройки
struct OPT {
  unsigned long  Tshnek;
  unsigned long  Trozhik;
  unsigned long  Tflamefix;
  unsigned long  waitTshnek;
  unsigned long Tflame;
  unsigned long Tvizh;
  unsigned long  TFlame;
  unsigned long TTemp;
  unsigned long timer500;
  unsigned long Tbtn;
  float flamePersent = 0;
  byte vspeed = 0;
  String defIP = "192.168.88.10";
  unsigned long countTimer;
  byte rozhikCount = 0;
  boolean isnet = false; 
  /*
Режимы - -
  0 - Ожидание
  1 - Розжик
  2 - Нагрев
  3 - Поддержание
  4 - Выжигание
  5 - Ошибка розжига
*/
  byte regim;
  byte prregim;
};
OPT opt;
//Настройки ------------>
struct SETTINGS {
  byte troz_sh; //Время подкидывания при розжиге 
  byte tnag_sh; //Время подкидывания при нагреве
  byte tpod_sh; //Время подкидывания при подержании
  byte tsh_st; //Промежуток между подкидываниями
  unsigned long troz; //Время отведённое на розжик
  byte fl_fix; //Время виксации пламяни
  byte tfl; //На каком проценте начинается фиксация
  byte vroz; //скорость вентилятора при розжиге
  byte v_nag; //скорость вентилятора при нагреве
  byte v_pod; //скорость вентилятора при поддержании
  byte v_og; //скорость вентилятора при ожидании
  byte temp; //Установка температуры
  byte gister; //Гистерезис
  byte t_vizh; //Время выжигания после пламя = 0
};

SETTINGS defaultSettings = {
  100, //Время подкидывания при розжиге 
  10, //Время подкидывания при нагреве
  5, //Время подкидывания при подержании
  23, //Промежуток между подкидываниями
  380, //Время отведённое на розжик
  35, //Время виксации пламяни
  23, //На каком проценте начинается фиксация
  30, //скорость вентилятора при розжиге
  100, //скорость вентилятора при нагреве
  75, //скорость вентилятора при поддержании
  0, //скорость вентилятора при ожидании
  30, //Установка температуры
  4, //Гистерезис
  180 //Время выжигания после пламя = 0
};
SETTINGS conf; 

// Датчик огня ----------->
byte analogPin = 13;
//String flame = "0";
//<------------------------

// Реле ------------>
const byte shnek = 17;
const byte lampa = 16;
bool shnekStart = false;
bool lampaStart = false;
//<------------------------

// Кнопка ----------->
bool bflag = false;
byte bpin = 32;
//<------------------------

// Вентилятор ----------->
const byte vent = 25;
byte vspeed = 0; //скорость вентилятора в процентах
byte vspeedtemp = 0;
//<------------------------


// MAC-адрес вашего Arduino
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x29, 0xED};

// Создаем сервер на порту 80
EthernetServer server(80);
byte serverPort = 80;
EthernetClient client;


//Функция инициализации сети
void netstart(){
  //Serial.println("y");
  Ethernet.init(5);
  //Serial.println("x");
  if (Ethernet.begin(mac) == 0) {
    ////Serial.println("Failed to configure Ethernet using DHCP");
    // Некоторые дополнительные действия при ошибке
    opt.defIP="No ip :(";
    //for(;;);
  } else {
    //My IP address:
    opt.defIP = "";
    for (byte thisByte = 0; thisByte < 4; thisByte++) {
      opt.defIP+=Ethernet.localIP()[thisByte];
      if (thisByte < 3) {
        opt.defIP+=".";
      }
    }
    opt.isnet=true;
    // Начинаем слушать порт 80
    server.begin();
  }
}

int percentToValue(int percent) { //Функция перевода процентов в число для вкентилятора
  if (percent == 0){
    return 0;
  } else {
    return map(percent, 0, 100, 73, 255);
  }
}

//Запись режима
void rwrite(){
  File regim = SPIFFS.open("/regim", "w");
  regim.write(opt.regim);
  regim.close();

}
void rload(){
  File regim = SPIFFS.open("/regim", "r");
  if (!regim) {
    File regim = SPIFFS.open("/regim", "w");
    regim.write(0);
    regim.close();
  } else {
    opt.regim = regim.read();
    regim.close();
  }
}



void setup() {
  esp_task_wdt_init(8, true);  // 8 секунд таймаута
  
  // Регистрируем основную задачу в WDT
  esp_task_wdt_add(NULL); 
  //Serial.begin(115200);
  //Сеть
  netstart();
  //Вентилятор
  pinMode(vent, OUTPUT);

  //Реле
  pinMode(lampa, OUTPUT);    
  pinMode(shnek, OUTPUT);  
  digitalWrite(lampa, LOW); // Выключаем реле
  digitalWrite(shnek, LOW); // Выключаем реле 
  //Читаем из памяти
  if (!SPIFFS.begin(true)) {
    //Serial.println("An error occurred while mounting SPIFFS");
    return;
  }
  
  if (!loadSettings()) {
    //Serial.println("Failed to load settings from file, using default settings");
    conf = defaultSettings;
    saveSettings();
  }
  rload();
  
  //Кнопка
  pinMode(bpin, INPUT_PULLUP); 

  //Дисплей
  display.init(); //  Инициализируем дисплей
  display.flipScreenVertically(); // Устанавливаем зеркальное отображение экрана, к примеру, удобно, если вы хотите желтую область сделать вверху
  display.setFontTableLookupFunction(FontUtf8Rus);


}
String x = "";
bool loadSettings() {
  File configFile = SPIFFS.open("/conf1.json", "r");
  if (!configFile) {
    return false;
  }

  size_t size = configFile.size();
  if (size == 0) {
    return false;
  }

  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  x=buf.get();
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, buf.get());
  if (error) {
    //Serial.println("Failed to parse config file");
    return false;
  }

  conf.troz_sh = doc["troz_sh"] | defaultSettings.troz_sh;
  conf.tnag_sh = doc["tnag_sh"] | defaultSettings.tnag_sh;
  conf.tpod_sh = doc["tpod_sh"] | defaultSettings.tpod_sh;
  conf.tsh_st = doc["tsh_st"] | defaultSettings.tsh_st;
  conf.troz = doc["troz"] | defaultSettings.troz;
  conf.fl_fix = doc["fl_fix"] | defaultSettings.fl_fix;
  conf.tfl = doc["tfl"] | defaultSettings.tfl;
  conf.vroz = doc["vroz"] | defaultSettings.vroz;
  conf.v_nag = doc["v_nag"] | defaultSettings.v_nag;
  conf.v_pod = doc["v_pod"] | defaultSettings.v_pod;
  conf.v_og = doc["v_og"] | defaultSettings.v_og;
  conf.temp = doc["temp"] | defaultSettings.temp;
  conf.gister = doc["gister"] | defaultSettings.gister;
  conf.t_vizh = doc["t_vizh"] | defaultSettings.t_vizh;

  configFile.close();
  return true;
}

void saveSettings() {
  File configFile = SPIFFS.open("/conf1.json", "w");
  if (!configFile) {
    //Serial.println("Failed to open config file for writing");
    return;
  }

  StaticJsonDocument<1024> doc;
  doc["troz_sh"] = conf.troz_sh;
  doc["tnag_sh"] = conf.tnag_sh;
  doc["tpod_sh"] = conf.tpod_sh;
  doc["tsh_st"] = conf.tsh_st;
  doc["troz"] = conf.troz;
  doc["fl_fix"] = conf.fl_fix;
  doc["tfl"] = conf.tfl;
  doc["vroz"] = conf.vroz;
  doc["v_nag"] = conf.v_nag;
  doc["v_pod"] = conf.v_pod;
  doc["v_og"] = conf.v_og;
  doc["temp"] = conf.temp;
  doc["gister"] = conf.gister;
  doc["t_vizh"] = conf.t_vizh;

  if (serializeJson(doc, configFile) == 0) {
    //Serial.println("Failed to write to config file");
  }

  configFile.close();
}


void Display(){
  display.clear(); // Очищаем экран
  display.setFont(ArialRus_Plain_10); // Шрифт кегль 10
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(display.getWidth() / 2, 0, "Температура");
  display.setFont(Arimo_Bold_13);
  display.drawString(display.getWidth() / 2, 13, String(temperVal));
  //Версия
  display.setFont(ArialRus_Plain_10); 
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 10, VERSION);
  //Пламя
  display.setFont(ArialRus_Plain_10); 
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 25, "Пламя");
  display.drawString(0, 35, String(opt.flamePersent,1)+"%");
  //Вентилятор
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(display.getWidth(), 25, "Вент");
  display.drawString(display.getWidth(), 35, String(vspeed)+"%");
  //Режим
  String regimStr;
  switch (opt.regim) {
    case 0:
      regimStr = F("|Ожидание|");
    break;
    case 1:
      regimStr = "|Розжик "+String(opt.rozhikCount+1)+ "|";
    break;
    case 2:
      regimStr = F("|Нагрев|");
    break;
    case 3:
      regimStr = F("|Поддерж|");
    break;
    case 4:
      regimStr = F("|Выжигание|");
    break;
    case 5:
      regimStr = F("|Ошибка|");
    break;
    }
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(display.getWidth() / 2, 30, regimStr);
  String status;
  switch (opt.prregim) {
    case 10:
      status = F("Подкидывание");
    break;
    case 11:
      status = "Подкидывание "+String(opt.countTimer);
    break;
    case 12:
      status = "Лампа "+String(opt.countTimer);
    break;
    case 13:
      status = F("Вижу пламя - фиксация");
    break;
    case 14:
      status = F("Разгорелось!");
    break;
    case 21:
      status = "Подкидывание "+String(opt.countTimer);
    break;
    case 22:
      status = "Подкидывание "+String(opt.countTimer);
    break;
    case 31:
      status = "Подкидывание "+String(opt.countTimer);
    break;
    case 32:
      status = "Подкидывание "+String(opt.countTimer);
    break;
    default:
      status = opt.defIP;
  }
  display.drawString(display.getWidth() / 2, 50, status);

  display.display(); // Выводим на экран
}



void loop() {
  if (opt.isnet)
  {
    web();
  } else 
  {
    netstart();
  }
  
  
  
  if (millis() - opt.TTemp >= 5000) {
    opt.TTemp = millis();
    temperVal = TempGetTepr();
    ////Serial.println(String(millis()) + " - "+ String(opt.TTemp) );
    ////Serial.println(String(temperVal));
  }
  if (millis() - opt.timer500 >= 500) {
    ////Serial.println("");
    ////Serial.println(x);
    opt.timer500=millis();
    esp_task_wdt_reset();
    flameGet();
    rele();
    Display();
    Vent();
    flamecheck();
    control();
  }

  //блок кнопки
  bool btnState = !digitalRead(bpin);
  if (btnState && !bflag && millis() - opt.Tbtn > 100) { //Кнопку нажали
    bflag = true;
    opt.Tbtn = millis();
  } 
  if (!btnState && bflag && millis() - opt.Tbtn > 100) {  //Кнопку отпустили
    bflag = false;
    opt.Tbtn = millis();
    pressputton();
  }
}


//Функция проверки не патух ли котел
void flamecheck(){
  if (opt.regim && opt.regim!=1 && opt.regim!=5 && opt.regim!=4 && !opt.Tflame){
    if (opt.flamePersent < conf.tfl) {
          //Если пламя пропало, то возможно его забросало и оно разгориться, ждём 20 секунд
          opt.Tflame=millis();
    }
  }
  if (opt.Tflame && opt.Tflame+(45*1000L)<millis()){
    if (opt.flamePersent < conf.tfl){
      //не разожглось, уходим в ошибку
      shnekStart=false;
      opt.prregim = 19;
      opt.regim = 5;
      rwrite();
      lampaStart=false;
      opt.Tflame=0;
    } else {
      opt.Tflame=0;
    }
  }
}


//Основная функция управления 
void control() {
  ////Serial.println(String(opt.prregim));
  switch (opt.prregim) {
      case 8:
        if (opt.flamePersent == 0) {
          opt.Tvizh = millis();
          opt.prregim = 9;
        }
      break;
      case 9:
        if (opt.Tvizh+(conf.t_vizh*1000L)<millis()){
          opt.prregim = 0;
          opt.regim = 0;
          rwrite();
          shnekStart=false;
          lampaStart=false;
          vspeedtemp = 0;
        }
      break;
      case 10:
        opt.prregim = 11;
        shnekStart=true;
        opt.Tshnek = millis();
        opt.countTimer=conf.troz_sh;
      break;
      case 11:
        opt.countTimer=(opt.Tshnek+(conf.troz_sh*1000L) - millis())/1000;
        if (opt.Tshnek+(conf.troz_sh*1000L) < millis()) {
          shnekStart=false;
          lampaStart=true;
          vspeedtemp = conf.vroz;
          opt.prregim = 12;
          opt.Trozhik = millis();
        }
      break;
      case 12:
        //Serial.println("12");
        opt.countTimer=(opt.Trozhik+(conf.troz*1000L) - millis())/1000;
        if (opt.Trozhik+(conf.troz*1000L) < millis()){
          ////Serial.println("fuck");
          if (!opt.rozhikCount) {
            opt.rozhikCount++;
            lampaStart=false;
            opt.prregim = 10;
          } else {
            opt.rozhikCount=0;
            opt.prregim = 19;
            opt.regim = 5;
            rwrite();
            lampaStart=false;
          }
        }
        if (opt.flamePersent > conf.tfl) {
          opt.Tflamefix = millis();
          opt.prregim = 13;
        }
      break;
      case 13:
      //Serial.println("13");
        //opt.countTimer=(opt.Tflamefix+(conf.tfl*1000L) - millis())/1000;
        if (opt.flamePersent > conf.tfl && opt.Tflamefix+(conf.fl_fix*1000L)< millis()) {
          //Если разгорелось
          lampaStart=false;
          opt.prregim = 14;
        }
        if (opt.flamePersent < conf.tfl) {
          //Если пламя пропало
          opt.prregim = 12;
        }
      break;
      case 14:
        if (temperVal>conf.temp-conf.gister) {
          //Если болше заданной температуры то поддержка
          opt.regim = 3;
          rwrite();
          opt.prregim = 30;
        } else {
          opt.regim = 2;
          rwrite();
          opt.prregim = 20;
        }
      break;
      case 20:
        vspeedtemp = conf.v_nag;
        opt.prregim = 21;
      break; 
      case 21: 
        if (temperVal>conf.temp) { //Если температура богльше то режим поддержания
          opt.regim = 3;
          rwrite();
          opt.prregim = 30;
        } else {
          shnekStart=true;
          opt.Tshnek = millis();
          opt.prregim = 22;
        }
      break;
      case 22: 
        opt.countTimer=(opt.Tshnek+(conf.tnag_sh*1000L) - millis())/1000;
        if (opt.Tshnek+(conf.tnag_sh*1000L)<millis()){
          shnekStart=false;
          opt.waitTshnek = millis();
          opt.prregim = 23;
        }
      break;
      case 23: 
        if (temperVal>conf.temp) { //Если температура богльше то режим поддержания
          opt.regim = 3;
          rwrite();
          opt.prregim = 30;
        }
        if (opt.waitTshnek+(conf.tsh_st*1000L)<millis()){
          opt.prregim = 21;
        }
      break;
      case 30: 
        vspeedtemp = conf.v_pod;
        opt.prregim = 31;
      break;
      case 31: 
        if (temperVal<conf.temp-conf.gister) { //Если темп меньше гистерезиса то нагрев
          opt.regim = 2;
          rwrite();
          opt.prregim = 20;
        } else {
          shnekStart=true;
          opt.Tshnek = millis();
          opt.prregim = 32;
        }
      break;
      case 32: 
        opt.countTimer=(opt.Tshnek+(conf.tpod_sh*1000L) - millis())/1000;
        if (opt.Tshnek+(conf.tpod_sh*1000L)<millis()){
          shnekStart=false;
          opt.waitTshnek = millis();
          opt.prregim = 33;
        }
      break;
      case 33: 
        if (temperVal<conf.temp-conf.gister) { //Если темп меньше гистерезиса то нагрев
          opt.regim = 2;
          rwrite();
          opt.prregim = 20;
        }
        if (opt.waitTshnek+(conf.tsh_st*1000L)<millis()){
          opt.prregim = 31;
        }
      break;
  }
}

//Блок вентилятора
void Vent(){
  if (vspeed != vspeedtemp) {
    vspeed = vspeedtemp;
    analogWrite(vent, 255);
  } else {
    analogWrite(vent, percentToValue(vspeed));
  }
}

//Функция нажатия кнопки --------------->
void pressputton(){
  if (opt.regim == 0 ){
    opt.regim = 1;
    opt.prregim =10;
  } else if (opt.regim == 4 || opt.regim == 5) {
    opt.regim = 0;
    opt.prregim = 0;
    opt.Tflame=0;
    shnekStart=false;
    lampaStart=false;
    vspeedtemp = 0;
  }
  else {
    //byte tregim = opt.regim;
    opt.regim = 4;
    opt.prregim = 8;
    opt.Tflame = 0;
    lampaStart=false;
    shnekStart=false;
    vspeedtemp = 100;
  }

  rwrite();
}

//блок реле
void rele(){
  if (shnekStart){  
    digitalWrite(shnek, HIGH); // Включаем реле
  } else {
    digitalWrite(shnek, LOW); // Выключаем реле
  }
  if (lampaStart) {
    digitalWrite(lampa, HIGH); // Выключаем реле
  } else {
    digitalWrite(lampa, LOW); // Выключаем реле
  }
}
char FontUtf8Rus(const byte ch) { 
    static uint8_t LASTCHAR;

    if ((LASTCHAR == 0) && (ch < 0xC0)) {
      return ch;
    }

    if (LASTCHAR == 0) {
        LASTCHAR = ch;
        return 0;
    }

    uint8_t last = LASTCHAR;
    LASTCHAR = 0;
    
    switch (last) {
        case 0xD0:
            if (ch == 0x81) return 0xA8;
            if (ch >= 0x90 && ch <= 0xBF) return ch + 0x30;
            break;
        case 0xD1:
            if (ch == 0x91) return 0xB8;
            if (ch >= 0x80 && ch <= 0x8F) return ch + 0x70;
            break;
    }

    return (uint8_t) 0;
}


//получение огня
void flameGet() {
  opt.flamePersent = 100 - ((float)analogRead(analogPin)/ 4095.0) * 100;
}

// Получение температуры---------------->
float TempReturn() {
  byte i;
  byte present = 0;
  byte data[12];
  present = ds.reset();
  ds.select(addr);    
  ds.write(0xBE); 
  for ( i = 0; i < 9; i++) {           // we need 9 bytes
    data[i] = ds.read();
  }
  int16_t raw = (data[1] << 8) | data[0];
  byte cfg = (data[4] & 0x60);
  if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
  else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
  else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
  return (float)raw / 16.0;
}

float Tempwait1 (int x, uint32_t m) {
  if (m - millis() >= x) {
    ////Serial.println("yes");
    return TempReturn();
  } else {
    ////Serial.println("no");
    return Tempwait1(x,m);
  }
}


float TempGetTepr() {
  if (!ds.search(addr)) {
    return TempGetTepr();
  } else {
    ds.reset();
    ds.select(addr);
    ds.write(0x44, 1);
    return Tempwait1(1000, millis());
  }
}
//<----------------------------------------


//Функция веб-морды --------------->
void web(){
  EthernetClient ethClient = server.available();
  if (ethClient) {
    if (ethClient.available()) {
      String request = ethClient.readStringUntil('\r');
      ethClient.flush();
      if (processSettingsUpdate(request)){
        if (sendparams(ethClient,request)){
          if (sendst(ethClient,request)){
            if (webpressbutton(ethClient,request)){
               sendSettingsPage(ethClient);
            }
          }
       }
      } 
    while (ethClient.available()) {
        ethClient.read();
    }

    // Закрываем соединение
    ethClient.stop();
    }
  }
}

boolean webpressbutton(EthernetClient& ethClient, String request) {
  if (request.indexOf("perssbutton") != -1 ){
    pressputton();
  return false;
  } else {
    return true;
  }
}


//Посылаем текущее состояние
boolean sendst(EthernetClient& ethClient, String request) {
  if (request.indexOf(F("getstate")) != -1 ){
    ethClient.println(F("HTTP/1.1 200 OK"));
    ethClient.println(F("Content-Type: text/JSON"));
    ethClient.println();
    ethClient.print("{");
    ethClient.print("\"regim\":" + String(opt.regim) + ",");
    ethClient.print("\"shnekStart\":" + String(shnekStart) + ",");
    ethClient.print("\"lampaStart\":" + String(lampaStart) + ",");
    ethClient.print("\"vspeed\":" + String(vspeed) + ",");
    ethClient.print("\"flamePersent\":" + String(opt.flamePersent) + ",");
    ethClient.print("\"temperVal\":" + String(temperVal) + "}");
    return false;
  } else {
    return true;
  }
}



//Посылаем текущие настройки
boolean sendparams(EthernetClient& ethClient, String request) {
  if (request.indexOf(F("getparams")) != -1 ){
    ethClient.println(F("HTTP/1.1 200 OK"));
    ethClient.println();
    ethClient.print(F("{\"troz_sh\":"));
    ethClient.print(conf.troz_sh);
    ethClient.print(F(",\"tnag_sh\":"));
    ethClient.print(conf.tnag_sh);
    ethClient.print(F(",\"tpod_sh\":"));
    ethClient.print(conf.tpod_sh);
    ethClient.print(F(",\"tsh_st\":"));
    ethClient.print(conf.tsh_st);
    ethClient.print(F(",\"troz\":"));
    ethClient.print(conf.troz);
    ethClient.print(F(",\"fl_fix\":"));
    ethClient.print(conf.fl_fix);
    ethClient.print(F(",\"tfl\":"));
    ethClient.print(conf.tfl);
    ethClient.print(F(",\"vroz\":"));
    ethClient.print(conf.vroz);
    ethClient.print(F(",\"v_nag\":"));
    ethClient.print(conf.v_nag);
    ethClient.print(F(",\"v_pod\":"));
    ethClient.print(conf.v_pod);
    ethClient.print(F(",\"v_og\":"));
    ethClient.print(conf.v_og);
    ethClient.print(F(",\"temp\":"));
    ethClient.print(conf.temp);
    ethClient.print(F(",\"gister\":"));
    ethClient.print(conf.gister);
    ethClient.print(F(",\"t_vizh\":"));
    ethClient.print(conf.t_vizh);
    ethClient.print("}");

    return false;
  } else {
    return true;
  }
}

//Устанавливаем настройку и записываем в память
void setval(const String keyString, const unsigned long val) {

  if (keyString == F("troz_sh")) {
    conf.troz_sh = val;
  } else if (keyString == F("tnag_sh")) {
    conf.tnag_sh = val;
  } else if (keyString == F("tpod_sh")) {
    conf.tpod_sh = val;
  } else if (keyString == F("tsh_st")) {
    conf.tsh_st = val;
  } else if (keyString == F("troz")) {
    conf.troz = val;
  } else if (keyString == F("fl_fix")) {
    conf.fl_fix = val;
  } else if (keyString == F("tfl")) {
    conf.tfl = val;
  } else if (keyString == F("vroz")) {
    conf.vroz = val;
  } else if (keyString == F("v_nag")) {
    conf.v_nag = val;
  } else if (keyString == F("v_pod")) {
    conf.v_pod = val;
  } else if (keyString == F("v_og")) {
    conf.v_og = val;
    vspeedtemp=val;
  } else if (keyString == F("temp")) {
    conf.temp = val;
  } else if (keyString == F("gister")) {
    conf.gister = val;
  } else if (keyString == F("t_vizh")) {
    conf.t_vizh = val;
  } 
}

//Принимаем изменившуюся настройку
boolean processSettingsUpdate(String request) {
  int paramStart = request.indexOf('?') + 1;
  // Поиск конца названия параметра
  int paramNameEnd = request.indexOf('=', paramStart);
  // Поиск конца значения параметра
  int paramValueEnd = request.indexOf(' ', paramNameEnd);
  if (paramStart != -1 && paramNameEnd != -1 && paramValueEnd != -1) {
    String paramName = request.substring(paramStart, paramNameEnd);
    String paramValue = request.substring(paramNameEnd + 1, paramValueEnd);
    setval(paramName.c_str(), paramValue.toInt());
    saveSettings();
    return false;
  } else {
    return true;
  }
}


//Отправляем стартовую страницу
void sendSettingsPage(EthernetClient& ethClient) {
  // Отправляем HTTP-заголовок и начало страницы
  ethClient.println(F("HTTP/1.1 200 OK"));
  ethClient.println(F("Content-Type: text/html"));
  ethClient.println();

  // Отправляем HTML-страницу с формой для изменения значений
  ethClient.print(F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'><style>body,html,iframe{width:100%;height:100%}body,html{margin:0;padding:0;overflow:hidden}iframe{border:none}</style></head><body><iframe id='i'></iframe><script>(async function l(url,id){document.getElementById('i').srcdoc=await(await fetch('https://raw.githubusercontent.com/arma666/apg25-arduino/main/html/loaded.html?'+(new Date).getTime())).text()})()</script></body></html>"));
}