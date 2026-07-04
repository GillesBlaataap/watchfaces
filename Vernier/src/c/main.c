#include <pebble.h>

static Window *s_main_window;
static Layer *s_dial_layer;
static Layer *s_rotating_ticks_layer;
static bool s_dark_mode = false;

// Forward declarations
static void update_dial_for_time(struct tm *t);
static void update_date(void);

// Date
static TextLayer *s_date_layer;
static char s_date_buffer[16];

// Base rotation from time
static float s_dial_angle = 0;

// Wiggle animation offset
static float s_wiggle_offset = 0;
static Animation *s_wiggle_anim;
static bool s_is_wiggling = false;

// ===============================
// USER PARAMETERS
// ===============================
static int RING_GAP = 40;
static int MOVING_TICK_LEN = 6;

// Forward declaration
static void update_dial_for_time(struct tm *t);

// ===============================
// ANGLE NORMALIZATION
// ===============================
static int32_t normalize_angle(float deg) {
  while(deg < 0) deg += 360;
  while(deg >= 360) deg -= 360;
  return (int32_t)(TRIG_MAX_ANGLE * deg / 360);
}

// ===============================
// DATE UPDATE
// ===============================
static void update_date(void) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  text_layer_set_text_color(s_date_layer,
    s_dark_mode ? GColorWhite : GColorBlack);
  text_layer_set_background_color(s_date_layer, GColorClear);

  strftime(s_date_buffer, sizeof(s_date_buffer), "%a %d %b", t);
  text_layer_set_text(s_date_layer, s_date_buffer);
}

// ===============================
// APPLY COLORS
// ===============================
static void apply_colors(GContext *ctx) {
  if(s_dark_mode) {
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_text_color(ctx, GColorWhite);
  } else {
    graphics_context_set_stroke_color(ctx, GColorBlack);
    graphics_context_set_text_color(ctx, GColorBlack);
  }
}

// ===============================
// ROTATING TICKMARKS
// ===============================
static void rotating_ticks_update_proc(Layer *layer, GContext *ctx) {
  apply_colors(ctx);

  GRect bounds = layer_get_bounds(layer);
  GPoint center = GPoint(bounds.size.w / 2,
                         bounds.size.h / 2 - (bounds.size.h - bounds.size.w)/2);

  int outer_r = bounds.size.w / 2 - 1;
  int inner_r = outer_r - RING_GAP;

  graphics_context_set_stroke_width(ctx, 2);

  int tick_outer_1 = outer_r - 2;
  int tick_inner_1 = tick_outer_1 - MOVING_TICK_LEN;

  int tick_inner_2 = inner_r + 2;
  int tick_outer_2 = tick_inner_2 + MOVING_TICK_LEN;

  for(int i = 0; i < 12; i++) {
    float angle_deg = (i * 27.5f) + s_dial_angle + s_wiggle_offset;
    int32_t angle = normalize_angle(angle_deg);

    GPoint a1 = {
      .x = center.x + (sin_lookup(angle) * tick_inner_1 / TRIG_MAX_RATIO),
      .y = center.y - (cos_lookup(angle) * tick_inner_1 / TRIG_MAX_RATIO)
    };
    GPoint b1 = {
      .x = center.x + (sin_lookup(angle) * tick_outer_1 / TRIG_MAX_RATIO),
      .y = center.y - (cos_lookup(angle) * tick_outer_1 / TRIG_MAX_RATIO)
    };
    graphics_draw_line(ctx, a1, b1);

    GPoint a2 = {
      .x = center.x + (sin_lookup(angle) * tick_inner_2 / TRIG_MAX_RATIO),
      .y = center.y - (cos_lookup(angle) * tick_inner_2 / TRIG_MAX_RATIO)
    };
    GPoint b2 = {
      .x = center.x + (sin_lookup(angle) * tick_outer_2 / TRIG_MAX_RATIO),
      .y = center.y - (cos_lookup(angle) * tick_outer_2 / TRIG_MAX_RATIO)
    };
    graphics_draw_line(ctx, a2, b2);
  }
}

