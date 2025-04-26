/****************************************************************************
 *  BodyCamScanner_main.cpp
 *  Tactical Radar Wi-Fi/BLE Scanner App (Corrected Version)
 ****************************************************************************/

 #include "config.h"
 #include "BodyCamScanner.h"
 #include "BodyCamScanner_main.h"
 
 #include "gui/mainbar/mainbar.h"
 #include "gui/statusbar.h"
 #include "gui/widget_factory.h"
 #include "gui/widget_styles.h"
 
 #include "hardware/gpsctl.h"
 #include "hardware/display.h"
 #include "hardware/sound.h"
 #include "hardware/motor.h"
 
 #include <WiFi.h>
 #include <NimBLEDevice.h>
 #include <vector>
 
 /*
  * Tile and UI elements
  */
 static lv_obj_t* radar_tile = NULL;
 static lv_obj_t* gps_status_label = NULL;
 
 /*
  * Radar data
  */
 struct SignalSource {
     String name;
     int rssi;
     String type;
     int angle;
     float latitude;
     float longitude;
     unsigned long timestamp;
 };
 
 static std::vector<SignalSource> signals;
 
 /*
  * Settings and State
  */
 static int detectionThreshold = -60; // dBm
 static int sweepAngle = 0;
 static bool pulseGrow = true;
 static int pulseSize = 5;
 static bool gpsFix = false;
 static float currentLat = 0.0;
 static float currentLon = 0.0;
 
 /*
  * History
  */
 static std::vector<SignalSource> history;
 static const int HISTORY_MAX = 50;
 
 /*
  * Tasks
  */
 static lv_task_t* scan_task = NULL;
 static lv_task_t* radar_draw_task = NULL;
 
 /*
  * Forward Declarations
  */
 static void scan_networks();
 static void draw_radar();
 static bool handle_gps_event(EventBits_t event, void* arg);
 static void open_settings_menu();
 static void adjust_threshold(bool increase);
 static void clear_history();
 static void open_about() ;
 static void display_popup(const char* message);

 /*
  * Setup Radar Screen
  */
 void BodyCamScanner_main_setup(uint32_t tile_num) {
    radar_tile = mainbar_get_tile_obj(tile_num);

    lv_obj_t* exit_btn = wf_add_exit_button(radar_tile);
    lv_obj_align(exit_btn, radar_tile, LV_ALIGN_IN_BOTTOM_LEFT, 10, -10);

    gps_status_label = lv_label_create(radar_tile, NULL);
    lv_label_set_text(gps_status_label, "❌ GPS");
    lv_obj_align(gps_status_label, radar_tile, LV_ALIGN_IN_TOP_RIGHT, -10, 10);

    lv_obj_set_click(radar_tile, true);
    lv_obj_set_event_cb(radar_tile, [](lv_obj_t* obj, lv_event_t event) {
        if (event == LV_EVENT_CLICKED) {
            open_settings_menu();
        }
    });

    gpsctl_register_cb(GPSCTL_FIX | GPSCTL_NOFIX | GPSCTL_UPDATE_LOCATION, handle_gps_event, "BodyCamScanner GPS");

    scan_task = lv_task_create([](lv_task_t*) { scan_networks(); }, 5000, LV_TASK_PRIO_LOW, NULL);
    radar_draw_task = lv_task_create([](lv_task_t*) { draw_radar(); }, 80, LV_TASK_PRIO_LOW, NULL);
}
 
 /*
 * GPS Event Callback
 */
static bool handle_gps_event(EventBits_t event, void* arg) {
    gps_data_t* gps = (gps_data_t*)arg;

    switch (event) {
        case GPSCTL_FIX:
            gpsFix = true;
            lv_label_set_text(gps_status_label, "✅ GPS");
            break;
        case GPSCTL_NOFIX:
            gpsFix = false;
            lv_label_set_text(gps_status_label, "❌ GPS");
            break;
        case GPSCTL_UPDATE_LOCATION:
            if (gps->valid_location) {
                currentLat = gps->lat;
                currentLon = gps->lon;
            }
            
            break;
    }
    return true;
}

/*
 * Scan WiFi and BLE Networks
 */
