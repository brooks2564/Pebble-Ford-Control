#include <pebble.h>

// ── Message keys (must match package.json messageKeys order) ─────────────────
#define KEY_ACTION         0
#define KEY_RESPONSE       1
#define KEY_IS_LOCKED      2
#define KEY_IS_RUNNING     3
#define KEY_FUEL_LEVEL     4
#define KEY_OIL_LIFE       5
#define KEY_TIRE_FL        6
#define KEY_TIRE_FR        7
#define KEY_TIRE_RL        8
#define KEY_TIRE_RR        9
#define KEY_MODEL_NAME     10
#define KEY_ERROR_MSG      11
#define KEY_CLIMATE_TEMP   12
#define KEY_CLIMATE_SEAT   13
#define KEY_CLIMATE_WHEEL  14
#define KEY_CLIMATE_DEFROST 15
#define KEY_APPLY_CLIMATE  16
#define KEY_DOOR_STATUS    17

// ── Actions ──────────────────────────────────────────────────────────────────
#define ACTION_LOCK        0
#define ACTION_UNLOCK      1
#define ACTION_START       2
#define ACTION_STOP        3
#define ACTION_FIND        4
#define ACTION_GET_STATUS  5
#define ACTION_GET_INFO    6
#define ACTION_CLIMATE     7

// ── Colors ────────────────────────────────────────────────────────────────────
#define FORD_BLUE      GColorOxfordBlue
#define FORD_BLUE_DARK GColorMidnightGreen
#define BTN_ACTIVE     GColorWhite
#define BTN_DIMMED     GColorLightGray
#define TXT_DARK       GColorBlack
#define TXT_LIGHT      GColorWhite
#define BTN_GREEN      GColorIslamicGreen
#define BTN_RED        GColorRed
#define BTN_AMBER      GColorChromeYellow
#define BTN_GRAY       GColorDarkGray

// ── Hold constants ────────────────────────────────────────────────────────────
#define HOLD_MS        1500
#define PROGRESS_TICK  50
#define PROGRESS_STEPS (HOLD_MS / PROGRESS_TICK)  // 30

// ── Held button IDs ───────────────────────────────────────────────────────────
#define BTN_NONE         -1
#define BTN_LOCK_UNLOCK   0
#define BTN_START_STOP    1
#define BTN_CLIMATE       2
#define BTN_FIND          3

// ── App state ────────────────────────────────────────────────────────────────
static bool  s_is_locked   = true;
static bool  s_is_running  = false;
static bool  s_is_loading  = false;
static char  s_model[32]   = "Maverick";
static char  s_error[64]   = "";

// Climate state
static int   s_climate_temp   = 72;
static bool  s_climate_seat   = false;
static bool  s_climate_wheel  = false;
static bool  s_climate_defrost = false;

// Info state
static int   s_fuel    = -1;
static int   s_oil     = -1;
static int   s_tire_fl = -1;
static int   s_tire_fr = -1;
static int   s_tire_rl = -1;
static int   s_tire_rr = -1;
static uint8_t s_door_status = 0xFF; // 0xFF = unknown; bits 0-3 = FL,FR,RL,RR open

// Hold state
static int         s_held_btn      = BTN_NONE;
static int         s_hold_progress = 0;  // 0..PROGRESS_STEPS
static AppTimer   *s_progress_timer = NULL;

// ── Windows & layers ─────────────────────────────────────────────────────────
static Window *s_main_window;
static Layer  *s_main_layer;

static Window *s_climate_window;
static Layer  *s_climate_layer;

static Window *s_info_window;
static Layer  *s_info_layer;

// ── Forward declarations ──────────────────────────────────────────────────────
static void send_action(int action);
static void send_climate();
static void send_action(int action);
static void main_layer_update(Layer *layer, GContext *ctx);
static void climate_layer_update(Layer *layer, GContext *ctx);
static void info_layer_update(Layer *layer, GContext *ctx);
static void hold_cancel();

static void open_info_window(void) {
    send_action(ACTION_GET_INFO);
    window_stack_push(s_info_window, true);
}

