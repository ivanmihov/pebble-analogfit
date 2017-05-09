#include "analog_fit.h"
#include "pebble.h"
#include <pebble-bluetooth-icon/pebble-bluetooth-icon.h>

static Window *s_window;
static Layer *s_hands_layer, *s_seconds_layer;
static TextLayer *s_label, *s_num_label, *s_time, *s_date, *s_bpm_label;
static TextLayer *s_battery_layer, *s_phone_battery_layer, *s_custom_text;
//static TextLayer *s_connection_layer;

static BitmapLayer *background_image_layer, *heart_image_layer, *shoe_image_layer;
static BitmapLayer *battery_image_layer, *phone_battery_image_layer;
static GBitmap *background_image, *heart_image, *shoe_image;
static GBitmap *battery_image, *phone_battery_image;

static BluetoothLayer *s_bluetooth_layer;

static GPath *s_tick_paths[NUM_CLOCK_TICKS];
static GPath *s_minute_arrow, *s_hour_arrow;

static GColor TextColor, BackgroundColor;
static uint32_t imageBackground, imageSteps, imageHrm, imageWatchBattery, imagePhoneBattery;

static bool stepsGoalReached = false;

static bool inverted = false;
static bool bt_connected = false;
static bool phone_baterry_available = false;

//static AppSync sync;
//static uint8_t sync_buffer[128];

// Largest expected inbox and outbox message sizes
const uint32_t inbox_size = 128;
const uint32_t outbox_size = 128;

/*enum {
//  PhoneBattery = 0x0,
};*/
static void update_colors() {
	// Invert Colors
	background_image = gbitmap_create_with_resource(imageBackground);
	heart_image = gbitmap_create_with_resource(imageHrm);
	shoe_image = gbitmap_create_with_resource(imageSteps);
	battery_image = gbitmap_create_with_resource(imageWatchBattery);
	phone_battery_image = gbitmap_create_with_resource(imagePhoneBattery);
	bitmap_layer_set_bitmap(background_image_layer, background_image);
	bitmap_layer_set_bitmap(heart_image_layer, heart_image);
	bitmap_layer_set_bitmap(shoe_image_layer, shoe_image);
	bitmap_layer_set_bitmap(battery_image_layer, battery_image);
	bitmap_layer_set_bitmap(phone_battery_image_layer, phone_battery_image);
	
	text_layer_set_text_color(s_time, TextColor);
	text_layer_set_text_color(s_date, TextColor);
	text_layer_set_text_color(s_battery_layer, TextColor);
	text_layer_set_text_color(s_phone_battery_layer, TextColor);
	text_layer_set_text_color(s_num_label, TextColor);
	text_layer_set_text_color(s_bpm_label, TextColor);
	text_layer_set_text_color(s_custom_text, TextColor);
}

static void defaultColors() {
	BackgroundColor = GColorBlack;
	TextColor = GColorWhite;
	imageBackground = RESOURCE_ID_IMAGE_BACKGROUND_BLACK;
	imageSteps = RESOURCE_ID_IMAGE_SHOE_WHITE;
	imageHrm = RESOURCE_ID_IMAGE_HEART_WHITE;
	imageWatchBattery = RESOURCE_ID_IMAGE_WATCH_WHITE;
	imagePhoneBattery = RESOURCE_ID_IMAGE_PHONE_WHITE;
	
	inverted = false;
}

static void invertedColors() {
	BackgroundColor = GColorWhite;
	TextColor = GColorBlack;
	imageBackground = RESOURCE_ID_IMAGE_BACKGROUND_WHITE;
	imageSteps = RESOURCE_ID_IMAGE_SHOE_BLACK;
	imageHrm = RESOURCE_ID_IMAGE_HEART_BLACK;
	imageWatchBattery = RESOURCE_ID_IMAGE_WATCH_BLACK;
	imagePhoneBattery = RESOURCE_ID_IMAGE_PHONE_BLACK;
	
	inverted = true;
}

