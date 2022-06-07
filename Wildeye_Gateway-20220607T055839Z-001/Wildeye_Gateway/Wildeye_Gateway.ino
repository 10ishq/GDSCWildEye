#include <RF24.h>
#include <SPI.h>
#include "DHT.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h> 

#define DHTPIN 16
#define MQ A0
#define DHTTYPE DHT11

DHT dht(5, DHTTYPE);
RF24 radio(2, 15);               // nRF24L01 (CE,CSN)

const char* ssid = "REPLACE_WITH_YOUR_SSID";
const char* password = "REPLACE_WITH_YOUR_PASSWORD";
const char* serverName = "Server_name/firebase";
const byte addresses[][6] = {"00001", "00002"};
int temp, smoke, alert;
String Id;

struct DataGate{             //Data pack for transmission
  byte fire;
  String DeviceId;
};
struct DataNode{             //Data pack for transmission
  byte fire;
  String DeviceId;
};

DataNode dataN;               //Object of Data pack
DataGate dataG;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  SPI.begin();
  radio.begin();
  radio.openWritingPipe(addresses[0]); // 00001
  radio.openReadingPipe(1, addresses[1]); // 00002
  radio.setPALevel(RF24_PA_MIN);
  radio.startListening();
  dht.begin();
  dataG.DeviceId = "GATE001";

}

void Post(){
  
  if(WiFi.status()== WL_CONNECTED){
      WiFiClient client;
      HTTPClient http;
      
      // Your Domain name with URL path or IP address with path
      http.begin(client, serverName);
     
      // If you need an HTTP request with a content type: application/json, use the following:
      http.addHeader("Content-Type", "application/json");
      // JSON data to send with HTTP POST
      String httpRequestData = "{\"wildid\":\"" + Id + "\",\"Fire-alert\":\"" + String(alert) + "\"}";           
      // Send HTTP POST request
      int httpResponseCode = http.POST(httpRequestData);
     
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
        
      // Free resources
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
}

void loop() {

  temp = dht.readTemperature();
  smoke = analogRead(MQ);
  Serial.print("Temprature: ");
  Serial.print(temp);
  Serial.print(" || Smoke: ");
  Serial.println(smoke);

  if (radio.available()) {
    radio.read(&dataN, sizeof(DataNode));
    Serial.print("Temperature: ");
    Serial.println(dataN.fire);
    }

  if(dataN.fire == 1 || dataG.fire == 1){
    alert = 1;
    if(dataG.fire == 1){
      Id = dataG.DeviceId;
    }
    else{
      Id = dataN.DeviceId;
    }
    Post();
  }

  else{
    alert = 0;
  }

  delay(10000);
}
