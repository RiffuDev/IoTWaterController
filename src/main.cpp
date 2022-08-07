#include <Arduino.h>
#include <ESP8266WiFi.h>  //lib needed for webserver
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

#include "pageIndex.h"  //html code for webpage
#include "updateIndex.h" //html code for OTA webpage
#include "wifiManager.h"

#include <Adafruit_NeoPixel.h>
#include "LittleFS.h"


// const char* ssidC = "Ashik";
// const char* passwordC = "@nafeela";

#define APSSID "Water_Controller"
#define APPASS "Wiot1234"

String ssid, pass;  // custom credentials from FS

// const char* ssidC = "Azonix_Deco";
// const char* passwordC = "a$ter1sk";

// pins for waterReading
#define pinD5 14
//#define pinD6 12
#define pinD7 13

#define relayPin 5   //relay Pin, Motor Pin
#define buzzerPin 16 //Buzzer pin

#define PIN       2 // On Trinket or Gemma, suggest changing this to 1
#define NUMPIXELS 1 // Popular NeoPixel ring size
//CRGB leds[NUM_LEDS];
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
//inversing the state for we'll be using the relay in NC 
#define rON LOW
#define rOFF HIGH

bool ledState = LOW;
bool fwUpdate = false;

bool dryPin;  //Dry run Pin
bool tnkUPin; //Tank Upper Pin
bool tnkLPin; //Tank Lower Pin

//------------------------func proto
//void ReadPin();

void TurnRelay();
void DryRun();
void BlinkBuzz();
void NeoLed();
void basicfadein(int colr);
void initFS();
void writeFile(const char * path, const char * message);
String readFile(const char * path);
void SendServer(String status);
//-------------------------

unsigned long prevBLmillis = 0;
unsigned long prevBLmillis1 = 0;
unsigned long prevMillisWM = 0;
bool dryMotor = false;
char chipID[7];

AsyncWebServer server(80);   //creating a object for asyncWebserver and setting it into port 80

bool wifiAp = false; // flag to check if the wifi is AP mode.
bool wifiAP(){
    Serial.println("Setting as AP....");
   WiFi.mode(WIFI_AP);
   bool apStatus = WiFi.softAP(APSSID, APPASS);  //default esp ip will be 192.168.4.1
    delay(500);
    // IPAddress IP = WiFi.softAPIP();
    // Serial.print("AP IP address: ");
    // Serial.println(IP); 
    wifiAp = true;
    return apStatus;  //returns true is connected in the AP mode
}


bool initWiFi(){
  if(ssid == "" && pass == ""){
    Serial.println("Undefined SSID....");
    return false;
  }

  WiFi.mode(WIFI_STA); //connecting wifi as Staion if creds found
  Serial.println(ssid); Serial.println(pass);
  delay(500);
  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");
  delay(500);

unsigned long currMillisWM = millis();
  while(WiFi.status() != WL_CONNECTED && (millis() - currMillisWM) < 10000) { //wait for 10 sec to connect to wifi
    delay(500);
    Serial.println("Connecting to WiFi...");
    if(WiFi.status() == WL_CONNECTED) break;
  }
     
      if(WiFi.status() != WL_CONNECTED){ Serial.println("Failed to connect.");
        WiFi.disconnect();  //disconnecting the wifi.begin()
        return false;
      }
  
  Serial.println(WiFi.localIP());
  return true;

}


unsigned long prevtnk =0, lastDebounceTime =0 ;
void ReadPin() {
  dryPin = digitalRead(pinD5);
  //tnkUPin = digitalRead(pinD6);
  //tnkLPin = digitalRead(pinD7);

  if(millis() - prevtnk >= 50){   // using analogread to be accurate in tnkU
  tnkUPin = (analogRead(A0) > 220 ) ? 1 : 0;
  prevtnk = millis();
  }

  //creating a debounce delay here to manage the unwanted noise.

  if (digitalRead(pinD7) == 1) {
    // reset the debouncing timer
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > 5000) {
    
   if(digitalRead(pinD7) != 1){
    tnkLPin = 0;
    Serial.println(tnkLPin);
  }
}else if (digitalRead(pinD7) == 1) tnkLPin = 1;

  // Serial.println(dryPin);
  // Serial.println(tnkUPin);
  // Serial.println(tnkLPin);
  // Serial.println("");
}

