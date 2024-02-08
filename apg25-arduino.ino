#include <SPI.h>
#include <Ethernet2.h>
#include <EEPROM.h>

byte rAddr= 0;
int mySettingAddr = sizeof(int);
unsigned long countTimer = 0;

//int16_t opt.TTemp, opt.timer500, opt.Tbtn;
//int32_t Tshnek,opt.Trozhik, opt.opt.Tflamefix, opt.waitTshnek, opt.Tflame=0, opt.Tvizh;

struct OPT {
  int32_t Tshnek;
  int32_t Trozhik;
  int32_t Tflamefix;
  int32_t waitTshnek;
  int32_t Tflame;
  int32_t Tvizh;
  int16_t TFlame;
  int16_t TTemp;
  int16_t timer500;
  int16_t Tbtn;
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
  byte rozhikCount;
};
OPT opt;
//Настройки ------------>
struct SETTINGS {
  byte troz_sh; //Время подкидывания при розжиге 
  byte tnag_sh; //Время подкидывания при нагреве
  byte tpod_sh; //Время подкидывания при подержании
  byte tsh_st; //Промежуток между подкидываниями
  byte troz; //Время отведённое на розжик
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
// byte regim;
// byte opt.prregim = 0;
// byte opt.rozhikCount = 0;
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
const byte shnek = 8;
const byte lampa = 7;
bool shnekStart = false;
bool lampaStart = false;
//<------------------------
// Кнопка ----------->
bool bflag = false;
//<------------------------


// Вентилятор ----------->
const byte vent = 5;
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
  //Serial.begin(9600);
  //Читае настройки из памяти
  opt.regim = EEPROM.read(rAddr);
   if (opt.regim == 0xFF) {
    opt.regim = 0;
    rwrite();
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
  if (opt.regim==1 || opt.regim==2 || opt.regim==3){
    byte flametemp = flameGet();
    //Проверяем есть ли пламя
    if (flametemp>conf.tfl) { //пламя больше пламени фиксации
      opt.regim = 1;
      rwrite();
      opt.prregim =14;
    }
    if (flametemp<conf.tfl){ //если меньше - режим розжига
      opt.regim = 1;
      rwrite();
      opt.prregim =10;
    }
  }
  if (opt.regim == 4){
    opt.prregim = 8;
    opt.Tflame = 0;
    lampaStart=false;
    shnekStart=false;
    vspeedtemp = 100;
  }

}

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
    // Закрываем соединение
    ethClient.stop();
    }
  }
}

boolean webpressbutton(EthernetClient& ethClient, String request) {
  if (request.indexOf(F("pressbutton")) != -1 ){
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
    ethClient.print("\"flamePersent\":" + String(flamePersent) + ",");
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
void setval(const String keyString, const byte val) {

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
  // Отправляем HTTP-заголовок и начало страницы
  ethClient.println(F("HTTP/1.1 200 OK"));
  ethClient.println(F("Content-Type: text/html"));
  ethClient.println();

  // Отправляем HTML-страницу с формой для изменения значений
  ethClient.print(F("<html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'><style>body,html,iframe{width:100%;height:100%}body,html{margin:0;padding:0;overflow:hidden}iframe{border:none}</style></head><body><iframe id='i'></iframe><script>(async function l(url,id){document.getElementById('i').srcdoc=await(await fetch('https://raw.githubusercontent.com/arma666/apg25-arduino/main/html/loaded.html')).text()})()</script></body></html>"));
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
  switch (opt.prregim) {
      case 8:
        if (flamePersent == 0) {
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
        countTimer=conf.troz_sh;
      break;
      case 11:
        countTimer=(opt.Tshnek+(conf.troz_sh*1000L) - millis())/1000;
        if (opt.Tshnek+(conf.troz_sh*1000L) < millis()) {
          shnekStart=false;
          lampaStart=true;
          vspeedtemp = conf.vroz;
          opt.prregim = 12;
          opt.Trozhik = millis();
        }
      break;
      case 12:
        if (opt.Trozhik+(conf.troz*1000L) < millis()){
          //Serial.println("fuck");
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
        if (flamePersent > conf.tfl) {
          opt.Tflamefix = millis();
          opt.prregim = 13;
        }
      break;
      case 13:
        if (flamePersent > conf.tfl && opt.Tflamefix+(conf.fl_fix*1000L)< millis()) {
          //Если разгорелось
          lampaStart=false;
          opt.prregim = 14;
        }
        if (flamePersent < conf.tfl) {
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

//Запись режима
void rwrite(){
  EEPROM.write(rAddr, opt.regim);
}

//Функция проверки не патух ли котел
void flamecheck(){
  if (opt.regim && opt.regim!=1 && opt.regim!=5 && opt.regim!=4 && !opt.Tflame){
    if (flamePersent < conf.tfl) {
          //Если пламя пропало, то возможно его забросало и оно разгориться, ждём 20 секунд
          opt.Tflame=millis();
    }
  }
  if (opt.Tflame && opt.Tflame+(10*1000L)<millis()){
    if (flamePersent < conf.tfl){
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


void loop(){
  
  if (millis() - opt.timer500 >= 500) {
    opt.timer500 = millis();
    
    //control();
    //flamecheck();

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
    switch (opt.regim) {
      case 0:
        regimStr = F("|J;blfybt|");
      break;
      case 1:
        regimStr = "|Hjp;br "+String(opt.rozhikCount+1)+ "|";
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
    switch (opt.prregim) {
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
        status = "Gjlrblsdfybt "+String(conf.tnag_sh)+'c';
      break;
      case 22:
        status = "Gjlrblsdfybt "+String(conf.tnag_sh)+'c';
      break;
      case 31:
        status = "Gjlrblsdfybt "+String(conf.tpod_sh)+'c';
      break;
      case 32:
        status = "Gjlrblsdfybt "+String(conf.tpod_sh)+'c';
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
  if (btnState && !bflag && millis() - opt.Tbtn > 100) { //Кнопку нажали
    bflag = true;
    opt.Tbtn = millis();
  } 
  if (!btnState && bflag && millis() - opt.Tbtn > 100) {  //Кнопку отпустили
    bflag = false;
    opt.Tbtn = millis();
    pressputton();
  }

  

  //Блок пламени
  if (millis() - opt.Tflame >= 1000) {
    opt.Tflame = millis();
    int flame = analogRead(analogPin);
    flamePersent  = flameGet();
  }
  //Блок температуры
  if (millis() - opt.TTemp >= 5000) {
    opt.TTemp = millis();
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
