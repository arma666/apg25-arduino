#include <SPI.h>
#include <Ethernet2.h>
#include <EEPROM.h>
//temp vars
int regim=1;
bool shnekStart = true;
bool lampaStart = false;
int vspeed = 0; //скорость вентилятора в процентах
float flamePersent = 0; //Глобальная пламя
float temperVal = 20; // Глобальная температура
////////////
int settingsAddress = 1;

// Структура для хранения настроек
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

SETTINGS defaultSettings  = {
  100, //Время подкидывания при розжиге 
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
// MAC-адрес вашего Arduino
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x29, 0xED};

// Создаем сервер на порту 80
EthernetServer server(80);

// Настройки HTTP-клиента для загрузки страницы с удаленного сервера
char serverAddress[] = "your-remote-server";
int serverPort = 80;
EthernetClient client;

void setup() {
  // Запускаем сериал для мониторинга через Serial Monitor
  Serial.begin(9600);

   EEPROM.get(settingsAddress, conf);
    // Проверка, есть ли данные в EEPROM
  bool hasData = true;
  for (size_t i = 0; i < sizeof(SETTINGS); i++) {
    if (EEPROM.read(settingsAddress + i) == 255) { // Проверка на значение 255, что соответствует непроинициализированным данным в EEPROM
      hasData = false;
      break;
    }
  }

  // Если данных нет в EEPROM, используйте дефолтные значения
  if (!hasData) {
    conf = defaultSettings;
    EEPROM.put(settingsAddress, conf);
    Serial.println("Default settings written to EEPROM");
  } else {
    Serial.println("Settings read from EEPROM:");
  }
  

  Serial.println(conf.t_rozhik_shnek);

  // Запускаем Ethernet с указанным MAC-адресом
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // Некоторые дополнительные действия при ошибке
    for(;;);
  }
  Serial.print("My IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print(".");
  }
  // Начинаем слушать порт 80
  server.begin();

  Serial.println("Server started");
}

void loop() {
  // Прослушиваем подключения
  EthernetClient ethClient = server.available();
  
  if (ethClient) {
    Serial.println("New client");
    // Ждем, пока клиент подключится и отправит запрос
    while (!ethClient.available()) {
      delay(1);
    }
    
    // Читаем первую строку запроса
    String request = ethClient.readStringUntil('\r');
    Serial.println(request);
    ethClient.flush();
    
    // Обработка запроса изменения значений структуры
    if (processSettingsUpdate(request)){
      if (sendparams(ethClient,request)){
        if (sendstate(ethClient,request)){
          if (webpressbutton(ethClient,request)){
            sendSettingsPage(ethClient);
          }
        }
      }
    } else {
      ethClient.println("HTTP/1.1 200 OK");
    }
    
    // Закрываем соединение
    ethClient.stop();
    Serial.println("Client disconnected");
  }
}

//Функция нажатия кнопки --------------->
void pressputton(){
  Serial.println("PressButoon");
  if (regim == 0 ){
    regim = 1;
  } else if (regim == 4 || regim == 5) {
    regim = 0;
    shnekStart=false;
    lampaStart=false;

  }
  else {
    int tregim = regim;
    regim = 4;
    lampaStart=false;
    shnekStart=false;

  }


}

//Нажатие на кнопку
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
    ethClient.println("HTTP/1.1 200 OK");
    ethClient.println("Content-Type: text/JSON");
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
    ethClient.println("HTTP/1.1 200 OK");
    ethClient.println("Content-Type: text/JSON");
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

  if (keyString == "t_rozhik_shnek") {
    conf.t_rozhik_shnek = val;
  } else if (keyString == "t_nagrev_shnek") {
    conf.t_nagrev_shnek = val;
  } else if (keyString == "t_podderg_shnek") {
    conf.t_podderg_shnek = val;
  } else if (keyString == "t_shnek_step") {
    conf.t_shnek_step = val;
  } else if (keyString == "t_rozhik") {
    conf.t_rozhik = val;
  } else if (keyString == "flame_fix") {
    conf.flame_fix = val;
  } else if (keyString == "t_flame") {
    conf.t_flame = val;
  } else if (keyString == "vent_rozhik") {
    conf.vent_rozhik = val;
  } else if (keyString == "vent_nagrev") {
    conf.vent_nagrev = val;
  } else if (keyString == "vent_podderg") {
    conf.vent_podderg = val;
  } else if (keyString == "vent_ogidanie") {
    conf.vent_ogidanie = val;
  } else if (keyString == "temp") {
    conf.temp = val;
  } else if (keyString == "gister") {
    conf.gister = val;
  } else if (keyString == "t_vizh") {
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
    EEPROM.put(settingsAddress, conf);
    return false;
  } else {
    return true;
  }
}


//Отправляем стартовую страницу
void sendSettingsPage(EthernetClient& ethClient) {
  // Отправляем HTTP-заголовок
  ethClient.println("HTTP/1.1 200 OK");
  ethClient.println("Content-Type: text/html");
  ethClient.println();

  // Отправляем HTML-страницу с формой для изменения значений
  ethClient.println("<html><head>");
  ethClient.println("<meta name='viewport' content='width=device-width, initial-scale=1.0'>");
  ethClient.println("<style>body,html,iframe{width:100%;height:100%}body,html{margin:0;padding:0;overflow:hidden}iframe{border:none}</style>");
  ethClient.println("</head><body><iframe id='i'></iframe><script>(async function l(url, id) {");
  ethClient.println("document.getElementById('i').srcdoc = await (await fetch('https://raw.githubusercontent.com/arma666/apg25-arduino/main/html/loaded.html')).text();");
  ethClient.println("})()</script></body></html>");
}
