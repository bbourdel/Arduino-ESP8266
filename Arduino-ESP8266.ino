
//definitions for jumpwire.io
#define jumpwire_io_token      "Set_your_token"
#define jumpwire_io_project    "A"
#define your_WiFi_SSID         "Set_your_ssid"
#define your_WiFi_password     "Set_your_password"
#define your_ESP8266_baud_rate 115200

//put your definitions here

//put your global variables here

void setup() {
  jumpwireIoSetup(); //always put this code in setup()
  //put your code here
  
}


void loop() {
  jumpwireIoLoop(); //always put this code in loop()
  //put your code here
  
}


void Catch(char key, float value) {
  //this function is called when byte values on jumpwire.io server are changed
  //put your code here
  
}
