#include <SPI.h>
#include <Ethernet2.h>


// Структура для хранения настроек
struct SETTINGS {
  int t_rozhik_shnek;
  // ... (остальные поля структуры)
  int t_vizh;
};

SETTINGS settings;

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
    processSettingsUpdate(request);

    // Отправляем ответ клиенту
    sendSettingsPage(ethClient);
    
    // Закрываем соединение
    ethClient.stop();
    Serial.println("Client disconnected");
  }
}

void processSettingsUpdate(String request) {
  // Парсим запрос и обновляем значения в структуре
  // Например, можно использовать функцию sscanf()
  if (request.indexOf("t_rozhik_shnek=") != -1) {
    sscanf(request.c_str(), "GET /?t_rozhik_shnek=%d", &settings.t_rozhik_shnek);
  }
  // Повторяем для остальных параметров
}

void sendSettingsPage(EthernetClient& ethClient) {
  // Отправляем HTTP-заголовок
  ethClient.println("HTTP/1.1 200 OK");
  ethClient.println("Content-Type: text/html");
  ethClient.println();

  // Отправляем HTML-страницу с формой для изменения значений
  ethClient.println("<html><head>");
  ethClient.println("<style>body,html{margin:0;padding:0;width:100%;height:100%;overflow:hidden}");
  ethClient.println("iframe{width:100%;height:100%;border:none}</style></head><body>");
  ethClient.println("<iframe src='http://192.168.99.252/arduino/apg25/-/raw/main/index.html' frameborder='0'>");
  ethClient.println("</iframe></body></html>");

}