// ===============================
// DIAL UPDATE
// ===============================
static void dial_update_proc(Layer *layer, GContext *ctx) {
  apply_colors(ctx);

  GRect bounds = layer_get_bounds(layer);
  GPoint center = GPoint(bounds.size.w / 2,
                         bounds.size.h / 2 - (bounds.size.h - bounds.size.w)/2);

  int outer_r = bounds.size.w / 2 - 1;
  int inner_r = outer_r - RING_GAP;

  graphics_context_set_stroke_width(ctx, 2);

  graphics_draw_circle(ctx, center, outer_r);
  graphics_draw_circle(ctx, center, inner_r);

  int hour_tick_r1 = inner_r - 10;
  int hour_tick_r2 = inner_r - 2;

  for(int i = 0; i < 12; i++) {
    int32_t angle = TRIG_MAX_ANGLE * i / 12;

    GPoint p1 = {
      .x = center.x + (sin_lookup(angle) * hour_tick_r1 / TRIG_MAX_RATIO),
      .y = center.y - (cos_lookup(angle) * hour_tick_r1 / TRIG_MAX_RATIO)
    };
    GPoint p2 = {
      .x = center.x + (sin_lookup(angle) * hour_tick_r2 / TRIG_MAX_RATIO),
      .y = center.y - (cos_lookup(angle) * hour_tick_r2 / TRIG_MAX_RATIO)
    };

    graphics_context_set_stroke_width(ctx, (i == 0 ? 5 : 2));
    graphics_draw_line(ctx, p1, p2);
  }

  graphics_context_set_stroke_width(ctx, 2);

  // RED DOT (unchanged)
  {
    int dot_r = 4;
    float angle_deg = s_dial_angle + s_wiggle_offset;
    int32_t angle = normalize_angle(angle_deg);

    int dot_dist = inner_r + 8;

    GPoint dot = {
      .x = center.x + (sin_lookup(angle) * dot_dist / TRIG_MAX_RATIO),
      .y = center.y - (cos_lookup(angle) * dot_dist / TRIG_MAX_RATIO)
    };

    graphics_context_set_fill_color(ctx, GColorRed);
    graphics_fill_circle(ctx, dot, dot_r);
  }

  // ROTATING NUMBERS
  for(int i = 0; i < 12; i++) {
    float angle_deg = i * 27.5f + s_dial_angle + s_wiggle_offset;
    int32_t angle = normalize_angle(angle_deg);

    int text_r = inner_r + 20;

    GPoint pos = {
      .x = center.x + (sin_lookup(angle) * text_r / TRIG_MAX_RATIO),
      .y = center.y - (cos_lookup(angle) * text_r / TRIG_MAX_RATIO) - 4
    };

    static char buf[4];
    snprintf(buf, sizeof(buf), "%d", i * 5);

    GRect text_rect = GRect(pos.x - 20, pos.y - 12, 40, 24);

    graphics_draw_text(
      ctx,
      buf,
      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
      text_rect,
      GTextOverflowModeWordWrap,
      GTextAlignmentCenter,
      NULL
    );
  }
}

// ===============================
// ROTATION CONTROL
// ===============================
static void update_dial_for_time(struct tm *t) {
  float hour_angle =
    (t->tm_hour % 12) * 30.0f +
    (t->tm_min / 60.0f) * 30.0f;

  s_dial_angle = hour_angle;

  layer_mark_dirty(s_dial_layer);
  layer_mark_dirty(s_rotating_ticks_layer);
}

// ===============================
// TICK HANDLER
// ===============================
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_dial_for_time(tick_time);

  // Always update date text + color
  update_date();

  // Redraw date layer
  layer_mark_dirty(text_layer_get_layer(s_date_layer));
}

// ===============================
// WIGGLE ANIMATION
// ===============================
static void wiggle_update(Animation *anim, const AnimationProgress progress) {
  float t = (float)progress / ANIMATION_NORMALIZED_MAX;

  float amplitude = 10.0f;
  float frequency = 2.0f;

  float decay = 1.0f - (t * t);

  float wiggle =
      amplitude *
      (sin_lookup(t * TRIG_MAX_ANGLE * frequency) / (float)TRIG_MAX_RATIO) *
      decay;

  s_wiggle_offset = wiggle;

  layer_mark_dirty(s_dial_layer);
  layer_mark_dirty(s_rotating_ticks_layer);
}