String outputState() {   // this fucn return the relayState to the webpage
  if (digitalRead(relayPin)) {
    return "checked";
  }
  else {
    return "";
  }
  return "";
}

String processor(const String& var) {  //this func replaces our variable with PlaceHolder in HTML code
  //Serial.println(var);
  if (var == "BUTTONPLACEHOLDER") {
    String buttons = "";
    String outputStateValue = outputState();
    //ToDo: change its algorith respective to its variable and connect it with dry run protection.
    if (tnkUPin == 0 && tnkLPin == 1 && dryMotor == false) { //the web-button only works when the tank level is betweeen lowPin and highPin and dryRun is not found
      buttons += "<h4>Device State <span id=\"outputState\"></span></h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"output\" " + outputStateValue + "><span class=\"slider\"></span></label>";
    } else {
      buttons += "<h4>Device State <span id=\"outputState\"></span></h4><label class=\"switch\"><input type=\"checkbox\" onchange=\"toggleCheckbox(this)\" id=\"output\" " + outputStateValue + " disabled=\"disabled\"><span class=\"slider\"></span></label><h5> Cant access button </h5>";
      if (dryMotor) buttons += "<h6> Dry Run Detected </h6>";
      if (tnkUPin == 1 && tnkLPin == 1) buttons += "<h6> Tank Full </h6>";
    }
    return buttons;
  }
  return String();
}

String summa() {

String reading =  ("D5  : " + String(dryPin) );
       reading += ("\nD6  : " + String(tnkUPin) );
       reading += ("\nD7  : " + String(tnkLPin) );
       reading += ("\nA0  : " + String( analogRead(A0) ) );
       
       reading += ("\n v2.4");
       
        Serial.println("D5  : " + String(digitalRead(D5)) );
        Serial.println("D6  : " + String(digitalRead(D6)) );
        Serial.println("D7  : " + String(digitalRead(D7)) );
return reading;
}

void wifiM(){
  server.on("/WM", HTTP_GET, [](AsyncWebServerRequest *request){
     request->send_P(200, "text/html", WM_html);
    });

  server.on("/WM", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == "ssid") {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile("/ssid.txt", ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == "pass") {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
             writeFile("/pass.txt", pass.c_str());
          }
          //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }  
    request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + WiFi.localIP().toString() );
      delay(3000);
      ESP.restart();  //resetting the esp to connect to WIFI
  });

}

void routes() { //routes for web page

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", index_html, processor);
  });

  // Send a GET request to <IP>/trigger?state=<inputMessage>
  server.on("/trigger", HTTP_GET, [] (AsyncWebServerRequest * request) {
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
  server.on("/state", HTTP_GET, [] (AsyncWebServerRequest * request) {
    request->send(200, "text/plain", String(digitalRead(relayPin)).c_str());
  });

  server.on("/summa", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", summa().c_str());
  });

  wifiM();  //calling wifi manager routes
}

void otaUpdate() {
  //Send a GET to get OTA update page
  server.on(PSTR("/update"), HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/html", serverIndex);
  });

  server.on(PSTR("/update"), HTTP_POST, [](AsyncWebServerRequest * request) {
    bool shouldReboot = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", shouldReboot ? "OK" : "FAIL");
    if (shouldReboot)
      request->onDisconnect([]() {
      ESP.restart();
    });
    //response->addHeader(F("Connection"), F("close"));
    request->send(response);
  }, [](AsyncWebServerRequest * request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
    //  digitalWrite(buzzerPin, HIGH);  // buzzer turing if update gets started
      Update.runAsync(true);
      //fwUpdate = true;
      pixels.show();
      pixels.setPixelColor(0, pixels.Color(255,20,147)); 
      Serial.println(F("Updating.....") );
      if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {  //starting update with available space
        Serial.print(F("Upload begin error: "));
        Update.printError(Serial);
      }
    }
    if (!Update.hasError()) {
      if (Update.write(data, len) != len) {
        Serial.print(F("Upload begin error: "));
        Update.printError(Serial);
      }
    }
    if (final) {
      if (Update.end(true)) {
        digitalWrite(buzzerPin, LOW); 
        Serial.printf("Update Success: %ud\n", index + len);
      } else {
        Update.printError(Serial);
      }
    }
  });
}

