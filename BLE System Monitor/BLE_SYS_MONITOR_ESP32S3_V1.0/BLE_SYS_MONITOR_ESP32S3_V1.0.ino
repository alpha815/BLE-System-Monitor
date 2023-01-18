/*

    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleWrite.cpp
    System Monitor code is bassed on Eric Nom USB version of System Monitor: https://github.com/0015/ThatProject/tree/master/ESP32_LVGL/LVGL8/5_System_Monitor
    Ported to Arduino ESP32 by Evandro Copercini

*/

//#define DEBUG

#include <ArduinoJson.h>        //https://arduinojson.org/v6/doc/installation/
#include <ESP32Time.h>          //https://github.com/fbiego/ESP32Time
#include <NimBLEDevice.h>        // reduced almost 35% memory footprint 
#include <lvgl.h>               //https://github.com/lvgl/lvgl
#include "config.h"

bool deviceConnected = false;
bool oldDeviceConnected = false;

static NimBLEServer* pServer;


#define SERVICE_UUID                "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define infoCHARACTERISTIC_UUID     "67360b78-8c45-11ed-a1eb-0242ac120002"


StaticJsonDocument<512> doc;
ESP32Time rtc;

static QueueHandle_t data_queue;
String data_string;

class MyCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic) {
      std::string value = pCharacteristic->getValue();
      data_string = value.c_str();
#ifdef DEBUG
      Serial.print("received data size: ");
      Serial.println(value.length());
#endif
      if (xQueueSend(data_queue, &data_string, portMAX_DELAY) == pdPASS) {
#ifdef DEBUG
        Serial.println("Data sent to queue successfully.");
#endif
      } else {
#ifdef DEBUG
        Serial.println("Failed to send data to queue. Queue is full.");
#endif

      }
    }
};
class MyServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer * pServer) {
      deviceConnected = true;
#ifdef DEBUG
      Serial.println("Device connected");
#endif
      NimBLEDevice::startAdvertising();
    };

    void onDisconnect(NimBLEServer * pServer) {
#ifdef DEBUG
      Serial.println("Device DISconnected");
#endif
      deviceConnected = false;
    }
};

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
#endif

  data_queue = xQueueCreate(8, sizeof(data_string));

  config_display();
  lv_main();
  NimBLEDevice::init("ESP32S3 SYSTEM MONITOR");
  NimBLEDevice::setMTU(517);
  pServer  = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  NimBLEService *sysmon = pServer->createService(SERVICE_UUID);
  NimBLECharacteristic *infoCharacteristic = sysmon->createCharacteristic(
        infoCHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ   |
        NIMBLE_PROPERTY::WRITE
      );
  infoCharacteristic->setValue("usage info");
  infoCharacteristic->setCallbacks(new MyCallbacks());
  sysmon->start();

  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
#ifdef DEBUG
  Serial.println("Waiting a client connection: ");
#endif

}

void loop() {
  if (deviceConnected) {
    printdata();
    loading_spinner_delete();
  }
  if (!deviceConnected && oldDeviceConnected) {
    loading_spinner_make();
    pServer->startAdvertising();              // restart advertising
#ifdef DEBUG
    Serial.println("start advertising");
#endif

    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    loading_spinner_make();
    oldDeviceConnected = deviceConnected;
  }
  lv_timer_handler();
  delay(5);
}

