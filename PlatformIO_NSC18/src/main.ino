#include <Wire.h>
#include "LiquidCrystal_I2C.h"
#include <SPI.h>
#include "RTClib.h"
#include "DHT.h"
#include <ESP8266WiFi.h>
#include <Esp.h>
#include <EEPROM.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <string.h>

// #define DEBUG_CODE
#define DEBUG_PRINTER Serial

#ifdef DEBUG_CODE

  #define DEBUG_PRINT(...) { DEBUG_PRINTER.print(__VA_ARGS__); }
  #define DEBUG_PRINTLN(...) { DEBUG_PRINTER.println(__VA_ARGS__); }

#else

  #define DEBUG_PRINT(...) {}
  #define DEBUG_PRINTLN(...) {}

#endif

#define DHTPIN              D4
#define DHTTYPE             DHT22

#define ACTIVE_RELEY        D0
#define PIN_SOIL            A0

#define BTN_LEFT            D5
#define BTN_RIGHT           D6
#define BTN_CENTER          D7
#define BTN_BACK            D8

#define I2C_SCL             D1
#define I2C_SDA             D2

char boot_mode[20];         // if setting don't forgot remove.
char directory[20];         // directory in current
char select_current[30]     = "Select timer"; // first select

struct Setting_timer
{
  int eeprom_addr_h         = 100;
  int eeprom_addr_after_h   = 101;
  int eeprom_addr_m         = 102;
  int eeprom_addr_after_m   = 103;

  int reboot_time           = 6; // second
  int move_right            = 0;
  int hour                  = 0;
  int minute                = 0;
  int after_hour            = 0;
  int after_minute          = 0;
  char display_h[2];
  char display_after_h[2];
  char display_m[2];
  char display_after_m[2];
  char display_reboot[2];

}setting_t;

int addr_eeprom_wifi        = 0;
int addr_length_ssid        = 256;
int addr_length_pass        = 257;
int addr_mode_auto          = 300;

int check_mode_auto         = 0;
/* Dht22  */
int dht_counting_fail       = 0;

/* detect button */
char run_left[20];
char run_right[20];
char run_center[20];
char run_back[20];

const char* db_host         = "192.168.0.3";
const char* db_url			= "/php/insert_sensor.php?id=0&";
int host_port        		= 80;

/*=== WiFiAccessPoint ===*/
const char* ssidAP          = "NSC18-Primary";
const char* ssidPass        = "";
const char* ap_ip[4]        = {"192", "168", "0", "100"};
const char* ap_subnet[4]    = {"255", "255", "255", "0"};
const char* ap_gateway[4]   = {"192", "168", "0", "1"};

int state_internet          = 0;
int counting_connect        = 0;

/*=======================*/

char memory_rx[1];
int read_rx                  = 0;

EspClass Esp;

WiFiClient client;
ESP8266WebServer server(80);
DHT dht(DHTPIN, DHTTYPE);
RTC_DS3231 RTC;
LiquidCrystal_I2C lcd(0x27, 20, 4);

unsigned long previousMillis  = 0;
unsigned long currentMillis;
const long    interval        = 2000;

/* ICON */
byte ICON_TIME[8]       {B11111,B11001,B11001,B01110,B01110,B10011,B10011,B11111};
byte ICON_TEMP[8]       {B00100,B01010,B01010,B01110,B01110,B11111,B11111,B01110};
byte ICON_HUMID[8]      {B00100,B00100,B01010,B01010,B10001,B10001,B10001,B01110};
byte ICON_SELECT[8]     {B00100,B00110,B00111,B11111,B11111,B00111,B00110,B00100};
byte ICON_SOIL[8]       {B00000,B00000,B00000,B11111,B10001,B11111,B11111,B11111};
byte ICON_CONNECT[8]    {B11111,B00000,B01110,B00000,B01110,B00100,B00100,B00100};

void wifi_ap() {

  WiFi.softAP(ssidAP);

  WiFi.softAPConfig(
    IPAddress(atoi(ap_ip[0]), atoi(ap_ip[1]), atoi(ap_ip[2]), atoi(ap_ip[3])),
    IPAddress(atoi(ap_gateway[0]), atoi(ap_gateway[1]), atoi(ap_gateway[2]), atoi(ap_gateway[3])),
    IPAddress(atoi(ap_subnet[0]), atoi(ap_subnet[1]), atoi(ap_subnet[2]), atoi(ap_subnet[3]))
  );

  DEBUG_PRINTLN();
  DEBUG_PRINTLN();
  DEBUG_PRINT("WiFi AP : ");
  DEBUG_PRINT(ssidAP);
  DEBUG_PRINTLN();


  IPAddress myIP = WiFi.softAPIP();


  DEBUG_PRINT("IP Address : ");
  DEBUG_PRINTLN(myIP);

}

