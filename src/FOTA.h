#include "ESPAsyncWebServer.h"
class OtaClass{

public: 

    bool getStatus(){
        if(_upSta == true) return true;
        else return false;
    }

    void begin(AsyncWebServer *server, const char* username, const char* password){
         _server = server;
        if(strlen(username) > 0){
            _authRequired = true;
            _username = username;
            _password = password;
        }else{
            _authRequired = false;
            _username = "";
            _password = "";
        }
     _server->on(PSTR("/update"), HTTP_GET, [](AsyncWebServerRequest * request) {
      if(_authRequired){
        if(!request->authenticate(_username.c_str(), _password.c_str())){
            return request->requestAuthentication();
        }
      }
        request->send_P(200, "text/html", serverIndex);
    });
    _server->on(PSTR("/update"), HTTP_POST, [](AsyncWebServerRequest * request) {
      if(_authRequired){
        if(!request->authenticate(_username.c_str(), _password.c_str())){
            return request->requestAuthentication();
        }
      }
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
            Update.runAsync(true);
            if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
                Update.printError(Serial);
            }
        }
        if (!Update.hasError()) {
            if (Update.write(data, len) != len) {
                Update.printError(Serial);
            }
        }
        if (final) {
            if (Update.end(true)) {
                Serial.printf("Update Success: %uB\n", index+len);
            } else {
                Update.printError(Serial);
            }
        }
    });
 }

 private:
 AsyncWebServer *_server;
  String _username = "";
  String _password = "";
  bool _upSta = false;
  bool _authRequired = false;

};

OtaClass Ota;