#include <SPI.h>
#include <Ethernet2.h>
#include <EEPROM.h>
byte rAddr= 0;
int mySettingAddr = sizeof(int);
unsigned long countTimer = 0;

int16_t TFlame, TTemp, timer500, Tbtn;
int32_t Tshnek,Trozhik, Tflamefix, waitTshnek, Tflame=0, Tvizh;
//Настройки ------------>
struct SETTINGS {
  byte t_rozhik_shnek; //Время подкидывания при розжиге 
  byte t_nagrev_shnek; //Время подкидывания при нагреве
  byte t_podderg_shnek; //Время подкидывания при подержании
  byte t_shnek_step; //Промежуток между подкидываниями
  byte t_rozhik; //Время отведённое на розжик
  byte flame_fix; //Время виксации пламяни
  byte t_flame; //На каком проценте начинается фиксация
  byte vent_rozhik; //скорость вентилятора при розжиге
  byte vent_nagrev; //скорость вентилятора при нагреве
  byte vent_podderg; //скорость вентилятора при поддержании
  byte vent_ogidanie; //скорость вентилятора при ожидании
  byte temp; //Установка температуры
  byte gister; //Гистерезис
  byte t_vizh; //Время выжигания после пламя = 0
};

SETTINGS defaultSettings = {
  10, //Время подкидывания при розжиге 
  8, //Время подкидывания при нагреве
  5, //Время подкидывания при подержании
  10, //Промежуток между подкидываниями
  50, //Время отведённое на розжик
  10, //Время виксации пламяни
  5, //На каком проценте начинается фиксация
  30, //скорость вентилятора при розжиге
  100, //скорость вентилятора при нагреве
  75, //скорость вентилятора при поддержании
  0, //скорость вентилятора при ожидании
  30, //Установка температуры
  4, //Гистерезис
  10 //Время выжигания после пламя = 0
};
SETTINGS conf; 
//<------------------------

//Режим ------------>
byte regim;
byte prregim = 0;
byte rozhikCount = 0;
/*
Режимы - -
  0 - Ожидание
  1 - Розжик
  2 - Нагрев
  3 - Поддержание
  4 - Выжигание
  5 - Ошибка розжига
*/
// Реле ------------>
byte shnek = 8;
byte lampa = 7;
bool shnekStart = false;
bool lampaStart = false;
//<------------------------
// Кнопка ----------->
bool bflag = false;
//<------------------------


// Вентилятор ----------->
byte vent = 5;
byte vspeed = 0; //скорость вентилятора в процентах
byte vspeedtemp = 0;
//<------------------------


// Датчик огня ----------->
byte analogPin = A0;
String flame = "0";
float flamePersent = 0; //Глобальная пламя
//<------------------------

// Температура ------------------------->
#include <OneWire.h>
OneWire  ds(3);
byte addr[8];
float temperVal = 0; // Глобальная температура
//<---------------------------------


// Дисплей ------------------------->
#include <OLED_I2C.h>
OLED  myOLED(SDA, SCL); 
extern uint8_t SmallFont[];
extern uint8_t RusFont[]; // Русский шрифт
extern uint8_t MediumNumbers[]; // Подключение больших шрифтов
extern uint8_t SmallFont[]; // Базовый шрифт без поддержки русскийх символов.

//<---------------------------------


// Лан ------------------------->
// MAC-адрес вашего Arduino
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x29, 0xED};
// Создаем сервер на порту 80
EthernetServer server(80);
EthernetClient client;
String defIP = "";
//<---------------------------------



int percentToValue(int percent) { //Функция перевода процентов в число для вкентилятора
  if (percent == 0){
    return 0;
  } else {
    return map(percent, 0, 100, 73, 255);
  }
}

