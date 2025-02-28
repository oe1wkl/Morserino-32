#ifndef MORSEWIFI_H_
#define MORSEWIFI_H_

/******************************************************************************************************************************
 *  morse_3 Software for the Morserino-32 multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module ***
 *  Copyright (C) 2018-2025  Willi Kraml, OE1WKL                                                                                 ***
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

#define MORSERINOPORT 7373
extern void checkShutDown(boolean enforce);

namespace MorseWiFi
{
    ////////////////// Variables for file handling and WiFi functions

    extern WebServer server;
    //extern WiFiUDP udp;
    extern AsyncUDP audp;

    extern File fsUploadFile;              // a File object to temporarily store the received file

    extern const char* host;               // hostname of the webserver

    /// WiFi constants
    extern const char* ssid;
    extern const char* password;

    // HTML for the AP server - ued to get SSID and Password for local WiFi network - needed for file upload and OTA SW updates
    extern const char* myForm;

    /*
     * HTML for Upload Login page
     */

    extern const char* uploadLoginIndex;
    extern const char* updateLoginIndex;
    extern const char* serverIndex;

    void menuExec(uint8_t command);         // called from main menu, with argument from menu
    void menuNetSelect();                   // display menu of wifi networks to select from
    void startAP();
    boolean wifiConnect();
    void uploadFile();
    void updateFirmware();
    String getContentType(String filename); // convert the file extension to the MIME type
    bool handleFileRead(String path);       // send the right file to the client (if it exists)
    void handleFileUpload();                // upload a new file to the SPIFFS
}

#endif /* MORSEWIFI_H_ */