static void wiggle_stopped(Animation *anim, bool finished, void *context) {
  s_wiggle_offset = 0;
  s_is_wiggling = false;

  if(s_wiggle_anim) {
    animation_destroy(s_wiggle_anim);
    s_wiggle_anim = NULL;
  }

  layer_mark_dirty(s_dial_layer);
  layer_mark_dirty(s_rotating_ticks_layer);
}

static const AnimationImplementation s_wiggle_impl = {
  .update = wiggle_update
};

static const AnimationHandlers s_wiggle_handlers = {
  .stopped = wiggle_stopped
};

static void trigger_wiggle(void) {
  if(s_is_wiggling) return;
  s_is_wiggling = true;

  if(s_wiggle_anim) {
    animation_destroy(s_wiggle_anim);
    s_wiggle_anim = NULL;
  }

  s_wiggle_anim = animation_create();
  animation_set_duration(s_wiggle_anim, 1000);
  animation_set_curve(s_wiggle_anim, AnimationCurveEaseOut);
  animation_set_implementation(s_wiggle_anim, &s_wiggle_impl);
  animation_set_handlers(s_wiggle_anim, s_wiggle_handlers, NULL);

  animation_schedule(s_wiggle_anim);
}

// ===============================
// TAP HANDLER
// ===============================
static void tap_handler(AccelAxisType axis, int32_t direction) {
  if(!s_main_window) return;
  trigger_wiggle();
}

// ===============================
// INBOX HANDLER
// ===============================
static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *dark_mode_t = dict_find(iter, MESSAGE_KEY_DarkMode);
  if (dark_mode_t) {
      s_dark_mode = dark_mode_t->value->uint8;

      // Update date text color
      text_layer_set_text_color(s_date_layer,
          s_dark_mode ? GColorWhite : GColorBlack);

      // Redraw date layer
      layer_mark_dirty(text_layer_get_layer(s_date_layer));

      // Update background
      window_set_background_color(s_main_window,
          s_dark_mode ? GColorBlack : GColorWhite);
  
      // Redraw other layers
      layer_mark_dirty(s_dial_layer);
     layer_mark_dirty(s_rotating_ticks_layer);
  }
}

// ===============================
// WINDOW LOAD
// ===============================
static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  window_set_background_color(s_main_window,
    s_dark_mode ? GColorBlack : GColorWhite);

  s_rotating_ticks_layer = layer_create(bounds);
  layer_set_update_proc(s_rotating_ticks_layer, rotating_ticks_update_proc);
  layer_add_child(window_layer, s_rotating_ticks_layer);

  s_dial_layer = layer_create(bounds);
  layer_set_update_proc(s_dial_layer, dial_update_proc);
  layer_add_child(window_layer, s_dial_layer);

  bool is_round = PBL_IF_ROUND_ELSE(true, false);

  GRect date_frame = is_round
      ? GRect(0, bounds.size.h/2 - 10, bounds.size.w, 24)
      : GRect(0, bounds.size.h - 26, bounds.size.w, 24);

  s_date_layer = text_layer_create(date_frame);
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_color(s_date_layer,
      s_dark_mode ? GColorWhite : GColorBlack);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));

  update_date();
  
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  update_dial_for_time(t);
  
  layer_mark_dirty(s_dial_layer);
  layer_mark_dirty(s_rotating_ticks_layer);
  layer_mark_dirty(text_layer_get_layer(s_date_layer));

  accel_tap_service_subscribe(tap_handler);
  trigger_wiggle();
}

// ===============================
static void main_window_unload(Window *window) {
  accel_tap_service_unsubscribe();

  if(s_wiggle_anim) {
    animation_destroy(s_wiggle_anim);
    s_wiggle_anim = NULL;
  }

  text_layer_destroy(s_date_layer);
  layer_destroy(s_rotating_ticks_layer);
  layer_destroy(s_dial_layer);
}

// ===============================
static void init(void) {

  // Load dark mode BEFORE creating the window
  if(persist_exists(1)) {
    s_dark_mode = persist_read_bool(1);
  } else {
    s_dark_mode = false;   // default: start with dark mode OFF
  }

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers){
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Apply background immediately
  window_set_background_color(s_main_window,
      s_dark_mode ? GColorBlack : GColorWhite);

  window_stack_push(s_main_window, true);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);

  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(128, 128);
}

static void deinit(void) {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