static void setDefaultColors() {
	defaultColors();
	update_colors();
}
static void setInvertedColors() {
	invertedColors();
	update_colors();
}

static void toggle_invert_colors() {
	if(inverted) {
		setDefaultColors();
	} else {
		setInvertedColors();
	}
}

////////////////////// CLAY START ///////////////////////////////

// A struct for our specific settings (see main.h)
ClaySettings settings;

// Initialize the default settings
static void prv_default_settings() {
	strcpy(settings.StepsGoal, "10000");
	strcpy(settings.CustomText, "");
	settings.InvertedColor = false;
	settings.ShowSeconds = false;
}

// Read settings from persistent storage
static void prv_load_settings() {
  	// Load the default settings
 		prv_default_settings();
	
  	// Read settings from persistent storage, if they exist
  	persist_read_data(SETTINGS_KEY, &settings, sizeof(settings));
	
		//assign colors if inverted
		if(settings.InvertedColor) {
			invertedColors();
		} else {
			defaultColors();
		}
}

// Save the settings to persistent storage
static void prv_save_settings() {
 	persist_write_data(SETTINGS_KEY, &settings, sizeof(settings));

	// Update the display based on new settings
  prv_update_display();
}

static void prv_update_phone_battery_display() {
	// Phone percent
	if(phone_baterry_available) {
		layer_set_hidden((Layer *)s_phone_battery_layer, false);
		layer_set_hidden((Layer *)phone_battery_image_layer, false);
		text_layer_set_text(s_phone_battery_layer, settings.PhoneBattery);
	} else {
		layer_set_hidden((Layer *)s_phone_battery_layer, true);
		layer_set_hidden((Layer *)phone_battery_image_layer, true);
	}
}

// Update the display elements
static void prv_update_display() {
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "prv_update_display called");

	if(settings.ShowSeconds) {
			layer_set_hidden(s_seconds_layer, false);
	} else {
			layer_set_hidden(s_seconds_layer, true);
	}
	
	if(settings.InvertedColor & !inverted) {
			setInvertedColors();
	} else if(!settings.InvertedColor & inverted) {
			setDefaultColors();
	}
	
	text_layer_set_text(s_custom_text, settings.CustomText);
}

//############################### APP SYNC MESSAGES ################################
//USED
static void prv_inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *steps_goal = dict_find(iter, MESSAGE_KEY_steps_goal);
	Tuple *phonebatt = dict_find(iter, MESSAGE_KEY_PhoneBattery);
	Tuple *inverted_color = dict_find(iter, MESSAGE_KEY_inverted_color);
	Tuple *show_seconds = dict_find(iter, MESSAGE_KEY_show_seconds);
	Tuple *custom_text = dict_find(iter, MESSAGE_KEY_custom_text);

	if(phonebatt) {
		if(strlen(phonebatt->value->cstring) > 0) {
			phone_baterry_available = true;
		}
    //GColor bg_color = GColorFromHEX(bg_color_t->value->int32);
		//APP_LOG(APP_LOG_LEVEL_DEBUG, "Battery: %s", phonebatt->value->cstring);
		strcpy(settings.PhoneBattery, phonebatt->value->cstring);
		prv_update_phone_battery_display();
  } else {
  		if(steps_goal) {
    		//APP_LOG(APP_LOG_LEVEL_DEBUG, "STEP GOAL: %s", steps_goal->value->cstring);
				strcpy(settings.StepsGoal, steps_goal->value->cstring);
				stepsGoalReached = false; //reset the steps reached goal, causes it to vibrate on window reload
			}
			if(inverted_color) {
				//APP_LOG(APP_LOG_LEVEL_DEBUG, "Inverted: %d", inverted_color->value->int8);
				settings.InvertedColor = inverted_color->value->int8;
			}
			if(show_seconds) {
				//APP_LOG(APP_LOG_LEVEL_DEBUG, "Show Secconds: %d", show_seconds->value->int8);
				settings.ShowSeconds = show_seconds->value->int8;
				//layer_mark_dirty(window_get_root_layer(s_main_window));
			}
			if(custom_text) {
				strcpy(settings.CustomText, custom_text->value->cstring);
			}
	
	// Save the new settings to persistent storage
  	prv_save_settings();
	}
}