void saveWiFi(const char *ssid, const char *pass) {

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);

    DEBUG_PRINT(".");

  }


  DEBUG_PRINT("IP address : ");
  DEBUG_PRINTLN(WiFi.localIP());
  DEBUG_PRINT("eeprom ssid : ");


  for (int i = 0; i < strlen(ssid); i++) {

    EEPROM.write(addr_eeprom_wifi, ssid[i]);
    addr_eeprom_wifi++;

    DEBUG_PRINT(ssid[i]);

  }


  DEBUG_PRINTLN();
  DEBUG_PRINT("eeprom pass : ");

  for (int j = 0; j < strlen(pass); j++) {

    EEPROM.write(addr_eeprom_wifi, pass[j]);

    addr_eeprom_wifi++;

    DEBUG_PRINT(pass[j]);

  }

  EEPROM.write(addr_length_ssid, strlen(ssid));
  EEPROM.write(addr_length_pass, strlen(pass));
  EEPROM.commit();
  Esp.reset();

}

void webserver_display() {

  String codeHtml = "\
  <html>\
    <head>\
      <meta name='viewport' content='initial-scale=1.0, user-scalable=no'>\
      <title>WiFi Config</title>\
    </head>\
    <body>\
      <form method='get'>\
        SSID&ensp;\
        <input type='text' name='SSID'>\
        </select><br><br>\
        PASS&ensp;<input type='text' name='password'><br><br>\
        <input type='submit' value='Connect'>\
      </form>\
    </body>\
  </html>";

  server.send(200, "text/html", codeHtml);


  if (server.arg(0) != "" && server.arg(1) != "") {

    char http_ssid[20];
    char http_pass[20];

    strcpy(http_ssid, server.arg(0).c_str());
    strcpy(http_pass, server.arg(1).c_str());

    saveWiFi(http_ssid, http_pass);

  }

}

void webserver_config() {

  server.on("/", webserver_display);
  server.begin();

  DEBUG_PRINTLN("HTTP server started");

}

String decryption_ascii(char ascii_code) {

  char buffer_x[1];

  switch (ascii_code) {
    case 32 : strcpy(buffer_x, " "); break;
    case 33 : strcpy(buffer_x, "!"); break;
    // case 34 : strcpy(buffer_x, '"'); break;
    case 35 : strcpy(buffer_x, "#"); break;
    case 36 : strcpy(buffer_x, "$"); break;
    case 37 : strcpy(buffer_x, "%"); break;
    case 38 : strcpy(buffer_x, "&"); break;
    case 39 : strcpy(buffer_x, "'"); break;
    case 40 : strcpy(buffer_x, "("); break;
    case 41 : strcpy(buffer_x, ")"); break;
    case 42 : strcpy(buffer_x, "*"); break;
    case 43 : strcpy(buffer_x, "+"); break;
    case 44 : strcpy(buffer_x, ","); break;
    case 45 : strcpy(buffer_x, "-"); break;
    case 46 : strcpy(buffer_x, "."); break;
    case 47 : strcpy(buffer_x, "/"); break;

    case 48 : strcpy(buffer_x, "0"); break;
    case 49 : strcpy(buffer_x, "1"); break;
    case 50 : strcpy(buffer_x, "2"); break;
    case 51 : strcpy(buffer_x, "3"); break;
    case 52 : strcpy(buffer_x, "4"); break;
    case 53 : strcpy(buffer_x, "5"); break;
    case 54 : strcpy(buffer_x, "6"); break;
    case 55 : strcpy(buffer_x, "7"); break;
    case 56 : strcpy(buffer_x, "8"); break;
    case 57 : strcpy(buffer_x, "9"); break;

    case 58 : strcpy(buffer_x, ":"); break;
    case 59 : strcpy(buffer_x, ";"); break;
    case 60 : strcpy(buffer_x, "<"); break;

    case 61 : strcpy(buffer_x, "="); break;
    case 62 : strcpy(buffer_x, ">"); break;
    case 63 : strcpy(buffer_x, "?"); break;
    case 64 : strcpy(buffer_x, "@"); break;

    case 65 : strcpy(buffer_x, "A"); break;
    case 66 : strcpy(buffer_x, "B"); break;
    case 67 : strcpy(buffer_x, "C"); break;
    case 68 : strcpy(buffer_x, "D"); break;
    case 69 : strcpy(buffer_x, "E"); break;
    case 70 : strcpy(buffer_x, "F"); break;
    case 71 : strcpy(buffer_x, "G"); break;
    case 72 : strcpy(buffer_x, "H"); break;
    case 73 : strcpy(buffer_x, "I"); break;
    case 74 : strcpy(buffer_x, "J"); break;
    case 75 : strcpy(buffer_x, "K"); break;
    case 76 : strcpy(buffer_x, "L"); break;
    case 77 : strcpy(buffer_x, "M"); break;
    case 78 : strcpy(buffer_x, "N"); break;
    case 79 : strcpy(buffer_x, "O"); break;
    case 80 : strcpy(buffer_x, "P"); break;
    case 81 : strcpy(buffer_x, "Q"); break;
    case 82 : strcpy(buffer_x, "R"); break;
    case 83 : strcpy(buffer_x, "S"); break;
    case 84 : strcpy(buffer_x, "T"); break;
    case 85 : strcpy(buffer_x, "U"); break;
    case 86 : strcpy(buffer_x, "V"); break;
    case 87 : strcpy(buffer_x, "W"); break;
    case 88 : strcpy(buffer_x, "X"); break;
    case 89 : strcpy(buffer_x, "Y"); break;
    case 90 : strcpy(buffer_x, "Z"); break;

    case 97 : strcpy(buffer_x, "a"); break;
    case 98 : strcpy(buffer_x, "b"); break;
    case 99 : strcpy(buffer_x, "c"); break;
    case 100 : strcpy(buffer_x, "d"); break;
    case 101 : strcpy(buffer_x, "e"); break;
    case 102 : strcpy(buffer_x, "f"); break;
    case 103 : strcpy(buffer_x, "g"); break;
    case 104 : strcpy(buffer_x, "h"); break;
    case 105 : strcpy(buffer_x, "i"); break;
    case 106 : strcpy(buffer_x, "j"); break;
    case 107 : strcpy(buffer_x, "k"); break;
    case 108 : strcpy(buffer_x, "l"); break;
    case 109 : strcpy(buffer_x, "m"); break;
    case 110 : strcpy(buffer_x, "n"); break;
    case 111 : strcpy(buffer_x, "o"); break;
    case 112 : strcpy(buffer_x, "p"); break;
    case 113 : strcpy(buffer_x, "q"); break;
    case 114 : strcpy(buffer_x, "r"); break;
    case 115 : strcpy(buffer_x, "s"); break;
    case 116 : strcpy(buffer_x, "t"); break;
    case 117 : strcpy(buffer_x, "u"); break;
    case 118 : strcpy(buffer_x, "v"); break;
    case 119 : strcpy(buffer_x, "w"); break;
    case 120 : strcpy(buffer_x, "x"); break;
    case 121 : strcpy(buffer_x, "y"); break;
    case 122 : strcpy(buffer_x, "z"); break;

    case 123 : strcpy(buffer_x, "{"); break;
    case 124 : strcpy(buffer_x, "|"); break;
    case 125 : strcpy(buffer_x, "}"); break;
    case 126 : strcpy(buffer_x, "~"); break;
  }

  return buffer_x;

}

