#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <ESP32Ping.h>

#define FIRMWARE_URL1 "http://192.168.1.39/uploads/firmware/firmware_metadata.json"  // URL to JSON metadata file
String current_version = "1.2.2";

// URL for the JSON status
const char *jsonURL = "http://192.168.1.39/uploads/firmware/status.json";  // location of status.json from batch script

// Deep sleep settings
#define TIME_TO_SLEEP 1785       // For testing, sleep 180 seconds (adjust as needed) 1785
#define TIME_TO_SLEEP_NoWIFI 30  // Sleep only 30 seconds if no WiFi
#define TIMEOUT_MS 30000         // 30-second inactivity timeout
#define uS_TO_S_FACTOR 1000000   /* Conversion factor for micro seconds to seconds */

// Button configuration
#define BUTTON_PIN 39          // Pin for button input
#define DEBOUNCE_DELAY 50      // milliseconds
#define DOUBLE_PRESS_TIME 500  // maximum time (ms) to detect a double press

// Network credentials
const char *ssid = "ssid";       // Enter your wifi ssid name here
const char *password = "password";  // Enter your wifi password here
const char *MyHostName = "UNRAIDServerMonitor";
IPAddress unRaidHost(192, 168, 1, 101);  // Enter your unraid IP here

UBYTE *partialBuffer = NULL;  // Buffer for partial refresh

//---------------------------------------------------------------------
// Global structures for status data
struct ServerStatus {
  String timestamp;
  String cpu_load;
  String cpu_model;
  String mem_total;
  String mem_used;
  String mem_free;
  String disk_free;
  String docker_vdisk_size;
  String ip;
  String name;
};

struct DockerStatus {
  String status;
  String cpu;
  String mem_usage;
  String mem_limit;
  String disk_free;
  String ip;
  String name;
  String disk_size;
  String disk_used;
};

struct VMStatus {
  String name;
  String state;
  String cpus;
  String max_memory_mb;
  String autostart;
  String ip;
};

// extract the following from JSON.
ServerStatus serverStatus;
DockerStatus jellyfinStatus, jellyseerrStatus, gluetunvpnStatus, immichStatus;
VMStatus vmStatus[3];  // Array to hold first 3 VMs

// Selected server index (0 to 5)
int selectedServer = 0;

bool httpFailed = false;

void wifiInit();
void ePaperInit();
void allocateMemoryForImages(UBYTE **BlackImage, UBYTE **RYImage, uint16_t Imagesize);
void drawBatteryAndNoWifi(int batteryLevel, String batLevel);
void drawMainServerStatus(int batteryLevel, String batLevel);
void drawTimeDate();
void drawBatteryBlack(int batteryLevel, String batLevel);
void drawBatteryRed(int batteryLevel, String batLevel);
void drawServerStatusONLINE(String serverText[]);
void drawServerStatusOFFLINE(String serverText[]);
void checkForFirmwareUpdate();
void performOTAUpdate(const String &url);
bool waitForButtonPress(unsigned long &pressTime);
bool isDoublePress(unsigned long firstPressTime);
void updateSelection(int selectedServer, int batteryLevel, String batLevel);
void drawServerTemplate(int selectedServer, int batteryLevel, String batLevel);
void drawDockerContainer(const String &containerTitle, const DockerStatus &status, int startY);
void drawUNRAID();
void drawVM();
void drawOffline(int batteryLevel, String batLevel, String nameOFFLINE);
bool fetchStatusFromJson();

