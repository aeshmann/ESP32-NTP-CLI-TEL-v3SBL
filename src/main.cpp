/* Version 1.6

ESP32-S3-WROOM NTP CLI TEL, no LCD, no BTN


*/


#include <WIFi.h>
#include <Arduino.h>
#include <ESPTelnet.h>
#include <Adafruit_NeoPixel.h>
#include "cline.h"

#define RXD1 18
#define TXD1 17

#define TIMEZONE "MSK-3"                      // local time zone definition (Moscow)
#define TRACE(...) Serial.printf(__VA_ARGS__) // Serial output simplified
#define TLNET(...) telnet.printf(__VA_ARGS__) // Telnet output simplified

#define RGB_LED_PIN 48
#define NUM_RGB_LEDS 1 // number of RGB LEDs (1 WS2812 LED)

const char *ntpHost0 = "0.ru.pool.ntp.org";
const char *ntpHost1 = "1.ru.pool.ntp.org";
const char *ntpHost2 = "2.ru.pool.ntp.org";

const char *mssid = "Xiaomi_065C";
const char *mpass = "43v3ry0nG";

IPAddress ip;
const uint16_t port = 23;
const int serial_speed = 115200;
String client_ip;
String boot_timestamp;
time_t boot_time;

bool initWiFi(const char *, const char *, int, int);
bool isConnected();

void setupSerial(int);
void setupTelnet();
void errorMsg(String, bool);
void onTelnetConnect(String ip);
void onTelnetDisconnect(String ip);
void onTelnetReconnect(String ip);
void onTelnetConnectionAttempt(String ip);
void onTelnetInput(String str);

String getTimeStr(int);
String getSensVal(char);
String uptimeCount();

void commSerial();
String infoWiFi();
String infoChip();
String scanWiFi();

ESPTelnet telnet;
Adafruit_NeoPixel rgb_led = Adafruit_NeoPixel(NUM_RGB_LEDS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);


bool initWiFi(const char *mssid, const char *mpass,
              int max_tries = 20, int pause = 500)
{
  int i = 0;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  digitalWrite(LED_BUILTIN, LOW);
  delay(100);
  TRACE("Connecting to Wifi:\n");
  WiFi.begin(mssid, mpass);
  do
  {
    delay(pause);
    TRACE(".");
    i++;
  } while (!isConnected() && i < max_tries);
  TRACE("\nConnected!\n");
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  return isConnected();
}

bool isConnected()
{
  return (WiFi.status() == WL_CONNECTED);
}

void errorMsg(String error, bool restart = true)
{
  TRACE("%s\n", error);
  if (restart)
  {
    TRACE("Rebooting now...\n");
    delay(2000);
    ESP.restart();
    delay(2000);
  }
}

void setupSerial(int uart_num)
{
  if (uart_num == 0)
  {
    Serial.begin(serial_speed);
    while (!Serial)
    {
      ;
    };
    delay(100);
    TRACE("Serial0 running\n");
    return;
  }
  else if (uart_num == 1)
  {
    Serial1.begin(serial_speed, SERIAL_8N1, RXD1, TXD1);
    while (!Serial1)
    {
      ;
    };
    delay(100);
    TRACE("Serial1 running\n");
    return;
  }
  else
  {
    return;
  }
}

String uptimeCount() {

  time_t curr_time = time(nullptr);
  time_t up_time = curr_time - boot_time;
  int uptime_dd = up_time / 86400;
  int uptime_hh = (up_time / 3600) % 24;
  int uptime_mm = (up_time / 60 ) % 60;
  int uptime_ss = up_time % 60;

  String uptimeStr(String(uptime_dd) + " days " + String(uptime_hh) + " hours " + String(uptime_mm) + " minutes " + String(uptime_ss) + " seconds");
  return uptimeStr;
}


void setupTelnet()
{
  // passing on functions for various telnet events
  telnet.onConnect(onTelnetConnect);
  telnet.onConnectionAttempt(onTelnetConnectionAttempt);
  telnet.onReconnect(onTelnetReconnect);
  telnet.onDisconnect(onTelnetDisconnect);
  telnet.onInputReceived(onTelnetInput);

  //TRACE("- Telnet: ");
  if (telnet.begin(port))
  {
    TRACE("- Telnet running\n");
  }
  else
  {
    TRACE("- Telnet error.");
    errorMsg("Will reboot...");
  }
  return;
}

