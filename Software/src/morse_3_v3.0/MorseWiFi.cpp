/******************************************************************************************************************************
 *  morse_3 Software for the Morserino-32 multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module ***
 *  Copyright (C) 2018-2020  Willi Kraml, OE1WKL                                                                                 ***
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with this program.
 *  If not, see <https://www.gnu.org/licenses/>.
 *****************************************************************************************************************************/
 
#include "morsedefs.h"
#include "MorseWiFi.h"
#include "MorseOutput.h"
#include "MorsePreferences.h"

////////////////// Variables for file handling and WiFi functions

// File file;

WebServer MorseWiFi::server(80);    // Create a webserver object that listens for HTTP request on port 80
//WiFiUDP   MorseWiFi::udp;           // Create udp socket for wifi tx
AsyncUDP MorseWiFi::audp;           // Create async udp socket for wifi rx

File MorseWiFi::fsUploadFile;              // a File object to temporarily store the received file

const char* MorseWiFi::host = "m32";               // hostname of the webserver


/// WiFi constants
const char* MorseWiFi::ssid = "morserino";
const char* MorseWiFi::password = "";


// HTML for the AP server - used to get SSID and Password for local WiFi network - needed for file upload and OTA SW updates
const char* MorseWiFi::myForm = "<html><head><meta charset='utf-8'><title>Get AP Info</title><style> form {width: 420px;}div { margin-bottom: 20px;}"
                "label {display: inline-block; width: 240px; text-align: right; padding-right: 10px;} button, input {float: right;}</style>"
                "</head>"
                "<body>"
                  "<form action='/set' method='get'>"
                    "<div>"   // fields for network 1
                      "<div>"
                        "<label for='ssid1'>SSID of WiFi network 1?</label>"
                        "<input name='ssid1' id='ssid1' value='SSIDV1'>"
                      "</div>"
                      "<div>"
                        "<label for='pw1'>WiFi Password 1?</label>"
                        "<input name='pw1' id='pw1'>"
                      "</div>"
                      "<div>"
                        "<label for='trxpeer1'>WiFi TRX Peer IP/Host 1?</label>" 
                        "<input name='trxpeer1' id='trxpeer1' value='PEERIPV1'>"
                      "</div>"
                    "</div>"
                  
                    "<div>"   // fields for network 2
                      "<div>"
                        "<label for='ssid2'>SSID of WiFi network 2?</label>"
                        "<input name='ssid2' id='ssid2' value='SSIDV2'>"
                      "</div>"
                      "<div>"
                        "<label for='pw2'>WiFi Password 2?</label>"
                        "<input name='pw2' id='pw2'>"
                      "</div>"
                      "<div>"
                        "<label for='trxpeer2'>WiFi TRX Peer IP/Host 2?</label>" 
                        "<input name='trxpeer2' id='trxpeer2' value='PEERIPV2'>"
                      "</div>"
                    "</div>"
                  
                    "<div>"   // fields for network 3
                      "<div>"
                        "<label for='ssid3'>SSID of WiFi network 3?</label>"
                        "<input name='ssid3' id='ssid3' value='SSIDV3'>"
                      "</div>"
                      "<div>"
                        "<label for='pw3'>WiFi Password 3?</label>"
                        "<input name='pw3' id='pw3'>"
                      "</div>"
                      "<div>"
                        "<label for='trxpeer3'>WiFi TRX Peer IP/Host 3?</label>" 
                        "<input name='trxpeer3' id='trxpeer3' value='PEERIPV3'>"
                      "</div>"
                    "</div>"
                  
                    "<div>"
                      "(255.255.255.255 = Local Broadcast IP will be used as Peer if empty)"
                    "</div>"
                    "<div>"
                      "<button>Submit</button>"
                    "</div>"
                  "</form>"
                "</body>"
              "</html>";


/*
 * HTML for Upload Login page
 */

const char* MorseWiFi::uploadLoginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>M32 File Upload - Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='m32' && form.pwd.value=='upload')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";


const char* MorseWiFi::updateLoginIndex = 
 "<form name='loginForm'>"
    "<table width='20%' bgcolor='A09F9F' align='center'>"
        "<tr>"
            "<td colspan=2>"
                "<center><font size=4><b>M32 Firmware Update Login Page</b></font></center>"
                "<br>"
            "</td>"
            "<br>"
            "<br>"
        "</tr>"
        "<td>Username:</td>"
        "<td><input type='text' size=25 name='userid'><br></td>"
        "</tr>"
        "<br>"
        "<br>"
        "<tr>"
            "<td>Password:</td>"
            "<td><input type='Password' size=25 name='pwd'><br></td>"
            "<br>"
            "<br>"
        "</tr>"
        "<tr>"
            "<td><input type='submit' onclick='check(this.form)' value='Login'></td>"
        "</tr>"
    "</table>"