static void in_dropped_handler(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Dropped!");
}

static void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
  //if (s_wasFirstMsg && s_dataInited) {
    // Ignore, was successful
  //} else {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Failed to Send!");
  //}

  //s_wasFirstMsg = false;
}

//######################## HANDLERS START ##############################

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Health //////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
static void health_handler(HealthEventType event, void *context) {
  static char s_value_buffer[8];
  if (event == HealthEventMovementUpdate) {
    // display the step count
    snprintf(s_value_buffer, sizeof(s_value_buffer), "%d", (int)health_service_sum_today(HealthMetricStepCount));
    text_layer_set_text(s_num_label, s_value_buffer);
						
		//Vibrate only once on steps goal reach
		if( atoi(settings.StepsGoal) > 0 &&
			 ((int)health_service_sum_today(HealthMetricStepCount) >= atoi(settings.StepsGoal)) && 
			 stepsGoalReached == false) {
			// Vibe pattern: ON for 200ms, OFF for 100ms, ON for 400ms:
			static const uint32_t segments[] = { 200, 100, 100, 50, 100, 50, 700};
			VibePattern pat = {
  			.durations = segments,
  			.num_segments = ARRAY_LENGTH(segments),
			};
			vibes_enqueue_custom_pattern(pat);
			stepsGoalReached = true;
		}
  }
	
	#if PBL_API_EXISTS(health_service_peek_current_value)
    /** Display the Heart Rate **/
    HealthValue value = health_service_peek_current_value(HealthMetricHeartRateBPM);
    static char s_hrm_buffer[8];
    snprintf(s_hrm_buffer, sizeof(s_hrm_buffer), "%lu", (uint32_t) value);
    text_layer_set_text(s_bpm_label, s_hrm_buffer);
    layer_set_hidden(text_layer_get_layer(s_bpm_label), false);
  #else
    layer_set_hidden(text_layer_get_layer(s_bpm_label), true);
  #endif
}

/*static void bg_update_proc(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, GColorWhite);
  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    const int x_offset = PBL_IF_ROUND_ELSE(18, 0);
    const int y_offset = PBL_IF_ROUND_ELSE(6, 0);
    gpath_move_to(s_tick_paths[i], GPoint(x_offset, y_offset));
    gpath_draw_filled(ctx, s_tick_paths[i]);
  }
}*/

/////////////////////////////////////////////////////////////////////////////
/////////////////////// DAY TICK HANDLER ///////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/*static void handle_day_tick(struct tm *tick_time, TimeUnits units_changed) {
  //reset goal every day
//	stepsGoalReached = false;
}*/

