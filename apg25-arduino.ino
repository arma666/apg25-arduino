#include <SPI.h>
#include <Ethernet2.h>
#include <EEPROM.h>
int myregimAddr= 0;
int mySettingAddr = sizeof(int);
unsigned long countTimer = 0;

uint32_t myTimerFlame, myTimertemp, myTimerDIsplay, myTimerVent, btnTimer, releTimer;
uint32_t regimTimer, shnekTimer,rozhikTimer, flamefixTimer, waitshnekTimer, flameTimer=0, vizhTimer;
//Настройки ------------>
struct SETTINGS {
  int t_rozhik_shnek; //Время подкидывания при розжиге 
  int t_nagrev_shnek; //Время подкидывания при нагреве
  int t_podderg_shnek; //Время подкидывания при подержании
  int t_shnek_step; //Промежуток между подкидываниями
  int t_rozhik; //Время отведённое на розжик
  int flame_fix; //Время виксации пламяни
  int t_flame; //На каком проценте начинается фиксация
  int vent_rozhik; //скорость вентилятора при розжиге
  int vent_nagrev; //скорость вентилятора при нагреве
  int vent_podderg; //скорость вентилятора при поддержании
  int vent_ogidanie; //скорость вентилятора при ожидании
  int temp; //Установка температуры
  int gister; //Гистерезис
  int t_vizh; //Время выжигания после пламя = 0
};

SETTINGS conf = {
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
//<------------------------

//Режим ------------>
int regim;
int prregim = 0;
int rozhikCount = 0;
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
int shnek = 8;
int lampa = 7;
bool shnekStart = false;
bool lampaStart = false;
//<------------------------
// Кнопка ----------->
bool bflag = false;
//<------------------------


// Вентилятор ----------->
int vent = 5;
int vspeed = 0; //скорость вентилятора в процентах
int vspeedtemp = 0;
//<------------------------


// Датчик огня ----------->
int analogPin = A0;
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
  regim = EEPROM.read(myregimAddr);
   if (regim == 0xFF) {
    regim = 0;
    EEPROM.write(myregimAddr, regim);
  } 


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
    int flametemp = flameGet();
    //Проверяем есть ли пламя
    if (flametemp>conf.t_flame) { //пламя больше пламени фиксации
      regim = 1;
      EEPROM.write(myregimAddr, regim);
      prregim =14;
    }
    if (flametemp<conf.t_flame){ //если меньше - режим розжига
      regim = 1;
      EEPROM.write(myregimAddr, regim);
      prregim =10;
    }
  }
  if (regim == 4){
    prregim = 8;
    flameTimer = 0;
    lampaStart=false;
    shnekStart=false;
    vspeedtemp = 100;
  }

}




//Функция нажатия кнопки --------------->
void pressputton(){
  if (regim == 0 ){
    regim = 1;
    EEPROM.write(myregimAddr, regim);
    prregim =10;
  } else if (regim == 4 || regim == 5) {
    regim = 0;
    EEPROM.write(myregimAddr, regim);
    prregim = 0;
    flameTimer=0;
    shnekStart=false;
    lampaStart=false;
    vspeedtemp = 0;
  }
  else {
    int tregim = regim;
    regim = 4;
    EEPROM.write(myregimAddr, regim);
    prregim = 8;
    flameTimer = 0;
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
          vizhTimer = millis();
          prregim = 9;
        }
      break;
      case 9:
        if (vizhTimer+(conf.t_vizh*1000L)<millis()){
          prregim = 0;
          regim = 0;
          EEPROM.write(myregimAddr, regim);
          shnekStart=false;
          lampaStart=false;
          vspeedtemp = 0;
        }
      break;
      case 10:
        prregim = 11;
        shnekStart=true;
        shnekTimer = millis();
        countTimer=conf.t_rozhik_shnek;
      break;
      case 11:
        countTimer=(shnekTimer+(conf.t_rozhik_shnek*1000L) - millis())/1000;
        if (shnekTimer+(conf.t_rozhik_shnek*1000L) < millis()) {
          shnekStart=false;
          lampaStart=true;
          vspeedtemp = conf.vent_rozhik;
          prregim = 12;
          rozhikTimer = millis();
        }
      break;
      case 12:
        if (rozhikTimer+(conf.t_rozhik*1000L) < millis()){
          //Serial.println("fuck");
          if (!rozhikCount) {
            rozhikCount++;
            lampaStart=false;
            prregim = 10;
          } else {
            rozhikCount=0;
            prregim = 19;
            regim = 5;
            EEPROM.write(myregimAddr, regim);
            lampaStart=false;
          }
        }
        if (flamePersent > conf.t_flame) {
          flamefixTimer = millis();
          prregim = 13;
        }
      break;
      case 13:
        if (flamePersent > conf.t_flame && flamefixTimer+(conf.flame_fix*1000L)< millis()) {
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
          EEPROM.write(myregimAddr, regim);
          prregim = 30;
        } else {
          regim = 2;
          EEPROM.write(myregimAddr, regim);
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
          EEPROM.write(myregimAddr, regim);
          prregim = 30;
        } else {
          shnekStart=true;
          shnekTimer = millis();
          prregim = 22;
        }
      break;
      case 22: 
        if (shnekTimer+(conf.t_nagrev_shnek*1000L)<millis()){
          shnekStart=false;
          waitshnekTimer = millis();
          prregim = 23;
        }
      break;
      case 23: 
        if (temperVal>conf.temp) { //Если температура богльше то режим поддержания
          regim = 3;
          EEPROM.write(myregimAddr, regim);
          prregim = 30;
        }
        if (waitshnekTimer+(conf.t_shnek_step*1000L)<millis()){
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
          EEPROM.write(myregimAddr, regim);
          prregim = 20;
        } else {
          shnekStart=true;
          shnekTimer = millis();
          prregim = 32;
        }
      break;
      case 32: 
        if (shnekTimer+(conf.t_podderg_shnek*1000L)<millis()){
          shnekStart=false;
          waitshnekTimer = millis();
          prregim = 33;
        }
      break;
      case 33: 
        if (temperVal<conf.temp-conf.gister) { //Если темп меньше гистерезиса то нагрев
          regim = 2;
          EEPROM.write(myregimAddr, regim);
          prregim = 20;
        }
        if (waitshnekTimer+(conf.t_shnek_step*1000L)<millis()){
          prregim = 31;
        }
      break;
  }
}

