/* Version 1.6

ESP32-S3-N16R8 NTP CLI TEL, no LCD, no BTN

*/


#include <Arduino.h>
#include <ESPTelnet.h>
#include <Adafruit_NeoPixel.h>
#include <cstdint>
#include <cctype>
#include <string>
#include <vector>
#include <utility>
#include <sstream>
#include <algorithm>
//#include "cline.h"

#define TRACE(...) Serial.printf(__VA_ARGS__)
#define TLNET(...) telnet.printf(__VA_ARGS__)

#define RXD1 18
#define TXD1 17

#define TIMEZONE "MSK-3"                      // local time zone definition (Moscow)

#define RGB_LED_PIN 48
#define NUM_RGB_LEDS 1 // number of RGB LEDs (1 WS2812 LED)

std::vector<std::pair<std::string, std::string>> commands = {
    {"", "error"},                           // 0
    {"abt", "information about chip"},       // 1
    {"cls", "clear terminal screen"},        // 2
    {"helo", "greets you, showing ip"},      // 3
    {"ping", "echoes reply \"pong\""},       // 4
    {"help", "prints command help"},         // 5
    {"wfinf", "prints wifi info"},           // 6
    {"hwinf", "prints chip info"},           // 7
    {"bye", "disconnects your session"},     // 8
    {"time", "prints current time"},         // 9
    {"date", "prints current date"},         // 10
    {"wfcon", "connects to wifi"},           // 11
    {"wfoff", "turns off wifi"},             // 12
    {"wfscn", "scans for wifi networks"},    // 13
    {"rssi", "prints wifi RSSI"},            // 14
    {"uname", "prints chip name"},           // 15
    {"rboot", "reboots MCU"},                // 16
    {"reset", "soft reset MCU"},             // 17
    {"utime", "prints uptime"},              // 18
    {"lcdsw", "switch LCD on/off"},          // 19
    {"comon", "launch setup Serial"},        // 20
    {"testb", "seial show btn arr output"},  // 21
    {"tests", "test SHT30 sensor"},          // 22
    {"sens", "show sensor values"},          // 23
    {"blink", "[count] [length] [pause]"},   // 24
    {"beep", "[count] [length] [pause]"}     // 25
};

const char *ntpHost0 = "0.ru.pool.ntp.org";
const char *ntpHost1 = "1.ru.pool.ntp.org";
const char *ntpHost2 = "2.ru.pool.ntp.org";
const char* build_date = __DATE__ " " __TIME__;
const char* term_clear = "\033[2J\033[H";
const char *mssid = "SSID";
const char *mpass = "PASS";

IPAddress local_ip;
const uint16_t telnet_port = 23;
const int serial_speed = 115200;
String client_ip;
String boot_date;
time_t boot_time;

void errorMsg(String, bool);

bool initWiFi(const char *, const char *, int, int);
bool isWiFiOn();

void setupSerial(int, int);
void setupTelnet();

void onTelnetConnect(String ip);
void onTelnetDisconnect(String ip);
void onTelnetReconnect(String ip);
void onTelnetConnectionAttempt(String ip);
void onTelnetInput(String str);
void readSerial0();
std::string reduceString(std::string);
String commHandler(String);
String getTimeStr(int);
String uptimeCount();
String infoWiFi();
String scanWiFi();
String infoChip();
void ledBlink(int, int, int);