/////////////////////////////////////////////////////////////////////////////
/////////////////////// TIME TICK HANDLER ///////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
static void handle_time_tick(struct tm *tick_time, TimeUnits units_changed) {
	if(units_changed & DAY_UNIT){ // The date has changed
		static char s_date_text[] = "Wed, Jan 01";
		// Date items in PblTm Struct if needed are int tm_mday, int tm_mon
		strftime(s_date_text, sizeof(s_date_text), "%a, %b %d", tick_time);
		text_layer_set_text(s_date, s_date_text);
		stepsGoalReached = false; //refresh the steps goal
  }
	
	if(units_changed & MINUTE_UNIT) {
		// Needs to be static because it's used by the system later.
  	static char s_time_text[] = "00:00 xx";
  	//strftime(s_time_text, sizeof(s_time_text), clock_is_24h_style()?"%T":"%I:%M %p", tick_time);
		clock_copy_time_string(s_time_text, sizeof(s_time_text));
  	text_layer_set_text(s_time, s_time_text);
		
		layer_mark_dirty(s_hands_layer);
	}
	
	if(units_changed & SECOND_UNIT & settings.ShowSeconds) {
		//hands_update_proc(window_get_root_layer(s_window), NULL);
		layer_mark_dirty(s_seconds_layer);
	}
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////// BT HANDLER //////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
static void handle_bluetooth(bool connected) {
	//handle bluetooth connections
	//TODO: invert on disconnect
  //text_layer_set_text(s_connection_layer, connected ? "connected" : "disconnected");
	if (connected) {
		layer_set_hidden(s_bluetooth_layer, false);
		bt_connected = true;
		if(settings.InvertedColor & !inverted) {
				setInvertedColors();
			} else if(!settings.InvertedColor & inverted) {
				setDefaultColors();
		}
	} else {
			layer_set_hidden(s_bluetooth_layer, true);
			bt_connected = false;
			if(settings.InvertedColor & inverted) {
				setDefaultColors();
			} else if(!settings.InvertedColor & !inverted) {
				setInvertedColors();
			}
	}
}

/////////////////////////////////////////////////////////////////////////////
////////////////////////// BATTERY HANDLER //////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
static void handle_battery(BatteryChargeState charge_state) {
  static char battery_text[] = "100";

  if (charge_state.is_charging) {
    snprintf(battery_text, sizeof(battery_text), "%d", charge_state.charge_percent);
  } else {
    snprintf(battery_text, sizeof(battery_text), "%d", charge_state.charge_percent);
  }
  text_layer_set_text(s_battery_layer, battery_text);
}

/*static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
  APP_LOG(APP_LOG_LEVEL_DEBUG, "App Message Sync Error: %d", app_message_error);
}

//UNUSED
void sync_tuple_changed_callback(const uint32_t key,
    const Tuple* new_tuple, const Tuple* old_tuple, void* context) {

	APP_LOG(APP_LOG_LEVEL_DEBUG, "MYMSG: %d", key);
  switch (key) {
    case PhoneBattery:
			APP_LOG(APP_LOG_LEVEL_DEBUG, "TESTA: %s", new_tuple->value->cstring);
      text_layer_set_text(s_phone_battery_layer, new_tuple->value->cstring);
      break;
  }
}*/

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////// HANDS UPDATE ////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
static void hands_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);
	
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  // minute/hour hand
  graphics_context_set_fill_color(ctx, TextColor);
  graphics_context_set_stroke_color(ctx, BackgroundColor);

  gpath_rotate_to(s_minute_arrow, TRIG_MAX_ANGLE * t->tm_min / 60);
  gpath_draw_filled(ctx, s_minute_arrow);
  gpath_draw_outline(ctx, s_minute_arrow);

  gpath_rotate_to(s_hour_arrow, (TRIG_MAX_ANGLE * (((t->tm_hour % 12) * 6) + (t->tm_min / 10))) / (12 * 6));
  gpath_draw_filled(ctx, s_hour_arrow);
  gpath_draw_outline(ctx, s_hour_arrow);

  // dot in the middle
  graphics_context_set_fill_color(ctx, BackgroundColor);
  graphics_fill_rect(ctx, GRect(bounds.size.w / 2 - 1, bounds.size.h / 2 - 1, 3, 3), 0, GCornerNone);
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////// Seconds UPDATE ////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
static void seconds_update_proc(Layer *layer, GContext *ctx) {
	GRect bounds = layer_get_bounds(layer);
  GPoint center = grect_center_point(&bounds);

  const int16_t second_hand_length = PBL_IF_ROUND_ELSE((bounds.size.w / 2) - 19, bounds.size.w / 2);

  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  int32_t second_angle = TRIG_MAX_ANGLE * t->tm_sec / 60;
  GPoint second_hand = {
    .x = (int16_t)(sin_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.x,
    .y = (int16_t)(-cos_lookup(second_angle) * (int32_t)second_hand_length / TRIG_MAX_RATIO) + center.y,
  };

  // second hand
  graphics_context_set_stroke_color(ctx, TextColor);
  graphics_draw_line(ctx, second_hand, center);
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////// WINDOWS LOAD ////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
static void window_load(Window *window) {
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "WINDOW_LOAD CALLED");
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
	
	//background start
	background_image_layer = bitmap_layer_create(bounds);
  background_image = gbitmap_create_with_resource(imageBackground);
  bitmap_layer_set_compositing_mode(background_image_layer, GCompOpAssign);
  bitmap_layer_set_bitmap(background_image_layer, background_image);
  bitmap_layer_set_alignment(background_image_layer, GAlignCenter);
	
	//heart image start
	heart_image_layer = bitmap_layer_create(GRect(127, 142, 10, 10));
  heart_image = gbitmap_create_with_resource(imageHrm);
  bitmap_layer_set_compositing_mode(heart_image_layer, GCompOpAssign);
  bitmap_layer_set_bitmap(heart_image_layer, heart_image);
  bitmap_layer_set_alignment(heart_image_layer, GAlignBottomRight);
	
	//shoe image start
	shoe_image_layer = bitmap_layer_create(GRect(7, 140, 15, 14));
  shoe_image = gbitmap_create_with_resource(imageSteps);
  bitmap_layer_set_compositing_mode(shoe_image_layer, GCompOpAssign);
  bitmap_layer_set_bitmap(shoe_image_layer, shoe_image);
  bitmap_layer_set_alignment(shoe_image_layer, GAlignBottomLeft);
	
	//electronic time start
	//GRect(x,y,w,h);
	//Screen Size: 144x168
  s_time = text_layer_create(GRect(0,5,bounds.size.w,24));
	text_layer_set_background_color(s_time, GColorClear);
  text_layer_set_text_color(s_time, TextColor);
  text_layer_set_font(s_time, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
	text_layer_set_text_alignment(s_time, GTextAlignmentCenter);
	
	//date start
	s_date = text_layer_create(GRect(0,30,bounds.size.w,20));
	text_layer_set_background_color(s_date, GColorClear);
  text_layer_set_text_color(s_date, TextColor);
  text_layer_set_font(s_date, fonts_get_system_font(FONT_KEY_GOTHIC_18));
	text_layer_set_text_alignment(s_date, GTextAlignmentCenter);
	
	//bluetooth start
	/*s_connection_layer = text_layer_create(GRect(0, 90, bounds.size.w, 20));
  text_layer_set_text_color(s_connection_layer, GColorWhite);
  text_layer_set_background_color(s_connection_layer, GColorClear);
  text_layer_set_font(s_connection_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_connection_layer, GTextAlignmentCenter);
  handle_bluetooth(connection_service_peek_pebble_app_connection());
	*/
	s_bluetooth_layer = bluetooth_layer_create();
	bluetooth_set_position(phone_baterry_available ? GPoint(120, 79) : GPoint(124, 15));
	bluetooth_vibe_disconnect(false);
	bluetooth_vibe_connect(false);
	
	//s_custom_text
	s_custom_text = text_layer_create(GRect(0, 110, bounds.size.w, 22));
  text_layer_set_text_color(s_custom_text, TextColor);
  text_layer_set_background_color(s_custom_text, GColorClear);
  text_layer_set_font(s_custom_text, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_custom_text, GTextAlignmentCenter);
	text_layer_set_text(s_custom_text, settings.CustomText);

	//battery start
  s_battery_layer = text_layer_create(GRect(17, 8, (bounds.size.w/2)-10, 20));
  text_layer_set_text_color(s_battery_layer, TextColor);
  text_layer_set_background_color(s_battery_layer, GColorClear);
  text_layer_set_font(s_battery_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_battery_layer, GTextAlignmentLeft);
	
	//battery icon
	battery_image_layer = bitmap_layer_create(GRect(7, 11, 10, 20));
  battery_image = gbitmap_create_with_resource(imageWatchBattery);
  bitmap_layer_set_compositing_mode(battery_image_layer, GCompOpAssign);
  bitmap_layer_set_bitmap(battery_image_layer, battery_image);
  bitmap_layer_set_alignment(battery_image_layer, GAlignCenter);
	
	//phone battery start
  s_phone_battery_layer = text_layer_create(GRect((bounds.size.w/2)-9, 8, (bounds.size.w/2)-8, 20));
  text_layer_set_text_color(s_phone_battery_layer, TextColor);
  text_layer_set_background_color(s_phone_battery_layer, GColorClear);
  text_layer_set_font(s_phone_battery_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
  text_layer_set_text_alignment(s_phone_battery_layer, GTextAlignmentRight);
	layer_set_hidden((Layer *)s_phone_battery_layer, true);
	
	//phone battery icon
	phone_battery_image_layer = bitmap_layer_create(GRect(127, 11, 10, 20));
  phone_battery_image = gbitmap_create_with_resource(imagePhoneBattery);
  bitmap_layer_set_compositing_mode(phone_battery_image_layer, GCompOpAssign);
  bitmap_layer_set_bitmap(phone_battery_image_layer, phone_battery_image);
  bitmap_layer_set_alignment(phone_battery_image_layer, GAlignCenter);
	layer_set_hidden((Layer *)phone_battery_image_layer, true);

	//steps start
  s_num_label = text_layer_create(PBL_IF_ROUND_ELSE(
    GRect(90, 114, 58, 20),
    GRect(23, 135, 62, 20)));
  text_layer_set_background_color(s_num_label, GColorClear);
  text_layer_set_text_color(s_num_label, TextColor);
  text_layer_set_font(s_num_label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_alignment(s_num_label, GTextAlignmentLeft);
	
	//bpm start
	s_bpm_label = text_layer_create(PBL_IF_ROUND_ELSE(
    GRect(90, 114, 58, 20),
    GRect(70, 135, 53, 20)));
  text_layer_set_background_color(s_bpm_label, GColorClear);
  text_layer_set_text_color(s_bpm_label, TextColor);
  text_layer_set_font(s_bpm_label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
	text_layer_set_text_alignment(s_bpm_label, GTextAlignmentRight);
	
	//hands start
	
	s_seconds_layer = layer_create(bounds);
	s_hands_layer = layer_create(bounds);
	layer_set_update_proc(s_seconds_layer, seconds_update_proc);
  layer_set_update_proc(s_hands_layer, hands_update_proc);
	
	if(!settings.ShowSeconds) {
			layer_set_hidden(s_seconds_layer, true);
	}

  // subscribe to health events
  if(health_service_events_subscribe(health_handler, NULL)) {
    // force initial steps display
    health_handler(HealthEventMovementUpdate, NULL);
  } else {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Health not available!");
  }
	
	/*Tuplet initial_values[] = {
  	TupletCString(PhoneBattery, "...")
	};
	
	app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
      sync_tuple_changed_callback, sync_error_callback, NULL);*/
	
	layer_add_child(window_layer, bitmap_layer_get_layer(background_image_layer));
	layer_add_child(window_layer, bitmap_layer_get_layer(heart_image_layer));
	layer_add_child(window_layer, bitmap_layer_get_layer(shoe_image_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_time));
	layer_add_child(window_layer, text_layer_get_layer(s_date));
	layer_add_child(window_layer, text_layer_get_layer(s_num_label));
	layer_add_child(window_layer, text_layer_get_layer(s_bpm_label));
	layer_add_child(window_layer, text_layer_get_layer(s_custom_text));
	//layer_add_child(window_layer, text_layer_get_layer(s_connection_layer));
	layer_add_child(window_layer, s_bluetooth_layer);
  layer_add_child(window_layer, text_layer_get_layer(s_battery_layer));
	layer_add_child(window_layer, text_layer_get_layer(s_phone_battery_layer));
	layer_add_child(window_layer, bitmap_layer_get_layer(battery_image_layer));
	layer_add_child(window_layer, bitmap_layer_get_layer(phone_battery_image_layer));
	layer_add_child(window_layer, s_seconds_layer);
	layer_add_child(window_layer, s_hands_layer);
	
  handle_battery(battery_state_service_peek());

	//Get a time structure so that the face doesn't start blank
  time_t temp = time(NULL);
  struct tm *t = localtime(&temp);
  //Manually call the tick handler when the window is loading
  handle_time_tick(t, HOUR_UNIT | MINUTE_UNIT | DAY_UNIT);
}

/////////////////////////////////////////////////////////////////////////////
///////////////////////////// WINDOWS UNLOAD ////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
static void window_unload(Window *window) {
  health_service_events_unsubscribe();
	tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  connection_service_unsubscribe();

  //layer_destroy(s_simple_bg_layer);
  //layer_destroy(s_date_layer);

	//text_layer_destroy(s_connection_layer);
  text_layer_destroy(s_battery_layer);
	text_layer_destroy(s_phone_battery_layer);
  text_layer_destroy(s_label);
  text_layer_destroy(s_num_label);
	text_layer_destroy(s_bpm_label);
	text_layer_destroy(s_time);
	
	text_layer_destroy(s_custom_text);
	
	bluetooth_layer_destroy(s_bluetooth_layer);

  layer_destroy(s_hands_layer);
	layer_destroy(s_seconds_layer);
	
	gbitmap_destroy(background_image);
	gbitmap_destroy(battery_image);
	gbitmap_destroy(phone_battery_image);
  bitmap_layer_destroy(background_image_layer);
	bitmap_layer_destroy(battery_image_layer);
	bitmap_layer_destroy(phone_battery_image_layer);
}

/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////// INIT ////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
static void init() {
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "init CALLED");
	//Load the settings
	prv_load_settings();
	
  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
	
	// Register message handlers
  app_message_register_inbox_received(prv_inbox_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_register_outbox_failed(out_failed_handler);
	
	// Init buffers
	app_message_open(inbox_size, outbox_size);

  // init hand paths
  s_minute_arrow = gpath_create(&MINUTE_HAND_POINTS);
  s_hour_arrow = gpath_create(&HOUR_HAND_POINTS);

  Layer *window_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(window_layer);
  GPoint center = grect_center_point(&bounds);
  gpath_move_to(s_minute_arrow, center);
  gpath_move_to(s_hour_arrow, center);
	
	for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    s_tick_paths[i] = gpath_create(&ANALOG_BG_POINTS[i]);
  }

  tick_timer_service_subscribe(SECOND_UNIT, handle_time_tick);
	//tick_timer_service_subscribe(MINUTE_UNIT, handle_time_tick);
	//tick_timer_service_subscribe(DAY_UNIT, handle_day_tick);
	
	battery_state_service_subscribe(handle_battery);
	connection_service_subscribe((ConnectionHandlers) {
    .pebble_app_connection_handler = handle_bluetooth
  });
}

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////// DEINIT ////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
static void deinit() {
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "deinit CALLED");
  gpath_destroy(s_minute_arrow);
  gpath_destroy(s_hour_arrow);

  for (int i = 0; i < NUM_CLOCK_TICKS; ++i) {
    gpath_destroy(s_tick_paths[i]);
  }

  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

/////////////////////////////////////////////////////////////////////////////
////////////////////////////////// MAIN /////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
int main() {
	//APP_LOG(APP_LOG_LEVEL_DEBUG, "main CALLED");
  init();
  app_event_loop();
  deinit();
}
