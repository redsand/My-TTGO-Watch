/****************************************************************************
 *  BodyCamScanner.cpp
 *  Tactical Radar Wi-Fi/BLE Scanner App
 ****************************************************************************/

 #include "config.h"
 #include "BodyCamScanner.h"
 #include "BodyCamScanner_main.h"
 
 #include "gui/mainbar/mainbar.h"
 #include "gui/statusbar.h"
 #include "gui/app.h"
 #include "gui/widget.h"
 
 /*
  * App tile and icon
  */
 uint32_t BodyCamScanner_main_tile_num;
 icon_t *BodyCamScanner = NULL;
 
 /*
  * Declare app icon (replace later with real tactical icon)
  */
 LV_IMG_DECLARE(tactical_shield_64px);
 
 /*
  * Enter app callback
  */
 static void enter_BodyCamScanner_event_cb(lv_obj_t * obj, lv_event_t event);
 
 /*
  * Register app on startup
  */
 static int registered = app_autocall_function(&BodyCamScanner_setup, 13);
 
 /*
  * Setup the app and register the radar tile
  */
 void BodyCamScanner_setup(void) {
     if (!registered) return;
 
 #if defined(ONLY_ESSENTIAL)
     return;
 #endif
 
     BodyCamScanner_main_tile_num = mainbar_add_app_tile(1, 1, "BodyCam Scanner");
     BodyCamScanner = app_register("BodyCam\nScanner", &tactical_shield_64px, enter_BodyCamScanner_event_cb);
     BodyCamScanner_main_setup(BodyCamScanner_main_tile_num);
 }
 
 /*
  * Return the main tile number
  */
 uint32_t BodyCamScanner_get_app_main_tile_num(void) {
     return BodyCamScanner_main_tile_num;
 }
 
 /*
  * When icon clicked, jump to radar tile
  */
 static void enter_BodyCamScanner_event_cb(lv_obj_t * obj, lv_event_t event) {
     if (event == LV_EVENT_CLICKED) {
         app_hide_indicator(BodyCamScanner);
         mainbar_jump_to_tilenumber(BodyCamScanner_main_tile_num, LV_ANIM_OFF, true);
     }
 }