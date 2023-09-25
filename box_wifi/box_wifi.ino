#include "FS.h"
#include "SD.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include "GyverButton.h"
#include "iarduino_RTC.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>

#include <Arduino.h>
#ifdef ESP32
  #include <WiFi.h>
  #include <AsyncTCP.h>
#else
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>

//RGB////
#define B 4
#define G 33
#define R 32

const uint8_t red[3]    = {255,0,0};    //Индикация ошибок
const uint8_t green[3]  = {0,255,0};    //100-50% charge
const uint8_t yellow[3] = {255,255,0};  //50-25% charge
const uint8_t orange[3] = {255,128,0};  //25-10% charge
const uint8_t off[3]    = {0,0,0};
const uint8_t rgb_on[3] = {255,255,255};
const uint8_t blue[3]   = {0,0,255};
const uint8_t purple[3] = {255,0,255};

AsyncWebServer server(80);
const char* LOGIN_1     = "login1";
const char* PASSWORD_1  = "password1";
const char* LOGIN_2     = "login2";
const char* PASSWORD_2  = "password2";
const char* LOGIN_3     = "login3";
const char* PASSWORD_3  = "password3";

// HTML web page to handle 3 input fields (input1, input2, input3, input4, input5, input6)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <title>ESP Input Form</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head><body>
  <form action="/get">
    Wifi 1<br>
    login: <input type="text" name="login1"> password: <input type="text" name="password1"><br><br>
    Wifi 2<br>
    login: <input type="text" name="login2"> password: <input type="text" name="password2"><br><br>
    Wifi 3<br>
    login: <input type="text" name="login3"> password: <input type="text" name="password3"><br><br>
    <input type="submit" value="OK">
  </form>
</body></html>)rawliteral";

const char index_html_success[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
  <meta charset="UTF-8">
  <title>Success ESP Init</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  </head>
  <body>
  Успешное завершение настройки. Перезагрузите устройство. 
</body></html>)rawliteral";

String inputMessage = "";
bool userInput = false;

void server_setup(){
  // Send web page with input fields to client
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
    
    inputMessage = "";

    inputMessage = request->getParam(LOGIN_1)->value();
    inputMessage += ";";
    inputMessage += request->getParam(PASSWORD_1)->value();
    inputMessage += "\n";

    inputMessage += request->getParam(LOGIN_2)->value();
    inputMessage += ";";
    inputMessage += request->getParam(PASSWORD_2)->value();
    inputMessage += "\n";

    inputMessage += request->getParam(LOGIN_3)->value();
    inputMessage += ";";
    inputMessage += request->getParam(PASSWORD_3)->value();
    inputMessage += "\n";

    Serial.println("inputMessage = ");
    Serial.println(inputMessage);

    String inputParam;
    request->send_P(200, "text/html", index_html_success);
    userInput = true;
  });
  server.onNotFound(notFound);
  server.begin();
  Serial.println("Server started");
}

bool setConfigFile(){
  while (!userInput){
    RGB_write(yellow);
  }
  RGB_write(off);
  userInput = false;
  Serial.println("\nOpen config file to write...");
  File cfg_file = SD.open("/config.txt", FILE_WRITE);

  if (!cfg_file) {
    Serial.println("\nCAN'T OPEN CONFIG FILE !");
    return false;
  }
  
  cfg_file.println(inputMessage);
  cfg_file.close();
  Serial.println("\nSUCCESS setup config file...");
  return true;
}

unsigned long activeTime = 0;

//WiFi////
String boxID                    = "asdfv";
String serverName               = "http://185.241.68.155:8001/send_data";
const char *esp32_wifi_ssid     = "cleaning box";
const char *esp32_wifi_password = "cleaningbox";

//acum//
#define BAT_CHARGE 34

//SPI//
#define SCK   (18)
#define MOSI  (23)
#define MISO  (19)
#define SD_SS (5)
SPIClass SPI_1(VSPI);

//PN532////
#define PN532_IRQ   (50)
#define PN532_RESET (51)  // Not connected by default on the NFC Shield
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

//btn////
#define BTN_PWD1 2
GButton btnPWD1(BTN_PWD1);

//RTC////
iarduino_RTC RTC(RTC_DS1302, 27, 25, 26);

//data///
char newChar    = 'i';
String result   = "";
String tagData  = "";