static void scan_networks() {
    signals.clear();

    // WiFi scan
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);
    int n = WiFi.scanNetworks(false, true);

    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);

        if (ssid.startsWith("YDXJ_") || ssid.startsWith("DS-MCW405") || ssid.indexOf("axon") != -1 || ssid.indexOf("vb400") != -1) {
            signals.push_back({ ssid, rssi, "WiFi", random(0, 360), currentLat, currentLon, millis() });
        }
    }

    WiFi.scanDelete();

    // BLE scan
    NimBLEDevice::init("");
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setActiveScan(true);
    pScan->start(5, false);

    NimBLEScanResults results = pScan->getResults();
    for (int i = 0; i < results.getCount(); i++) {
        NimBLEAdvertisedDevice dev = results.getDevice(i);
        String name = String(dev.getName().c_str());
        int rssi = dev.getRSSI();

        if (name.startsWith("Axon") || name.startsWith("DS-MCW405") || name.indexOf("vb400") != -1) {
            signals.push_back({ name, rssi, "BLE", random(0, 360), currentLat, currentLon, millis() });
        }
    }

    pScan->clearResults();
}

/*
 * Draw Radar and Signals
 */
static void draw_radar() {
    if (!radar_tile) return;

    // Clear old objects (keep exit + GPS label)
    lv_obj_clean(radar_tile);
    BodyCamScanner_main_setup(0); // quick re-add buttons/labels

    int cx = lv_disp_get_hor_res(NULL) / 2;
    int cy = lv_disp_get_ver_res(NULL) / 2;
    int radius = (cx < cy ? cx : cy) - 30;

    // Radar circles
    for (int r = radius; r > 0; r -= radius / 3) {
        lv_obj_t* arc = lv_arc_create(radar_tile, NULL);
        lv_obj_set_size(arc, r * 2, r * 2);
        lv_arc_set_angles(arc, 0, 360);
        lv_obj_align(arc, NULL, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_local_border_color(arc, LV_ARC_PART_BG, LV_STATE_DEFAULT, LV_COLOR_GRAY);
    }

    // Sweep Line
    float sweep_rad = sweepAngle * 3.14159 / 180.0;
    int x2 = cx + radius * cos(sweep_rad);
    int y2 = cy + radius * sin(sweep_rad);

    static lv_point_t points[2];
    points[0].x = cx;
    points[0].y = cy;
    points[1].x = x2;
    points[1].y = y2;

    lv_obj_t* line = lv_line_create(radar_tile, NULL);
    lv_line_set_points(line, points, 2);
    lv_obj_set_style_local_line_color(line, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_GREEN);
    lv_obj_set_style_local_line_width(line, LV_LINE_PART_MAIN, LV_STATE_DEFAULT, 2);

    sweepAngle = (sweepAngle + 5) % 360;
    pulseSize += (pulseGrow ? 1 : -1);
    if (pulseSize > 8) pulseGrow = false;
    if (pulseSize < 5) pulseGrow = true;

    // Draw detected signals
    for (auto& sig : signals) {
        float norm_strength = map(sig.rssi, -100, -30, 0, radius);
        norm_strength = constrain(norm_strength, 0, radius);

        float ang = sig.angle * 3.14159 / 180.0;
        int x = cx + norm_strength * cos(ang);
        int y = cy + norm_strength * sin(ang);

        lv_color_t color = LV_COLOR_GREEN;
        if (sig.rssi > detectionThreshold) {
            color = LV_COLOR_RED;
            motor_vibe(100);
            // sound_beep();
            history.insert(history.begin(), sig);
            if (history.size() > HISTORY_MAX) history.pop_back();
        } else if (sig.rssi > -80) {
            color = LV_COLOR_YELLOW;
        }

        lv_obj_t* dot = lv_obj_create(radar_tile, NULL);
        lv_obj_set_size(dot, pulseSize, pulseSize);
        lv_obj_align(dot, NULL, LV_ALIGN_CENTER, x - cx, y - cy);
        lv_obj_set_style_local_bg_color(dot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, color);
        lv_obj_set_style_local_radius(dot, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_RADIUS_CIRCLE);
    }
}

/*
 * Open Settings Menu (Swipeable)
 */
static void open_settings_menu() {
    lv_obj_t* settings_tile = lv_obj_create(NULL, NULL);
    lv_obj_set_size(settings_tile, LV_HOR_RES, LV_VER_RES);
    lv_scr_load(settings_tile);

    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_bg_color(&style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_obj_add_style(settings_tile, LV_OBJ_PART_MAIN, &style);

    // Title
    lv_obj_t* title = lv_label_create(settings_tile, NULL);
    lv_label_set_text(title, "Settings");
    lv_obj_align(title, NULL, LV_ALIGN_IN_TOP_MID, 0, 10);

    // Threshold control
    lv_obj_t* thresh_label = lv_label_create(settings_tile, NULL);
    char thresh_txt[30];
    sprintf(thresh_txt, "Threshold: %d dBm", detectionThreshold);
    lv_label_set_text(thresh_label, thresh_txt);
    lv_obj_align(thresh_label, NULL, LV_ALIGN_IN_TOP_LEFT, 10, 50);

    lv_obj_t* up_btn = lv_btn_create(settings_tile, NULL);
    lv_obj_set_size(up_btn, 50, 30);
    lv_obj_align(up_btn, thresh_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
    lv_obj_t* up_lbl = lv_label_create(up_btn, NULL);
    lv_label_set_text(up_lbl, "+");
    lv_obj_set_user_data(up_btn, (lv_obj_user_data_t)true);

    lv_obj_t* down_btn = lv_btn_create(settings_tile, NULL);
    lv_obj_set_size(down_btn, 50, 30);
    lv_obj_align(down_btn, up_btn, LV_ALIGN_OUT_RIGHT_MID, 20, 0);
    lv_obj_t* down_lbl = lv_label_create(down_btn, NULL);
    lv_label_set_text(down_lbl, "-");
    lv_obj_set_user_data(down_btn, (lv_obj_user_data_t)false);

    // Clear History Button
    lv_obj_t* clear_btn = lv_btn_create(settings_tile, NULL);
    lv_obj_set_size(clear_btn, 150, 40);
    lv_obj_align(clear_btn, thresh_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 80);
    lv_obj_t* clear_lbl = lv_label_create(clear_btn, NULL);
    lv_label_set_text(clear_lbl, "Clear History");

    // About Button
    lv_obj_t* about_btn = lv_btn_create(settings_tile, NULL);
    lv_obj_set_size(about_btn, 150, 40);
    lv_obj_align(about_btn, clear_btn, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 20);
    lv_obj_t* about_lbl = lv_label_create(about_btn, NULL);
    lv_label_set_text(about_lbl, "About");

    // Button Callbacks
    lv_obj_set_event_cb(up_btn, [](lv_obj_t* btn, lv_event_t e) {
        if (e == LV_EVENT_CLICKED) adjust_threshold(true);
    });
    lv_obj_set_event_cb(down_btn, [](lv_obj_t* btn, lv_event_t e) {
        if (e == LV_EVENT_CLICKED) adjust_threshold(false);
    });
    lv_obj_set_event_cb(clear_btn, [](lv_obj_t* btn, lv_event_t e) {
        if (e == LV_EVENT_CLICKED) clear_history();
    });
    lv_obj_set_event_cb(about_btn, [](lv_obj_t* btn, lv_event_t e) {
        if (e == LV_EVENT_CLICKED) open_about();
    });
}

/*
 * Adjust Detection Threshold
 */
static void adjust_threshold(bool increase) {
    if (increase) detectionThreshold += 5;
    else detectionThreshold -= 5;
    if (detectionThreshold > -30) detectionThreshold = -30;
    if (detectionThreshold < -90) detectionThreshold = -90;
}

/*
 * Clear Threat History
 */
static void clear_history() {
    history.clear();
    display_popup("History Cleared");
}

/*
 * About Page
 */
static void open_about() {
    lv_obj_t* about_tile = lv_obj_create(NULL, NULL);
    lv_obj_set_size(about_tile, LV_HOR_RES, LV_VER_RES);
    lv_scr_load(about_tile);

    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_bg_color(&style, LV_STATE_DEFAULT, LV_COLOR_NAVY);
    lv_obj_add_style(about_tile, LV_OBJ_PART_MAIN, &style);

    lv_obj_t* label = lv_label_create(about_tile, NULL);
    lv_label_set_text(label, "BodyCam Scanner\nTactical v1.0\n2025");
    lv_obj_align(label, NULL, LV_ALIGN_CENTER, 0, 0);
}

/*
 * Display Small Popup
 */
static void display_popup(const char* message) {
    lv_obj_t* popup = lv_label_create(lv_scr_act(), NULL);
    lv_label_set_text(popup, message);
    lv_obj_align(popup, NULL, LV_ALIGN_CENTER, 0, 0);

    static lv_style_t popup_style;
    lv_style_init(&popup_style);
    lv_style_set_bg_color(&popup_style, LV_STATE_DEFAULT, LV_COLOR_GRAY);
    lv_obj_add_style(popup, LV_OBJ_PART_MAIN, &popup_style);

    // Auto delete popup after 2 seconds
    lv_task_create([](lv_task_t* task) {
        lv_obj_del((lv_obj_t*)task->user_data);
        lv_task_del(task);
    }, 2000, LV_TASK_PRIO_LOW, popup);
}