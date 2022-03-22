#include <Arduino.h>
#include <ESP8266WiFi.h>  //lib needed for webserver
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "pageIndex.h"  //html code for webpage

const char* ssid = "Ashik 1";
const char* password = "@rumaysa";  

// pins for waterReading
#define pinD5 14
#define pinD6 12
#define pinD7 13

#define relayPin 5   //relay Pin, Motor Pin
#define buzzerPin 16 //Buzzer pin
bool ledState = LOW;

bool dryPin;  //Dry run Pin 
bool tnkUPin; //Tank Upper Pin  
bool tnkLPin; //Tank Lower Pin

//func proto
void ReadPin();
void TurnRelay();
void DryRun();
void BlinkBuzz();

unsigned long prevBLmillis = 0;   
unsigned long prevBLmillis1 = 0;

AsyncWebServer server(80);   //creating a object for asyncWebserver and setting it into port 80

String outputState(){    // this fucn return the relayState to the webpage
  if(digitalRead(relayPin)){
    return "checked";
  }
  else {
    return "";
  }
  return "";
}

String processor(const String& var){   //this func replaces our variable with PlaceHolder in HTML code
  //Serial.println(var);
  if(var == "BUTTONPLACEHOLDER"){
    String buttons ="";
    String outputStateValue = outputState();
    //change its algorith respective to its variable and connect it with dry run protection.
    if (digitalRead(tnkUPin) == 0){ //the buuton on the page gets disabled when the tank is full.
    buttons+= "<h4>Output - GPIO 5 - State <span id=\"outputState\"></span></h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"output\" " + outputStateValue + " disabled=\"disabled\"><span class=\"slider\"></span></label><h5> Cant access button </h5>";
    }else {buttons+= "<h4>Output - GPIO 5 - State <span id=\"outputState\"></span></h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"output\" " + outputStateValue + "><span class=\"slider\"></span></label>";
          }
    return buttons;
  }
  return String();
}

void routes(){  //routes for web page

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  // Send a GET request to <IP>/update?state=<inputMessage>
  server.on("/update", HTTP_GET, [] (AsyncWebServerRequest *request) {
    String inputMessage;
    if (request->hasParam("state")) {    //gets the argument parameters of "state" from client browser
      inputMessage = request->getParam("state")->value();
      digitalWrite(relayPin, inputMessage.toInt()); //turns relay respective to the arg.
    }
    else {
      inputMessage = "No message sent";
    }
    Serial.println(inputMessage);
    request->send(200, "text/plain", "OK");
  });

  // Send a GET request to get updated in webpage 
  server.on("/state", HTTP_GET, [] (AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(digitalRead(relayPin)).c_str());
  });
}

void setup(){

  Serial.begin(115200);
  pinMode(pinD5, INPUT_PULLUP);  // using internal pullResistor of esp to measure DigitaRead without floating issue.
  pinMode(pinD6, INPUT_PULLUP);
  pinMode(pinD7, INPUT_PULLUP);

  pinMode(relayPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  
    // Connecting to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  // Print device Local IP Address
  Serial.println(WiFi.localIP());
 
 routes(); // initializes the route for GET api request
 server.begin(); //Starts the server.
}

void loop(){
  ReadPin();
  TurnRelay();
  DryRun();
 //delay(1000);
}

void ReadPin(){
  dryPin = digitalRead(pinD5);
  tnkUPin = digitalRead(pinD6);
  tnkLPin = digitalRead(pinD7);

  // Serial.println(dryPin);
  // Serial.println(tnkUPin);
  // Serial.println(tnkLPin);
  // Serial.println("");
}
 // if water availble = 0 , else : = 1
void TurnRelay(){
  if(dryPin){   // valPin returns true if water aint available ; i.e : pin = 1
    Serial.println("D5 Tank is LOW\n");
  }else if(!dryPin)
  Serial.println(dryPin);

  if(tnkUPin){   // valPin returns true if water aint available ; i.e : pin = 1
    Serial.println("D6 Tank is LOW\n");
  }else if(!tnkUPin)
  Serial.println(tnkUPin);

    if(tnkLPin){   // valPin returns true if water aint available ; i.e : pin = 1
    Serial.println("D7 Tank is LOW\n");
  }else if(!tnkLPin)
  Serial.println(tnkLPin);

//Turing Relay on
    if (!tnkUPin && !tnkLPin){
      digitalWrite(relayPin, LOW);
      Serial.println("Turning Relay OFF..");
    }

    if(tnkLPin){
      digitalWrite(relayPin, HIGH);
      Serial.println("Turning ON Relay..");
    }

}

void DryRun(){   //turn of the relay is Dry run is found when the relay is HIGH for specific time
  if ( digitalRead(relayPin) == HIGH && dryPin == 1){
    unsigned long timeout = millis();

    while ((millis() - timeout) < 10000) Serial.println("waiting");  // waits for specific time and checks again is dry is found,
        while (dryPin == 1){                                           // doing this to avoid unexpected error in sensor.
         digitalWrite(relayPin, LOW);
         BlinkBuzz();    // turns the buzzer ON and OFF to notify the user of dry run detect
         Serial.println("END");
            if (digitalRead(pinD5) != 1) {  // breaking the loop if no dry run is detected
              while ((millis() - timeout) < 5000) Serial.println("waiting for confirmation");  // waiting to confirm 
                if (digitalRead(pinD5) != 1) {
                    digitalWrite(buzzerPin, LOW);
                    break; 
                }
            }
        }
    }
}

void BlinkBuzz(){
    if (millis() -  prevBLmillis1 >= 1000){
   
   // (ledState == LOW) ? ledState = HIGH : ledState = LOW; //inverting the buzzer to sound in a period
    prevBLmillis1 = millis();
    digitalWrite(buzzerPin, LOW);
  }

  if (millis() -  prevBLmillis >= 10000){
   
   // (ledState == LOW) ? ledState = HIGH : ledState = LOW; //inverting the buzzer to sound in a period
    prevBLmillis = millis();
    digitalWrite(buzzerPin, HIGH);
  }

}