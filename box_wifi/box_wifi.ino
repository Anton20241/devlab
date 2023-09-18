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

const String boxID= "asdfv";

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
bool start      = false;

//RGB////
#define B 4
#define G 33
#define R 32

const uint8_t red[3] =    {255,0,0};    //Индикация ошибок
const uint8_t green[3] =  {0,255,0};    //100-50% charge
const uint8_t yellow[3] = {255,255,0};  //50-25% charge
const uint8_t orange[3] = {255,128,0};  //25-10% charge

const uint8_t off[3] =    {0,0,0};
const uint8_t rgb_on[3] = {255,255,255};
const uint8_t blue[3] =   {0,128,255};
const uint8_t purple[3] = {255,0,255};

//WiFi////
const char* ssid              = "Keeneti-0154";
const char* password          = "uhBkDPUJ";
const char* serverName        = "http://185.241.68.155:8001/send_data";
const char* ssid_reserve      = "POCO M3";
const char* password_reserve  = "0987654321";

void setup(void){
  Serial.begin(115200);
  while (!Serial) delay(10); 
  Serial.println("loh");

  btnPWD1.setType(LOW_PULL);
  
  pinMode(R, OUTPUT);
  pinMode(G, OUTPUT);
  pinMode(B, OUTPUT);
  RGB_write(rgb_on);
  
  RTC.begin();
  RTC.settimeUnix(1111111);


  // SD card
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
  
  // nfc
  nfc.begin();
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.println("Didn't find PN53x board");
    while (! versiondata){
      RGB_error();
      delay(500);
    }
    RGB_write(rgb_on);
  }
  Serial.println("PN53x board");

  //config setup
  
  delay(1000);
  RGB_write(off);
}

void loop(void) {
  btnPWD1.tick();

  if (btnPWD1.isSingle() && start) {
    Serial.println("btnPWD1.isSingle() && start");
    start = false;
  } else if (btnPWD1.isSingle() && !start) {
    Serial.println("btnPWD1.isSingle() && !start");
    if(readNFC()) sendDataToWIFI();
  }

  if (btnPWD1.isDouble()) {
    Serial.println("btnPWD1.isDouble()");
    show_charge(get_voltage(1), 50, 25, 10);
  }

  if (btnPWD1.isTriple()) {
    Serial.println("btnPWD1.isTriple()");
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 1);
    start = true;
    esp_light_sleep_start();
  }
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

void sendDataToWIFI(){

  int failSendCount = 0;

  // Wifi settings
  WiFiMulti wifiMulti;
  wifi_setup(wifiMulti);

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
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 500);

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

void wifi_setup(WiFiMulti &wifiMulti) {
  wifiMulti.addAP(ssid, password);
  wifiMulti.addAP(ssid_reserve, password_reserve);

  Serial.println("Connecting Wifi...");
  wifiMulti.run();
  Serial.print("WiFi.status() = ");
  Serial.println(WiFi.status());
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

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