void setup()
{
  //Читае настройки из памяти
  regim = EEPROM.read(rAddr);
   if (regim == 0xFF) {
    regim = 0;
    EEPROM.write(rAddr, regim);
  } 
  EEPROM.get(rAddr+1, conf);
  bool hasData = true;
  for (size_t i = 0; i < sizeof(SETTINGS); i++) {
    if (EEPROM.read(rAddr+1 + i) == 255) { 
      hasData = false;
      break;
    }
  }
  // Если данных нет в EEPROM, используйте дефолтные значения
  if (!hasData) {
    conf = defaultSettings;
    EEPROM.put(rAddr+1, conf);
    //Serial.println("Default settings written to EEPROM");
  } else {
    //Serial.println("Settings read from EEPROM:");
  }

  //Ethernet
  Ethernet.begin(mac);
  //Узнаём ip
  defIP = "";
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    defIP+=Ethernet.localIP()[thisByte];
    defIP+=".";
  }
  server.begin();

  pinMode(lampa, OUTPUT);    
  pinMode(shnek, OUTPUT);    
  digitalWrite(lampa, HIGH); // Выключаем реле
  digitalWrite(shnek, HIGH); // Выключаем реле

  myOLED.begin(); 
  pinMode(vent, OUTPUT); 
  analogWrite(vent, percentToValue(0));
  pinMode(4, INPUT_PULLUP); //Кнопка
  Serial.begin(9600);

  //Блок какой режим был до выключения
  if (regim==1 || regim==2 ||regim==3){
    byte flametemp = flameGet();
    //Проверяем есть ли пламя
    if (flametemp>conf.t_flame) { //пламя больше пламени фиксации
      regim = 1;
      EEPROM.write(rAddr, regim);
      prregim =14;
    }
    if (flametemp<conf.t_flame){ //если меньше - режим розжига
      regim = 1;
      EEPROM.write(rAddr, regim);
      prregim =10;
    }
  }
  if (regim == 4){
    prregim = 8;
    Tflame = 0;
    lampaStart=false;
    shnekStart=false;
    vspeedtemp = 100;
  }

}


//Функция веб-морды --------------->
void web(){
  EthernetClient ethClient = server.available();
  if (ethClient) {
    // Ждем, пока клиент подключится и отправит запрос
    while (!ethClient.available()) {
      delay(1);
    }
    // Читаем первую строку запроса
    String request = ethClient.readStringUntil('\r');
    ethClient.flush();
    if (processSettingsUpdate(request)){
      if (sendparams(ethClient,request)){
        if (sendstate(ethClient,request)){
          if (webpressbutton(ethClient,request)){
            sendSettingsPage(ethClient);
          }
        }
      }
    } else {
      ethClient.println(F("HTTP/1.1 200 OK"));
    }
    
    // Закрываем соединение
    ethClient.stop();
  }
}

boolean webpressbutton(EthernetClient& ethClient, String request) {
  if (request.indexOf("pressbutton") != -1 ){
    pressputton();
  return false;
  } else {
    return true;
  }
}


//Посылаем текущее состояние
boolean sendstate(EthernetClient& ethClient, String request) {
  if (request.indexOf("getstate") != -1 ){
    ethClient.println(F("HTTP/1.1 200 OK"));
    ethClient.println(F("Content-Type: text/JSON"));
    ethClient.println();
    ethClient.print("{");
    ethClient.print("\"regim\":" + String(regim) + ",");
    ethClient.print("\"shnekStart\":" + String(shnekStart) + ",");
    ethClient.print("\"lampaStart\":" + String(lampaStart) + ",");
    ethClient.print("\"vspeed\":" + String(vspeed) + ",");
    ethClient.print("\"flamePersent\":" + String(flamePersent) + ",");
    ethClient.print("\"temperVal\":" + String(temperVal) + "}");
    return false;
  } else {
    return true;
  }
}



//Посылаем текущие настройки
boolean sendparams(EthernetClient& ethClient, String request) {
  if (request.indexOf("getparams") != -1 ){
    ethClient.println(F("HTTP/1.1 200 OK"));
    ethClient.println(F("Content-Type: text/JSON"));
    ethClient.println();
    ethClient.print("{");
    ethClient.print("\"t_rozhik_shnek\":" + String(conf.t_rozhik_shnek) + ",");
    ethClient.print("\"t_nagrev_shnek\":" + String(conf.t_nagrev_shnek) + ",");
    ethClient.print("\"t_podderg_shnek\":" + String(conf.t_podderg_shnek) + ",");
    ethClient.print("\"t_shnek_step\":" + String(conf.t_shnek_step) + ",");
    ethClient.print("\"t_rozhik\":" + String(conf.t_rozhik) + ",");
    ethClient.print("\"flame_fix\":" + String(conf.flame_fix) + ",");
    ethClient.print("\"t_flame\":" + String(conf.t_flame) + ",");
    ethClient.print("\"vent_rozhik\":" + String(conf.vent_rozhik) + ",");
    ethClient.print("\"vent_nagrev\":" + String(conf.vent_nagrev) + ",");
    ethClient.print("\"vent_podderg\":" + String(conf.vent_podderg) + ",");
    ethClient.print("\"vent_ogidanie\":" + String(conf.vent_ogidanie) + ",");
    ethClient.print("\"temp\":" + String(conf.temp) + ",");
    ethClient.print("\"gister\":" + String(conf.gister) + ",");
    ethClient.print("\"t_vizh\":" + String(conf.t_vizh)+" }");


    return false;
  } else {
    return true;
  }
}