void onTelnetConnect(String ip)
{
  TRACE("- Telnet: %s connected\n", ip.c_str());
  TLNET("\nWelcome %s\n(Use ^] +q to disconnect)\n", telnet.getIP().c_str());
  client_ip = ip.c_str();
}

void onTelnetDisconnect(String ip)
{
  TRACE("- Telnet: %s disconnected\n", ip.c_str());
}

void onTelnetReconnect(String ip)
{
  TRACE("- Telnet: %s reconnected\n", ip.c_str());
}

void onTelnetConnectionAttempt(String ip)
{
  TRACE("- Telnet: %s tried to connect\n", ip.c_str());
}

void onTelnetInput(String comm_telnet)
{
  TLNET("[%s] > %s\n", getTimeStr(0), comm_telnet);
  int comm_case_t = 0;
  for (int i = 1; i < comm_qty; i++)
  {
    if (comm_array[i][0] == comm_telnet)
    {
      comm_case_t = i;
      break;
    }
    else
    {
      comm_case_t = 0;
    }
  }

  switch (comm_case_t)
  {
  case 0:
  {
    TLNET("Command \"%s\" not understood\n", comm_telnet);
    break;
  }
  case 1:
  {
    TLNET("%s %s %s\n",
          comm_about, __DATE__, __TIME__);
    break;
  }
  case 2:
  {
    TLNET("%s", term_clear);
    break;
  }
  case 3:
  {
    TLNET("Hi, %s!\n", client_ip.c_str());
    break;
  }
  case 4:
  {
    TLNET(">pong\n");
    TRACE("- Telnet: >pong\n");

    break;
  }
  case 5:
  {
    for (int k = 1; k < comm_qty; k++)
    {
      TLNET("[ %d ] %s\t- %s\n", k, comm_array[k][0], comm_array[k][1].c_str());
    }
    break;
  }
  case 6:
  {
    TLNET("%s\n", infoWiFi().c_str());
    break;
  }
  case 7:
  {
    TLNET("%s\n", infoChip().c_str());
    break;
  }
  case 8:
  {
    TLNET("Disconnecting you!\n");
    telnet.disconnectClient();

    break;
  }
  case 9:
  {
    TLNET("%s\n", getTimeStr(0));
    break;
  }
  case 10:
  {
    TLNET("%s\n", getTimeStr(7));
    break;
  }
  case 11:
  {
    TLNET("Turning WiFi on/off unavaliable on telnet session\n");
    break;
  }
  case 12:
  {
    TLNET("Turning WiFi on/off unavaliable on telnet session\n");
    break;
  }
  case 13:
  {
    TLNET("WiFi scan start at %s\n", getTimeStr(0));
    TLNET("%s\n", scanWiFi().c_str());
    TLNET("Scan complete.\n");
    WiFi.scanDelete();
    break;
  }
  case 14:
  {
    TLNET("WiFi RSSI: %d dBm\n", WiFi.RSSI());
    break;
  }
  case 15:
  {
    TLNET("%s\n", ESP.getChipModel());
    break;
  }
  case 16:
  {
    TLNET("ESP restating, you'll be disconnected\n");
    TLNET("Bye, %s\n", client_ip.c_str());
    delay(1000);
    telnet.disconnectClient();
    delay(2000);
    ESP.restart();
    break;
  }
  case 17:
  {
    TLNET("Starting soft reset MCU,\ndon't know where this will lead :/\n");
    setup();
    break;
  }
  case 18:
  {
    TLNET("System start: %s\n", boot_timestamp.c_str());
    TLNET("Uptime: %s\n", uptimeCount().c_str());
    break;
  }
  case 19:
  {
    TLNET("LCD dismantled\n");
    break;
  }

  case 20:
  {
    if (!Serial)
    {
      TLNET("Starting setup Serial\n");
      setupSerial(0);
    }
    else
    {
      TLNET("Serial is already up!\n");
    }
    break;
  }
  case 21:
  {
    TLNET("BTN dismantled\n");
    break;
  }
  case 22:
  {
    TLNET("Sensor dismantled\n");
    break;
  }
  case 23:
  {
    TLNET("Sensor dismantled\n");
  }
  }
}

