#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#include "resource_ids.auto.h"


#define MY_UUID { 0x61, 0x8C, 0xA5, 0x8D, 0xC0, 0xEB, 0x49, 0xDB, 0x98, 0x56, 0x03, 0x40, 0x36, 0xAE, 0xBC, 0x45 }
PBL_APP_INFO(MY_UUID,
             "Timer", "Micah Wylde",
             0, 3, /* App version */
             RESOURCE_ID_IMAGE_MENU_ICON,
             APP_INFO_STANDARD_APP);

#define WHITE_ON_BLACK

#ifndef WHITE_ON_BLACK
#define COLOR_FOREGROUND GColorBlack
#define COLOR_BACKGROUND GColorWhite
#else
#define COLOR_FOREGROUND GColorWhite
#define COLOR_BACKGROUND GColorBlack
#endif

enum State {
  DONE,
  SETTING,
  PAUSED,
  COUNTING_DOWN
};


// Main window stuff
Window window;

TextLayer title;
TextLayer count_down;
TextLayer time_text;

enum State current_state = DONE;

int total_seconds = 5 * 60;
int current_seconds = 5 * 60;
int last_set_time = -1;

const VibePattern alarm_finished = {
  .durations = (uint32_t []) {300, 150, 150, 150,  300, 150, 300},
  .num_segments = 7
};

// Setting state

enum SettingUnit {
  SETTING_SECOND = 2,
  SETTING_MINUTE = 1,
  SETTING_HOUR = 0,
};

enum SettingUnit setting_unit = SETTING_MINUTE;

void update_countdown() {
  if (current_seconds == last_set_time) {
    return;
  }

  static char time_text[] = " 00:00:00";

  PblTm time;
  time.tm_hour = current_seconds / (60 * 60);
  time.tm_min  = (current_seconds - time.tm_hour * 60 * 60) / 60;
  time.tm_sec  = current_seconds - time.tm_hour * 60 * 60 - time.tm_min * 60;

  string_format_time(time_text, sizeof(time_text), " %T", &time);

  text_layer_set_text(&count_down, time_text);

  last_set_time = current_seconds;
}


Layer unit_marker;
bool setting_blink_state = true;

void draw_setting_unit() {
  layer_mark_dirty(&unit_marker);  
}

void toggle_setting_mode(ClickRecognizerRef recognizer, Window *window) {
  if (current_state == SETTING) {
    current_state = DONE;
  }
  else {
    current_seconds = total_seconds;
    update_countdown();
    current_state = SETTING;
    setting_unit = SETTING_MINUTE;
    draw_setting_unit();
  }
}

void unit_marker_update_callback(Layer *me, GContext* ctx) {
  (void)me;

  int width = 32;
  int start = 8 + (width + 14) * setting_unit;

  if (current_state == SETTING && setting_blink_state) {
    graphics_context_set_stroke_color(ctx, COLOR_BACKGROUND);

    for (int i = 0; i < 4; i++) {
      graphics_draw_line(ctx, GPoint(start, 95 + i), GPoint(start + width, 95 + i));
    }
  }
}


void select_pressed(ClickRecognizerRef recognizer, Window *window) {
  if (current_state == SETTING) {
    setting_unit = (setting_unit + 1) % 3;
    setting_blink_state = true;
    draw_setting_unit();
  }
  else if (current_state == PAUSED || current_state == DONE) {
    current_state = COUNTING_DOWN;
  }
  else {
    current_state = PAUSED;
  }
}

void select_long_release_handler(ClickRecognizerRef recognizer, Window *window) {
  // This is needed to avoid missing clicks. Seems to be a bug in the SDK.
}

void increment_time(int direction) {
  if (current_state == SETTING) {
    switch (setting_unit) {
    case SETTING_HOUR: direction *= 60;
    case SETTING_MINUTE: direction *= 60;
    default: break;
    }

    int new_seconds = total_seconds + direction;
    if (new_seconds >= 0 && new_seconds < 100 * 60 * 60) {
      total_seconds = new_seconds;
      current_seconds = total_seconds;
      update_countdown();
    }
  }
}


void button_pressed_up(ClickRecognizerRef recognizer, Window *window) {
  increment_time(1);
}

void button_pressed_down(ClickRecognizerRef recognizer, Window *window) {
  increment_time(-1);
}