// ── Layout helpers ────────────────────────────────────────────────────────────
static GRect btn_lock_rect(GRect bounds) {
    return GRect(0, bounds.size.h - 68, bounds.size.w / 2, 68);
}
static GRect btn_start_rect(GRect bounds) {
    return GRect(bounds.size.w / 2, bounds.size.h - 68, bounds.size.w / 2, 68);
}
static GRect btn_climate_rect(GRect bounds) {
    int mid_y = bounds.size.h - 68 - 48 - 8;
    return GRect(6, mid_y, bounds.size.w / 2 - 9, 48);
}
static GRect btn_find_rect(GRect bounds) {
    int mid_y = bounds.size.h - 68 - 48 - 8;
    return GRect(bounds.size.w / 2 + 3, mid_y, bounds.size.w / 2 - 9, 48);
}
static GRect status_rect(GRect bounds) {
    int top_btn_y = bounds.size.h - 68 - 48 - 8;
    return GRect(0, 24, bounds.size.w, top_btn_y - 24);
}

// ── AppMessage ────────────────────────────────────────────────────────────────
static void inbox_received(DictionaryIterator *it, void *ctx) {
    Tuple *t;

    t = dict_find(it, KEY_IS_LOCKED);
    if (t) s_is_locked = (bool)t->value->uint8;

    t = dict_find(it, KEY_IS_RUNNING);
    if (t) s_is_running = (bool)t->value->uint8;

    t = dict_find(it, KEY_MODEL_NAME);
    if (t) snprintf(s_model, sizeof(s_model), "%s", t->value->cstring);

    t = dict_find(it, KEY_FUEL_LEVEL);
    if (t) s_fuel = (int)t->value->int32;

    t = dict_find(it, KEY_OIL_LIFE);
    if (t) s_oil = (int)t->value->int32;

    t = dict_find(it, KEY_TIRE_FL);
    if (t) s_tire_fl = (int)t->value->int32;

    t = dict_find(it, KEY_TIRE_FR);
    if (t) s_tire_fr = (int)t->value->int32;

    t = dict_find(it, KEY_TIRE_RL);
    if (t) s_tire_rl = (int)t->value->int32;

    t = dict_find(it, KEY_TIRE_RR);
    if (t) s_tire_rr = (int)t->value->int32;

    t = dict_find(it, KEY_DOOR_STATUS);
    if (t) s_door_status = (uint8_t)t->value->uint8;

    t = dict_find(it, KEY_CLIMATE_TEMP);
    if (t) s_climate_temp = (int)t->value->int32;

    t = dict_find(it, KEY_CLIMATE_SEAT);
    if (t) s_climate_seat = (bool)t->value->uint8;

    t = dict_find(it, KEY_CLIMATE_WHEEL);
    if (t) s_climate_wheel = (bool)t->value->uint8;

    t = dict_find(it, KEY_CLIMATE_DEFROST);
    if (t) s_climate_defrost = (bool)t->value->uint8;

    t = dict_find(it, KEY_ERROR_MSG);
    if (t) snprintf(s_error, sizeof(s_error), "%s", t->value->cstring);
    else   s_error[0] = '\0';

    s_is_loading = false;

    if (s_main_layer)    layer_mark_dirty(s_main_layer);
    if (s_climate_layer) layer_mark_dirty(s_climate_layer);
    if (s_info_layer)    layer_mark_dirty(s_info_layer);
}

static void inbox_dropped(AppMessageResult reason, void *ctx) {
    s_is_loading = false;
    snprintf(s_error, sizeof(s_error), "Msg error %d", (int)reason);
    if (s_main_layer) layer_mark_dirty(s_main_layer);
}

static void send_action(int action) {
    DictionaryIterator *out;
    if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
    dict_write_uint8(out, KEY_ACTION, (uint8_t)action);
    if (app_message_outbox_send() == APP_MSG_OK) {
        s_is_loading = true;
        s_error[0] = '\0';
        if (s_main_layer) layer_mark_dirty(s_main_layer);
    }
}

static void send_climate() {
    DictionaryIterator *out;
    if (app_message_outbox_begin(&out) != APP_MSG_OK) return;
    dict_write_uint8(out, KEY_ACTION,          ACTION_CLIMATE);
    dict_write_int32(out, KEY_CLIMATE_TEMP,    s_climate_temp);
    dict_write_uint8(out, KEY_CLIMATE_SEAT,    (uint8_t)s_climate_seat);
    dict_write_uint8(out, KEY_CLIMATE_WHEEL,   (uint8_t)s_climate_wheel);
    dict_write_uint8(out, KEY_CLIMATE_DEFROST, (uint8_t)s_climate_defrost);
    dict_write_uint8(out, KEY_APPLY_CLIMATE,   1);
    if (app_message_outbox_send() == APP_MSG_OK) {
        s_is_loading = true;
        s_error[0] = '\0';
    }
}