ESPTelnet telnet;
Adafruit_NeoPixel rgb_led = Adafruit_NeoPixel(NUM_RGB_LEDS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

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

bool initWiFi(const char *mssid, const char *mpass,
              int max_tries = 20, int pause = 500)
{
  int i = 0;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  TRACE("Connecting to Wifi:\n");
  WiFi.begin(mssid, mpass);
  do
  {
    delay(pause);
    TRACE(".");
    i++;
  } while (!isWiFiOn() && i < max_tries);
  WiFi.setSleep(WIFI_PS_NONE);
  TRACE("\nConnected!\n");
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  return isWiFiOn();
}

bool isWiFiOn()
{
  return (WiFi.status() == WL_CONNECTED);
}

void setupSerial(int uart_num, int uart_baud)
{
  if (uart_num == 0)
  {
    Serial.begin(uart_baud);
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
    Serial1.begin(uart_baud, SERIAL_8N1, RXD1, TXD1);
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

void setupTelnet()
{
  // passing on functions for various telnet events
  telnet.onConnect(onTelnetConnect);
  telnet.onConnectionAttempt(onTelnetConnectionAttempt);
  telnet.onReconnect(onTelnetReconnect);
  telnet.onDisconnect(onTelnetDisconnect);
  telnet.onInputReceived(onTelnetInput);

  //TRACE("- Telnet: ");
  if (telnet.begin(telnet_port))
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
  client_ip = ip;
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
  TLNET("%s", commHandler(comm_telnet).c_str());
}

void readSerial0() {
  if (Serial.available())
  {
    String comm_serial = Serial.readStringUntil('\n');
    TRACE("%s", commHandler(comm_serial).c_str());
  }
}

std::string reduceString(std::string str) {
  int i = 0;
  while (i < str.length()) {
    if (str[i] == str[i + 1] && str[i] == ' ') {
      str.erase(i + 1, 1);
    } else {
      i++;
    }
  }
  if (str[0] == ' ') {
    str.erase(0, 1);
  }
  return str;
}

String commHandler(const String comm_input) {
  
  std::string raw_input(comm_input.c_str());
  std::string cmd_input(reduceString(raw_input));
  std::stringstream cmd_strm(cmd_input);
  std::vector<std::string> cmd_vect;
  std::string token;

  while (getline(cmd_strm, token, ' ')) {
    cmd_vect.push_back(token);
  }

  String comm_output("");
  int exec_case = 0;
  for (int i = 1; i < commands.size(); i++) {
    if (commands[i].first == cmd_vect[0]) {
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
      for (int k = 1; k < commands.size(); k++) {
        if (k < 10) {
          comm_output += "[  " + String(k) + " ] " + String(commands[k].first.c_str()) + "\t - " + String(commands[k].second.c_str()) + '\n';
        } else {
          comm_output += "[ " + String(k) + " ] " + String(commands[k].first.c_str()) + "\t - " + String(commands[k].second.c_str()) + '\n';
        }
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
      if (!isWiFiOn()) {
        comm_output += "Connecting WiFi to " + String(mssid) + '\n';
        initWiFi(mssid, mpass, 20, 500);
      } else {
        comm_output += "WiFi is already connected!\n";
      }
      break;
    }
    case 12:
    {
      if (isWiFiOn()) {
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
        setupSerial(0, serial_speed);
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
    case 24:
    {
      int blk_count = 1;
      int blk_length = 50;
      int blk_pause = 100;
      if (cmd_vect.size() >= 2) {
        blk_count = stoi(cmd_vect[1]);
      }
      if (cmd_vect.size() >= 3) {
        blk_length = stoi(cmd_vect[2]);
      }
      if (cmd_vect.size() >= 4) {
        blk_pause = stoi(cmd_vect[3]);
      }
      ledBlink(blk_count, blk_length, blk_pause);
      comm_output += "Blink " + (String)blk_count + " times, " + \
      String(blk_length) + "ms length, " + (String)blk_pause +   \
      "ms pause\n";
      break;
    }
    case 25:
    {
      int sig_count = 1;
      int sig_length = 50;
      int sig_pause = 100;
      if (cmd_vect.size() >= 2) {
        sig_count = stoi(cmd_vect[1]);
      }
      if (cmd_vect.size() >= 3) {
        sig_length = stoi(cmd_vect[2]);
      }
      if (cmd_vect.size() >= 4) {
        sig_pause = stoi(cmd_vect[3]);
      }
      //signalBuzz(sig_count, sig_length, sig_pause);
      comm_output += "Beep " + (String)sig_count + " times, " \
      + (String)sig_length + "ms length, " + (String)sig_pause\
      + "ms pause\n";
      break;
    }
  }
  return comm_output;
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

String infoWiFi() {
  String wfinf("");
  if (isWiFiOn()) {
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

void ledBlink(int blink_count, int blink_length, int blink_pause) {
  
  for (int i = 0; i < blink_count; i++) {
    uint32_t blink_start = millis();
    uint32_t blink_stop = blink_start + blink_length;
    while (millis() <= blink_stop) {
      rgb_led.setPixelColor(0, rgb_led.Color(0, 128, 0));
      rgb_led.show();
    }
    while (millis() < blink_stop + blink_pause) {
      rgb_led.setPixelColor(0, rgb_led.Color(0, 0, 0));
      rgb_led.show();
    }
  }
  rgb_led.clear();
}

void setup()
{
  rgb_led.begin();
  rgb_led.setPixelColor(0, rgb_led.Color(64, 0, 0)); // Red
  rgb_led.show();
  setupSerial(0, serial_speed);
  setupSerial(1, serial_speed);
  initWiFi(mssid, mpass, 20, 500);
  WiFi.printDiag(Serial);

  if (isWiFiOn())
  {
    local_ip = WiFi.localIP();
    TRACE("\n- Telnet: %s:%u\n", local_ip.toString().c_str(), telnet_port);
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
  readSerial0();
}