void autoConnect() {

  int len_ssid = EEPROM.read(addr_length_ssid);
  int len_pass = EEPROM.read(addr_length_pass);

  char buff_ssid[20];
  char buff_pass[20];

  DEBUG_PRINTLN();

  String decryp_ssid = "";
  String decryp_pass = "";

  for (int i = 0; i < len_ssid; i++) {

    int ascii_code = EEPROM.read(addr_eeprom_wifi);
    decryp_ssid += decryption_ascii(ascii_code);
    addr_eeprom_wifi++;
  }

  for (int j = 0; j < len_pass; j++) {
    int ascii_code = EEPROM.read(addr_eeprom_wifi);
    decryp_pass += decryption_ascii(ascii_code);
    addr_eeprom_wifi++;
  }


  DEBUG_PRINTLN();
  DEBUG_PRINT("decryp ssid : ");
  DEBUG_PRINT(decryp_ssid);
  DEBUG_PRINT(" / len : ");
  DEBUG_PRINT(decryp_ssid.length());
  DEBUG_PRINTLN();
  DEBUG_PRINT("decryp pass : ");
  DEBUG_PRINT(decryp_pass);
  DEBUG_PRINT(" / len : ");
  DEBUG_PRINT(decryp_pass.length());
  DEBUG_PRINTLN();

  //=======

  DEBUG_PRINTLN();
  DEBUG_PRINTLN();
  DEBUG_PRINT("Connecting to ");
  DEBUG_PRINTLN(decryp_ssid);

  lcd.setCursor(0, 0);
  lcd.print("State : Connecting.");
  lcd.setCursor(0, 2);
  lcd.print("SSID  : ");
  lcd.print(decryp_ssid);

  WiFi.begin(decryp_ssid.c_str(), decryp_pass.c_str());

  while (counting_connect <= 30) {

    if (WiFi.status() != WL_CONNECTED) {
      delay(500);
      DEBUG_PRINT(".");
    } else {
      DEBUG_PRINTLN("");
      DEBUG_PRINTLN("WiFi connected");
      DEBUG_PRINTLN("IP address: ");
      DEBUG_PRINTLN(WiFi.localIP());
      Serial.println(decryp_ssid.length());
      Serial.println(decryp_ssid);
      counting_connect = 31; // exit if connected
      state_internet = 1;
    }

    counting_connect++;

  }

}