//Функция проверки не патух ли котел
void flamecheck(){
  if (regim && regim!=1 && regim!=5 && regim!=4 && !flameTimer){
    Serial.println("Chech flame");
    if (flamePersent < conf.t_flame) {
          //Если пламя пропало, то возможно его забросало и оно разгориться, ждём 20 секунд
          flameTimer=millis();
    }
  }
  if (flameTimer && flameTimer+(10*1000L)<millis()){
    if (flamePersent < conf.t_flame){
      //не разожглось, уходим в ошибку
      shnekStart=false;
      prregim = 19;
      regim = 5;
      EEPROM.write(myregimAddr, regim);
      lampaStart=false;
      flameTimer=0;
    } else {
      flameTimer=0;
    }
  }


}


void loop()
{
  
  if (millis() - regimTimer >= 500) {
    regimTimer = millis();
    control();
    flamecheck();
  }


  //блок реле
  if (millis() - releTimer >= 500) {
    releTimer = millis();
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
  }

  //блок кнопки
  bool btnState = !digitalRead(4);
  if (btnState && !bflag && millis() - btnTimer > 100) { //Кнопку нажали
    bflag = true;
    btnTimer = millis();
  } 
  if (!btnState && bflag && millis() - btnTimer > 100) {  //Кнопку отпустили
    bflag = false;
    btnTimer = millis();
    pressputton();
  }

  //Блок вентилятора
  if (millis() - myTimerVent >= 500) {
    myTimerVent=millis();
    if (vspeed != vspeedtemp) {
      vspeed = vspeedtemp;
      analogWrite(vent, 255);
    } else {
      analogWrite(vent, percentToValue(vspeed));
    }
  }
  //Блок дисплея
  if (millis() - myTimerDIsplay >= 500) {
    myTimerDIsplay=millis();
    myOLED.clrScr(); // очищаем экран
    myOLED.setFont(RusFont); // Устанавливаем русский шрифт
    myOLED.print("Ntvgthfnehf", CENTER, 0); // Выводим надпись "Температура"
    myOLED.setFont(MediumNumbers);
    myOLED.print(String(temperVal), CENTER, 9);   // Отображение температуры
    
    myOLED.setFont(RusFont); 
    myOLED.print("Gkfvz", LEFT, 33); // Выводим надпись "Пламя"
    myOLED.setFont(SmallFont);
    myOLED.print(String(flamePersent,1)+"%", LEFT, 43); // вывод текста

    myOLED.setFont(RusFont);
    myOLED.print("Dtyn", RIGHT, 33); // Выводим надпись "Вент"
    myOLED.setFont(SmallFont);
    myOLED.print(String(vspeed)+"%", RIGHT, 43); // вывод текста

    myOLED.setFont(RusFont); 
    String regimStr;
    switch (regim) {
      case 0:
        regimStr = "|J;blfybt|";
      break;
      case 1:
        regimStr = "|Hjp;br "+String(rozhikCount+1)+ "|";
      break;
      case 2:
        regimStr = "|Yfuhtd|";
      break;
      case 3:
        regimStr = "|Gjllth;|";
      break;
      case 4:
        regimStr = "|Ds;bufybt|";
      break;
      case 5:
        regimStr = "|Ji hjp;buf|";
      break;
    
    }
    myOLED.print(regimStr, CENTER, 37); // Выводим надпись "Ожидание"
   
    String status;
    myOLED.setFont(RusFont); 
    switch (prregim) {
      case 10:
        status = "Gjlrblsdfybt";
      break;
      case 11:
        status = "Gjlrblsdfybt "+String(countTimer);
      break;
      case 12:
        status = "Kfvgf";
      break;
      case 13:
        status = "Db;e gkfvz - abrcfwbz";
      break;
      case 14:
        status = "Hfpujhtkjcm!";
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
        status = "192.168.88.16";
    }
    myOLED.print(status, CENTER, 57); // Выводим надпись "Айпишник"


    myOLED.update();
  } 

  //Блок пламени
  if (millis() - myTimerFlame >= 1000) {
    myTimerFlame = millis();
    int flame = analogRead(analogPin);
    flamePersent  = flameGet();
  }
  //Блок температуры
  if (millis() - myTimertemp >= 5000) {
    myTimertemp = millis();
    temperVal = TempGetTepr();
  }

}

int flameGet() {
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