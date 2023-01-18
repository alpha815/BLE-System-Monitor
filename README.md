# BLE-System-Monitor
ESP32 bassed System Resource Monitor
This is wireless version of thatProject's System Monitor.
That project's Youtube Link:https://youtu.be/gliwNg25fLE
Github Link:https://github.com/0015/ThatProject/tree/master/ESP32_LVGL/LVGL8/5_System_Monitor
## Dependencies:
### Arduino:
    1. LVGL version 8.2.0: https://github.com/lvgl/lvgl
    2. NimBLE library: https://github.com/h2zero/NimBLE-Arduino
    3. Arduino JSON: https://arduinojson.org/v6/doc/installation/
    4. ESP32 time: https://github.com/fbiego/ESP32Time

### Python:
    1. Bleak:https://bleak.readthedocs.io/en/latest/
    2. psutil:https://github.com/giampaolo/psutil
### description
Install the required libraries in Arduino IDE and upload the code to esp32. 
Open the main.py file with any text editor and edit your device's mac address.
On Windows, You can use thony IDE to run and install required modules. I was having trouble installing a few dependencies on windows 10, Thony IDE can save you time.
On Ubuntu: you can directly run main.py, after editing your device's mac address and installing the required modules assuming you have python 3 installed on your PC.