void setup()
{

  Serial.begin(115200);

  pinMode(BTN_LEFT,  INPUT);  // detect boot
  pinMode(BTN_RIGHT, INPUT); // detect boot setting wifi

  EEPROM.begin(512);
  lcd.begin();
  lcd.home();

  if (digitalRead(BTN_LEFT) == 1 || strcmp(boot_mode, "setting") == 0) {
    strcpy(boot_mode, "setting");

    //pinMode(BTN_RIGHT,  INPUT); // RIGHT
    pinMode(BTN_CENTER, INPUT); // ENTER
    pinMode(BTN_BACK,   INPUT); // BACK

    lcd.createChar(4, ICON_SELECT);

    DEBUG_PRINTLN();
    DEBUG_PRINTLN();
    DEBUG_PRINTLN("Boot setting");


  } else {

    if (digitalRead(BTN_RIGHT) == 1) {
      strcpy(boot_mode, "setting wifi");

      pinMode(BTN_LEFT,   OUTPUT);
      pinMode(BTN_RIGHT,  OUTPUT);
      pinMode(ACTIVE_RELEY, OUTPUT);
      digitalWrite(ACTIVE_RELEY, 1);


      DEBUG_PRINTLN();
      DEBUG_PRINTLN();
      DEBUG_PRINTLN("Boot setting wifi.");


      lcd.setCursor(0, 0);
      lcd.print("Access Point.");
      lcd.setCursor(0, 2);
      lcd.print("IP : 192.168.0.100");

      wifi_ap();
      webserver_config();

    } else {

      pinMode(BTN_LEFT,     OUTPUT);
      pinMode(BTN_RIGHT,    OUTPUT);
      pinMode(ACTIVE_RELEY, OUTPUT);
      pinMode(PIN_SOIL,     INPUT);

      digitalWrite(ACTIVE_RELEY, 1);

      autoConnect();

      int get_mode = EEPROM.read(addr_mode_auto);
      if (get_mode == 1) {
        check_mode_auto = 1;
      } else {
        check_mode_auto = 0;
      }

      dht.begin();
      lcd.begin();
      RTC.begin();
    //  RTC.adjust(DateTime(__DATE__, __TIME__));

      if (! RTC.isrunning()) {

        DEBUG_PRINTLN("RTC is NOT running!");
        RTC.adjust(DateTime(__DATE__, __TIME__));

      }

      DateTime now = RTC.now();
    //  RTC.setAlarm1Simple(23, 9);
    //  RTC.turnOnAlarm(1);
    //  if (RTC.checkAlarmEnabled(1)) {
    //    DEBUG_PRINTLN("Alarm Enabled");
    //  }

      lcd.createChar(1, ICON_TIME);
      lcd.createChar(2, ICON_TEMP);
      lcd.createChar(3, ICON_HUMID);
      lcd.createChar(4, ICON_SELECT);
      lcd.createChar(5, ICON_SOIL);
      lcd.createChar(6, ICON_CONNECT);
      lcd.home();

    }
  }

}

void SENT_THINGSPEAK(int temp, int humid, int soil) {

  char buffer_t[10];
  char buffer_h[10];
  char buffer_s[10];

  String data 	= "temp=";
  sprintf(buffer_t, "%d", temp);
  data 		   	+= buffer_t;
  data 		   	+= "&humid=";
  sprintf(buffer_h, "%d", humid);
  data 		   	+= buffer_h;
  data 		   	+= "&soil=";
  sprintf(buffer_s, "%d", soil);
  data 		   	+= buffer_s;

  if (!client.connect(db_host, host_port)) {

    DEBUG_PRINTLN("retry connection");

    return;
  } else {

    DEBUG_PRINTLN("connection success");

  }

  client.println(String("GET ") + db_url + String(data) + " HTTP/1.1");
  client.println(String("Host: ") + db_host);
  client.println();

}

void* FILTER_NUMBER(int n) {
  char* v;
  switch (n) {
    case 1 : v = "01"; break;
    case 2 : v = "02"; break;
    case 3 : v = "03"; break;
    case 4 : v = "04"; break;
    case 5 : v = "05"; break;
    case 6 : v = "06"; break;
    case 7 : v = "07"; break;
    case 8 : v = "08"; break;
    case 9 : v = "09"; break;
    case 0 : v = "00"; break;
  }
  return v;
}