"</form>"
"<script>"
    "function check(form)"
    "{"
    "if(form.userid.value=='m32' && form.pwd.value=='update')"
    "{"
    "window.open('/serverIndex')"
    "}"
    "else"
    "{"
    " alert('Error Password or Username')/*displays error message*/"
    "}"
    "}"
"</script>";


const char* MorseWiFi::serverIndex = 
"<script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>"
"<form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>"
   "<input type='file' name='update'>"
        "<input type='submit' value='Begin'>"
    "</form>"
 "<div id='prg'>Progress: 0%</div>"
 "<script>"
  "$('form').submit(function(e){"
  "e.preventDefault();"
  "var form = $('#upload_form')[0];"
  "var data = new FormData(form);"
  " $.ajax({"
  "url: '/update',"
  "type: 'POST',"
  "data: data,"
  "contentType: false,"
  "processData:false,"
  "xhr: function() {"
  "var xhr = new window.XMLHttpRequest();"
  "xhr.upload.addEventListener('progress', function(evt) {"
  "if (evt.lengthComputable) {"
  "var per = evt.loaded / evt.total;"
  "$('#prg').html('progress: ' + Math.round(per*100) + '%');"
  "}"
  "}, false);"
  "return xhr;"
  "},"
  "success:function(d, s) {"
  "console.log('success!')" 
 "},"
 "error: function (a, b, c) {"
 "}"
 "});"
 "});"
 "</script>";

namespace internal 
{
    String getContentType(String filename); // convert the file extension to the MIME type
    boolean handleFileRead(String path);       // send the right file to the client (if it exists)
    void handleFileUpload();                // upload a new file to the SPIFFS
    void handleNotFound();
    void startMDNS();
    boolean errorConnect(String msg);
}



void internal::handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += MorseWiFi::server.uri();
  message += "\nMethod: ";
  message += (MorseWiFi::server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += MorseWiFi::server.args();
  message += "\n";
  for (uint8_t i = 0; i < MorseWiFi::server.args(); i++) {
    message += " " + MorseWiFi::server.argName(i) + ": " + MorseWiFi::server.arg(i) + "\n";
  }
  MorseWiFi::server.send(404, "text/plain", message);
}

void MorseWiFi::menuNetSelect() {
  const int numNetworks = 3;
  String names[numNetworks];
  names[0] = "1: " + MorsePreferences::wlanSSID1;
  names[1] = "2: " + MorsePreferences::wlanSSID2;
  names[2] = "3: " + MorsePreferences::wlanSSID3;

  MorseOutput::clearDisplay();
  MorseOutput::printOnStatusLine( true, 0,  "Select Wifi");

  int btnClicks;
  int choice = 0;
  int previousChoice = -1;
  while (true) {
    checkShutDown(false);  // possibly time-out: go to sleep
    if(choice!=previousChoice) {
      MorseOutput::clearThreeLines();
      MorseOutput::printOnScroll(0, REGULAR, 0, names[choice] );
      previousChoice = choice;
    }
    switch(checkEncoder()){
      case -1:
        if(choice == 0) {
          choice = numNetworks;
        }
        choice--;
        break;
      case 1:
        choice++;
        if(choice >= numNetworks){
          choice = 0;
        }
        break;
    }
    Buttons::modeButton.Update();
    btnClicks = Buttons::modeButton.clicks;
    if(btnClicks == 1)
      break;
    if(btnClicks == -1){
      choice = -1;
      break;
    }
  }

  switch(choice) {
    case 0: MorsePreferences::writeWifiInfo(MorsePreferences::wlanSSID1, MorsePreferences::wlanPassword1, MorsePreferences::wlanTRXPeer1);
      break;
    case 1: MorsePreferences::writeWifiInfo(MorsePreferences::wlanSSID2, MorsePreferences::wlanPassword2, MorsePreferences::wlanTRXPeer2);
      break;
    case 2: MorsePreferences::writeWifiInfo(MorsePreferences::wlanSSID3, MorsePreferences::wlanPassword3, MorsePreferences::wlanTRXPeer3);
      break;
    case -1: // double click pressed to exit menu
      break;
  }
  
}