String infoWiFi() {
  String wfinf("");
  if (isConnected()) {
    wfinf += "WiFi SSID: ";
    wfinf += WiFi.SSID();
    wfinf += " (ch ";
    wfinf += WiFi.channel();
    wfinf += ")\n";
    wfinf += "WiFi RSSI: ";
    wfinf += WiFi.RSSI();
    wfinf += "dBm\n";
    wfinf += "Local ip: ";
    wfinf += WiFi.localIP().toString();
    wfinf += '\n';
  } else {
    wfinf += "WiFi not connected\n";
  }
  return wfinf;
}

String scanWiFi() {
  String scanResult(' ');
  uint32_t scan_start = millis();
  int n = WiFi.scanNetworks();
  uint32_t scan_elapse = millis() - scan_start;
  if (n == 0) {
   scanResult += "No networks found";
  } else {
  scanResult += n;
  scanResult += " networks found for ";
  scanResult += scan_elapse;
  scanResult+= "ms at ";
  scanResult += getTimeStr(0).c_str();
  scanResult += '\n';
  int lengthMax = 0;
  for (int l = 0; l < n; l++) {
    if (WiFi.SSID(l).length() >= lengthMax) {
      lengthMax = WiFi.SSID(l).length();
    }
  }
  scanResult += " Nr |";
  scanResult +=  " SSID";
  String spaceStr = "";
  for (int m = 0; m < lengthMax - 3; m++) {
    spaceStr += ' ';
  }
  scanResult += spaceStr;
  scanResult += "|RSSI | CH | Encrypt\n";
  for (int i = 0; i < n; i++) {
    scanResult += ' ';
    if (i < 9) {
      scanResult += '0';
    }
    scanResult += i + 1;
    scanResult += " | ";
    int lengthAdd = lengthMax - WiFi.SSID(i).length();
    String cmStr = "";
    for (int k = 0; k < lengthAdd; k++) {
      cmStr += ' ';
    }
    scanResult += WiFi.SSID(i).c_str();
    scanResult += cmStr;
    scanResult += " | ";
    scanResult += WiFi.RSSI(i);
    scanResult += " | ";
    scanResult += WiFi.channel(i);
    if (WiFi.channel(i) < 10) {
      scanResult += ' ';
    }
    scanResult += " | ";
    switch (WiFi.encryptionType(i)) {
      case WIFI_AUTH_OPEN:
      scanResult += "open";
      break;
      case WIFI_AUTH_WEP:
      scanResult += "WEP";
      break;
      case WIFI_AUTH_WPA_PSK:
      scanResult += "WPA";
      break;
      case WIFI_AUTH_WPA2_PSK:
      scanResult += "WPA2";
      break;
      case WIFI_AUTH_WPA_WPA2_PSK:
      scanResult += "WPA+WPA2";
      break;
      case WIFI_AUTH_WPA2_ENTERPRISE:
      scanResult += "WPA2-EAP";
      break;
      case WIFI_AUTH_WPA3_PSK:
      scanResult += "WPA3";
      break;
      case WIFI_AUTH_WPA2_WPA3_PSK:
      scanResult += "WPA2+WPA3";
      break;
      case WIFI_AUTH_WAPI_PSK:
      scanResult += "WAPI";
      break;
      default:
      scanResult += "unk";
    }
    scanResult += '\n';
    delay(10);
  }
 }
 return scanResult;
}