void setup() {
  delay(250);
  Serial.begin(115200);
  pinMode(pinD5, INPUT);  
 // pinMode(pinD6, INPUT);
  pinMode(pinD7, INPUT);

  pinMode(D8, OUTPUT);
  digitalWrite(D8, LOW);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, rOFF);

    pixels.clear(); // Set all pixel colors to 'off'
    pixels.setPixelColor(0, pixels.Color(255, 215, 0)); // GOLD
    pixels.show();  // need to call this everytime for leds to work

  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, HIGH);
  delay(500);
  digitalWrite(buzzerPin, LOW);

  initFS();
  delay(250);
  ssid = readFile("/ssid.txt");
  pass = readFile("/pass.txt");
  delay(250);
  if(!initWiFi()){
    wifiAP();
  }
  // WiFi.begin(ssidC, passwordC);
  //   while (WiFi.status() != WL_CONNECTED) {
  //     delay(1000);
  //     Serial.println("Connecting to WiFi..");
  //   }
  //   // Print device Local IP Address
  //   Serial.println(WiFi.localIP());
  routes(); // initializes the route for GET api request
  server.begin(); //Starts the server.
  otaUpdate();   //initalize OTA update
  pixels.begin();

  sprintf(chipID, "%06X", ESP.getChipId());  //getting the last 6 digits of MAC address
}

unsigned long prevW =0 ,  currW =0;

void loop() {
  ReadPin();
  TurnRelay();
  DryRun();
  NeoLed();

  if(WiFi.status() != WL_CONNECTED && wifiAp == false){ 
      Serial.println("No WiFi found");
      //wifiAP() ;// setting wifi mode as AP when wifi.   
      currW = millis();
  }
  //   unsigned long currSleep;
  //   if(WiFi.status() != WL_CONNECTED){ 
  //     Serial.println("No WiFi found");
  //     currSleep = millis();
  // }

  // if(WiFi.status() != WL_CONNECTED && wifiAp == false){ 

  //   //unsigned long currSleep = millis();
  //  // Serial.println("No WiFi found");
  //   if(millis() - currSleep > 15000){
  //      if(WiFi.status() != WL_CONNECTED){
  //         Serial.println("Turning AP mode");
  //         wifiAP() ;// setting wifi mode as AP when wifi. 
  //   }
    
  // }
  Serial.print("PREVWW: "); Serial.println(prevW);
  Serial.print("CURRRW: "); Serial.println(currW);
    if(currW - millis() > 15000){
            if(WiFi.status() != WL_CONNECTED && !wifiAp){
              Serial.println("Turning AP mode");
              wifiAP() ;// setting wifi mode as AP when wifi. 
              //prevW = currW;
            }
    }

}

int bState = rOFF;
// if water availble = 0 , else : = 1
void TurnRelay() {
  // if (dryPin) { // valPin returns true if water aint available ; i.e : pin = 1
  //   Serial.println("D5 Tank is LOW\n");
  // } else if (!dryPin)
  //   //Serial.println(dryPin);

  //   if (tnkUPin) { // valPin returns true if water aint available ; i.e : pin = 1
  //     Serial.println("D6 Tank is LOW\n");
  //   } else if (!tnkUPin)
  //     //Serial.println(tnkUPin);

  //     if (tnkLPin) { // valPin returns true if water aint available ; i.e : pin = 1
  //       Serial.println("D7 Tank is LOW\n");
  //     } else if (!tnkLPin)
  //       //Serial.println(tnkLPin);

        //Turing Relay on
        if (tnkUPin && tnkLPin) {
          digitalWrite(relayPin, rOFF);
          //Serial.println("Turning Relay OFF..");
        }

   if (tnkUPin == 0 && tnkLPin == 0) {
    //if (tnkUPin == 0 && tnkLPin == 0) {
    digitalWrite(relayPin, rON);
    //Serial.println("Turning ON Relay..");
  }

  //if (tnkUPin == 0 ){
  //   if(tnkLPin == 0){
  //     // if(dryMotor == false){
  //       digitalWrite(relayPin, rON);
  //     //}
  //   }
  // }

bool currState = (digitalRead(relayPin) == rON) ? rON : rOFF;

  if(bState != currState){
    bState = (digitalRead(relayPin) == rON) ? rON : rOFF;
    String status = (digitalRead(relayPin) == rON) ? "ON" : "OFF";
    SendServer(status);
    Serial.println(status);
  }

}