void LCD_DISPLAY(float temp, float humid) {

  DateTime now = RTC.now();

  int soil_sensor = analogRead(PIN_SOIL);

  lcd.setCursor(0, 0);

  if (int(now.day()) <= 9) {
    lcd.print((char*)FILTER_NUMBER(int(now.day())));
  } else {
    lcd.print(now.day(), DEC);
  }

  lcd.print("/");

  if (int(now.month()) <= 9) {
    lcd.print((char*)FILTER_NUMBER(int(now.month())));
  } else {
    lcd.print(now.month(), DEC);
  }

  lcd.print("/");

  lcd.print(now.year(), DEC);
  lcd.print("  ");

  if (int(now.hour()) <= 9) {
    lcd.print((char*)FILTER_NUMBER(int(now.hour())));
  } else {
    lcd.print(now.hour(), DEC);
  }

  lcd.print(":");

  if (int(now.minute()) <= 9) {
    lcd.print((char*)FILTER_NUMBER(int(now.minute())));
  } else {
    lcd.print(now.minute(), DEC);
  }

  lcd.print(" ");

  if (state_internet == 1) {
    lcd.write(6);
  } else {
    lcd.print("N");
  }

  // lcd.write(1);
  lcd.setCursor(0, 1);
  lcd.print(" ");
  lcd.write(2);
  lcd.print(" Temp     ");
  lcd.print(temp);
  lcd.print(" C");
  lcd.setCursor(0, 2);
  lcd.print(" ");
  lcd.write(3);
  lcd.print(" Humid    ");
  lcd.print(humid);
  lcd.print(" %");
  lcd.setCursor(0, 3);
  lcd.print(" ");
  lcd.write(5);
  lcd.print(" Soil     ");

  if (soil_sensor >= 600) {
    lcd.print("0     %");
  } else if (soil_sensor >= 500) {
    lcd.print("40    %");
  } else if (soil_sensor >= 300) {
    lcd.print("80    %");
  } else if (soil_sensor >= 100) {
    lcd.print("100   %");
  }

}

void DEBUG(float temp, float humid) {

  DateTime now = RTC.now();

  DEBUG_PRINTLN("====================");
  DEBUG_PRINT("Temp : ");
  DEBUG_PRINTLN(temp);
  DEBUG_PRINT("Humid : ");
  DEBUG_PRINTLN(humid);
  DEBUG_PRINT("Day : ");
  DEBUG_PRINTLN(now.day(), DEC);
  DEBUG_PRINT("Month : ");
  DEBUG_PRINTLN(now.month(), DEC);
  DEBUG_PRINT("Year : ");
  DEBUG_PRINTLN(now.year(), DEC);
  DEBUG_PRINT("H : ");
  DEBUG_PRINTLN(now.hour(), DEC);
  DEBUG_PRINT("M : ");
  DEBUG_PRINTLN(now.minute(), DEC);

  DEBUG_PRINTLN("====================");
  DEBUG_PRINT("Setting hour : ");
  DEBUG_PRINT(EEPROM.read(setting_t.eeprom_addr_h));
  DEBUG_PRINT("-");
  DEBUG_PRINTLN(EEPROM.read(setting_t.eeprom_addr_m));

  DEBUG_PRINT("After Hour : ");
  DEBUG_PRINT(EEPROM.read(setting_t.eeprom_addr_after_h));
  DEBUG_PRINT("-");
  DEBUG_PRINTLN(EEPROM.read(setting_t.eeprom_addr_after_m));

  DEBUG_PRINTLN("====================");
  DEBUG_PRINT("Mode auto : ");
  DEBUG_PRINTLN(EEPROM.read(addr_mode_auto));

}

void FUNCTION_WRITE_EEPROM() {
  setting_t.reboot_time--;
  lcd.home();
  lcd.setCursor(0,0);
  lcd.print("Save success.");
  lcd.setCursor(0,2);
  lcd.print("Reboot in ");
  sprintf(setting_t.display_reboot, "%d", setting_t.reboot_time);
  lcd.print(setting_t.display_reboot);
  lcd.print(" second");
  if (setting_t.reboot_time == 0) {
    lcd.clear();
    Esp.reset();
  }
  delay(1000);
}

void FUNCTION_NORMAL() {

  currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    int humid = dht.readHumidity();
    int temp  = dht.readTemperature();

    int soil_sensor = analogRead(PIN_SOIL);

    if (soil_sensor >= 600) {
      soil_sensor = 0;
    } else if (soil_sensor >= 500) {
      soil_sensor = 40;
    } else if (soil_sensor >= 300) {
      soil_sensor = 80;
    } else if (soil_sensor >= 100) {
      soil_sensor = 100;
    }

  //      if (RTC.checkIfAlarm(1)) {
  //        DEBUG_PRINTLN("Alarm Triggered");
  //      }

    DEBUG_PRINTLN();

    lcd.home();

    if (isnan(h) || isnan(t)) {

      if (dht_counting_fail == 20) {
        Esp.reset();
      }

      dht_counting_fail++;

      DEBUG_PRINTLN("Failed to read from DHT sensor!");

      return;
    }

    LCD_DISPLAY(t, h);

    if (state_internet == 1 ) {
      SENT_THINGSPEAK(temp, humid, soil_sensor);
    }

    DEBUG(t, h);

  }
}

void FUNCTION_SETTING() {
  strcpy(directory, "root");

  lcd.home();
  lcd.setCursor(0, 0);
  lcd.print("Slect Mode");
  lcd.setCursor(0, 2);
  lcd.write(4);
  lcd.print(" Set Timer");

}