void setup(void){

  Serial.begin(115200);
  while (!Serial) delay(10); 

  btnPWD1.setType(LOW_PULL);
  btnPWD1.setTimeout(15000);
  
  pinMode(R, OUTPUT);
  pinMode(G, OUTPUT);
  pinMode(B, OUTPUT);
  RGB_write(rgb_on);

  // RTC setup
  RTC.begin();
  RTC.settimeUnix(1111111);

 // SD card setup
 if(!SD.begin(SD_SS)){
   Serial.println("Card Mount Failed");
   while (!SD.begin(SD_SS)) {
     RGB_error();
     delay(500);
   }
   RGB_write(rgb_on);
 }
 Serial.println("SD Card Mounted");

 if (!SD.exists("/id.txt")) {
   File myFile = SD.open("/id.txt", FILE_WRITE);
   myFile.close();
 }
  
 // nfc setup
 if (!nfc.begin()) {
   Serial.println("Didn't find PN53x board");
   while (!nfc.begin()){
     RGB_error();
     delay(500);
   }
   RGB_write(rgb_on);
 }
 Serial.println("PN53x board");

  //config setup
  if (!config_found()) doHardReset();

  delay(1000);
  RGB_write(off);
  Serial.println("SUCCESS BOX SETUP");
  activeTime = millis();
}

void checkTimeForSleeping(){
  unsigned long currentTime = millis();
  if (currentTime - activeTime > 60000){
    activeTime = millis();
    Serial.println("SLEEP");
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 1);
    esp_light_sleep_start();
  }
}

void checkSingleClick(){
  if (btnPWD1.isSingle()) {
    activeTime = millis();
    Serial.println("btnPWD1.isSingle()");
    if(readNFC()) sendDataToWIFI();
  }
}

void checkDoubleClick(){
  if (btnPWD1.isDouble()) {
    activeTime = millis();
    Serial.println("btnPWD1.isDouble()");
    show_charge(get_voltage(1), 50, 25, 10);
  }
}

void checkLongClick(){
  if (btnPWD1.isHolded()){
    RGB_write(blue);
    Serial.println("PRESS BUTTON 3 TIMES TO hard reset");
    unsigned long currentTime = millis();
    bool configIsChanged = false;
    while (millis() - currentTime < 10000){
      btnPWD1.tick();
      if (btnPWD1.isTriple()) {
        configIsChanged = true;
        doHardReset();
      }
    }
    RGB_write(off);
    if (!configIsChanged) Serial.println("DO NOT HARD RESET");
  }
}

void loop(void) {
  btnPWD1.tick();
  checkTimeForSleeping();
  checkSingleClick();
  checkDoubleClick();
  checkLongClick();
}

void doHardReset(){
  if(SD.exists("/config.txt")){
    Serial.println("\nDelete config file ...");
    SD.remove("/config.txt");
  }
  RGB_write(purple);
  startAccessPoint();
  if (setConfigFile()) Serial.println("SUCCESS BOX SETUP");
}

void notFound(AsyncWebServerRequest *request) {
  request->send(404, "text/plain", "Not found");
}

void network_config(WiFiMulti &wifiMulti){  // Poco m3;0987654321\nwifi2id;wifi2pas;
  Serial.println("\nOpen config file to read...");
  File cfg_file = SD.open("/config.txt");

  while (cfg_file.available()) {
    String cfg_str = cfg_file.readStringUntil('\n');
    int space_index = cfg_str.indexOf(";");
    String ssid = cfg_str.substring(0,space_index);
    String password = cfg_str.substring(space_index + 1);
    const char * c_ssid = ssid.c_str();
    const char * c_password = password.c_str();
    Serial.println(ssid);
    Serial.println(password);
    wifiMulti.addAP(c_ssid, c_password);
  }
  cfg_file.close();
}

bool config_found() {
  if(!SD.exists("/config.txt")) {
    Serial.println("\nCONFIG FILE IS NOT FOUND!");
    return false;
  }
  return true;
}

void startAccessPoint(){
  Serial.println();
  Serial.println("Configuring access point...");
  
  if (!WiFi.softAP(esp32_wifi_ssid, esp32_wifi_password)) {
    Serial.println("ESP32 soft AP creation failed.");
    while (!WiFi.softAP(esp32_wifi_ssid, esp32_wifi_password)){
      RGB_error();
      delay(500);
    }
    RGB_write(rgb_on);
  }

  Serial.println("ESP32 soft AP creation SUCCESS");
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  server_setup();
}

void sendDataToSD(String fileName, String data){
  File myFile = SD.open(fileName, FILE_APPEND);
  if (myFile) {
    myFile.println(data);
    Serial.println(data + " send to " + fileName);
  }
  else {
    Serial.println("Can`t open file " + fileName);
  }
  myFile.close();
}

void renameFile(){
  File myFile = SD.open("/id.txt", FILE_WRITE);
  File myFile2 = SD.open("/id2.txt");
  while (myFile2.available()){
    String buffer = myFile2.readStringUntil('\n');
    buffer.trim();
    myFile.println(buffer);
  }
  SD.remove("/id2.txt");
  myFile.close();
  myFile2.close();
}

bool sendToWifi(HTTPClient &http, String data, bool ledOn){
  int httpResponseCode = http.POST(data);
  Serial.println(data);
  Serial.println(httpResponseCode);
  if (httpResponseCode == 200){
    if (ledOn){
      RGB_success();
      RGB_success();
    }
    return true;
  } else {
    Serial.println("error wifi sending");
    if (ledOn){
      RGB_error();
      RGB_error();
    }
    return false;
  }
}