void MorseWiFi::menuExec(uint8_t command) {
  switch (command) {
    case  _wifi_mac:
                  WiFi.mode(WIFI_MODE_STA);               // init WiFi as client
                  MorseOutput::clearDisplay();
                  MorseOutput::printOnStatusLine( true, 0,  WiFi.macAddress());
                  delay(2000);
                  MorseOutput::printOnScroll(0, REGULAR, 0, "RED: restart" );
                  delay(1000);  
                  while (true) {
                    checkShutDown(false);  // possibly time-out: go to sleep
                    if (digitalRead(volButtonPin) == LOW)
                      ESP.restart();
                  }
                  break;
        case  _wifi_config:
                  MorseWiFi::startAP();          // run as AP to get WiFi credentials from user
                  break;
        case _wifi_check:
                  MorseOutput::clearDisplay();
                  MorseOutput::printOnStatusLine( true, 0,  "Connecting... ");
                  if (! MorseWiFi::wifiConnect())
                      ; //return false;  
                  else {
                      MorseOutput::printOnStatusLine( true, 0,  "Connected!    ");
                      MorseOutput::printOnScroll(0, REGULAR, 0, MorsePreferences::wlanSSID);
                      MorseOutput::printOnScroll(1, REGULAR, 0, WiFi.localIP().toString());
                  }
                  //WiFi.mode( WIFI_MODE_NULL ); // switch off WiFi                      
                  delay(1000);
                  MorseOutput::printOnScroll(2, REGULAR, 0, "RED: return" );
                  while (true) {
                        checkShutDown(false);  // possibly time-out: go to sleep
                        if (digitalRead(volButtonPin) == LOW) {
                          WiFi.disconnect(true,false);
                          return;
                        }
                  }
                  break;
        case _wifi_upload:
                  MorseWiFi::uploadFile();       // upload a text file
                  break;
        case _wifi_update:
                  MorseWiFi::updateFirmware();   // run OTA update
                  break;
  }
}

void MorseWiFi::startAP() {

  WiFi.mode(WIFI_AP);
  WiFi.setHostname(ssid);
  WiFi.softAP(ssid);
  MorseOutput::clearDisplay();
  MorseOutput::printOnStatusLine( true, 0,    "Enter Wifi Info @");
  MorseOutput::printOnScroll(0, REGULAR, 0,  "AP: morserino");
  MorseOutput::printOnScroll(1, REGULAR, 0,  "URL: m32.local");
  MorseOutput::printOnScroll(2, REGULAR, 0,  "RED to abort");
  internal::startMDNS();
  
  server.on("/", HTTP_GET, []() { 
    String formular;
    formular.reserve(1024);
    formular = myForm;

    formular.replace("SSIDV1", MorsePreferences::wlanSSID1);
    formular.replace("PEERIPV1", MorsePreferences::wlanTRXPeer1); 

    formular.replace("SSIDV2", MorsePreferences::wlanSSID2);
    formular.replace("PEERIPV2", MorsePreferences::wlanTRXPeer2); 

    formular.replace("SSIDV3", MorsePreferences::wlanSSID3);
    formular.replace("PEERIPV3", MorsePreferences::wlanTRXPeer3); 

    server.sendHeader("Connection", "close");
    server.send(200, "text/html", formular);
  });
  
  server.on("/set", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", "Wifi Info updated - now restarting Morserino-32...");

    MorsePreferences::writeWifiInfoMultiple(
      String(server.arg("ssid1")), String(server.arg("pw1")), String(server.arg("trxpeer1")),
      String(server.arg("ssid2")), String(server.arg("pw2")), String(server.arg("trxpeer2")),
      String(server.arg("ssid3")), String(server.arg("pw3")), String(server.arg("trxpeer3"))
    );   

    //DEBUG("SSID: " + String(MorsePreferences::wlanSSID) + " Password: " + String(MorsePreferences::wlanPassword) + " Peer. " + String(MorsePreferences::wlanTRXPeer));
    ESP.restart();
  });
  
  server.onNotFound(internal::handleNotFound);
  
  server.begin();
  while (true) {
      server.handleClient();
      delay(20);
      Buttons::volButton.Update();
      if (Buttons::volButton.clicks) {
        MorseOutput::clearDisplay();
        MorseOutput::printOnStatusLine( true, 0, "Resetting now...");
        delay(2000);
        ESP.restart();
      }
  }
}