void FUNCTION_SET_TIMER_HOUR() {

  lcd.setCursor(0,0);
  lcd.print("Set Timer (24 Hour)");
  lcd.setCursor(0,1);
  lcd.print(" Day   : every day");
  lcd.setCursor(0,2);
  lcd.write(4);

  lcd.print("Hour  : ");
  sprintf(setting_t.display_h, "%d", setting_t.hour);
  lcd.print(setting_t.display_h);
  lcd.print("-");
  sprintf(setting_t.display_after_h, "%d", setting_t.after_hour);
  lcd.print(setting_t.display_after_h);

  lcd.setCursor(0,3);
  lcd.print(" Minute: ");
  sprintf(setting_t.display_m, "%d", setting_t.minute);
  lcd.print(setting_t.display_m);
  lcd.print("-");
  sprintf(setting_t.display_after_m, "%d", setting_t.after_minute);
  lcd.print(setting_t.display_after_m);

  if (setting_t.minute >= 10 && setting_t.after_minute < 10) {
    lcd.print("   ");
  }

  if (setting_t.minute < 10 && setting_t.after_minute >= 10) {
    lcd.print("   ");
  }

  if (setting_t.minute >= 10 && setting_t.after_minute >= 10) {
    lcd.print("  ");
  }

  if (setting_t.minute < 10 && setting_t.after_minute < 10) {
    lcd.print("    ");
  }

  lcd.print("Save");
}

void FUNCTION_SET_TIMER_MINUTE() {
  lcd.setCursor(0,0);
  lcd.print("Set Timer (24 Hour)");
  lcd.setCursor(0,1);
  lcd.print(" Day   : every day");
  lcd.setCursor(0,2);

  lcd.print(" Hour  : ");
  sprintf(setting_t.display_h, "%d", setting_t.hour);
  lcd.print(setting_t.display_h);
  lcd.print("-");
  sprintf(setting_t.display_after_h, "%d", setting_t.after_hour);
  lcd.print(setting_t.display_after_h);

  lcd.setCursor(0,3);
  lcd.write(4);
  lcd.print("Minute: ");
  sprintf(setting_t.display_m, "%d", setting_t.minute);
  lcd.print(setting_t.display_m);
  lcd.print("-");
  sprintf(setting_t.display_after_m, "%d", setting_t.after_minute);
  lcd.print(setting_t.display_after_m);

  if (setting_t.minute >= 10 && setting_t.after_minute < 10) {
    lcd.print("   ");
  }

  if (setting_t.minute < 10 && setting_t.after_minute >= 10) {
    lcd.print("   ");
  }

  if (setting_t.minute >= 10 && setting_t.after_minute >= 10) {
    lcd.print("  ");
  }

  if (setting_t.minute < 10 && setting_t.after_minute < 10) {
    lcd.print("    ");
  }

  lcd.print("Save");
}

void FUNCTION_SET_TIMER_SAVE() {
  lcd.setCursor(0,0);
  lcd.print("Set Timer (24 Hour)");
  lcd.setCursor(0,1);
  lcd.print(" Day   : every day");
  lcd.setCursor(0,2);

  lcd.print(" Hour  : ");
  sprintf(setting_t.display_h, "%d", setting_t.hour);
  lcd.print(setting_t.display_h);
  lcd.print("-");
  sprintf(setting_t.display_after_h, "%d", setting_t.after_hour);
  lcd.print(setting_t.display_after_h);

  lcd.setCursor(0,3);
  lcd.print(" Minute: ");
  sprintf(setting_t.display_m, "%d", setting_t.minute);
  lcd.print(setting_t.display_m);
  lcd.print("-");
  sprintf(setting_t.display_after_m, "%d", setting_t.after_minute);
  lcd.print(setting_t.display_after_m);

  if (setting_t.minute >= 10 && setting_t.after_minute < 10) {
    lcd.print("   ");
  }

  if (setting_t.minute >= 10 && setting_t.after_minute >= 10) {
    lcd.print(" ");
  }

  if (setting_t.minute < 10 && setting_t.after_minute < 10) {
    lcd.print("   ");
  }

  lcd.write(4);
  lcd.print("Save");
}

void FUNCTION_SET_TIMER() {
  strcpy(directory, "root/set_timer");

  if (setting_t.reboot_time < 6) {
    FUNCTION_WRITE_EEPROM();
  } else {

    if (strcmp(select_current, "Select hour") == 0 || strcmp(select_current, "Set hour is active.") == 0) {
      FUNCTION_SET_TIMER_HOUR();
    }

    if (strcmp(select_current, "Set after hour is active.") == 0) {
      FUNCTION_SET_TIMER_HOUR();
    }

    if (strcmp(select_current, "Select minute") == 0 || strcmp(select_current, "Set minute is active.") == 0) {
      FUNCTION_SET_TIMER_MINUTE();
    }

    if (strcmp(select_current, "Set after minute is active.") == 0) {
      FUNCTION_SET_TIMER_MINUTE();
    }

    if (strcmp(select_current, "Select save") == 0) {
      FUNCTION_SET_TIMER_SAVE();
    }

  } // End check reboot time

}

