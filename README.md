# Unraid ePaper Server Monitor

This project is a custom Unraid monitoring solution that uses an ESP32 paired with a 2.9″ ePaper display to present real‑time server metrics. It leverages a bash script (run as a Unraid User Script) to gather system information and Docker/VM stats, formats the data into a JSON file, and then the ESP32 fetches and displays the data.

I am not affiliated with UNRAID and this is an unofficial project.


https://github.com/user-attachments/assets/5fe0ccbe-fdf2-4162-a24b-4c84326a66ac


## Key Features

### Unraid System Metrics
- **CPU Load:** Extracted from `uptime`
- **CPU Model:** Retrieved from `/proc/cpuinfo`
- **Memory Usage:** Total, used, and free (in MB)
- **Disk Free Space:** Obtained from `df -h`

### Docker Container Stats
Monitors the following containers:
- **Jellyfin**
- **Jellyseerr**
- **GluetunVPN**
- **immich**

For each container, the script displays:
- Online status (using `docker ps`)
- CPU usage, memory usage/limit, and IP address (via `docker stats` and `docker inspect`)
- Disk information: total disk size, used space, and free space (using `df -h`)

### Virtual Machine Stats
Uses `virsh` to gather details for VMs including:
- Name, state, CPU cores, and maximum memory (in MB)
- Autostart settings
- IP address (using the guest agent)

### JSON Output
All collected metrics are aggregated into a JSON file (saved to `/mnt/user/appdata/Apache-PHP/www/firmware/status.json`), which the ESP32 uses for display updates.

## Hardware & Power Management

### ESP32 & ePaper Display
- Driven by the Waveshare ePaper libraries (`DEV_Config.h`, `EPD.h`, `GUI_Paint.h`)
- Uses partial refreshes for efficient display updates

### Power Supply
- Powered by a **TPS63070** buck‑boost regulator that converts battery voltage to a clean 3.3V
- Uses a **4.2V, 1100mAh** battery charged via a **TP5100** module
- A push button connected to **GPIO39** (with a 10kΩ resistor to ground) wakes the ESP32 from deep sleep

### Battery Monitoring Voltage
- 27kΩ resistor from battery voltage & 100kΩ resistor from ground. These are connected to **GIO33**.

### OTA Updates
- The ESP32 checks for firmware updates via HTTP and uses the Update library for over‑the‑air (OTA) updates

## Software Libraries & Tools

### ESP32 Libraries
- **WiFi & HTTPClient:** For network connectivity
- **ArduinoJson:** For parsing the JSON status file
- **Update:** For OTA firmware updates

### Bash Script Tools
- Standard Linux utilities: `uptime`, `free`, `df`, `grep`, `awk`, etc.
- Docker commands: `docker stats` and `docker inspect` for container monitoring
- `virsh` for Virtual Machine statistics

## How It Works

1. **Data Collection:**  
   A bash script (run as a Unraid User Script) collects Unraid system metrics, Docker container stats (including disk size, used, and free space), and VM information, then formats all of the data into a JSON file.

2. **Display Update:**  
   The ESP32 downloads and parses the JSON file using ArduinoJson, and updates the ePaper display with formatted text and graphical progress bars.

3. **Low-Power Operation:**  
   The ESP32 is designed for low-power consumption. It uses deep sleep and can be awakened via a push button (GPIO39), ensuring efficient operation.

## Repository Structure

- **ESP32 Firmware:**  
  Contains the code for the ESP32, which handles fetching, parsing, and displaying the JSON data.

- **Bash Script (`json_script.sh`):**  
  Collects and formats system, Docker, and VM metrics into a JSON file.

## Branching Strategy

Due to significant differences between the original ping-based monitor and this new JSON-based Unraid monitoring version (which removes some libraries and functionality), this version is maintained in a separate repository. This clear separation ensures that users can easily choose the version that best fits their needs.

## License

This project is licensed under the **MIT License**.  
See the [LICENSE](LICENSE) file for details.

## Additional Notes

- **Hardware Integration:**  
  The ESP32 is powered from a 3.3V source derived from the TP5100 charger and TPS63070 regulator. The push button on GPIO39 (with a 10kΩ resistor to ground) is used for waking the device from deep sleep.
  
- **Libraries Used:**  
  - **ESP32:** WiFi, HTTPClient, ArduinoJson, Update  
  - **ePaper:** DEV_Config, EPD, GUI_Paint  
  - **Bash Tools:** Standard Linux commands, Docker, virsh