//Устанавливаем настройку и записываем в память
void setval(String keyString, int val) {

  if (keyString == F("t_rozhik_shnek")) {
    conf.t_rozhik_shnek = val;
  } else if (keyString == F("t_nagrev_shnek")) {
    conf.t_nagrev_shnek = val;
  } else if (keyString == F("t_podderg_shnek")) {
    conf.t_podderg_shnek = val;
  } else if (keyString == F("t_shnek_step")) {
    conf.t_shnek_step = val;
  } else if (keyString == F("t_rozhik")) {
    conf.t_rozhik = val;
  } else if (keyString == F("flame_fix")) {
    conf.flame_fix = val;
  } else if (keyString == F("t_flame")) {
    conf.t_flame = val;
  } else if (keyString == F("vent_rozhik")) {
    conf.vent_rozhik = val;
  } else if (keyString == F("vent_nagrev")) {
    conf.vent_nagrev = val;
  } else if (keyString == F("vent_podderg")) {
    conf.vent_podderg = val;
  } else if (keyString == F("vent_ogidanie")) {
    conf.vent_ogidanie = val;
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
    EEPROM.put(rAddr+1, conf);
    return false;
  } else {
    return true;
  }
}


//Отправляем стартовую страницу
void sendSettingsPage(EthernetClient& ethClient) {
  // Отправляем HTTP-заголовок
  ethClient.println(F("HTTP/1.1 200 OK"));
  ethClient.println(F("Content-Type: text/html"));
  ethClient.println();

  // Отправляем HTML-страницу с формой для изменения значений
  ethClient.println(F("<html><head>"));
  ethClient.println(F("<meta name='viewport' content='width=device-width, initial-scale=1.0'>"));
  ethClient.println(F("<style>body,html,iframe{width:100%;height:100%}body,html{margin:0;padding:0;overflow:hidden}iframe{border:none}</style>"));
  ethClient.println(F("</head><body><iframe id='i'></iframe><script>(async function l(url, id) {"));
  ethClient.println(F("document.getElementById('i').srcdoc = await (await fetch('https://raw.githubusercontent.com/arma666/apg25-arduino/main/html/loaded.html')).text();"));
  ethClient.println(F("})()</script></body></html>"));
}