void DryRun() {  //turn of the relay is Dry run is found when the relay is HIGH for specific time
  if ( digitalRead(relayPin) == rON && digitalRead(pinD5) == 0) {
    unsigned long timeout = millis();
    //pinD5 is dryPin..
    Serial.print("waiting");
    while ((millis() - timeout) < 10000) Serial.print(".");  // waits for specific time and checks again is dry is found,
    while (digitalRead(pinD5) == 0) {                                          // doing this to avoid unexpected error in sensor.
      digitalWrite(relayPin, rOFF);
      dryMotor = true;
      digitalWrite(D8, HIGH);
      pixels.show();
      pixels.setPixelColor(0, pixels.Color(255, 0, 0)); 
      BlinkBuzz();    // turns the buzzer ON and OFF to notify the user of dry run detect
      Serial.println("END");
      if (digitalRead(pinD5) != 0) {  // breaking the loop if no dry run is detected
        timeout = millis();
        while ((millis() - timeout) < 5000) Serial.println("waiting for confirmation");  // waiting to confirm
        if (digitalRead(pinD5) != 0) {
          digitalWrite(buzzerPin, LOW);
          dryMotor = false;
          break;
        }
      }
    }
   //} 
  }
}

void BlinkBuzz() {
  if (millis() -  prevBLmillis1 >= 2000) {

    // (ledState == LOW) ? ledState = HIGH : ledState = LOW; //inverting the buzzer to sound in a period
    prevBLmillis1 = millis();
    digitalWrite(buzzerPin, LOW);
  }

  if (millis() -  prevBLmillis >= 10000) {

    // (ledState == LOW) ? ledState = HIGH : ledState = LOW; //inverting the buzzer to sound in a period
    prevBLmillis = millis();
    digitalWrite(buzzerPin, HIGH);
  }

}

unsigned long previousFadein = 0;
unsigned long previousFadeout = 0;
unsigned int fadeInterval = 1000;

void basicfadein(int colr) {
  int j;
  unsigned long currentFade = millis();
  if (currentFade - previousFadein >= fadeInterval) {
    for (j = 355; j > -150; j--) {
       //Serial.print("Jfor1: "); Serial.println(j);  
      //pixels.setPixelColor(0, red, green, j);
        uint32_t fade = pixels.gamma32(pixels.ColorHSV(colr,255, j) );
        pixels.fill(fade);
      pixels.show();
    }
    previousFadein = currentFade; 
 }
  if (currentFade - previousFadeout >= fadeInterval) {  
    for (j = -150; j < 355; j++) {
     //Serial.print("Jfor2: "); Serial.println(j);
      //pixels.setPixelColor(0, red, green, j);
        uint32_t fade = pixels.gamma32(pixels.ColorHSV(colr,255, j) );
       pixels.fill(fade);
      pixels.show();
    }
    previousFadeout = currentFade;
 } 
} 
void NeoLed() {
  //  int i = 0;
  pixels.show();
  //if ( WiFi.status() == WL_CONNECTED) {
    if (!fwUpdate) {
    //   //basicfadein(0, 255); 
    //   basicfadein( (65536*5)/6 );
    //   //Serial.println("Updating.....");
    // } else 
    if (digitalRead(relayPin) == rON) {
     // pixels.setPixelColor(0, pixels.Color(0, 0, 255)); 
      //basicfadein(0, 0);  // need pure blue so passing 0 in other colours
      basicfadein( (65536*2)/3 );
    }else
    (tnkUPin != 0) ?  pixels.setPixelColor(0, pixels.Color(0, 255, 0))  :  pixels.setPixelColor(0, pixels.Color(255, 140, 0)); 
  }
  
}//NeoPixel

