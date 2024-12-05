/* Version 1.6

ESP32-S3-WROOM NTP CLI TEL, no LCD, no BTN

*/


#include <WIFi.h>
#include <Arduino.h>
#include <ESPTelnet.h>
#include <Adafruit_NeoPixel.h>
#include "cline.h"
#include "comm.h"

#define RXD1 18
#define TXD1 17

#define TIMEZONE "MSK-3"                      // local time zone definition (Moscow)

#define RGB_LED_PIN 48
#define NUM_RGB_LEDS 1 // number of RGB LEDs (1 WS2812 LED)

const char *ntpHost0 = "0.ru.pool.ntp.org";
const char *ntpHost1 = "1.ru.pool.ntp.org";
const char *ntpHost2 = "2.ru.pool.ntp.org";

const char *mssid = "SSID";
const char *mpass = "PASS";

IPAddress ip;
const uint16_t port = 23;
const int serial_speed = 115200;
String client_ip;
String boot_date;
time_t boot_time;

bool initWiFi(const char *, const char *, int, int);
bool isOnWiFi();

void setupSerial(int);
void setupTelnet();
void errorMsg(String, bool);
void onTelnetConnect(String ip);
void onTelnetDisconnect(String ip);
void onTelnetReconnect(String ip);
void onTelnetConnectionAttempt(String ip);
void onTelnetInput(String str);
void onSerialInput();
String commandHandler(String);
String getTimeStr(int);
String uptimeCount();
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
  } while (!isOnWiFi() && i < max_tries);
  WiFi.setSleep(WIFI_PS_NONE);
  TRACE("\nConnected!\n");
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  return isOnWiFi();
}

bool isOnWiFi()
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

void onTelnetConnect(const String ip)
{
  TRACE("- Telnet: %s connected\n", ip.c_str());
  TLNET("\nWelcome %s\n(Use ^] +q to disconnect)\n", telnet.getIP().c_str());
  client_ip = ip.c_str();
}

void onTelnetDisconnect(const String ip)
{
  TRACE("- Telnet: %s disconnected\n", ip.c_str());
}

void onTelnetReconnect(const String ip)
{
  TRACE("- Telnet: %s reconnected\n", ip.c_str());
}

void onTelnetConnectionAttempt(const String ip)
{
  TRACE("- Telnet: %s tried to connect\n", ip.c_str());
}

void onTelnetInput(const String comm_telnet)
{
  TLNET("[%s] > %s\n", getTimeStr(0), comm_telnet);
  TLNET("%s", commandHandler(comm_telnet).c_str());
}

void onSerialInput() {
  if (Serial.available())
  {
    String comm_serial = Serial.readStringUntil('\n');
    TRACE("[%s] > %s :\n", getTimeStr(0), comm_serial);
    TRACE("%s", commandHandler(comm_serial).c_str());
  }
}

String infoWiFi() {
  String wfinf("");
  if (isOnWiFi()) {
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

String commandHandler(const String comm_input) {
  int exec_case = 0;
  String comm_output("");
  for (int i = 1; i < comm_qty; i++) {
    if (comm_array[i][0] == comm_input) {
      exec_case = i;
      break;
    } else {
      exec_case = 0;
    }
  }

  switch (exec_case)
  {
    case 0:
    {
      comm_output += "Command \"" + comm_input + "\" not understood\n";
      break;
    }
    case 1:
    {
      comm_output += "ESP32-S3 Boad CLI. Built: " + String(__DATE__) + ' ' + String(__TIME__) + '\n';
      break;
    }
    case 2:
    {
      comm_output += term_clear;
      break;
    }
    case 3:
    {
      comm_output += "Hi, " + client_ip + '\n';
      break;
    }
    case 4:
    {
      comm_output += "Pong!\n";
      break;
    }
    case 5:
    {
      for (int k = 1; k < comm_qty; k++) {
        comm_output += "[ " + String(k) + " ] " + comm_array[k][0] + "\t - " + comm_array[k][1] + '\n';
      }
      break;
    }
    case 6:
    {
      comm_output += infoWiFi();
      comm_output += '\n';
      break;
    }
    case 7:
    {
      comm_output += infoChip();
      comm_output += '\n';
      break;
    }
    case 8:
    {
      comm_output += "Client " + client_ip + " disconnected\n";
      telnet.disconnectClient();
      break;
    }
    case 9:
    {
      comm_output += getTimeStr(0) + '\n';
      break;
    }
    case 10:
    {
      comm_output += getTimeStr(7) + '\n';
      break;
    }
    case 11:
    {
      if (!isOnWiFi()) {
        comm_output += "Connecting WiFi to " + String(mssid) + '\n';
        initWiFi(mssid, mpass, 20, 500);
      } else {
        comm_output += "WiFi is already connected!\n";
      }
      break;
    }
    case 12:
    {
      if (isOnWiFi()) {
        WiFi.disconnect();
        comm_output += "WiFi disconnected\n";
      } else {
        comm_output += "WiFi is already disconnected\n";
      }
      break;
    }
    case 13:
    {
      comm_output += "WiFi scan starts at " + getTimeStr(0) + '\n';
      comm_output += scanWiFi();
      comm_output += "Scan complete\n";
      break;
    }
    case 14:
    {
      comm_output += "RSSI: " + String(WiFi.RSSI()) + "dBm\n";
      break;
    }
    case 15:
    {
      comm_output += String(ESP.getChipModel()) + '\n';
      break;
    }
    case 16:
    {
      comm_output += "ESP restating in 3 sec\n";
      TRACE("%s", comm_output.c_str());
      if (telnet.isConnected()) {
        TLNET("%s", comm_output.c_str());
        TLNET("Disconnecting you!\n");
        telnet.disconnectClient();
      }
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
      comm_output+= "Running setup()\n";
      setup();
      break;
    }
    case 18:
    {
      comm_output += "System start : " + boot_date + '\n';
      comm_output += "System uptime: " + uptimeCount() + '\n';
      break;
    }
    case 19:
    {
      comm_output += "Considering LCD switched\n";
      break;
    }
    case 20:
    {
      if (Serial) {
        comm_output += "Serial is already up!\n";
      } else {
        setupSerial(0);
        comm_output += "Setting up Serial\n";
      }
      break;
    }
    case 21:
    {
      comm_output += "Considering Button test\n";
      break;
    }
    case 22:
    {
      comm_output += "Considering Sensor test\n";
      break;
    }
    case 23:
    {
      comm_output += "Considering sensor vals\n";
      break;
    }
  }
  return comm_output;
}

void setup()
{
  rgb_led.begin();
  rgb_led.setPixelColor(0, rgb_led.Color(64, 0, 0)); // Red
  rgb_led.show();
  setupSerial(0);
  setupSerial(1);
  initWiFi(mssid, mpass, 20, 500);
  WiFi.printDiag(Serial);

  if (isOnWiFi())
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

  boot_date = getTimeStr(0) + ' ' + getTimeStr(6);
  boot_time = time(nullptr);

  TRACE("\nGuru meditates...\nESP32 Ready.\n");
  for (int i = 0; i < 2; i++)
  {
    rgb_led.setPixelColor(0, rgb_led.Color(0, 64, 0));
    rgb_led.show();
    delay(50);
    rgb_led.setPixelColor(0, rgb_led.Color(0, 0, 0));
    rgb_led.show();
    delay(150);
  }
}

void loop(void)
{
  telnet.loop();
  onSerialInput();
}