// ── Hold / progress ───────────────────────────────────────────────────────────
static void fire_held_action(int btn) {
    switch (btn) {
        case BTN_LOCK_UNLOCK:
            send_action(s_is_locked ? ACTION_UNLOCK : ACTION_LOCK);
            break;
        case BTN_START_STOP:
            send_action(s_is_running ? ACTION_STOP : ACTION_START);
            break;
        case BTN_FIND:
            send_action(ACTION_FIND);
            break;
        default: break;
    }
}

static void progress_tick(void *data) {
    s_hold_progress++;
    if (s_main_layer) layer_mark_dirty(s_main_layer);

    if (s_hold_progress >= PROGRESS_STEPS) {
        s_progress_timer = NULL;
        int fired_btn = s_held_btn;
        hold_cancel();
        if (fired_btn == BTN_CLIMATE) {
            window_stack_push(s_climate_window, true);
        } else {
            fire_held_action(fired_btn);
        }
        return;
    }
    s_progress_timer = app_timer_register(PROGRESS_TICK, progress_tick, NULL);
}

static void hold_start(int btn_id) {
    hold_cancel();
    s_held_btn      = btn_id;
    s_hold_progress = 0;
    s_progress_timer = app_timer_register(PROGRESS_TICK, progress_tick, NULL);
    if (s_main_layer) layer_mark_dirty(s_main_layer);
}

static void hold_cancel() {
    if (s_progress_timer) {
        app_timer_cancel(s_progress_timer);
        s_progress_timer = NULL;
    }
    s_held_btn      = BTN_NONE;
    s_hold_progress = 0;
    if (s_main_layer) layer_mark_dirty(s_main_layer);
}

// ── Touch handling ────────────────────────────────────────────────────────────
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
static bool rect_contains(GRect r, int x, int y) {
    return x >= r.origin.x && x < r.origin.x + r.size.w &&
           y >= r.origin.y && y < r.origin.y + r.size.h;
}

static int s_swipe_start_y = -1;

static void touch_handler(const TouchEvent *event, void *ctx) {
    GRect bounds = layer_get_bounds(window_get_root_layer(s_main_window));
    int x = event->x, y = event->y;

    if (event->type == TouchEvent_Touchdown) {
        s_swipe_start_y = y;
        if (rect_contains(btn_lock_rect(bounds), x, y))
            hold_start(BTN_LOCK_UNLOCK);
        else if (rect_contains(btn_start_rect(bounds), x, y))
            hold_start(BTN_START_STOP);
        else if (rect_contains(btn_climate_rect(bounds), x, y))
            window_stack_push(s_climate_window, true);
        else if (rect_contains(btn_find_rect(bounds), x, y))
            open_info_window();
    } else if (event->type == TouchEvent_Liftoff) {
        int delta = y - s_swipe_start_y;
        if (delta < -40) {
            hold_cancel();
            open_info_window();
        } else if (delta > 40) {
            hold_cancel();
            window_stack_push(s_climate_window, true);
        } else {
            hold_cancel();
        }
        s_swipe_start_y = -1;
    }
}

static void climate_touch_handler(const TouchEvent *event, void *ctx) {
    if (event->type != TouchEvent_Touchdown) return;
    GRect bounds = layer_get_bounds(window_get_root_layer(s_climate_window));
    int x = event->x, y = event->y;
    int w = bounds.size.w;

    // Temperature +/- buttons
    GRect minus_rect = GRect(w/2 - 70, 34, 44, 30);
    GRect plus_rect  = GRect(w/2 + 26, 34, 44, 30);
    // Toggles
    GRect seat_rect    = GRect(0,  78, w, 36);
    GRect wheel_rect   = GRect(0, 118, w, 36);
    GRect defrost_rect = GRect(0, 158, w, 36);
    // Apply
    GRect apply_rect = GRect(w/2 - 55, 198, 110, 26);

    if (rect_contains(minus_rect, x, y) && s_climate_temp > 60)
        s_climate_temp--;
    else if (rect_contains(plus_rect, x, y) && s_climate_temp < 85)
        s_climate_temp++;
    else if (rect_contains(seat_rect, x, y))
        s_climate_seat = !s_climate_seat;
    else if (rect_contains(wheel_rect, x, y))
        s_climate_wheel = !s_climate_wheel;
    else if (rect_contains(defrost_rect, x, y))
        s_climate_defrost = !s_climate_defrost;
    else if (rect_contains(apply_rect, x, y)) {
        send_climate();
        window_stack_pop(true);
        return;
    }

    if (s_climate_layer) layer_mark_dirty(s_climate_layer);
}
#endif