//------------------FS-------------------
void initFS() {
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
  }
  else{
    Serial.println("LittleFS mounted successfully");
  }
}

void writeFile(const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);

  File file = LittleFS.open(path, "w");
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if (file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}

String readFile(const char * path){
  Serial.printf("Reading file: %s\r\n", path);

  File file = LittleFS.open(path, "r");
  if(!file){
    Serial.println("Failed to open file for reading");
    return String();
  }

  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;     
  }
  file.close();
  return fileContent;
}
//----------FS--------

// void SendServer(String status, bool stop){
//   //for(; stop; stop= false){
//   if(millis() - timesumma >= 30000){
//     timesumma = millis();
//     if (WiFi.status() == WL_CONNECTED) {

//     char chipID[7];
//       sprintf(chipID, "%06X", ESP.getChipId());  //getting the last 6 digits of MAC address
//       // Serial.print("Chip ID: ");
//       // Serial.println(chipID);

//     //creating a string of JSON with required data
//       const String QUOTE = "\"" ;
//       String infoJson = "{\n" ;

//         infoJson += QUOTE + "Device ID" + QUOTE + ": " + QUOTE + chipID + QUOTE + "," + '\n';
//         infoJson += QUOTE + "Status" + QUOTE + ": " + QUOTE + status + QUOTE + '\n';
//         infoJson += "}" ;

//         WiFiClientSecure client;
//         HTTPClient http;

//         //use SHA1 fingerprint method to enable SSL encryption
//         client.setInsecure();  // since the data we're sending isnt confidential or something that need certificate enabled security 
        
//         http.begin(client, "https://patrickbateman.ddns.net/Wiot/script/wiot.php");
//         http.addHeader("Content-Type", "application/json");

//         int httpResponseCode = http.PUT(infoJson);
//         if (httpResponseCode > 0) {
//           String response = http.getString();
//           Serial.println(httpResponseCode);
//           Serial.println(response);

//         } else {
//           Serial.print("Error on sending PUT Request: ");
//           Serial.println(httpResponseCode);
//         }

//         http.end();

//       } else {
//         Serial.println("Error in WiFi connection");
//       }
//   }
// }


void SendServer(String status) {
 
  if ((WiFi.status() == WL_CONNECTED)) {

    //     //creating a string of JSON with required data
    const String QUOTE = "\"" ;
    String infoJson = "{\n" ;

      infoJson += QUOTE + "Device ID" + QUOTE + ": " + QUOTE + chipID + QUOTE + "," + '\n';
      infoJson += QUOTE + "Status" + QUOTE + ": " + QUOTE + status + QUOTE + '\n';
      infoJson += "}" ;

    WiFiClient client;
    HTTPClient http;

    // configure traged server and url
    //http.begin(client, "http://3.20.1.193/Wiot/script/wiot.php"); //HTTP
    bool httpInit = http.begin(client, "http://3.20.1.193/Wiot/script/wiot.php"); //HTTP
    //http.setConnectTimeout(4000);
    http.setTimeout(4000);
    if (!httpInit)
    {
        Serial.print(F("http begin Failed!!"));
    }else{

      http.addHeader("Content-Type", "application/json");

      Serial.print("[HTTP] PUT...\n");
      // start connection and send HTTP header and body
      int httpCode = http.PUT(infoJson);

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] PUT... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK) {
          const String& payload = http.getString();
          Serial.println("received payload:\n<<");
          Serial.println(payload);
          Serial.println(">>");
        }
      } else {
        Serial.printf("[HTTP] PUT... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }

      http.end();
    }
  
  }

 //} //delay(500);
}
//}