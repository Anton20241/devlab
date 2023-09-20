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

unsigned long activeTime = 0;

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

//GSM
#define RXD2 16
#define TXD2 17




void setup(void){
  Serial.begin(115200);
  while (!Serial) delay(10); 
  
  btnPWD1.setType(LOW_PULL);
  
  pinMode(R, OUTPUT);
  pinMode(G, OUTPUT);
  pinMode(B, OUTPUT);
  RGB_write(rgb_on);
  
  // RTC setup
  RTC.begin();
  RTC.settimeUnix(1111111);

  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  
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

  // sim card setup
  int simCardFail = 0;
  String RespCodeStr = "";
  do{
    if (simCardFail > 3){
      RGB_error();
    }
    Serial2.println("AT+CSTT=\"internet.mts.ru\",\"mts\",\"mts\"");// Get IMEI
    updateSerial();
    Serial2.println("AT+CIICR");
    updateSerial();
    if (simCardFail > 3){
      RGB_error();
    }
    Serial2.println("AT+CREG?");
    delay(1500);
    RespCodeStr = Serial2.readStringUntil('\n');
    RespCodeStr = Serial2.readStringUntil('\n');
    Serial.print("RespCodeStr = ");
    Serial.println(RespCodeStr);
    simCardFail++;
  } while (!(RespCodeStr.indexOf("+CREG") >= 0));

  RGB_write(off);
  activeTime = millis();
}

void loop(void) {

  btnPWD1.tick();

  unsigned long currentTime = millis();
  if (currentTime - activeTime > 60000){
    activeTime = millis();
    Serial.println("SLEEP");
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_2, 1);
    esp_light_sleep_start();
  }

  if (btnPWD1.isSingle()) {
    activeTime = millis();
    Serial.println("btnPWD1.isSingle()");
    if(readNFC()) sendDataToGSM();
  }

  if (btnPWD1.isDouble()) {
    activeTime = millis();
    Serial.println("btnPWD1.isDouble()");
    show_charge(get_voltage(1), 50, 25, 10);
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


void updateSerial()
{
  delay(1500);
  while(Serial2.available()) {
    Serial.write(Serial2.read());//Data received by Serial2 will be outputted by Serial}
  }
}

bool sendToGSM(String data, bool ledOn){
  Serial2.println("AT+HTTPINIT"); //The basic adhere network command of Internet connection
  updateSerial();
  Serial2.println("AT+HTTPPARA=\"CID\",\"1\"");//Set PDP parameter
  updateSerial();
  Serial2.println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");//Activate PDP; Internet connection is available after successful PDP activation
  updateSerial();
  Serial2.println("AT+HTTPPARA=\"URL\",\"http://185.241.68.155:8001/send_data\"");//Get local IP address
  updateSerial();
  Serial2.println("AT+HTTPDATA");// Connect to the server then the server will send back former data
  updateSerial();
  Serial2.println(data);// Send data request to the server
  updateSerial();
  Serial2.write(26);// Terminator
  updateSerial();
  Serial2.println("AT+HTTPACTION=1");// Send data request to the server
  delay(1500);
  String RespCodeStr = Serial2.readStringUntil('\n');
  RespCodeStr = Serial2.readStringUntil('\n');
  RespCodeStr = Serial2.readStringUntil('\n');
  Serial2.println("AT+HTTPTERM");// Send data request to the server
  updateSerial();

  Serial.print("data = ");
  Serial.println(data);
  Serial.print("RespCodeStr = ");
  Serial.println(RespCodeStr);
  Serial.println("lohhh");

  if (!RespCodeStr.isEmpty() && RespCodeStr.indexOf("200") > 0){
    Serial.println("here1");
    Serial.println(RespCodeStr.indexOf("200"));
    if (ledOn){
      RGB_success();
      RGB_success();
    }
    return true;
  } else {
    Serial.println("here2");
    Serial.println("error gsm connection");
    if (ledOn){
      RGB_error();
      RGB_error();
    }
    return false;
  }
}

void sendDataToGSM(){

  int failSendCount = 0;








  

  if(!sendToGSM(result, 0)){
    sendDataToSD("/id.txt", result);
    failSendCount++;
    Serial.print("failSendCount = ");
    Serial.println(failSendCount);
    
    return;
  }

  // Send data from SD to wifi
  File myFile = SD.open("/id.txt", FILE_READ);
  if (myFile){
    Serial.println("/id.txt:");
    while (myFile.available()){
      String buffer = myFile.readStringUntil('\n');      // Считываем с карты весь дотекст в строку до 
                                                          // символа окончания + перевод каретки (без удаления строки)
      if(!sendToGSM(buffer, 0)){
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
  delay(300);
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