// ── Button (physical) handlers ────────────────────────────────────────────────
static AppTimer *s_backlight_timer = NULL;

static void backlight_off_callback(void *ctx) {
    s_backlight_timer = NULL;
    light_enable(false);
}

static void up_click(ClickRecognizerRef r, void *ctx) {
    open_info_window();
}
static void down_click(ClickRecognizerRef r, void *ctx) {
    window_stack_push(s_climate_window, true);
}
static void select_click(ClickRecognizerRef r, void *ctx) {
    if (s_backlight_timer) app_timer_cancel(s_backlight_timer);
    light_enable(true);
    s_backlight_timer = app_timer_register(5000, backlight_off_callback, NULL);
}

static void click_config(void *ctx) {
    window_single_click_subscribe(BUTTON_ID_UP,     up_click);
    window_single_click_subscribe(BUTTON_ID_DOWN,   down_click);
    window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
}

// Climate window buttons
static void climate_up_click(ClickRecognizerRef r, void *ctx) {
    open_info_window();
}
static void climate_down_click(ClickRecognizerRef r, void *ctx) {
    window_stack_pop(true);
}
static void climate_select_click(ClickRecognizerRef r, void *ctx) {
    if (s_backlight_timer) app_timer_cancel(s_backlight_timer);
    light_enable(true);
    s_backlight_timer = app_timer_register(5000, backlight_off_callback, NULL);
}
static void climate_click_config(void *ctx) {
    window_single_click_subscribe(BUTTON_ID_UP,     climate_up_click);
    window_single_click_subscribe(BUTTON_ID_DOWN,   climate_down_click);
    window_single_click_subscribe(BUTTON_ID_SELECT, climate_select_click);
}

