#include <RF24.h>
//#include <RF24Network.h>
#include <SPI.h>
#include "DHT.h"

#define DHTPIN 1
#define MQ A0
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
RF24 radio(2, 4);               // nRF24L01 (CE,CSN)
const byte addresses[][6] = {"00001", "00002"};
int temp, smoke;

struct DataPackage{             //Data pack for transmission
  byte fire;
  byte hum;
};

DataPackage data;               //Object of Data pack

void setup() {
  Serial.begin(115200); 
  SPI.begin();
  radio.begin();
  radio.openWritingPipe(addresses[1]); // 00002
  radio.openReadingPipe(1, addresses[0]); // 00001
  radio.setPALevel(RF24_PA_MIN);
  radio.stopListening();
  dht.begin();

}

void loop() {

  temp = dht.readTemperature();
  smoke = analogRead(MQ);
  Serial.print("Temprature: ");
  Serial.print(temp);
  Serial.print(" || Smoke: ");
  Serial.println(smoke);

  if(temp >= 40 || smoke >= 600){
    data.fire = 1;
    radio.write(&data, sizeof(DataPackage));
  }
  else{
    data.fire = 20;
    radio.write(&data, sizeof(DataPackage));
  }
  delay(10000);

}