String infoChip() {
  uint32_t total_internal_memory = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
  uint32_t free_internal_memory = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  String infoChipStr[8];

  infoChipStr[0] += "Chip:";
  infoChipStr[0] += ESP.getChipModel();
  infoChipStr[0] += " R";
  infoChipStr[0] += ESP.getChipRevision();
  
  infoChipStr[1] += "CPU :";
  infoChipStr[1] += ESP.getChipCores();
  infoChipStr[1] += " cores @ ";
  infoChipStr[1] += ESP.getCpuFreqMHz();
  infoChipStr[1] += "MHz";
  
  infoChipStr[2] += "Flash frequency ";
  infoChipStr[2] += ESP.getFlashChipSpeed() / 1000000;
  infoChipStr[2] += "MHz";

  infoChipStr[3] += "Flash size: ";
  infoChipStr[3] += ESP.getFlashChipSize() * 0.001;
  infoChipStr[3] += "kb";

  infoChipStr[4] += "Flash size: ";
  infoChipStr[4] += ESP.getFlashChipSize() / 1024;
  infoChipStr[4] += " kib";

  infoChipStr[5] += "Free  heap: ";
  infoChipStr[5] += esp_get_free_heap_size() * 0.001;
  infoChipStr[5] += " kb";

  infoChipStr[6] += "Free  DRAM: ";
  infoChipStr[6] += free_internal_memory * 0.001;
  infoChipStr[6] += " kb";

  infoChipStr[7] += "Total DRAM: ";
  infoChipStr[7] += total_internal_memory * 0.001;
  infoChipStr[7] += " kb";

  String infoChipAll = "";
  for (int i = 0; i < 8; i++) {
    infoChipAll += i + 1;
    infoChipAll += ". ";
    infoChipAll += infoChipStr[i];
    infoChipAll += '\n';
  }
  return infoChipAll;
}


String getTimeStr(int numStr)
{
  time_t tnow = time(nullptr);
  struct tm *timeinfo;
  time(&tnow);
  timeinfo = localtime(&tnow);
  char timeStrRet[32];
  switch (numStr)
  {
  case 0:
    strftime(timeStrRet, 32, "%T", timeinfo); // ISO8601 (HH:MM:SS)
    break;
  case 1:
    strftime(timeStrRet, 32, "%R", timeinfo); // HH:MM
    break;
  case 3:
    strftime(timeStrRet, 32, "%H", timeinfo); // HH
    break;
  case 4:
    strftime(timeStrRet, 32, "%M", timeinfo); // MM
    break;
  case 5:
    strftime(timeStrRet, 32, "%S", timeinfo); // SS
    break;
  case 6:
    strftime(timeStrRet, 32, "%a %d %h", timeinfo); // Thu 23 Aug
    break;
  case 7:
    strftime(timeStrRet, 32, "%d.%m.%g", timeinfo); // 23.08.24
    break;
  case 8:
    strftime(timeStrRet, 32, "%d %h", timeinfo); // 23 Aug
    break;
  case 9:
    strftime(timeStrRet, 32, "%c", timeinfo); // Thu Aug 23 14:55:02 2001
    break;
  }
  return timeStrRet;
}