// ── Draw helpers ──────────────────────────────────────────────────────────────
static void draw_rounded_btn(GContext *ctx, GRect r, GColor bg, GColor fg,
                              const char *label, GFont font) {
    graphics_context_set_fill_color(ctx, bg);
    graphics_fill_rect(ctx, r, 6, GCornersAll);
    graphics_context_set_text_color(ctx, fg);
    graphics_draw_text(ctx, label, font,
                       GRect(r.origin.x, r.origin.y + r.size.h/2 - 20,
                             r.size.w, 32),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
}

static void draw_sweep_btn(GContext *ctx, GRect r, GColor color_from, GColor color_to,
                           GColor fg, const char *label, GFont font, int progress) {
    // Base button in current color
    graphics_context_set_fill_color(ctx, color_from);
    graphics_fill_rect(ctx, r, 6, GCornersAll);
    // Sweep target color from left
    int sweep_w = (r.size.w * progress) / PROGRESS_STEPS;
    if (sweep_w > 0) {
        graphics_context_set_fill_color(ctx, color_to);
        graphics_fill_rect(ctx, GRect(r.origin.x, r.origin.y, sweep_w, r.size.h), 0, GCornerNone);
    }
    // Label on top
    graphics_context_set_text_color(ctx, fg);
    graphics_draw_text(ctx, label, font,
                       GRect(r.origin.x, r.origin.y + r.size.h / 2 - 20, r.size.w, 32),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

// ── Main layer ────────────────────────────────────────────────────────────────
static void main_layer_update(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    GFont font_lg  = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    GFont font_btn = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);

    // Background
    graphics_context_set_fill_color(ctx, FORD_BLUE);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    // Model name strip
    graphics_context_set_text_color(ctx, TXT_LIGHT);
    graphics_draw_text(ctx, s_model, font_lg,
                       GRect(0, 2, bounds.size.w, 22),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);

    // Status text — centered between model name and top buttons, no background
    GRect st = status_rect(bounds);
    char status_str[48];
    const char *lock_str  = s_is_locked  ? "Locked"  : "Unlocked";
    const char *eng_str   = s_is_running ? "Running" : "Off";
    snprintf(status_str, sizeof(status_str), "%s · %s", lock_str, eng_str);

    if (s_is_loading) snprintf(status_str, sizeof(status_str), "Loading...");
    if (s_error[0])   snprintf(status_str, sizeof(status_str), "Error");

    GFont font_xl = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
    int status_text_h = 36;
    int status_y = st.origin.y + (st.size.h - status_text_h) / 2;
    graphics_context_set_text_color(ctx, TXT_LIGHT);
    graphics_draw_text(ctx, status_str, font_xl,
                       GRect(0, status_y, bounds.size.w, status_text_h),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);

    // ── Middle buttons ─────────────────────────────────────────────────────
    GRect climate_r = btn_climate_rect(bounds);
    GRect find_r    = btn_find_rect(bounds);
    bool climate_active = s_is_running;

    draw_rounded_btn(ctx, climate_r,
                     climate_active ? GColorDukeBlue : GColorDarkGray,
                     TXT_LIGHT, "Climate", font_btn);
    draw_rounded_btn(ctx, find_r, GColorDarkGray, TXT_LIGHT, "Find", font_btn);

    // ── Bottom buttons ─────────────────────────────────────────────────────
    GRect lock_r  = btn_lock_rect(bounds);
    GRect start_r = btn_start_rect(bounds);

    // Lock/Unlock — sweep from current color to target color on hold
    GColor lock_from = s_is_locked ? GColorIslamicGreen : GColorRed;
    GColor lock_to   = s_is_locked ? GColorRed : GColorIslamicGreen;
    int lock_prog    = (s_held_btn == BTN_LOCK_UNLOCK) ? s_hold_progress : 0;
    GRect lock_inner = GRect(lock_r.origin.x + 3, lock_r.origin.y + 4,
                             lock_r.size.w - 6, lock_r.size.h - 8);
    draw_sweep_btn(ctx, lock_inner, lock_from, lock_to, TXT_LIGHT,
                   s_is_locked ? "Unlock" : "Lock", font_btn, lock_prog);

    // Start/Stop — sweep from current color to target color on hold
    GColor start_from = s_is_running ? BTN_RED   : BTN_GREEN;
    GColor start_to   = s_is_running ? BTN_GREEN : BTN_RED;
    int start_prog    = (s_held_btn == BTN_START_STOP) ? s_hold_progress : 0;
    GRect start_inner = GRect(start_r.origin.x + 3, start_r.origin.y + 4,
                              start_r.size.w - 6, start_r.size.h - 8);
    draw_sweep_btn(ctx, start_inner, start_from, start_to, TXT_LIGHT,
                   s_is_running ? "Stop" : "Start", font_btn, start_prog);

    // Arc for climate and find buttons
    if ((s_held_btn == BTN_CLIMATE || s_held_btn == BTN_FIND) && s_hold_progress > 0) {
        GRect held_r = (s_held_btn == BTN_CLIMATE) ? climate_r : find_r;
        graphics_context_set_stroke_color(ctx, GColorWhite);
        graphics_context_set_stroke_width(ctx, 4);
        int32_t angle = (TRIG_MAX_ANGLE * s_hold_progress) / PROGRESS_STEPS;
        GPoint center = GPoint(held_r.origin.x + held_r.size.w / 2,
                               held_r.origin.y + held_r.size.h / 2);
        int radius = (held_r.size.w < held_r.size.h ? held_r.size.w : held_r.size.h) / 2 + 4;
        graphics_draw_arc(ctx,
                          GRect(center.x - radius, center.y - radius, radius * 2, radius * 2),
                          GOvalScaleModeFitCircle,
                          DEG_TO_TRIGANGLE(0) - TRIG_MAX_ANGLE / 4,
                          DEG_TO_TRIGANGLE(0) - TRIG_MAX_ANGLE / 4 + angle);
    }
}

// ── Climate layer ─────────────────────────────────────────────────────────────
static void climate_layer_update(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    GFont font_sm  = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
    GFont font_med = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    GFont font_lg  = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

    int w = bounds.size.w;

    // Background
    graphics_context_set_fill_color(ctx, FORD_BLUE);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    // Title
    graphics_context_set_text_color(ctx, TXT_LIGHT);
    graphics_draw_text(ctx, "Climate", font_lg,
                       GRect(0, 2, w, 26),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);

    // Temperature row
    char temp_str[8];
    snprintf(temp_str, sizeof(temp_str), "%d\xc2\xb0" "F", s_climate_temp);

    // Minus button (blue)
    draw_rounded_btn(ctx, GRect(w/2 - 70, 34, 44, 30),
                     GColorVividCerulean, TXT_LIGHT, "-", font_lg);
    // Temp display
    graphics_context_set_text_color(ctx, TXT_LIGHT);
    graphics_draw_text(ctx, temp_str, font_lg,
                       GRect(w/2 - 22, 34 + 30/2 - 14, 44, 28),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    // Plus button (red)
    draw_rounded_btn(ctx, GRect(w/2 + 26, 34, 44, 30),
                     GColorRed, TXT_LIGHT, "+", font_lg);

    // Separator
    graphics_context_set_stroke_color(ctx, GColorDarkGray);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, GPoint(10, 72), GPoint(w - 10, 72));

    // Toggle rows
    struct { const char *label; bool *val; int y; } toggles[] = {
        { "Seat",    &s_climate_seat,    78  },
        { "Wheel",   &s_climate_wheel,   118 },
        { "Defrost", &s_climate_defrost, 158 },
    };

    for (int i = 0; i < 3; i++) {
        bool on = *toggles[i].val;
        int row_h = 32;
        // Label
        graphics_context_set_text_color(ctx, TXT_LIGHT);
        graphics_draw_text(ctx, toggles[i].label, font_lg,
                           GRect(12, toggles[i].y + row_h/2 - 14, w - 80, 28),
                           GTextOverflowModeTrailingEllipsis,
                           GTextAlignmentLeft, NULL);
        // Toggle pill
        GRect pill = GRect(w - 60, toggles[i].y + 2, 52, 28);
        graphics_context_set_fill_color(ctx, on ? BTN_GREEN : GColorDarkGray);
        graphics_fill_rect(ctx, pill, 8, GCornersAll);
        graphics_context_set_text_color(ctx, TXT_LIGHT);
        graphics_draw_text(ctx, on ? "ON" : "OFF", font_med,
                           GRect(pill.origin.x, pill.origin.y + pill.size.h/2 - 11, pill.size.w, 22),
                           GTextOverflowModeTrailingEllipsis,
                           GTextAlignmentCenter, NULL);
    }

    // Apply button
    draw_rounded_btn(ctx,
                     GRect(w/2 - 55, 198, 110, 26),
                     BTN_GREEN, TXT_LIGHT, "Apply", font_lg);
}

// ── Info layer ────────────────────────────────────────────────────────────────
static void draw_bar(GContext *ctx, int x, int y, int w, int h,
                     int pct, GColor fill) {
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_rect(ctx, GRect(x, y, w, h), 2, GCornersAll);
    if (pct > 0) {
        int fill_w = (w * pct) / 100;
        graphics_context_set_fill_color(ctx, fill);
        graphics_fill_rect(ctx, GRect(x, y, fill_w, h), 2, GCornersAll);
    }
}

static void draw_maverick_overhead(GContext *ctx, int cx, int cy) {
    GColor body  = GColorDarkGray;
    GColor glass = GColorCadetBlue;
    GColor dark  = GColorBlack;

    // Wheels drawn first — body will overlay the inner wheel arch portion
    GPoint wheels[4] = {
        GPoint(cx-24, cy-27),  // FR (top-left on screen)
        GPoint(cx+24, cy-27),  // FL (top-right on screen)
        GPoint(cx-24, cy+20),  // RR (bottom-left on screen)
        GPoint(cx+24, cy+20),  // RL (bottom-right on screen)
    };
    for (int i = 0; i < 4; i++) {
        graphics_context_set_fill_color(ctx, GColorBlack);
        graphics_fill_circle(ctx, wheels[i], 6);
        graphics_context_set_fill_color(ctx, GColorLightGray);
        graphics_fill_circle(ctx, wheels[i], 2);
    }

    // Front bumper (narrow)
    graphics_context_set_fill_color(ctx, body);
    graphics_fill_rect(ctx, GRect(cx-12, cy-40, 24, 4), 2, GCornersAll);

    // Hood (widens toward cab)
    graphics_fill_rect(ctx, GRect(cx-17, cy-36, 34, 14), 2, GCornersTop);

    // Windshield (glass)
    graphics_context_set_fill_color(ctx, glass);
    graphics_fill_rect(ctx, GRect(cx-19, cy-22, 38, 9), 0, GCornerNone);

    // Cab body sides
    graphics_context_set_fill_color(ctx, body);
    graphics_fill_rect(ctx, GRect(cx-20, cy-22, 40, 28), 0, GCornerNone);

    // Front seat row (dark interior)
    graphics_context_set_fill_color(ctx, dark);
    graphics_fill_rect(ctx, GRect(cx-15, cy-20, 30, 8), 0, GCornerNone);

    // B-pillar (body color bar between seat rows)
    graphics_context_set_fill_color(ctx, body);
    graphics_fill_rect(ctx, GRect(cx-20, cy-12, 40, 3), 0, GCornerNone);

    // Rear seat row (dark interior)
    graphics_context_set_fill_color(ctx, dark);
    graphics_fill_rect(ctx, GRect(cx-15, cy-9, 30, 7), 0, GCornerNone);

    // Rear window (glass)
    graphics_context_set_fill_color(ctx, glass);
    graphics_fill_rect(ctx, GRect(cx-19, cy-2, 38, 6), 0, GCornerNone);

    // Bed side walls
    graphics_context_set_fill_color(ctx, body);
    graphics_fill_rect(ctx, GRect(cx-20, cy+4, 6, 26), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(cx+14, cy+4, 6, 26), 0, GCornerNone);

    // Tailgate
    graphics_fill_rect(ctx, GRect(cx-20, cy+30, 40, 5), 0, GCornerNone);

    // Rear bumper (narrow)
    graphics_fill_rect(ctx, GRect(cx-12, cy+35, 24, 4), 2, GCornersAll);
}

static void draw_corner_info(GContext *ctx, int x, int y, int bw,
                              const char *label, int tire_psi,
                              bool door_open, bool unknown,
                              GTextAlignment txt_align,
                              GFont font_label, GFont font_data) {
    char buf[16];
    int pill_w = 50;
    int pill_x = (txt_align == GTextAlignmentRight) ? x + bw - pill_w : x;

    graphics_context_set_text_color(ctx, TXT_LIGHT);
    graphics_draw_text(ctx, label, font_label,
                       GRect(x, y + 2, bw, 20),
                       GTextOverflowModeTrailingEllipsis, txt_align, NULL);

    if (tire_psi >= 0) snprintf(buf, sizeof(buf), "%d psi", tire_psi);
    else snprintf(buf, sizeof(buf), "-- psi");
    graphics_draw_text(ctx, buf, font_data,
                       GRect(x, y + 24, bw, 16),
                       GTextOverflowModeTrailingEllipsis, txt_align, NULL);

    GColor dc = unknown ? GColorDarkGray : (door_open ? GColorRed : BTN_GREEN);
    const char *ds = unknown ? "?" : (door_open ? "OPEN" : "OK");
    GRect pill = GRect(pill_x, y + 43, pill_w, 18);
    graphics_context_set_fill_color(ctx, dc);
    graphics_fill_rect(ctx, pill, 4, GCornersAll);
    graphics_context_set_text_color(ctx, TXT_LIGHT);
    graphics_draw_text(ctx, ds, font_data,
                       GRect(pill.origin.x, pill.origin.y + 1, pill.size.w, 14),
                       GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
}

static void info_layer_update(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    GFont font_sm  = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
    GFont font_med = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    int w = bounds.size.w;
    int h = bounds.size.h;

    graphics_context_set_fill_color(ctx, FORD_BLUE);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    if (s_is_loading) {
        graphics_context_set_text_color(ctx, TXT_LIGHT);
        graphics_draw_text(ctx, "Loading...", font_med,
                           GRect(0, h/2 - 12, w, 24),
                           GTextOverflowModeTrailingEllipsis,
                           GTextAlignmentCenter, NULL);
        return;
    }

    bool unknown = (s_door_status == 0xFF);
    bool door_fl = !unknown && (s_door_status & (1 << 0));
    bool door_fr = !unknown && (s_door_status & (1 << 1));
    bool door_rl = !unknown && (s_door_status & (1 << 2));
    bool door_rr = !unknown && (s_door_status & (1 << 3));

    int cx = w / 2;    // 100
    int cy = h / 2;    // 114
    int bw = 62;       // corner box width
    int bh = 64;       // corner box height

    // Corner info: top-left=FR, top-right=FL, bottom-left=RR, bottom-right=RL
    draw_corner_info(ctx, 2, 4, bw, "FR",
                     s_tire_fr, door_fr, unknown,
                     GTextAlignmentLeft, font_med, font_sm);
    draw_corner_info(ctx, w - bw - 2, 4, bw, "FL",
                     s_tire_fl, door_fl, unknown,
                     GTextAlignmentRight, font_med, font_sm);
    draw_corner_info(ctx, 2, h - bh - 4, bw, "RR",
                     s_tire_rr, door_rr, unknown,
                     GTextAlignmentLeft, font_med, font_sm);
    draw_corner_info(ctx, w - bw - 2, h - bh - 4, bw, "RL",
                     s_tire_rl, door_rl, unknown,
                     GTextAlignmentRight, font_med, font_sm);

    // Maverick overhead silhouette centered on screen
    draw_maverick_overhead(ctx, cx, cy);
}

// ── Window lifecycle ──────────────────────────────────────────────────────────
static void main_window_load(Window *w) {
    Layer *root = window_get_root_layer(w);
    GRect bounds = layer_get_bounds(root);
    s_main_layer = layer_create(bounds);
    layer_set_update_proc(s_main_layer, main_layer_update);
    layer_add_child(root, s_main_layer);

    window_set_click_config_provider(w, click_config);

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    touch_service_subscribe(touch_handler, NULL);
#endif
}

static void main_window_unload(Window *w) {
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    touch_service_unsubscribe();
#endif
    hold_cancel();
    layer_destroy(s_main_layer);
    s_main_layer = NULL;
}

static void climate_window_load(Window *w) {
    Layer *root = window_get_root_layer(w);
    GRect bounds = layer_get_bounds(root);
    s_climate_layer = layer_create(bounds);
    layer_set_update_proc(s_climate_layer, climate_layer_update);
    layer_add_child(root, s_climate_layer);

    window_set_click_config_provider(w, climate_click_config);

#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    touch_service_subscribe(climate_touch_handler, NULL);
#endif
}

static void climate_window_unload(Window *w) {
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
    touch_service_unsubscribe();
#endif
    layer_destroy(s_climate_layer);
    s_climate_layer = NULL;
}

static void info_window_load(Window *w) {
    Layer *root = window_get_root_layer(w);
    GRect bounds = layer_get_bounds(root);
    s_info_layer = layer_create(bounds);
    layer_set_update_proc(s_info_layer, info_layer_update);
    layer_add_child(root, s_info_layer);
}

static void info_window_unload(Window *w) {
    layer_destroy(s_info_layer);
    s_info_layer = NULL;
}

// ── Init / deinit ─────────────────────────────────────────────────────────────
static void init() {
    app_message_register_inbox_received(inbox_received);
    app_message_register_inbox_dropped(inbox_dropped);
    app_message_open(512, 256);

    // Main window
    s_main_window = window_create();
    window_set_background_color(s_main_window, FORD_BLUE);
    WindowHandlers mh = { .load = main_window_load, .unload = main_window_unload };
    window_set_window_handlers(s_main_window, mh);
    window_stack_push(s_main_window, true);

    // Climate window
    s_climate_window = window_create();
    window_set_background_color(s_climate_window, FORD_BLUE);
    WindowHandlers ch = { .load = climate_window_load, .unload = climate_window_unload };
    window_set_window_handlers(s_climate_window, ch);

    // Info window
    s_info_window = window_create();
    window_set_background_color(s_info_window, FORD_BLUE);
    WindowHandlers ih = { .load = info_window_load, .unload = info_window_unload };
    window_set_window_handlers(s_info_window, ih);

    // Request status on open
    send_action(ACTION_GET_STATUS);
}

static void deinit() {
    hold_cancel();
    window_destroy(s_main_window);
    window_destroy(s_climate_window);
    window_destroy(s_info_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
    return 0;
}
