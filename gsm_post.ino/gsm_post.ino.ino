
void updateSerial()
{
  delay(2000);
  while (Serial.available()) {
    Serial2.write(Serial.read());//Data received by Serial will be outputted by Serial2
  }
  while(Serial2.available()) {
    Serial.write(Serial2.read());//Data received by Serial2 will be outputted by Serial}
}
}

#define RXD2 16
#define TXD2 17

void setup()
{
  Serial.begin(115200);
  Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
  Serial.write("setup");
}

void loop()
{
  Serial.write("loop");
  Serial2.println("AT+CSTT=\"internet.mts.ru\",\"mts\",\"mts\"");// Get IMEI
  updateSerial();
  Serial2.println("AT+CIICR");
  updateSerial();
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
  Serial2.println("{\"box_id\":\"asdfv\", \"mark_id\":\"444444444\", \"event_time\":\"123123\"}");// Send data request to the server
  updateSerial();
  Serial2.write(26);// Terminator
  updateSerial();
  Serial2.println("AT+HTTPACTION=1");// Send data request to the server
  updateSerial();
  Serial2.println("AT+HTTPTERM");// Send data request to the server
  updateSerial();
  while(1)
    {
    if(Serial2.available())
    {
      Serial.write(Serial2.read());//Data received by Serial2 will be outputted by Serial
    }
    if(Serial.available())
    {
      Serial2.write(Serial.read());//Data received by Serial will be outputted by Serial2
    }
}
}