void reset_timer(ClickRecognizerRef recognizer, Window *window) {
  if (current_state != SETTING) {
    current_state = DONE;
    current_seconds = total_seconds;
    update_countdown();
  }
}


void main_click_provider(ClickConfig **config, Window *window) {
  // See ui/click.h for more information and default values.

  config[BUTTON_ID_SELECT]->click.handler = 
    (ClickHandler) select_pressed;

  config[BUTTON_ID_SELECT]->long_click.handler = 
    (ClickHandler) toggle_setting_mode;

  config[BUTTON_ID_SELECT]->multi_click.handler = (ClickHandler) reset_timer;
  config[BUTTON_ID_SELECT]->multi_click.min = 2;
  config[BUTTON_ID_SELECT]->multi_click.max = 2;

  config[BUTTON_ID_SELECT]->long_click.release_handler = 
    (ClickHandler) select_long_release_handler;

  config[BUTTON_ID_SELECT]->long_click.delay_ms = 700;

  config[BUTTON_ID_UP]->click.handler = (ClickHandler) button_pressed_up;
  config[BUTTON_ID_UP]->click.repeat_interval_ms = 300;

  config[BUTTON_ID_DOWN]->click.handler = (ClickHandler) button_pressed_down;
  config[BUTTON_ID_DOWN]->click.repeat_interval_ms = 300;


  (void)window;
}


void handle_second_counting_down() {
  current_seconds--;

  update_countdown();

  if (current_seconds == 0) {
    vibes_enqueue_custom_pattern(alarm_finished);
    current_state = DONE;
  }
}

void handle_second_waiting() {
  current_seconds = total_seconds;
  update_countdown();
}

void handle_second_setting() {
  setting_blink_state = !setting_blink_state;
  layer_mark_dirty(&unit_marker);
}

void update_time(PblTm *tick_time) {
  static char time[] = "Xxxxxxxxx - 00 00:00";

  char *time_format;

  if (clock_is_24h_style()) {
    time_format = "%B %e   %R";
  } else {
    time_format = "%B %e   %I:%M";
  }

  string_format_time(time, sizeof(time), time_format, tick_time);

  text_layer_set_text(&time_text, time);
}

void handle_second_tick(AppContextRef ctx, PebbleTickEvent *t) {
  switch(current_state) {
  case DONE:
    handle_second_waiting();
    break;
  case COUNTING_DOWN:
    handle_second_counting_down();
    break;
  case SETTING:
    handle_second_setting();
    break;
  default:
    break;
  }

  if (t->units_changed & MINUTE_UNIT) {
    update_time(t->tick_time);
  }
}

void handle_init(AppContextRef ctx) {
  (void)ctx;

  resource_init_current_app(&TIMER_RESOURCES);

  window_init(&window, "Main Window");
  window_stack_push(&window, true /* Animated */);

  window_set_background_color(&window, COLOR_BACKGROUND);
  window_set_fullscreen(&window, true);
  window_set_click_config_provider(&window, (ClickConfigProvider) main_click_provider);

  GFont custom_font = \
    fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_32));

  text_layer_init(&count_down, GRect(0, 62, 144, 40));
  text_layer_set_font(&count_down, custom_font);
  text_layer_set_text_color(&count_down, COLOR_BACKGROUND);
  text_layer_set_background_color(&count_down, COLOR_FOREGROUND);
  update_countdown();
  layer_add_child(&window.layer, &count_down.layer);

  layer_init(&unit_marker, window.layer.frame);
  unit_marker.update_proc = unit_marker_update_callback;
  layer_add_child(&window.layer, &unit_marker);

  text_layer_init(&title, GRect(30, 5, 100, 24));
  text_layer_set_font(&title, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_color(&title, COLOR_FOREGROUND);
  text_layer_set_background_color(&title, GColorClear);
  text_layer_set_text(&title, "pebble timer");
  layer_add_child(&window.layer, &title.layer);

  text_layer_init(&time_text, GRect(20, 130, 110, 24));
  text_layer_set_font(&time_text, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_color(&time_text, COLOR_FOREGROUND);
  text_layer_set_background_color(&time_text, GColorClear);
  PblTm time;
  get_time(&time);
  update_time(&time);
  layer_add_child(&window.layer, &time_text.layer);
}


void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,

    .tick_info = {
      .tick_handler = &handle_second_tick,
      .tick_units = SECOND_UNIT
    }
  };
  app_event_loop(params, &handlers);
}