//---------------------------------------------------------------------
// Function to fetch and parse status JSON
bool fetchStatusFromJson() {
  const int maxAttempts = 5;
  int attempt = 0;
  bool success = false;

  while (attempt <= maxAttempts && !success) {
    attempt++;
    Serial.print("Attempt ");
    Serial.print(attempt);
    Serial.println(" to fetch JSON...");

    HTTPClient http;
    http.begin(jsonURL);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      // Adjust document size if necessary
      DynamicJsonDocument doc(4096);
      DeserializationError error = deserializeJson(doc, payload);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        http.end();
        delay(1000);  // short delay before retrying
        continue;
      }
      // Server section
      serverStatus.timestamp = doc["timestamp"].as<String>();
      serverStatus.cpu_load = doc["server"]["cpu_load"].as<String>();
      serverStatus.cpu_model = doc["server"]["cpu_model"].as<String>();
      serverStatus.mem_total = doc["server"]["memory"]["total_mb"].as<String>();
      serverStatus.mem_used = doc["server"]["memory"]["used_mb"].as<String>();
      serverStatus.mem_free = doc["server"]["memory"]["free_mb"].as<String>();
      serverStatus.disk_free = doc["server"]["disk_free"].as<String>();
      serverStatus.docker_vdisk_size = doc["server"]["docker_vdisk_size"].as<String>();
      serverStatus.ip = "192.168.1.101";
      serverStatus.name = "UNRAID";

      // Docker containers - Jellyfin
      jellyfinStatus.status = doc["jellyfin"]["status"].as<String>();
      jellyfinStatus.cpu = doc["jellyfin"]["cpu"].as<String>();
      jellyfinStatus.mem_usage = doc["jellyfin"]["memory"]["usage"].as<String>();
      jellyfinStatus.mem_limit = doc["jellyfin"]["memory"]["limit"].as<String>();
      jellyfinStatus.disk_free = doc["jellyfin"]["disk_free"].as<String>();
      jellyfinStatus.ip = doc["jellyfin"]["ip"].as<String>();
      jellyfinStatus.name = "Jellyfin";
      jellyfinStatus.disk_size = doc["jellyfin"]["disk_size"].as<String>();
      jellyfinStatus.disk_used = doc["jellyfin"]["disk_used"].as<String>();

      // Docker containers - Jellyseerr
      jellyseerrStatus.status = doc["jellyseerr"]["status"].as<String>();
      jellyseerrStatus.cpu = doc["jellyseerr"]["cpu"].as<String>();
      jellyseerrStatus.mem_usage = doc["jellyseerr"]["memory"]["usage"].as<String>();
      jellyseerrStatus.mem_limit = doc["jellyseerr"]["memory"]["limit"].as<String>();
      jellyseerrStatus.disk_free = doc["jellyseerr"]["disk_free"].as<String>();
      jellyseerrStatus.ip = doc["jellyseerr"]["ip"].as<String>();
      jellyseerrStatus.name = "Jellyseerr";
      jellyseerrStatus.disk_size = doc["jellyseerr"]["disk_size"].as<String>();
      jellyseerrStatus.disk_used = doc["jellyseerr"]["disk_used"].as<String>();

      // Docker containers - GluetunVPN
      gluetunvpnStatus.status = doc["gluetunvpn"]["status"].as<String>();
      gluetunvpnStatus.cpu = doc["gluetunvpn"]["cpu"].as<String>();
      gluetunvpnStatus.mem_usage = doc["gluetunvpn"]["memory"]["usage"].as<String>();
      gluetunvpnStatus.mem_limit = doc["gluetunvpn"]["memory"]["limit"].as<String>();
      gluetunvpnStatus.disk_free = doc["gluetunvpn"]["disk_free"].as<String>();
      gluetunvpnStatus.ip = doc["gluetunvpn"]["ip"].as<String>();
      gluetunvpnStatus.name = "VPN Server";
      gluetunvpnStatus.disk_size = doc["gluetunvpn"]["disk_size"].as<String>();
      gluetunvpnStatus.disk_used = doc["gluetunvpn"]["disk_used"].as<String>();

      // Docker containers - Immich
      immichStatus.status = doc["immich"]["status"].as<String>();
      immichStatus.cpu = doc["immich"]["cpu"].as<String>();
      immichStatus.mem_usage = doc["immich"]["memory"]["usage"].as<String>();
      immichStatus.mem_limit = doc["immich"]["memory"]["limit"].as<String>();
      immichStatus.disk_free = doc["immich"]["disk_free"].as<String>();
      immichStatus.ip = doc["immich"]["ip"].as<String>();
      immichStatus.name = "immich";
      immichStatus.disk_size = doc["immich"]["disk_size"].as<String>();
      immichStatus.disk_used = doc["immich"]["disk_used"].as<String>();

      // Parse the first 3 VM entries from the vms array and get their IP addresses
      JsonArray vms = doc["vms"].as<JsonArray>();
      for (int i = 0; i < 3; i++) {
        if (i < vms.size()) {
          vmStatus[i].name = vms[i]["name"].as<String>();
          vmStatus[i].state = vms[i]["state"].as<String>();
          vmStatus[i].cpus = vms[i]["cpus"].as<String>();
          vmStatus[i].max_memory_mb = vms[i]["max_memory_mb"].as<String>();
          vmStatus[i].autostart = vms[i]["autostart"].as<String>();
          vmStatus[i].ip = vms[i]["ip"].as<String>();  // New field for VM IP
        } else {
          vmStatus[i].name = "";
          vmStatus[i].state = "";
          vmStatus[i].cpus = "";
          vmStatus[i].max_memory_mb = "";
          vmStatus[i].autostart = "";
          vmStatus[i].ip = "";
        }
      }
      http.end();
      success = true;
      return true;
    } else {
      Serial.print("HTTP GET failed, error code: ");
      Serial.println(httpCode);
      http.end();
      if (attempt == maxAttempts) {
        httpFailed = true;
      }
      delay(1000);
    }
  }
  return false;
}

// Helper function: Format a value in GB to a string with appropriate unit.
String formatStorage(float valueInGB) {
  String result;
  if (valueInGB < 1.0) {
    // Convert to MB and show no decimals.
    float valueInMB = valueInGB * 1024.0;
    result = String(valueInMB, 0) + "MB";
  } else if (valueInGB >= 1024.0) {
    // Convert to TB and show one decimal.
    float valueInTB = valueInGB / 1024.0;
    result = String(valueInTB, 1) + "TB";
  } else {
    // Value in GB; show no decimals.
    result = String(valueInGB, 1) + "GB";
  }
  return result;
}

// Helper function: Convert a string with a unit (GB, MB, TB) to a float value in GB.
float convertToGB(String val) {
  val.trim();
  float num = val.toFloat();
  if (val.indexOf("TB") != -1) {
    return num * 1024.0;  // 1 TB = 1024 GB
  } else if (val.indexOf("MB") != -1) {
    return num / 1024.0;  // 1024 MB = 1 GB
  } else if (val.indexOf("GB") != -1) {
    return num;
  } else {
    return num;  // Default assume GB if no unit found
  }
}