void BTN_STATE() {

  if (digitalRead(BTN_LEFT) == 1) {

    strcpy(run_left, "detect_left");

  } else {

    if (strcmp(run_left, "detect_left") == 0) { // leave button for run


      DEBUG_PRINTLN("LEFT");


      if (strcmp(directory, "root/set_timer") == 0) {

        if (strcmp(select_current, "Set hour is active.") == 0) {
          setting_t.hour--;
          if (setting_t.hour < 0) {
              setting_t.hour = 23;
          }
        }

        if (strcmp(select_current, "Set after hour is active.") == 0) {
          setting_t.after_hour--;
          if (setting_t.after_hour < 0) {
              setting_t.after_hour = 23;
          }
        }

        if (strcmp(select_current, "Set minute is active.") == 0) {
          setting_t.minute--;
          if (setting_t.minute < 0) {
              setting_t.minute = 59;
          }
        }

        if (strcmp(select_current, "Set after minute is active.") == 0) {
          setting_t.after_minute--;
          if (setting_t.after_minute < 0) {
              setting_t.after_minute = 59;
          }
        }

        lcd.home();
        FUNCTION_SET_TIMER();
      }

      strcpy(run_left, "");

    }

  }

  if (digitalRead(BTN_RIGHT) == 1) {

    strcpy(run_right, "detect_right");

  } else {

    if (strcmp(run_right, "detect_right") == 0) { // leave button for run


      DEBUG_PRINTLN("RIGHT");


      if (strcmp(directory, "root/set_timer") == 0) {

        if (strcmp(select_current, "Set hour is active.") != 0 && strcmp(select_current, "Set minute is active.") != 0) {
          if (strcmp(select_current, "Set after hour is active.") != 0 && strcmp(select_current, "Set after minute is active.") != 0) {
            setting_t.move_right++;

            if (setting_t.move_right == 3) {
                strcpy(select_current, "Select hour");
                setting_t.move_right = 0;
            }

            if (setting_t.move_right == 2) {
                strcpy(select_current, "Select save");
            }

            if (setting_t.move_right == 1) {
                strcpy(select_current, "Select minute");
            }
          }
        }

        if (strcmp(select_current, "Set hour is active.") == 0) {
          setting_t.hour++;
          if (setting_t.hour > 23) {
              setting_t.hour = 0;
          }
        }

        if (strcmp(select_current, "Set after hour is active.") == 0) {
          setting_t.after_hour++;
          if (setting_t.after_hour > 23) {
              setting_t.after_hour = 0;
          }
        }

        if (strcmp(select_current, "Set minute is active.") == 0) {
          setting_t.minute++;
          if (setting_t.minute > 59) {
              setting_t.minute = 0;
          }
        }

        if (strcmp(select_current, "Set after minute is active.") == 0) {
          setting_t.after_minute++;
          if (setting_t.after_minute > 59) {
              setting_t.after_minute = 0;
          }
        }

        lcd.home();
        FUNCTION_SET_TIMER();
      }

      strcpy(run_right, "");

    }

  }

  if (digitalRead(BTN_CENTER) == 1) {

    strcpy(run_center, "detect_center");

  } else {

    if (strcmp(run_center, "detect_center") == 0) { // leave button for run


      DEBUG_PRINTLN("CENTER");


      if (strcmp(directory, "root/set_timer") == 0) {

        if (strcmp(select_current, "Set hour is active.") == 0) {
          strcpy(select_current, "Set after hour is active.");
        }

        if (strcmp(select_current, "Set minute is active.") == 0) {
          strcpy(select_current, "Set after minute is active.");
        }

        if (strcmp(select_current, "Select hour") == 0) {
          strcpy(select_current, "Set hour is active.");
        }

        if (strcmp(select_current, "Select minute") == 0) {
          strcpy(select_current, "Set minute is active.");
        }

        if (strcmp(select_current, "Select save") == 0) {

          if (setting_t.after_hour < setting_t.hour) {
            lcd.home();
            setting_t.move_right = 0;
            strcpy(select_current, "Select hour");
            FUNCTION_SET_TIMER_HOUR();
          } else {
            if (setting_t.after_minute < setting_t.minute) {
              lcd.home();
              setting_t.move_right = 1;
              strcpy(select_current, "Select minute");
              FUNCTION_SET_TIMER_MINUTE();
            }
          }

          if (setting_t.after_hour >= setting_t.hour && setting_t.after_minute >= setting_t.minute) {
            strcpy(select_current, "Save data to eeprom.");

            DEBUG_PRINTLN("Write");

            EEPROM.write(setting_t.eeprom_addr_h, setting_t.hour);
            EEPROM.write(setting_t.eeprom_addr_after_h, setting_t.after_hour);
            EEPROM.write(setting_t.eeprom_addr_m, setting_t.minute);
            EEPROM.write(setting_t.eeprom_addr_after_m, setting_t.after_minute);

            EEPROM.commit();

            FUNCTION_WRITE_EEPROM();
          }

        }

      }

      if (strcmp(select_current, "Select timer") == 0) { //select timer
        strcpy(select_current, "Select hour");
        lcd.home();
        FUNCTION_SET_TIMER();
      }

      strcpy(run_center, "");

    }

  }

  if (digitalRead(BTN_BACK) == 1) {

    strcpy(run_back, "detect_back");

  } else {

    if (strcmp(run_back, "detect_back") == 0) { // leave button for run


      DEBUG_PRINTLN("BACK");


      if (strcmp(directory, "root/set_timer") == 0) {

        if (strcmp(select_current, "Set hour is active.") == 0) {
          strcpy(select_current, "Select hour");
          lcd.home();
          FUNCTION_SET_TIMER();
        }

        if (strcmp(select_current, "Set after hour is active.") == 0) {
          strcpy(select_current, "Select hour");
          lcd.home();
          FUNCTION_SET_TIMER();
        }

        if (strcmp(select_current, "Set minute is active.") == 0) {
          strcpy(select_current, "Select minute");
          lcd.home();
          FUNCTION_SET_TIMER();
        }

        if (strcmp(select_current, "Set after minute is active.") == 0) {
          strcpy(select_current, "Select minute");
          lcd.home();
          FUNCTION_SET_TIMER();
        }

      }

      strcpy(run_back, "");

    }

  }

}