//Функция нажатия кнопки --------------->
void pressputton(){
  if (regim == 0 ){
    regim = 1;
    EEPROM.write(rAddr, regim);
    prregim =10;
  } else if (regim == 4 || regim == 5) {
    regim = 0;
    EEPROM.write(rAddr, regim);
    prregim = 0;
    Tflame=0;
    shnekStart=false;
    lampaStart=false;
    vspeedtemp = 0;
  }
  else {
    int tregim = regim;
    regim = 4;
    EEPROM.write(rAddr, regim);
    prregim = 8;
    Tflame = 0;
    lampaStart=false;
    shnekStart=false;
    vspeedtemp = 100;
  }


}
//<----------------------------------------
/*
8  - выжигание
10 - начало розжига, подача
11 - окончание подачи, лампа
12 - Ждём фиксации пламяни. Если ошибка розжига- заглушка на 19
13 - Увидели пламя, фексируем
14 - разгорелось, проверяем температуру
20 - Установка вентилятора - нагрев
21 - старт нагрева
22 - подкидывание
23 - ожидание перед подкидыванием
30 - Установка вентилятора - поддержание
31 - старт поддержания
32 - подкидывание
33 - ожидание перед подкидыванием
*/
void control() {
  switch (prregim) {
      case 8:
        if (flamePersent == 0) {
          Tvizh = millis();
          prregim = 9;
        }
      break;
      case 9:
        if (Tvizh+(conf.t_vizh*1000L)<millis()){
          prregim = 0;
          regim = 0;
          EEPROM.write(rAddr, regim);
          shnekStart=false;
          lampaStart=false;
          vspeedtemp = 0;
        }
      break;
      case 10:
        prregim = 11;
        shnekStart=true;
        Tshnek = millis();
        countTimer=conf.t_rozhik_shnek;
      break;
      case 11:
        countTimer=(Tshnek+(conf.t_rozhik_shnek*1000L) - millis())/1000;
        if (Tshnek+(conf.t_rozhik_shnek*1000L) < millis()) {
          shnekStart=false;
          lampaStart=true;
          vspeedtemp = conf.vent_rozhik;
          prregim = 12;
          Trozhik = millis();
        }
      break;
      case 12:
        if (Trozhik+(conf.t_rozhik*1000L) < millis()){
          //Serial.println("fuck");
          if (!rozhikCount) {
            rozhikCount++;
            lampaStart=false;
            prregim = 10;
          } else {
            rozhikCount=0;
            prregim = 19;
            regim = 5;
            EEPROM.write(rAddr, regim);
            lampaStart=false;
          }
        }
        if (flamePersent > conf.t_flame) {
          Tflamefix = millis();
          prregim = 13;
        }
      break;
      case 13:
        if (flamePersent > conf.t_flame && Tflamefix+(conf.flame_fix*1000L)< millis()) {
          //Если разгорелось
          lampaStart=false;
          prregim = 14;
        }
        if (flamePersent < conf.t_flame) {
          //Если пламя пропало
          prregim = 12;
        }
      break;
      case 14:
        if (temperVal>conf.temp-conf.gister) {
          //Если болше заданной температуры то поддержка
          regim = 3;
          EEPROM.write(rAddr, regim);
          prregim = 30;
        } else {
          regim = 2;
          EEPROM.write(rAddr, regim);
          prregim = 20;
        }
      break;
      case 20:
        vspeedtemp = conf.vent_nagrev;
        prregim = 21;
      break; 
      case 21: 
        if (temperVal>conf.temp) { //Если температура богльше то режим поддержания
          regim = 3;
          EEPROM.write(rAddr, regim);
          prregim = 30;
        } else {
          shnekStart=true;
          Tshnek = millis();
          prregim = 22;
        }
      break;
      case 22: 
        if (Tshnek+(conf.t_nagrev_shnek*1000L)<millis()){
          shnekStart=false;
          waitTshnek = millis();
          prregim = 23;
        }
      break;
      case 23: 
        if (temperVal>conf.temp) { //Если температура богльше то режим поддержания
          regim = 3;
          EEPROM.write(rAddr, regim);
          prregim = 30;
        }
        if (waitTshnek+(conf.t_shnek_step*1000L)<millis()){
          prregim = 21;
        }
      break;
      case 30: 
        vspeedtemp = conf.vent_podderg;
        prregim = 31;
      break;
      case 31: 
        if (temperVal<conf.temp-conf.gister) { //Если темп меньше гистерезиса то нагрев
          regim = 2;
          EEPROM.write(rAddr, regim);
          prregim = 20;
        } else {
          shnekStart=true;
          Tshnek = millis();
          prregim = 32;
        }
      break;
      case 32: 
        if (Tshnek+(conf.t_podderg_shnek*1000L)<millis()){
          shnekStart=false;
          waitTshnek = millis();
          prregim = 33;
        }
      break;
      case 33: 
        if (temperVal<conf.temp-conf.gister) { //Если темп меньше гистерезиса то нагрев
          regim = 2;
          EEPROM.write(rAddr, regim);
          prregim = 20;
        }
        if (waitTshnek+(conf.t_shnek_step*1000L)<millis()){
          prregim = 31;
        }
      break;
  }
}

//Функция проверки не патух ли котел
void flamecheck(){
  if (regim && regim!=1 && regim!=5 && regim!=4 && !Tflame){
    Serial.println("Chech flame");
    if (flamePersent < conf.t_flame) {
          //Если пламя пропало, то возможно его забросало и оно разгориться, ждём 20 секунд
          Tflame=millis();
    }
  }
  if (Tflame && Tflame+(10*1000L)<millis()){
    if (flamePersent < conf.t_flame){
      //не разожглось, уходим в ошибку
      shnekStart=false;
      prregim = 19;
      regim = 5;
      EEPROM.write(rAddr, regim);
      lampaStart=false;
      Tflame=0;
    } else {
      Tflame=0;
    }
  }


}