void MorseWiFi::updateFirmware()   {                   /// start wifi client, web server and upload new binary from a local computer
  if (! wifiConnect())
    return;
 
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", updateLoginIndex);
  });
  
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });
  
  /*handling uploading firmware file */
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { //start with max available size
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      /* flashing firmware to ESP*/
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) { //true to set the size to the current progress
        DEBUG("Update Success: Rebooting...\n");
      } else {
        Update.printError(Serial);
      }
    }
  });
  //DEBUG("Starting web server");
  server.begin();
  MorseOutput::clearDisplay();
  MorseOutput::printOnStatusLine( true, 0, "Waiting f. Update ");
  MorseOutput::printOnScroll(0, REGULAR, 0,  "URL: m32.local");
  MorseOutput::printOnScroll(1, REGULAR, 0,  "IP:");
  MorseOutput::printOnScroll(2, REGULAR, 0, WiFi.localIP().toString());
  while(true) {
    server.handleClient();
    delay(10);
  }
}

  
boolean MorseWiFi::wifiConnect() {                   // connect to local WLAN
  // Connect to WiFi network
  if (MorsePreferences::wlanSSID == "") 
      return internal::errorConnect(String("WiFi Not Conf"));

  WiFi.begin(MorsePreferences::wlanSSID.c_str(), MorsePreferences::wlanPassword.c_str());
  // Wait for connection
  long unsigned int wait = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    if ((millis() - wait) > 10000)
      return internal::errorConnect(String("No WiFi:"));
  }
  //DEBUG("Connected to " + String(p_wlanSSID));
  //DEBUG("IP address: " + String(WiFi.localIP());
  internal::startMDNS();
  return true;
}


boolean internal::errorConnect(String msg) {
  MorseOutput::clearDisplay();
  MorseOutput::printOnStatusLine( true, 0, "Not connected");
  MorseOutput::printOnScroll(0, INVERSE_BOLD, 0, msg);
  MorseOutput::printOnScroll(1, REGULAR, 0, MorsePreferences::wlanSSID);
  delay(3500);
  return false;
}

void internal::startMDNS() {
  /*use mdns for host name resolution*/
  if (!MDNS.begin(MorseWiFi::host)) {         //http://m32.local
    DEBUG("Error setting up MDNS responder!");
    while (1) {
      delay(1000);
      if (MDNS.begin(MorseWiFi::host))
        break;
    }
  }
  //DEBUG("mDNS responder started");
}


void MorseWiFi::uploadFile() {
  if (! wifiConnect())
    return;
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", uploadLoginIndex);
  });
  server.on("/serverIndex", HTTP_GET, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/html", serverIndex);
  });

server.on("/update", HTTP_POST, [](){                  // if the client posts to the upload page
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", "OK");
    ESP.restart();},                                  // Send status 200 (OK) to tell the client we are ready to receive; when done, restart the ESP32
    internal::handleFileUpload                                    // Receive and save the file
  );
  
  server.onNotFound([]() {                              // If the client requests any URI
    if (!internal::handleFileRead(server.uri()))                  // send it if it exists
      server.send(404, "text/plain", "404: Not Found"); // otherwise, respond with a 404 (Not Found) error
  });

  server.begin();                           // Actually start the server
  //DEBUG("HTTP server started");
  MorseOutput::clearDisplay();
  MorseOutput::printOnStatusLine( true, 0, "Waiting f. Upload ");
  MorseOutput::printOnScroll(0, REGULAR, 0,  "URL: m32.local");
  MorseOutput::printOnScroll(1, REGULAR, 0,  "IP:");
  MorseOutput::printOnScroll(2, REGULAR, 0, WiFi.localIP().toString());  
  while(true) {
    server.handleClient();
    //delay(5);
  }
}


String internal::getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}


bool internal::handleFileRead(String path) { // send the right file to the client (if it exists)
  //DEBUG("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";          // If a folder is requested, send the index file
  String contentType = internal::getContentType(path);             // Get the MIME type
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)) {     // If the file exists, either as a compressed archive, or normal
    if (SPIFFS.exists(pathWithGz))                            // If there's a compressed version available
      path += ".gz";                                          // Use the compressed verion
    File file = SPIFFS.open(path, "r");                       // Open the file
    MorseWiFi::server.streamFile(file, contentType);                     // Send it to the client
    file.close();                                             // Close the file again
    DEBUG(String("\tSent file: ") + path);
    return true;
  }
  //DEBUG(String("\tFile Not Found: ") + path);   // If the file doesn't exist, return false
  return false;
}

void internal::handleFileUpload(){ // upload a new file to the SPIFFS
  HTTPUpload& upload = MorseWiFi::server.upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    //DEBUG("handleFileUpload Name: " + filename);
    MorseWiFi::fsUploadFile = SPIFFS.open("/player.txt", "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(MorseWiFi::fsUploadFile)
      MorseWiFi::fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if(upload.status == UPLOAD_FILE_END){
    if(MorseWiFi::fsUploadFile) {                                    // If the file was successfully created
      MorseWiFi::fsUploadFile.close();                               // Close the file again
      MorsePreferences::fileWordPointer = 0;                              // reset word counter for file player
DEBUG("fileWordPointer @ file upload: " + String(MorsePreferences::fileWordPointer));
      MorsePreferences::writeWordPointer();
      //DEBUG("handleFileUpload Size: " + String(upload.totalSize));
      //server.sendHeader("Location","/success.html");      // Redirect the client to the success page
      //server.send(303);
    } else {
      MorseWiFi::server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}