void wifi_connecting(WiFiMulti &wifiMulti) {
  Serial.println("Connecting Wifi...");
  wifiMulti.run();
  Serial.print("WiFi.status() = ");
  Serial.println(WiFi.status());
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
}

void sendDataToWIFI(){

  int failSendCount = 0;

  // Wifi settings
  WiFiMulti wifiMulti;
  network_config(wifiMulti); //пишем все ssid и пароли из конфига
  wifi_connecting(wifiMulti);//и подключаемся

  WiFiClient client;
  HTTPClient http;
  http.begin(client, serverName);                       // Your Domain name with URL path or IP address with path
  http.addHeader("Content-Type", "application/json");   // Specify content-type header

  if(!sendToWifi(http, result, 0)){
    sendDataToSD("/id.txt", result);
    failSendCount++;
    Serial.print("failSendCount = ");
    Serial.println(failSendCount);
    http.end();
    return;
  }

  // Send data from SD to wifi
  File myFile = SD.open("/id.txt", FILE_READ);
  if (myFile){
    Serial.println("/id.txt:");
    while (myFile.available()){
      String buffer = myFile.readStringUntil('\n');      // Считываем с карты весь дотекст в строку до 
                                                          // символа окончания + перевод каретки (без удаления строки)
      if(!sendToWifi(http, buffer, 0)){
        buffer.trim();
        sendDataToSD("/id2.txt", buffer);
        failSendCount++;
      }
    }
  } else {
    Serial.println("error opening /id.txt");
  }
  SD.remove("/id.txt");
  myFile.close();
  renameFile();
  Serial.print("failSendCount = ");
  Serial.println(failSendCount);
  http.end();
}

bool readNFC(){
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  // Wait for an NTAG203 card.  When one is found 'uid' will be populated with
  // the UID, and uidLength will indicate the size of the UUID (normally 7)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 3000);

  if (success) {
    // Display some basic information about the card
    if (uidLength == 7) {
      uint8_t data[32];
      for (uint8_t i = 7; i < 10; i++) { // starting serial output at Page 7 and stop reading at Page 11
        success = nfc.ntag2xx_ReadPage(i, data);
        // Display the results, depending on 'success'
        if (success) {
          printHexCharAsOneLine(data,4);
        }
        else {
          Serial.println("Unable to read the requested page!");
          RGB_error();
          return false;
        }
      }
      result = "{\"box_id\":\"";
      result += boxID;
      result += "\", \"mark_id\":\"";
      result += tagData.substring(2,11);
      result += "\", \"event_time\":\"";
      result += String(RTC.gettimeUnix());
      result += "\"}";
      // {"box_id":"asdfv", "mark_id":"444444444", "event_time":"123123"}
      Serial.println(result);
      RGB_success();
      return true;
    }
    else {
      Serial.println("This doesn't seem to be an NTAG203 tag (UUID length != 7 bytes)!");
      RGB_error();
      return false;
    }  
  }
  else {
    Serial.println("cant read tag");
    RGB_error();
    return false;
  }
}



void printHexCharAsOneLine(const byte *data, const uint32_t numBytes) {
  uint32_t szPos;
  for (szPos = 0; szPos < numBytes; szPos++) {
    newChar = ((char)data[szPos]); //makes the character into a variable
    tagData += newChar; //adds that character to the result string
  }
}

void RGB_write(byte r_val, byte g_val, byte b_val){
  analogWrite(R, r_val);
  analogWrite(G, g_val);
  analogWrite(B, b_val);
}

void RGB_write(const uint8_t* color) {
  analogWrite(R, color[0]);
  analogWrite(G, color[1]);
  analogWrite(B, color[2]);
}

void RGB_error(){
  RGB_write(red);
  delay(1000);
  RGB_write(off);
}

void RGB_success(){
  RGB_write(green);
  delay(300);
  RGB_write(off);
  delay(300);
  RGB_write(green);
  delay(300);
  RGB_write(off);
}

int get_voltage(bool debug_mode) {
  int voltage = analogRead(BAT_CHARGE);
  if(debug_mode) {
    Serial.print("VOLTAGE:");
    Serial.println(voltage);
  }
  return voltage;
}

/*Показывает заряд батарейки, зажигая светодиод в соответствии с границами, соответствующими 50,25,10 процентам */
void show_charge(int voltage, const int bnd_50, const int bnd_25, const int bnd_10) {
  if (voltage < bnd_10) {
    RGB_write(red);
  }
  else if (voltage < bnd_25) {
    RGB_write(orange);
  }
  else if (voltage < bnd_50) {
    RGB_write(yellow);
  }
  else RGB_write(green);
  delay(1000);
  RGB_write(off);
}