void loop(){
  
  if (millis() - timer500 >= 500) {
    timer500 = millis();
   
    control();
    flamecheck();

    //блок реле
    if (shnekStart){  
      digitalWrite(shnek, LOW); // Включаем реле
    } else {
      digitalWrite(shnek, HIGH); // Выключаем реле
    }
    if (lampaStart) {
      digitalWrite(lampa, LOW); // Выключаем реле
    } else {
      digitalWrite(lampa, HIGH); // Выключаем реле
    }

    //Блок вентилятора
    if (vspeed != vspeedtemp) {
      vspeed = vspeedtemp;
      analogWrite(vent, 255);
    } else {
      analogWrite(vent, percentToValue(vspeed));
    }


    //Блок дисплея
    myOLED.clrScr(); // очищаем экран
    myOLED.setFont(RusFont); // Устанавливаем русский шрифт
    myOLED.print(F("Ntvgthfnehf"), CENTER, 0); // Выводим надпись "Температура"
    myOLED.setFont(MediumNumbers);
    myOLED.print(String(temperVal), CENTER, 9);   // Отображение температуры
    
    myOLED.setFont(RusFont); 
    myOLED.print(F("Gkfvz"), LEFT, 33); // Выводим надпись "Пламя"
    myOLED.setFont(SmallFont);
    myOLED.print(String(flamePersent,1)+"%", LEFT, 43); // вывод текста

    myOLED.setFont(RusFont);
    myOLED.print(F("Dtyn"), RIGHT, 33); // Выводим надпись "Вент"
    myOLED.setFont(SmallFont);
    myOLED.print(String(vspeed)+"%", RIGHT, 43); // вывод текста

    myOLED.setFont(RusFont); 
    String regimStr;
    switch (regim) {
      case 0:
        regimStr = F("|J;blfybt|");
      break;
      case 1:
        regimStr = "|Hjp;br "+String(rozhikCount+1)+ "|";
      break;
      case 2:
        regimStr = F("|Yfuhtd|");
      break;
      case 3:
        regimStr = F("|Gjllth;|");
      break;
      case 4:
        regimStr = F("|Ds;bufybt|");
      break;
      case 5:
        regimStr = F("|Ji hjp;buf|");
      break;
    
    }
    myOLED.print(regimStr, CENTER, 37); // Выводим надпись "Ожидание"
   
    String status;
    myOLED.setFont(RusFont); 
    switch (prregim) {
      case 10:
        status = F("Gjlrblsdfybt");
      break;
      case 11:
        status = "Gjlrblsdfybt "+String(countTimer);
      break;
      case 12:
        status = F("Kfvgf");
      break;
      case 13:
        status = F("Db;e gkfvz - abrcfwbz");
      break;
      case 14:
        status = F("Hfpujhtkjcm!");
      break;
      case 21:
        status = "Gjlrblsdfybt "+String(conf.t_nagrev_shnek)+'c';
      break;
      case 22:
        status = "Gjlrblsdfybt "+String(conf.t_nagrev_shnek)+'c';
      break;
      case 31:
        status = "Gjlrblsdfybt "+String(conf.t_podderg_shnek)+'c';
      break;
      case 32:
        status = "Gjlrblsdfybt "+String(conf.t_podderg_shnek)+'c';
      break;
      default:
        myOLED.setFont(SmallFont);
        status = defIP;
    }
    myOLED.print(status, CENTER, 57); // Выводим надпись "Айпишник"
    myOLED.update();

  }




  //блок кнопки
  bool btnState = !digitalRead(4);
  if (btnState && !bflag && millis() - Tbtn > 100) { //Кнопку нажали
    bflag = true;
    Tbtn = millis();
  } 
  if (!btnState && bflag && millis() - Tbtn > 100) {  //Кнопку отпустили
    bflag = false;
    Tbtn = millis();
    pressputton();
  }

  

  //Блок пламени
  if (millis() - TFlame >= 1000) {
    TFlame = millis();
    int flame = analogRead(analogPin);
    flamePersent  = flameGet();
  }
  //Блок температуры
  if (millis() - TTemp >= 5000) {
    TTemp = millis();
    temperVal = TempGetTepr();
  }

} //End loop

byte flameGet() {
  int flame = analogRead(analogPin);
    if (flame<= 1000) {
      return 100 - ((float)flame / 1000.0) * 100;
    } else {
      return 0;
    }
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
    //Serial.println("yes");
    return TempReturn();
  } else {
    //Serial.println("no");
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