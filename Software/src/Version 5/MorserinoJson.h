#include "MorsePreferences.h"

void jsonDevice() ;
void jsonError(String errormessage) ;
void jsonOK() ;
void jsonMenu(String path, unsigned int number, boolean active, boolean exec) ;
void jsonMenuList() ;
void jsonParameter(String token) ;
void jsonParameterList() ;
void jsonGetKoch() ;
void jsonConfigLong(MorsePreferences::parameter p) ;
void jsonConfigShort(String item, int value, String displayed) ;
void jsonCreate(String objName, String path, String state) ;
void jsonActivate(actMessage active) ;
void jsonControl(String item, uint8_t value, uint8_t mini, uint8_t maxi, boolean detailed) ;
void jsonControls() ;
void jsonSnapshots() ;
void jsonFileStats() ;
void jsonFileFirstLine() ;
void jsonFileText() ;
void jsonGetWifi() ;
void jsonGetCwStores() ;
void jsonGetCwStore(String value) ;