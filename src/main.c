#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"

#define MY_UUID {0xe4, 0x34, 0x7d, 0x44, 0xc2, 0xc8, 0x4e, 0x3c, 0xa4, 0x09, 0xd0, 0x22, 0xa2, 0x5c, 0x04, 0xcb}
PBL_APP_INFO(MY_UUID, "Tesla Status", "N/A", 3, 0 /* App version */, RESOURCE_ID_IMAGE_MENU_ICON, APP_INFO_WATCH_FACE);

Window window;

Layer time_layer;

TextLayer text_date_layer;
TextLayer text_time_layer;
Layer line_layer;


Layer car_layer;

TextLayer text_rated_layer;
TextLayer text_state_layer;
Layer battery_layer;

AppSync sync;
uint8_t sync_buffer[64];

uint8_t battery_percent;

enum {
  TESLA_RATED_REMAIN_KEY = 0x0,         // TUPLE_INT
  TESLA_STATE_STRING_KEY = 0x1,         // TUPLE_CSTRING
  TESLA_BATTERY_PERCENT_KEY = 0x2,      // TUPLE_INT
};


void line_layer_update_callback(Layer *me, GContext* ctx) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
	
  graphics_draw_line(ctx, GPoint(8, 29), GPoint(131, 29));
  graphics_draw_line(ctx, GPoint(8, 30), GPoint(131, 30));
}

void battery_layer_update_callback(Layer *me, GContext* ctx) {

  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);

  graphics_draw_rect(ctx, GRect(0, 0, 50, 10));
  graphics_fill_rect(ctx, GRect(0, 0, battery_percent/2, 10), 0, 0);
  graphics_fill_rect(ctx, GRect(50, 2, 6, 6), 2, GCornerTopRight  | GCornerBottomRight);
}

// TODO: Error handling
static void sync_error_callback(DictionaryResult dict_error, AppMessageResult app_message_error, void *context) {
}

static void sync_tuple_changed_callback(const uint32_t key, const Tuple* new_tuple, const Tuple* old_tuple, void* context) {
	static char remain_str[32] = "XXX";
	
	switch (key) {
		case TESLA_RATED_REMAIN_KEY:
			snprintf(remain_str, sizeof(remain_str), "%d mi", (int)(new_tuple->value->uint16));
			text_layer_set_text(&text_rated_layer, remain_str);		
			break;
		case TESLA_BATTERY_PERCENT_KEY:
			battery_percent = new_tuple->value->uint8;
			layer_mark_dirty(&battery_layer);
			break;
		case TESLA_STATE_STRING_KEY:
			text_layer_set_text(&text_state_layer, new_tuple->value->cstring);		
			break;
	}
}

void handle_init(AppContextRef ctx) {

  window_init(&window, "Simplicity");
  window_stack_push(&window, true /* Animated */);
  window_set_background_color(&window, GColorBlack);

  resource_init_current_app(&APP_RESOURCES);

  layer_init(&time_layer, GRect(0, 12, 144, 168-68));
  layer_add_child(&window.layer, &time_layer);

  text_layer_init(&text_date_layer, window.layer.frame);
  text_layer_set_text_color(&text_date_layer, GColorWhite);
  text_layer_set_background_color(&text_date_layer, GColorClear);
  layer_set_frame(&text_date_layer.layer, GRect(8, 0, 144-8, 168-68));
  text_layer_set_font(&text_date_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_21)));
  layer_add_child(&time_layer, &text_date_layer.layer);


  text_layer_init(&text_time_layer, window.layer.frame);
  text_layer_set_text_color(&text_time_layer, GColorWhite);
  text_layer_set_background_color(&text_time_layer, GColorClear);
  layer_set_frame(&text_time_layer.layer, GRect(7, 24, 144-7, 168-92));
  text_layer_set_font(&text_time_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_BOLD_SUBSET_49)));
  layer_add_child(&time_layer, &text_time_layer.layer);


  layer_init(&line_layer, window.layer.frame);
  line_layer.update_proc = &line_layer_update_callback;
  layer_add_child(&time_layer, &line_layer);

  layer_init(&car_layer, GRect(0, 90, 144, 168-90));
  layer_add_child(&window.layer, &car_layer);

  text_layer_init(&text_rated_layer, window.layer.frame);
  text_layer_set_text_color(&text_rated_layer, GColorWhite);
  text_layer_set_background_color(&text_rated_layer, GColorClear);
  layer_set_frame(&text_rated_layer.layer, GRect(8, 0, 144-8, 168-68));
  text_layer_set_font(&text_rated_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_21)));
  layer_add_child(&car_layer, &text_rated_layer.layer);
	
  text_layer_init(&text_state_layer, window.layer.frame);
  text_layer_set_text_color(&text_state_layer, GColorWhite);
  text_layer_set_background_color(&text_state_layer, GColorClear);
  layer_set_frame(&text_state_layer.layer, GRect(8, 20, 144-8, 168-68));
  text_layer_set_font(&text_state_layer, fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ROBOTO_CONDENSED_21)));
  layer_add_child(&car_layer, &text_state_layer.layer);
	
  layer_init(&battery_layer, GRect(72, 10, 144-70, 10));
  battery_layer.update_proc = &battery_layer_update_callback;
  layer_add_child(&car_layer, &battery_layer);

  Tuplet initial_values[] = {
    TupletInteger(TESLA_RATED_REMAIN_KEY, (uint16_t) 148),
    TupletCString(TESLA_STATE_STRING_KEY, "Charging"),
	TupletInteger(TESLA_BATTERY_PERCENT_KEY, (uint8_t) 72),
  };
  app_sync_init(&sync, sync_buffer, sizeof(sync_buffer), initial_values, ARRAY_LENGTH(initial_values),
                sync_tuple_changed_callback, sync_error_callback, NULL);
}

void handle_deinit(AppContextRef ctx) {
	app_sync_deinit(&sync);
}


void handle_minute_tick(AppContextRef ctx, PebbleTickEvent *t) {

  // Need to be static because they're used by the system later.
  static char time_text[] = "00:00";
  static char date_text[] = "Xxxxxxxxx 00";

  char *time_format;


  // TODO: Only update the date when it's changed.
  string_format_time(date_text, sizeof(date_text), "%B %e", t->tick_time);
  text_layer_set_text(&text_date_layer, date_text);


  if (clock_is_24h_style()) {
    time_format = "%R";
  } else {
    time_format = "%I:%M";
  }

  string_format_time(time_text, sizeof(time_text), time_format, t->tick_time);

  // Kludge to handle lack of non-padded hour format string
  // for twelve hour clock.
  if (!clock_is_24h_style() && (time_text[0] == '0')) {
    memmove(time_text, &time_text[1], sizeof(time_text) - 1);
  }

  text_layer_set_text(&text_time_layer, time_text);

}


void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,
    .messaging_info = {
      .buffer_sizes = {
        .inbound = 64,
        .outbound = 16,
      }
    },

    .tick_info = {
      .tick_handler = &handle_minute_tick,
      .tick_units = MINUTE_UNIT
    }

  };
  app_event_loop(params, &handlers);
}