void printdata() {
  if (xQueueReceive(data_queue, &data_string, 0) == pdPASS) {
#ifdef DEBUG
    Serial.println(data_string);
#endif

    dataParser(data_string);
  }
}
void dataParser(String jsonData) {
  if (jsonData.length() < 10) {
#ifdef DEBUG
    Serial.println("Invalid data length");
#endif
    return;
  }
  DeserializationError error = deserializeJson(doc, jsonData);
  if (error) {
#ifdef DEBUG
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
#endif
    return;
  }

  long local_time = doc["local_time"];
  long utc_offset = doc["utc_offset"];
  float cpu_percent_total = doc["cpu_percent_total"];
  float mem_percent = doc["mem_usage"];

  float battery_percent = doc["battery_info"];
  bool power_plugged = doc["power_plugged"];
  float core0_temp = doc["core0_temp"];

  update_cpu_mem_usage(cpu_percent_total, mem_percent);
  update_time(local_time, utc_offset);
  update_battery(battery_percent, power_plugged);
  update_core_temp_data(core0_temp);
  update_process(doc["cpu_top5_process"]);
}
static lv_obj_t *loading_screen;
static lv_obj_t *meter;
static lv_obj_t *date_time_label;
static lv_obj_t *cpu_label;
static lv_obj_t *mem_label;
static lv_obj_t *battery_label;
static lv_meter_indicator_t *cpu_indic;
static lv_meter_indicator_t *mem_indic;
static lv_obj_t *process_list;
static lv_anim_t a;
static lv_anim_t a1;
static lv_obj_t * bar;
static lv_obj_t *temp_label;
static void set_value(void *indic, int32_t v) {
  lv_meter_set_indicator_end_value(meter, (lv_meter_indicator_t *)indic, v);
}
static void set_temp(void * bar, int32_t temp)
{
  lv_bar_set_value((lv_obj_t*)bar, temp, LV_ANIM_ON);
}
void lv_main(void) {
  meter = lv_meter_create(lv_scr_act());
  lv_obj_align(meter, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_size(meter, 280, 280);
  lv_obj_remove_style(meter, NULL, LV_PART_INDICATOR);

  lv_meter_scale_t *scale = lv_meter_add_scale(meter);
  lv_meter_set_scale_ticks(meter, scale, 11, 2, 10, lv_palette_main(LV_PALETTE_GREY));
  lv_meter_set_scale_major_ticks(meter, scale, 1, 2, 30, lv_color_hex3(0xeee), 15);
  lv_meter_set_scale_range(meter, scale, 0, 100, 270, 90);

  cpu_indic = lv_meter_add_arc(meter, scale, 10, lv_palette_main(LV_PALETTE_RED), 0);
  mem_indic = lv_meter_add_arc(meter, scale, 10, lv_palette_main(LV_PALETTE_BLUE), -20);

  lv_anim_init(&a);
  lv_anim_set_exec_cb(&a, set_value);

  static lv_style_t style_red;
  lv_style_init(&style_red);
  lv_style_set_radius(&style_red, 5);
  lv_style_set_bg_opa(&style_red, LV_OPA_COVER);
  lv_style_set_bg_color(&style_red, lv_palette_main(LV_PALETTE_RED));
  lv_style_set_outline_width(&style_red, 2);
  lv_style_set_outline_color(&style_red, lv_palette_main(LV_PALETTE_RED));
  lv_style_set_outline_pad(&style_red, 4);

  static lv_style_t style_blue;
  lv_style_init(&style_blue);
  lv_style_set_radius(&style_blue, 5);
  lv_style_set_bg_opa(&style_blue, LV_OPA_COVER);
  lv_style_set_bg_color(&style_blue, lv_palette_main(LV_PALETTE_BLUE));
  lv_style_set_outline_width(&style_blue, 2);
  lv_style_set_outline_color(&style_blue, lv_palette_main(LV_PALETTE_BLUE));
  lv_style_set_outline_pad(&style_blue, 4);

  lv_obj_t *red_obj = lv_obj_create(lv_scr_act());
  lv_obj_set_size(red_obj, 10, 10);
  lv_obj_add_style(red_obj, &style_red, 0);
  lv_obj_align(red_obj, LV_ALIGN_CENTER, -60, 80);

  cpu_label = lv_label_create(lv_scr_act());
  lv_obj_set_width(cpu_label, 200);
  lv_label_set_text(cpu_label, "CPU Usage: 0%");
  lv_obj_align_to(cpu_label, red_obj, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

  lv_obj_t *blue_obj = lv_obj_create(lv_scr_act());
  lv_obj_set_size(blue_obj, 10, 10);
  lv_obj_add_style(blue_obj, &style_blue, 0);
  lv_obj_align(blue_obj, LV_ALIGN_CENTER, -60, 110);

  mem_label = lv_label_create(lv_scr_act());
  lv_obj_set_width(mem_label, 200);
  lv_label_set_text(mem_label, "MEM Usage: 0%");
  lv_obj_align_to(mem_label, blue_obj, LV_ALIGN_OUT_RIGHT_MID, 10, 0);

  date_time_label = lv_label_create(lv_scr_act());
  lv_obj_set_style_text_align(date_time_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(date_time_label, LV_ALIGN_TOP_RIGHT, -10, 0);
  lv_label_set_text(date_time_label, "Mon 1:25 PM");

  battery_label = lv_label_create(lv_scr_act());
  lv_label_set_text(battery_label, LV_SYMBOL_CHARGE "  0%");
  lv_obj_set_style_text_align(battery_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(battery_label, LV_ALIGN_TOP_RIGHT, -130, 0);

  static lv_style_t style_indic;

  lv_style_init(&style_indic);
  lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
  lv_style_set_bg_color(&style_indic, lv_palette_main(LV_PALETTE_BLUE));
  lv_style_set_bg_grad_color(&style_indic, lv_palette_main(LV_PALETTE_RED));
  lv_style_set_bg_grad_dir(&style_indic, LV_GRAD_DIR_HOR);

  bar = lv_bar_create(lv_scr_act());
  lv_obj_add_style(bar, &style_indic, LV_PART_INDICATOR);
  lv_obj_set_size(bar, 200, 20);
  lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 5);
  lv_bar_set_range(bar, 20, 80);

  temp_label = lv_label_create(lv_scr_act());
  lv_obj_align_to(temp_label, bar, LV_ALIGN_RIGHT_MID, 35, 0);
  lv_label_set_text(temp_label, "00.00 C");

  lv_anim_init(&a1);
  lv_anim_set_exec_cb(&a1, set_temp);


  process_list = lv_list_create(lv_scr_act());
  lv_obj_set_size(process_list, 200, 180);
  lv_obj_align(process_list, LV_ALIGN_TOP_RIGHT, 0, 30);
  lv_obj_set_style_pad_row(process_list, -10, 0);
  lv_obj_set_style_pad_hor(process_list, -6, 0);


  loading_spinner_make();
}
void loading_spinner_make() {

  loading_screen = lv_obj_create(lv_scr_act());
  lv_obj_set_size(loading_screen, 480, 320);
  lv_obj_center(loading_screen);

  lv_obj_t *loading_spinner = lv_spinner_create(loading_screen, 1000, 60);
  lv_obj_set_size(loading_spinner, 240, 240);
  lv_obj_center(loading_spinner);

  lv_obj_t *loading_label = lv_label_create(loading_spinner);
  lv_label_set_text(loading_label, "Waiting for Data...");
  lv_obj_center(loading_label);
}

void loading_spinner_delete() {
  if (loading_screen != NULL) {
    lv_obj_del(loading_screen);
    loading_screen = NULL;
  }
}
void update_time(long local_time, long utc_offset) {
  rtc.setTime(local_time);
  rtc.offset = utc_offset;
  lv_label_set_text(date_time_label, rtc.getTime("%a %I:%M %p").c_str());
}
void update_cpu_mem_usage(float cpu_percent_total, float mem_percent) {
  String cpu_text = String("CPU Usage: ") + cpu_percent_total + "%";
  lv_label_set_text(cpu_label, cpu_text.c_str());

  lv_anim_set_var(&a, cpu_indic);
  lv_anim_set_time(&a, 250);
  lv_anim_set_values(&a, prev_cpu_usage, int(cpu_percent_total));
  lv_anim_start(&a);
  prev_cpu_usage = int(cpu_percent_total);

  String mem_text = String("MEM Usage: ") + mem_percent + "%";
  lv_label_set_text(mem_label, mem_text.c_str());

  lv_anim_set_var(&a, mem_indic);
  lv_anim_set_time(&a, 250);
  lv_anim_set_values(&a, prev_mem_usage, int(mem_percent));
  lv_anim_start(&a);
  prev_mem_usage = int(mem_percent);
}
void update_battery(float battery_percent, bool power_plugged) {
  char buffer[16];
  if (power_plugged) {
    sprintf(buffer, LV_SYMBOL_CHARGE " %d %%", int(battery_percent));
  } else {
    if (battery_percent >= 95) {
      sprintf(buffer, "\xEF\x89\x80  %d %%", int(battery_percent));
    } else if (battery_percent >= 75 && battery_percent < 95) {
      sprintf(buffer, "\xEF\x89\x81  %d %%", int(battery_percent));
    } else if (battery_percent >= 50 && battery_percent < 75) {
      sprintf(buffer, "\xEF\x89\x82  %d %%", int(battery_percent));
    } else if (battery_percent >= 20 && battery_percent < 50) {
      sprintf(buffer, "\xEF\x89\x83  %d %%", int(battery_percent));
    } else {
      sprintf(buffer, "\xEF\x89\x84  %d %%", int(battery_percent));
    }
  }

  lv_label_set_text(battery_label, buffer);
}

void update_core_temp_data(float temp_data) {
  String cpu_temp = String(temp_data) + " °C";
  lv_label_set_text(temp_label, cpu_temp.c_str());

  int previous_temp = temp_data;
  lv_anim_set_time(&a1, 250);
  lv_anim_set_var(&a1, bar);
  lv_anim_set_values(&a1, previous_temp, temp_data);
  lv_anim_start(&a1);
}
void update_process(JsonArray processes) {
  int arraySize = processes.size();
  if (arraySize > 0) {
    lv_obj_clean(process_list);
    for (int i = 0; i < arraySize; i++) {
      lv_obj_t *list_item_button = lv_list_add_btn(process_list, LV_SYMBOL_WARNING, processes[i]);
      lv_obj_t *child = lv_obj_get_child(list_item_button, 1);
      lv_label_set_long_mode(child, LV_LABEL_LONG_WRAP);
    }
  }
}