//function refactoring
void wifiInit() {
  WiFi.setHostname(MyHostName);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
}

void ePaperInit() {
  if (DEV_Module_Init() != 0) {
    while (1)
      ;
  }
  EPD_2IN9B_V4_Init();
  EPD_2IN9B_V4_Clear();
  DEV_Delay_ms(500);
}


void drawOffline(int batteryLevel, String batLevel, String nameOFFLINE) {
  UBYTE *BlackImage, *RYImage;
  UWORD Imagesize = ((EPD_2IN9B_V4_WIDTH % 8 == 0) ? (EPD_2IN9B_V4_WIDTH / 8) : (EPD_2IN9B_V4_WIDTH / 8 + 1)) * EPD_2IN9B_V4_HEIGHT;
  allocateMemoryForImages(&BlackImage, &RYImage, Imagesize);

  Paint_NewImage(BlackImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
  Paint_NewImage(RYImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);

  // *----------- Draw black image ------------*
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);
  drawBatteryBlack(batteryLevel, batLevel);
  Paint_DrawRectangle(7, 17, 290, 113, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);

  // *----------- Draw red image ------------*
  Paint_SelectImage(RYImage);
  Paint_Clear(WHITE);

  Paint_DrawString_EN(0, 0, (String("IP:") + WiFi.localIP().toString() + "     Server Monitor v" + current_version).c_str(), &Font12, BLACK, WHITE);

  Paint_DrawString_EN(10, 50, nameOFFLINE.c_str(), &Font16, BLACK, WHITE);
  Paint_DrawString_EN(10, 65, "IS OFFLINE!", &Font16, BLACK, WHITE);
  drawBatteryRed(batteryLevel, batLevel);
  EPD_2IN9B_V4_Display_Base(BlackImage, RYImage);
  //EPD_2IN9B_V4_Display(BlackImage, RYImage);
  DEV_Delay_ms(2000);
  EPD_2IN9B_V4_Sleep();
  free(BlackImage);
  free(RYImage);
  DEV_Delay_ms(2000);  //important, at least 2s
  esp_deep_sleep_start();
}

void drawBatteryAndNoWifi(int batteryLevel, String batLevel) {
  UBYTE *BlackImage, *RYImage;
  UWORD Imagesize = ((EPD_2IN9B_V4_WIDTH % 8 == 0) ? (EPD_2IN9B_V4_WIDTH / 8) : (EPD_2IN9B_V4_WIDTH / 8 + 1)) * EPD_2IN9B_V4_HEIGHT;
  allocateMemoryForImages(&BlackImage, &RYImage, Imagesize);

  Paint_NewImage(BlackImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
  Paint_NewImage(RYImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);

  // *----------- Draw black image ------------*
  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);
  drawBatteryBlack(batteryLevel, batLevel);
  Paint_DrawRectangle(7, 17, 290, 113, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);

  // *----------- Draw red image ------------*
  Paint_SelectImage(RYImage);
  Paint_Clear(WHITE);

  String titleIPString = "Not connected";
  Paint_DrawString_EN(0, 0, titleIPString.c_str(), &Font12, BLACK, WHITE);

  Paint_DrawString_EN(10, 50, "WiFi connection is not", &Font16, BLACK, WHITE);
  Paint_DrawString_EN(10, 65, "available", &Font16, BLACK, WHITE);
  drawBatteryRed(batteryLevel, batLevel);
  EPD_2IN9B_V4_Display_Base(BlackImage, RYImage);
  //EPD_2IN9B_V4_Display(BlackImage, RYImage);
  DEV_Delay_ms(2000);
  EPD_2IN9B_V4_Sleep();
  free(BlackImage);
  free(RYImage);
  DEV_Delay_ms(2000);  //important, at least 2s
  esp_deep_sleep_start();
}

void allocateMemoryForImages(UBYTE **BlackImage, UBYTE **RYImage, UWORD Imagesize) {
  *BlackImage = (UBYTE *)malloc(Imagesize);
  *RYImage = (UBYTE *)malloc(Imagesize);
  if (*BlackImage == NULL || *RYImage == NULL) {
    Serial.println("Memory allocation failed");
    while (1)
      ;
  }
}

// --- Button Functions ---
bool waitForButtonPress(unsigned long &pressTime) {
  unsigned long start = millis();
  while (millis() - start < TIMEOUT_MS) {
    if (digitalRead(BUTTON_PIN) == HIGH) {  // HIGH means pressed
      delay(DEBOUNCE_DELAY);
      if (digitalRead(BUTTON_PIN) == HIGH) {
        pressTime = millis();
        // Wait until the button is released
        while (digitalRead(BUTTON_PIN) == HIGH) { delay(10); }
        return true;
      }
    }
    delay(10);
  }
  Serial.println("No button press detected for 30 seconds. Going to sleep.");
  //esp_deep_sleep_start();
  return false;  // Not reached
}