void get_heap() {

  DEBUG_PRINT("Heap : ");
  DEBUG_PRINTLN(Esp.getFreeHeap());

}

void get_modeTimer() {
  DateTime now = RTC.now();

  int h_old = EEPROM.read(setting_t.eeprom_addr_h);
  int m_old = EEPROM.read(setting_t.eeprom_addr_m);

  int h_new = EEPROM.read(setting_t.eeprom_addr_after_h);
  int m_new = EEPROM.read(setting_t.eeprom_addr_after_m);

  if (h_old > 0) {

    if (h_old == now.hour() && m_old == now.minute()) {
      digitalWrite(ACTIVE_RELEY, 0);
      DEBUG_PRINTLN("relay on");
    } else {
      if (now.hour() >= h_new && now.minute() >= m_new) {
        digitalWrite(ACTIVE_RELEY, 1);
        DEBUG_PRINTLN("relay off");
      }
    }

  }
}

void get_modeAuto() {

  if (check_mode_auto == 1) {
    int soil_read = analogRead(A0);
    if (soil_read > 300) {
      digitalWrite(ACTIVE_RELEY, 0);
    } else {
      digitalWrite(ACTIVE_RELEY, 1);
    }
  }

}

void loop() {

  if (strcmp(boot_mode, "setting") == 0) {

    // get_heap();

    if (setting_t.reboot_time < 6) {
      FUNCTION_WRITE_EEPROM();
    } else {

      if (strcmp(directory, "") == 0) {
        strcpy(select_current, "Select timer");
        FUNCTION_SETTING();
      }

      BTN_STATE();

    }


      DEBUG_PRINTLN("==============");
      DEBUG_PRINT("directory : ");
      DEBUG_PRINTLN(directory);
      DEBUG_PRINT("select current : ");
      DEBUG_PRINTLN(select_current);
      DEBUG_PRINT("move right : ");
      DEBUG_PRINTLN(setting_t.move_right);
      DEBUG_PRINTLN("==============");



  } else {

    if (strcmp(boot_mode, "setting wifi") == 0) {

      server.handleClient();

    } else {
      // get_heap();

      if (Serial.available() > 0) {

        switch (Serial.read()) {
          case 48 : strcpy(memory_rx, "0"); break;
          case 49 : strcpy(memory_rx, "1"); break;
          case 50 : strcpy(memory_rx, "2"); break;
          case 51 : strcpy(memory_rx, "3"); break;
          case 52 : strcpy(memory_rx, "4"); break;
          case 53 : strcpy(memory_rx, "5"); break;
          case 54 : strcpy(memory_rx, "6"); break;
          case 55 : strcpy(memory_rx, "7"); break;
          case 56 : strcpy(memory_rx, "8"); break;
          case 57 : strcpy(memory_rx, "9"); break;
        }

        if (strcmp(memory_rx, "1") == 0) {
          DEBUG_PRINT("Messages : ");
          DEBUG_PRINTLN("auto is online");
          EEPROM.write(addr_mode_auto, 1);
          EEPROM.commit();
          lcd.home();
          lcd.setCursor(0, 0);
          lcd.print("Messages : ");
          lcd.setCursor(0, 2);
          lcd.print("auto is online");
          Esp.reset();
        }

        if (strcmp(memory_rx, "0") == 0) {
          DEBUG_PRINT("Messages : ");
          DEBUG_PRINTLN("auto is offline");
          EEPROM.write(addr_mode_auto, 0);
          EEPROM.commit();
          lcd.home();
          lcd.setCursor(0, 0);
          lcd.print("Messages : ");
          lcd.setCursor(0, 2);
          lcd.print("auto is offline");
          Esp.reset();
        }

      }

      FUNCTION_NORMAL();
      get_modeTimer();
      get_modeAuto();

    }

  }

  delay(100);
}