void commSerial()
{
  if (Serial.available())
  {
    String comm_serial = Serial.readStringUntil('\n');
    TRACE("[%s] > %s :\n", getTimeStr(0), comm_serial);
    int comm_case_s = 0;
    for (int i = 1; i < comm_qty; i++)
    {
      if (comm_array[i][0] == comm_serial)
      {
        comm_case_s = i;
        break;
      }
      else
      {
        comm_case_s = 0;
      }
    }

    switch (comm_case_s)
    {
    case 0:
    {
      TRACE("%s \"%s\"\n", comm_err, comm_serial);
      break;
    }
    case 1:
    {
      TRACE("%s %s\n", comm_about, build_date);
      break;
    }
    case 2:
    {
      TRACE("%s\n", term_clear);
      break;
    }
    case 3:
    {
      TRACE("%s %s !\n", comm_helo, client_ip.c_str());
      break;
    }
    case 4:
    {
      TRACE("Pong!\n");
      break;
    }
    case 5:
    {
      for (int j = 1; j < comm_qty; j++)
      {
        TRACE("[ %d ] %s\t- %s\n", j, comm_array[j][0], comm_array[j][1].c_str());
      }
      break;
    }
    case 6:
    {
      TRACE("%s\n", infoWiFi().c_str());
      break;
    }
    case 7:
    {
      //infoChip(5); // ag int: 5 - to Seial, 7 - to telnet
      TRACE("%s\n", infoChip().c_str());
      break;
    }
    case 8:
    {
      TRACE("This command will stop serial, proceed? yes/no\n");
      while (!Serial.available())
      {
        ;
      }
      String serial_switch = Serial.readStringUntil('\n');
      if (serial_switch == "yes")
      {
        TRACE("Serial will shut down\n");
        delay(1000);
        Serial.end();
        serial_switch = "";
      }
      else
      {
        TRACE("Serial status unchanged\n");
      }
      TRACE("(command under development)\n");
      break;
    }
    case 9:
    {
      TRACE("%s\n", getTimeStr(0));
      break;
    }
    case 10:
    {
      TRACE("%s\n", getTimeStr(7));
      break;
    }
    case 11:
    {
      if (!isConnected())
      {
        initWiFi(mssid, mpass, 20, 500);
      }
      else
      {
        TRACE("WiFi is already connected!\n");
      }
      break;
    }
    case 12:
    {
      if (isConnected())
      {
        WiFi.disconnect();
        digitalWrite(LED_BUILTIN, LOW);
        TRACE("WiFi disconnected\n");
      }
      else
      {
        TRACE("WiFi is already disconnected!\n");
      }
      break;
    }
    case 13:
    {
      TRACE("WiFi scan start at %s\n", getTimeStr(0));
      TRACE("%s\n", scanWiFi().c_str());
      TRACE("Scan complete.\n");
      WiFi.scanDelete();
      break;
    }
    case 14:
    {
      TRACE("WiFi RSSI: %d dBm\n", WiFi.RSSI());
      break;
    }
    case 15:
    {
      TRACE("%s\n", ESP.getChipModel());
      break;
    }
    case 16:
    {
      TRACE("ESP restarting in 3 sec\n");
      static int reboot_time = millis() + 3000;
      while (millis() < reboot_time)
      {
        TRACE(".");
        delay(200);
      }
      ESP.restart();
      break;
    }
    case 17:
    {
      TRACE("Starting soft reset MCU\n");
      setup();
    }
    break;
    case 18:
    {
      TRACE("System start: %s\n", boot_timestamp.c_str());
      TRACE("Uptime: %s\n", uptimeCount().c_str());
      break;
    }
    case 19:
    {
      TRACE("LCD dismantled\n");
      break;
    }
    case 20:
    {
      if (Serial)
      {
        TRACE("Serial is already up!\n");
      }
      else
      {
        setupSerial(0);
      }
      break;
    }
    case 21:
    {
      TRACE("Buttons are dismantled\n");
      break;
    }
    case 22:
    {
      TRACE("Sensors are dismantled\n");
      break;
    }
    case 23:
    {
      TRACE("Sensors are dismantled\n");
    }
    }
  }
}

void setup()
{
  setupSerial(0);
  setupSerial(1);
  initWiFi(mssid, mpass, 20, 500);
  WiFi.printDiag(Serial);

  if (isConnected())
  {
    ip = WiFi.localIP();
    TRACE("\n- Telnet: %s:%u\n", ip.toString().c_str(), port);
    setupTelnet();
  }
  else
  {
    TRACE("\n");
    errorMsg("Error connecting to WiFi");
  }

  TRACE("Setting up NTP time...");
  configTzTime(TIMEZONE, ntpHost0, ntpHost1, ntpHost2);
  TRACE("Done.\n");

  for (int i = 0; i < 40; i++)
  {
    TRACE("%c", '*');
    delay(25);
  }
  TRACE("\n");
  String greet = "     Welcome to ESP32-WROOM board!      ";
  for (char letter : greet)
  {
    TRACE("%c", letter);
    delay(25);
  }
  TRACE("\n");
  TRACE("Program built at %s %s\n", __DATE__ , __TIME__);

  for (int i = 0; i < 40; i++)
  {
    TRACE("%c", '*');
    delay(25);
  }
  TRACE("\n");

  boot_timestamp = getTimeStr(0) + ' ' + getTimeStr(6);
  boot_time = time(nullptr);

  TRACE("\nGuru meditates...\nESP32 Ready.\n");
}

void loop(void)
{
  telnet.loop();
  commSerial();
}