bool isDoublePress(unsigned long firstPressTime) {
  unsigned long start = millis();
  while (millis() - start < DOUBLE_PRESS_TIME) {
    if (digitalRead(BUTTON_PIN) == HIGH) {  // HIGH means pressed
      delay(DEBOUNCE_DELAY);
      if (digitalRead(BUTTON_PIN) == HIGH) {
        // Wait for release
        while (digitalRead(BUTTON_PIN) == HIGH) { delay(10); }
        return true;
      }
    }
    delay(10);
  }
  return false;
}


// --- updateSelection ---
// Re-draws the full-screen main status using the partial update buffer.
// It draws the server list (all 6 servers) with server texts at x = 10 (as in base refresh)
// and overlays a selector on top the currently selected row.
void updateSelection(int selectedServer, int batteryLevel, String batLevel) {
  uint16_t Imagesize = ((EPD_2IN9B_V4_WIDTH % 8 == 0) ? (EPD_2IN9B_V4_WIDTH / 8) : (EPD_2IN9B_V4_WIDTH / 8 + 1)) * EPD_2IN9B_V4_HEIGHT;
  Paint_NewImage(partialBuffer, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
  Paint_SelectImage(partialBuffer);
  Paint_Clear(WHITE);
  Serial.println("update section");
  String serverText[] = {
    (serverStatus.name + " " + serverStatus.ip),
    (jellyfinStatus.status == "online" ? jellyfinStatus.name + " " + jellyfinStatus.ip : jellyfinStatus.name + " is OFFLINE"),
    (jellyseerrStatus.status == "online" ? jellyseerrStatus.name + " " + jellyseerrStatus.ip : jellyseerrStatus.name + " is OFFLINE"),
    (immichStatus.status == "online" ? immichStatus.name + " " + immichStatus.ip : immichStatus.name + " is OFFLINE"),
    (gluetunvpnStatus.status == "online" ? gluetunvpnStatus.name + " " + gluetunvpnStatus.ip : gluetunvpnStatus.name + "s are OFFLINE"),
    (vmStatus[0].state == "running" ? "Virtual Machines" : "VMs are OFFLINE")
  };

  for (int i = 0; i < 6; i++) {
    if (i == selectedServer) {
      /*if (blink) { // removed blinking idea for now      
        //Paint_DrawString_EN(5, 20 + selectedServer * 15, ".", &Font16, BLACK, WHITE);
        Paint_DrawString_EN(10, 20 + selectedServer * 15, serverText[selectedServer], &Font16, BLACK, WHITE);
      }*/
      Paint_DrawString_EN(10, 20 + selectedServer * 15, serverText[selectedServer].c_str(), &Font16, BLACK, WHITE);
    } else {
      if (String(serverText[i]).indexOf("OFFLINE") >= 0) {  // partial refresh issue
        //Paint_DrawString_EN(10, 20 + i * 15, serverText[i].c_str(), &Font16, BLACK, WHITE);
        // DEV_Delay_ms(500);
        Paint_DrawString_EN(10, 20 + i * 15, serverText[i].c_str(), &Font16, WHITE, BLACK);
      } else {
        Paint_DrawString_EN(10, 20 + i * 15, serverText[i].c_str(), &Font16, WHITE, BLACK);
      }
    }
  }

  Paint_DrawRectangle(7, 17, 290, 113, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);

  // Draw time and battery in black
  drawBatteryBlack(batteryLevel, batLevel);
  drawTimeDate();

  EPD_2IN9B_V4_Display_Partial(partialBuffer, 0, 0, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT);
  DEV_Delay_ms(500);
}

// Reusable function to draw a Docker container's stats
void drawDockerContainer(const String &containerTitle, const DockerStatus &status, int startY) {
  // Define bar parameters
  int barMaxWidth = 65;                    // Maximum width of a bar (in pixels)
  int barHeight = 10;                      // Height of each bar
  int startX = 10;                         // Left margin for bars
  int labelX = startX + barMaxWidth + 10;  // X coordinate for text labels

  // --- CPU Usage Bar ---
  String cpuStr = status.cpu;
  cpuStr.replace("%", "");
  float cpuPerc = cpuStr.toFloat();  // Percentage value (0 to 100)
  int cpuBarWidth = (int)(barMaxWidth * cpuPerc / 100.0);
  int cpuY = startY;
  Paint_DrawRectangle(startX, cpuY, startX + barMaxWidth, cpuY + barHeight, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  if (cpuBarWidth > 0) {
    Paint_DrawRectangle(startX, cpuY, startX + cpuBarWidth, cpuY + barHeight, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  }
  String cpuLabel = "CPU:" + status.cpu;
  Paint_DrawString_EN(labelX - 5, cpuY - 3, cpuLabel.c_str(), &Font16, WHITE, BLACK);

  // --- RAM Usage Bar ---
  // Assume mem_usage and mem_limit are strings like "2.76GB" or "134.1MB"
  // Convert both values to MB first.
  String memUsageStr = status.mem_usage;
  String memLimitStr = status.mem_limit;
  float memUsage, memLimit;

  if (memUsageStr.indexOf("GB") != -1) {
    memUsageStr.replace("GB", "");
    memUsage = memUsageStr.toFloat() * 1024.0;  // Convert GB to MB
  } else if (memUsageStr.indexOf("MB") != -1) {
    memUsageStr.replace("MB", "");
    memUsage = memUsageStr.toFloat();
  } else {
    memUsage = memUsageStr.toFloat();
  }

  if (memLimitStr.indexOf("GB") != -1) {
    memLimitStr.replace("GB", "");
    memLimit = memLimitStr.toFloat() * 1024.0;  // Convert GB to MB
  } else if (memLimitStr.indexOf("MB") != -1) {
    memLimitStr.replace("MB", "");
    memLimit = memLimitStr.toFloat();
  } else {
    memLimit = memLimitStr.toFloat();
  }

  float ramPerc = (memLimit > 0) ? (memUsage / memLimit * 100.0) : 0;
  float ramRemain = memLimit - memUsage;
  int ramBarWidth = (int)(barMaxWidth * ramPerc / 100.0);
  int ramY = cpuY + barHeight + 15;

  Paint_DrawRectangle(startX, ramY, startX + barMaxWidth, ramY + barHeight, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  if (ramBarWidth > 0) {
    Paint_DrawRectangle(startX, ramY, startX + ramBarWidth, ramY + barHeight, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  }

  // Calculate new field mem_used for display.
  // Convert memUsage (in MB) to GB:
  float memUsageGB = memUsage / 1024.0;
  float memLimitGB = memLimit / 1024.0;
  float ramRemainGB = ramRemain / 1024.0;
  String memUsedFormatted = formatStorage(memUsageGB);
  String memLimitFormatted = formatStorage(memLimitGB);

  // Build label with the new calculated mem_used
  String ramLabel = "RAM:" + memUsedFormatted + "/" + memLimitFormatted;

  if (ramLabel.length() > 19) {
    ramLabel = ramLabel.substring(0, 19);
  }
  String ramSmallLabel = formatStorage(ramRemainGB);
  Paint_DrawString_EN(labelX - 5, ramY - 3, ramLabel.c_str(), &Font16, WHITE, BLACK);
  Paint_DrawString_EN(labelX - 76, ramY - 9, ramSmallLabel.c_str(), &Font8, WHITE, BLACK);


  // --- Disk Used Bar ---
  // Now using the new fields: disk_size and disk_used.
  // The bar shows how much of the disk is used.
  String diskSizeStr = status.disk_size;
  String diskUsedStr = status.disk_used;
  float totalGB = convertToGB(diskSizeStr);
  float usedGB = convertToGB(diskUsedStr);
  float freeGB = convertToGB(String(status.disk_free));
  float usedPerc = (totalGB > 0) ? (usedGB / totalGB * 100.0) : 0;
  int diskBarWidth = (int)(barMaxWidth * usedPerc / 100.0);
  int diskY = ramY + barHeight + 15;

  Paint_DrawRectangle(startX, diskY, startX + barMaxWidth, diskY + barHeight, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  if (diskBarWidth > 0) {
    Paint_DrawRectangle(startX, diskY, startX + diskBarWidth, diskY + barHeight, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
  }

  // Build a label that shows total disk size and used space.
  String diskLabel = "Disk:" + formatStorage(usedGB) + "/" + formatStorage(totalGB);
  String smallLabel = formatStorage(freeGB);
  if (diskLabel.length() > 19) {
    diskLabel = diskLabel.substring(0, 19);
  }
  Paint_DrawString_EN(labelX - 5, diskY - 3, diskLabel.c_str(), &Font16, WHITE, BLACK);
  Paint_DrawString_EN(labelX - 76, diskY - 9, smallLabel.c_str(), &Font8, WHITE, BLACK);
}

void drawUNRAID() {
  // Build text lines with serverStatus values.
  float memLimit = convertToGB(serverStatus.mem_total);
  float memUsed = convertToGB(serverStatus.mem_used);
  float diskFree = convertToGB(serverStatus.disk_free);
  float dockerSize = convertToGB(serverStatus.docker_vdisk_size);
  String line1 = serverStatus.cpu_model;
  String line2 = "CPU:" + serverStatus.cpu_load + ", Docker:" + formatStorage(dockerSize);

  String line3 = "RAM Used:" + formatStorage(memUsed) + "/" + formatStorage(memLimit);
  line3.replace("TB", "GB");
  String line4 = "Disk Free:" + formatStorage(diskFree);

  // Draw the text inside the rectangle.
  Paint_DrawString_EN(10, 35, line1.c_str(), &Font12, BLACK, WHITE);
  Paint_DrawString_EN(10, 50, line2.c_str(), &Font16, WHITE, BLACK);
  Paint_DrawString_EN(10, 70, line3.c_str(), &Font16, WHITE, BLACK);
  Paint_DrawString_EN(10, 90, line4.c_str(), &Font16, WHITE, BLACK);
}

void drawVM() {
  // Build text lines with serverStatus values.
  String title0 = vmStatus[0].name + " " + vmStatus[0].ip;
  String vm0 = "Cores:" + vmStatus[0].cpus + ",Mem:" + vmStatus[0].max_memory_mb + "," + vmStatus[0].state;

  String title1 = vmStatus[1].name + " " + vmStatus[1].ip;
  String vm1 = "Cores:" + vmStatus[1].cpus + ",Mem:" + vmStatus[1].max_memory_mb + "," + vmStatus[1].state;

  String title2 = vmStatus[2].name + " " + vmStatus[2].ip;
  String vm2 = "Cores:" + vmStatus[2].cpus + ",Mem:" + vmStatus[2].max_memory_mb + "," + vmStatus[2].state;

  // Draw the text inside the rectangle.
  Paint_DrawString_EN(10, 20, title0.c_str(), &Font16, BLACK, WHITE);
  Paint_DrawString_EN(10, 37, vm0.c_str(), &Font16, WHITE, BLACK);

  Paint_DrawString_EN(10, 52, title1.c_str(), &Font16, BLACK, WHITE);
  Paint_DrawString_EN(10, 69, vm1.c_str(), &Font16, WHITE, BLACK);

  Paint_DrawString_EN(10, 82, title2.c_str(), &Font16, BLACK, WHITE);
  Paint_DrawString_EN(10, 99, vm2.c_str(), &Font16, WHITE, BLACK);
}

void drawServerTemplate(int selectedServer, int batteryLevel, String batLevel) {
  String serverText[] = {
    serverStatus.name + " " + serverStatus.ip,
    jellyfinStatus.name + " " + jellyfinStatus.ip,
    jellyseerrStatus.name + " " + jellyseerrStatus.ip,
    immichStatus.name + " " + immichStatus.ip,
    "VPN " + gluetunvpnStatus.ip,
    "Virtual Machines"
  };
  uint16_t Imagesize = ((EPD_2IN9B_V4_WIDTH % 8 == 0) ? (EPD_2IN9B_V4_WIDTH / 8) : (EPD_2IN9B_V4_WIDTH / 8 + 1)) * EPD_2IN9B_V4_HEIGHT;
  Paint_NewImage(partialBuffer, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
  Paint_SelectImage(partialBuffer);
  Paint_Clear(WHITE);


  if (String(serverText[selectedServer]).indexOf("UNRAID") >= 0) {
    Paint_DrawString_EN(10, 20, serverText[selectedServer].c_str(), &Font16, BLACK, WHITE);
    drawUNRAID();

  } else if (String(serverText[selectedServer]).indexOf("Jellyfin") >= 0) {
    Paint_DrawString_EN(10, 20, serverText[selectedServer].c_str(), &Font16, BLACK, WHITE);
    drawDockerContainer("Jellyfin", jellyfinStatus, 45);

  } else if (String(serverText[selectedServer]).indexOf("Jellyseerr") >= 0) {
    Paint_DrawString_EN(10, 20, serverText[selectedServer].c_str(), &Font16, BLACK, WHITE);
    drawDockerContainer("Jellyseerr", jellyseerrStatus, 45);

  } else if (String(serverText[selectedServer]).indexOf("immich") >= 0) {
    Paint_DrawString_EN(10, 20, serverText[selectedServer].c_str(), &Font16, BLACK, WHITE);
    drawDockerContainer("immich", immichStatus, 45);

  } else if (String(serverText[selectedServer]).indexOf("VPN") >= 0) {
    Paint_DrawString_EN(10, 20, serverText[selectedServer].c_str(), &Font16, BLACK, WHITE);
    drawDockerContainer("GluetunVPN", gluetunvpnStatus, 45);

  } else {
    drawVM();
  }

  // Draw time and battery in black
  Paint_DrawRectangle(7, 17, 290, 113, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  drawBatteryBlack(batteryLevel, batLevel);
  drawTimeDate();

  //EPD_2IN9B_V4_Init();
  EPD_2IN9B_V4_Display_Partial(partialBuffer, 0, 0, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT);
  DEV_Delay_ms(500);

  Serial.println("Entering selection loop. Waiting for button press...");

  while (true) {
    unsigned long pressTime = 0;
    // waitForButtonPress() waits for a press up to TIMEOUT_MS.
    if (!waitForButtonPress(pressTime))
      break;  // deep sleep will be triggered on timeout

    // consider polling here until the display update is finished.

    if (isDoublePress(pressTime)) {
      Serial.println("Double press detected. Showing dummy template.");
      drawServerTemplate(selectedServer, batteryLevel, batLevel);
      break;
    } else {
      // Single press: increment the selection.
      selectedServer = (selectedServer + 1) % 6;
      updateSelection(selectedServer, batteryLevel, batLevel);
      // Note: updateSelection() is blocking until the epaper is ready.
    }
  }
}

//-------------------- Integrated mainServerStatus --------------------
// This function draws the main server status (full refresh) and then
// checks for a button press. If no button is pressed within the timeout,
// it goes to deep sleep. If a button press is detected, it enters selection mode
// (partial update) to allow cycling the selector.
void drawMainServerStatus(int batteryLevel, String batLevel) {
  uint16_t Imagesize = ((EPD_2IN9B_V4_WIDTH % 8 == 0) ? (EPD_2IN9B_V4_WIDTH / 8) : (EPD_2IN9B_V4_WIDTH / 8 + 1)) * EPD_2IN9B_V4_HEIGHT;
  UBYTE *BlackImage = (UBYTE *)malloc(Imagesize);
  UBYTE *RYImage = (UBYTE *)malloc(Imagesize);
  if (BlackImage == NULL || RYImage == NULL) {
    Serial.println("Memory allocation failed for base refresh");
    while (1)
      ;
  }

  // Base refresh: create full-screen images.
  Paint_NewImage(BlackImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
  Paint_NewImage(RYImage, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);

  Paint_SelectImage(BlackImage);
  Paint_Clear(WHITE);

  // Draw a black border box.
  Paint_DrawRectangle(7, 17, 290, 113, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);

  String serverText[] = {
    (serverStatus.name + " " + serverStatus.ip),
    (jellyfinStatus.status == "online" ? jellyfinStatus.name + " " + jellyfinStatus.ip : jellyfinStatus.name + " is OFFLINE"),
    (jellyseerrStatus.status == "online" ? jellyseerrStatus.name + " " + jellyseerrStatus.ip : jellyseerrStatus.name + " is OFFLINE"),
    (immichStatus.status == "online" ? immichStatus.name + " " + immichStatus.ip : immichStatus.name + " is OFFLINE"),
    (gluetunvpnStatus.status == "online" ? gluetunvpnStatus.name + " " + gluetunvpnStatus.ip : gluetunvpnStatus.name + "s are OFFLINE"),
    (vmStatus[0].state == "running" ? "Virtual Machines" : "VMs are OFFLINE")
  };

  // Draw black image elements
  drawServerStatusONLINE(serverText);
  drawBatteryBlack(batteryLevel, batLevel);
  drawTimeDate();

  // Draw red image elements.
  Paint_SelectImage(RYImage);
  Paint_Clear(WHITE);
  drawServerStatusOFFLINE(serverText);
  drawBatteryRed(batteryLevel, batLevel);
  Paint_DrawString_EN(0, 0, (String("IP:") + WiFi.localIP().toString() + "     Server Monitor v" + current_version).c_str(), &Font12, BLACK, WHITE);

  EPD_2IN9B_V4_Init();
  EPD_2IN9B_V4_Display_Base(BlackImage, RYImage);
  DEV_Delay_ms(3000);


  free(BlackImage);
  free(RYImage);
  partialBuffer = (UBYTE *)malloc(Imagesize);
  if (partialBuffer == NULL) {
    Serial.println("Failed to allocate partial update buffer");
    while (1)
      ;
  }

  // Loop waiting for button presses
  int selectedServer = -1;
  // Free base refresh buffers if no button press.
  unsigned long firstPressTime = 0;

  // In drawMainServerStatus(), after the base refresh:
  Serial.println("Entering selection loop. Waiting for button press...");
  bool pressedButton = false;
  while (true) {

    unsigned long pressTime = 0;
    // waitForButtonPress() waits for a press up to TIMEOUT_MS.
    if (!waitForButtonPress(pressTime))
      break;  // deep sleep will be triggered on timeout

    // consider polling here until the display update is finished.

    if (isDoublePress(pressTime)) {
      Serial.println("Double press detected. Showing dummy template.");
      pressedButton = true;
      drawServerTemplate(selectedServer, batteryLevel, batLevel);
      break;  // Exit loop after handling double press.
    } else {
      // Single press: increment the selection.
      pressedButton = true;
      selectedServer = (selectedServer + 1) % 6;
      updateSelection(selectedServer, batteryLevel, batLevel);
      // Note: updateSelection() is blocking until the epaper is ready.
      // consider reducing the update region or using an interrupt-driven approach.
    }
  }
  if (pressedButton == true) {
    // Clear the selection text before sleep
    Paint_NewImage(partialBuffer, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT, 270, WHITE);
    Paint_SelectImage(partialBuffer);
    Paint_Clear(WHITE);
    drawServerStatusONLINE(serverText);
    drawServerStatusOFFLINE(serverText);
    drawBatteryBlack(batteryLevel, batLevel);
    drawTimeDate();
    drawBatteryRed(batteryLevel, batLevel);
    Paint_DrawRectangle(7, 17, 290, 113, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
    EPD_2IN9B_V4_Display_Partial(partialBuffer, 0, 0, EPD_2IN9B_V4_WIDTH, EPD_2IN9B_V4_HEIGHT);
  }

  Serial.println("MainServerStatus complete. Going to sleep.");
  DEV_Delay_ms(2000);
  EPD_2IN9B_V4_Sleep();
  DEV_Delay_ms(2000);
  esp_deep_sleep_start();
}


void drawServerStatusONLINE(String serverText[]) {
  for (int i = 0; i < 6; i++) {
    if (String(serverText[i]).indexOf("OFFLINE") == -1) {
      Paint_DrawString_EN(10, 20 + i * 15, serverText[i].c_str(), &Font16, WHITE, BLACK);
    }
  }
}

void drawServerStatusOFFLINE(String serverText[]) {
  for (int i = 0; i < 6; i++) {
    if (String(serverText[i]).indexOf("OFFLINE") >= 0) {
      Paint_DrawString_EN(10, 20 + i * 15, serverText[i].c_str(), &Font16, WHITE, BLACK);
    }
  }
}

void drawTimeDate() {


  Paint_DrawString_EN(140, 115, serverStatus.timestamp.c_str(), &Font12, BLACK, WHITE);
}


void drawBatteryBlack(int batteryLevel, String batLevel) {

  // draw black empty battery icon
  Paint_DrawRectangle(7, 117, 25, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
  Paint_DrawRectangle(25, 120, 27, 123, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);

  if (batteryLevel > 20) {
    Paint_DrawString_EN(28, 116, batLevel.c_str(), &Font12, WHITE, BLACK);  // battery percentage readout black
  }

  if (batteryLevel >= 90) {
    Paint_DrawRectangle(7, 117, 25, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // 90-100%
  } else if (batteryLevel >= 80) {
    Paint_DrawRectangle(7, 117, 22, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // 80-90%
  } else if (batteryLevel >= 65) {
    Paint_DrawRectangle(7, 117, 20, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // 65-80%
  } else if (batteryLevel >= 50) {
    Paint_DrawRectangle(7, 117, 18, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // 50-65%
  } else if (batteryLevel >= 40) {
    Paint_DrawRectangle(7, 117, 16, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // 40-50%
  } else if (batteryLevel >= 30) {
    Paint_DrawRectangle(7, 117, 14, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // 30-40%
  } else if (batteryLevel > 20) {
    Paint_DrawRectangle(7, 117, 12, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // 20-30%
  }
}

void drawBatteryRed(int batteryLevel, String batLevel) {
  if (batteryLevel <= 20) {
    Paint_DrawString_EN(28, 116, batLevel.c_str(), &Font12, WHITE, BLACK);  // battery percentage readout red
  }
  if (batteryLevel > 10 && batteryLevel <= 20) {
    Paint_DrawRectangle(8, 118, 11, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // 10-20%
  } else if (batteryLevel == 0) {
    Paint_DrawRectangle(8, 118, 8, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // 0%
  } else if (batteryLevel <= 10) {
    Paint_DrawRectangle(8, 118, 9, 125, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);  // less than 10%
  }
}

void checkForFirmwareUpdate() {
  HTTPClient http;
  http.begin(FIRMWARE_URL1);
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    String firmware_version = doc["version"].as<String>();
    String firmware_url = doc["url"].as<String>();
    String firmware_checksum = doc["checksum"].as<String>();
    if (firmware_version != current_version) {
      Serial.println("Updating firmware...");
      performOTAUpdate(firmware_url, firmware_checksum);
    } else {
      Serial.println("No new firmware available.");
    }
  } else {
    Serial.println("Failed to fetch metadata from server. HTTP Code: " + String(httpCode));
  }
  http.end();
}


void performOTAUpdate(const String &url, const String &firmware_md5) {
  WiFiClient client;
  HTTPClient http;
  http.begin(client, url);
  int httpCode = http.GET();
  if (httpCode == 200) {
    int contentLength = http.getSize();
    if (contentLength <= 0) {
      Serial.println("Invalid content length.");
      return;
    }
    WiFiClient *stream = http.getStreamPtr();
    Serial.println("Starting OTA update...");
    if (Update.begin(contentLength)) {
      if (firmware_md5.length() > 0) {
        Update.setMD5(firmware_md5.c_str());
      }
      size_t written = Update.writeStream(*stream);
      if (written == contentLength) {
        Serial.println("Written: " + String(written) + " bytes successfully.");
      } else {
        Serial.println("Write failed.");
      }
      if (Update.end()) {
        Serial.println("OTA update completed. Rebooting...");
        ESP.restart();
      } else {
        String errStr = Update.errorString();
        if (errStr.indexOf("MD5") != -1) {
          Serial.println("MD5 checksum mismatch. OTA update failed.");
        } else {
          Serial.println("Error during update: " + errStr);
        }
      }
    } else {
      Serial.println("Not enough space for OTA.");
    }
  } else {
    Serial.println("Firmware download failed. HTTP Code: " + String(httpCode));
  }
  http.end();
}


//-------------------- Setup & Loop --------------------
void setup() {
  delay(500);
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, HIGH);
  wifiInit();
  delay(5000);
  checkForFirmwareUpdate();
  analogRead(33);  // Battery voltage read
  delay(500);
  int batteryLevel = map(analogRead(33), 3100, 4095, 0, 100);
  if (batteryLevel < 0) {
    batteryLevel = 0;
  }
  String batLevel = String(batteryLevel) + "%";
  ePaperInit();

  if (WiFi.status() == WL_CONNECTED) {
    fetchStatusFromJson();
    delay(1000);
    // can't read json ping UNRAID check if online else apache must be offline
    if (httpFailed == true) {
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_NoWIFI * uS_TO_S_FACTOR);
      bool isReachable = Ping.ping(unRaidHost, 3);  // 3 retries
      if (isReachable) {
        drawOffline(batteryLevel, batLevel, "UNRAID SERVER");
      } else {
        drawOffline(batteryLevel, batLevel, "APACHE PHP PROBLEM OR");
      }
    } else {
      // Draw the main server status with integrated selection logic.
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      drawMainServerStatus(batteryLevel, batLevel);
    }
  } else {
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP_NoWIFI * uS_TO_S_FACTOR);
    drawBatteryAndNoWifi(batteryLevel, batLevel);
  }
}

void loop() {
  // Loop left empty; all logic is executed in setup() and drawMainServerStatus().
}
