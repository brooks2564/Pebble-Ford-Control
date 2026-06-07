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
static void main_layer_update(Layer *layer, GContext *ctx);
static void climate_layer_update(Layer *layer, GContext *ctx);
static void info_layer_update(Layer *layer, GContext *ctx);
static void hold_cancel();

// ── Layout helpers ────────────────────────────────────────────────────────────
static GRect btn_lock_rect(GRect bounds) {
    return GRect(0, bounds.size.h - 68, bounds.size.w / 2, 68);
}
static GRect btn_start_rect(GRect bounds) {
    return GRect(bounds.size.w / 2, bounds.size.h - 68, bounds.size.w / 2, 68);
}
static GRect btn_climate_rect(GRect bounds) {
    int mid_y = 40 + (bounds.size.h - 68 - 40) / 2 - 24;
    return GRect(6, mid_y, bounds.size.w / 2 - 9, 48);
}
static GRect btn_find_rect(GRect bounds) {
    int mid_y = 40 + (bounds.size.h - 68 - 40) / 2 - 24;
    return GRect(bounds.size.w / 2 + 3, mid_y, bounds.size.w / 2 - 9, 48);
}
static GRect status_rect(GRect bounds) {
    return GRect(0, 24, bounds.size.w, 22);
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

static void touch_handler(const TouchEvent *event, void *ctx) {
    GRect bounds = layer_get_bounds(window_get_root_layer(s_main_window));
    int x = event->x, y = event->y;

    if (event->type == TouchEvent_Touchdown) {
        if (rect_contains(btn_lock_rect(bounds), x, y))
            hold_start(BTN_LOCK_UNLOCK);
        else if (rect_contains(btn_start_rect(bounds), x, y))
            hold_start(BTN_START_STOP);
        else if (rect_contains(btn_climate_rect(bounds), x, y) && s_is_running)
            hold_start(BTN_CLIMATE);
        else if (rect_contains(btn_find_rect(bounds), x, y))
            hold_start(BTN_FIND);
        else if (rect_contains(status_rect(bounds), x, y)) {
            send_action(ACTION_GET_INFO);
            window_stack_push(s_info_window, true);
        }
    } else if (event->type == TouchEvent_Liftoff) {
        hold_cancel();
    }
}

static void climate_touch_handler(const TouchEvent *event, void *ctx) {
    if (event->type != TouchEvent_Touchdown) return;
    GRect bounds = layer_get_bounds(window_get_root_layer(s_climate_window));
    int x = event->x, y = event->y;
    int w = bounds.size.w;

    // Temperature +/- buttons
    GRect minus_rect = GRect(w/2 - 70, 60, 44, 36);
    GRect plus_rect  = GRect(w/2 + 26, 60, 44, 36);
    // Toggles
    GRect seat_rect    = GRect(0, 112, w, 32);
    GRect wheel_rect   = GRect(0, 148, w, 32);
    GRect defrost_rect = GRect(0, 184, w, 32);
    // Apply
    GRect apply_rect = GRect(w/2 - 55, bounds.size.h - 52, 110, 36);

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
static void up_click(ClickRecognizerRef r, void *ctx) {
    // Navigate focus between buttons — toggle lock/unlock
    send_action(s_is_locked ? ACTION_UNLOCK : ACTION_LOCK);
}
static void down_click(ClickRecognizerRef r, void *ctx) {
    send_action(s_is_running ? ACTION_STOP : ACTION_START);
}
static void select_click(ClickRecognizerRef r, void *ctx) {
    if (s_is_running) window_stack_push(s_climate_window, true);
}

static void click_config(void *ctx) {
    window_single_click_subscribe(BUTTON_ID_UP,     up_click);
    window_single_click_subscribe(BUTTON_ID_DOWN,   down_click);
    window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
}

// Climate window buttons
static void climate_up_click(ClickRecognizerRef r, void *ctx) {
    if (s_climate_temp < 85) s_climate_temp++;
    if (s_climate_layer) layer_mark_dirty(s_climate_layer);
}
static void climate_down_click(ClickRecognizerRef r, void *ctx) {
    if (s_climate_temp > 60) s_climate_temp--;
    if (s_climate_layer) layer_mark_dirty(s_climate_layer);
}
static void climate_select_click(ClickRecognizerRef r, void *ctx) {
    send_climate();
    window_stack_pop(true);
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
                       GRect(r.origin.x, r.origin.y + r.size.h/2 - 10,
                             r.size.w, 22),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
}

static void draw_progress_arc(GContext *ctx, GRect btn_rect, int progress_steps) {
    // Draw a thick white arc around the button showing hold progress
    GPoint center = GPoint(btn_rect.origin.x + btn_rect.size.w / 2,
                           btn_rect.origin.y + btn_rect.size.h / 2);
    int radius = (btn_rect.size.w < btn_rect.size.h ?
                  btn_rect.size.w : btn_rect.size.h) / 2 + 4;
    int32_t angle = (TRIG_MAX_ANGLE * progress_steps) / PROGRESS_STEPS;

    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_arc(ctx,
                      GRect(center.x - radius, center.y - radius,
                            radius * 2, radius * 2),
                      GOvalScaleModeFitCircle,
                      DEG_TO_TRIGANGLE(0) - TRIG_MAX_ANGLE / 4,
                      DEG_TO_TRIGANGLE(0) - TRIG_MAX_ANGLE / 4 + angle);
}

// ── Main layer ────────────────────────────────────────────────────────────────
static void main_layer_update(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    GFont font_sm  = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
    GFont font_med = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    GFont font_lg  = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);

    // Background
    graphics_context_set_fill_color(ctx, FORD_BLUE);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    // Model name strip
    graphics_context_set_text_color(ctx, TXT_LIGHT);
    graphics_draw_text(ctx, s_model, font_lg,
                       GRect(0, 2, bounds.size.w, 22),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);

    // Status line (dark bg, tap for info)
    GRect st = status_rect(bounds);
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_rect(ctx, st, 0, GCornerNone);

    char status_str[48];
    const char *lock_str  = s_is_locked  ? "Locked"  : "Unlocked";
    const char *eng_str   = s_is_running ? "Running" : "Off";
    snprintf(status_str, sizeof(status_str), "%s  ·  %s", lock_str, eng_str);

    if (s_is_loading) snprintf(status_str, sizeof(status_str), "Loading...");
    if (s_error[0])   snprintf(status_str, sizeof(status_str), "Error");

    graphics_context_set_text_color(ctx, TXT_LIGHT);
    graphics_draw_text(ctx, status_str, font_sm,
                       GRect(0, st.origin.y + 4, bounds.size.w, 16),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);

    // ── Middle buttons ─────────────────────────────────────────────────────
    GRect climate_r = btn_climate_rect(bounds);
    GRect find_r    = btn_find_rect(bounds);
    bool climate_active = s_is_running;

    draw_rounded_btn(ctx, climate_r,
                     climate_active ? GColorDukeBlue : GColorDarkGray,
                     TXT_LIGHT, "Climate", font_sm);
    draw_rounded_btn(ctx, find_r, GColorDarkGray, TXT_LIGHT, "Find", font_sm);

    // ── Bottom buttons ─────────────────────────────────────────────────────
    GRect lock_r  = btn_lock_rect(bounds);
    GRect start_r = btn_start_rect(bounds);

    // Lock/Unlock
    GColor lock_color = s_is_locked ? BTN_AMBER : GColorDarkGray;
    draw_rounded_btn(ctx, GRect(lock_r.origin.x + 3, lock_r.origin.y + 4,
                                lock_r.size.w - 6, lock_r.size.h - 8),
                     lock_color, TXT_DARK,
                     s_is_locked ? "Unlock" : "Lock", font_med);

    // Start/Stop
    GColor start_color = s_is_running ? BTN_RED : BTN_GREEN;
    draw_rounded_btn(ctx, GRect(start_r.origin.x + 3, start_r.origin.y + 4,
                                start_r.size.w - 6, start_r.size.h - 8),
                     start_color, TXT_LIGHT,
                     s_is_running ? "Stop" : "Start", font_med);

    // Progress arc on held button
    if (s_held_btn != BTN_NONE && s_hold_progress > 0) {
        GRect held_r;
        switch (s_held_btn) {
            case BTN_LOCK_UNLOCK: held_r = lock_r;    break;
            case BTN_START_STOP:  held_r = start_r;   break;
            case BTN_CLIMATE:     held_r = climate_r; break;
            case BTN_FIND:        held_r = find_r;    break;
            default: held_r = GRect(0,0,0,0); break;
        }
        draw_progress_arc(ctx, held_r, s_hold_progress);
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
                       GRect(0, 4, w, 26),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);

    // Temperature row
    char temp_str[8];
    snprintf(temp_str, sizeof(temp_str), "%d\xc2\xb0" "F", s_climate_temp);

    // Minus button
    draw_rounded_btn(ctx, GRect(w/2 - 70, 60, 44, 36),
                     GColorDarkGray, TXT_LIGHT, "-", font_lg);
    // Temp display
    graphics_context_set_text_color(ctx, TXT_LIGHT);
    graphics_draw_text(ctx, temp_str, font_lg,
                       GRect(w/2 - 22, 64, 44, 28),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
    // Plus button
    draw_rounded_btn(ctx, GRect(w/2 + 26, 60, 44, 36),
                     GColorDarkGray, TXT_LIGHT, "+", font_lg);

    // Separator
    graphics_context_set_stroke_color(ctx, GColorDarkGray);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, GPoint(10, 105), GPoint(w - 10, 105));

    // Toggle rows
    struct { const char *label; bool *val; int y; } toggles[] = {
        { "Seat Heat",      &s_climate_seat,    112 },
        { "Wheel Heat",     &s_climate_wheel,   148 },
        { "Front Defrost",  &s_climate_defrost, 184 },
    };

    for (int i = 0; i < 3; i++) {
        bool on = *toggles[i].val;
        graphics_context_set_text_color(ctx, TXT_LIGHT);
        graphics_draw_text(ctx, toggles[i].label, font_med,
                           GRect(12, toggles[i].y + 6, w - 80, 22),
                           GTextOverflowModeTrailingEllipsis,
                           GTextAlignmentLeft, NULL);
        // Toggle pill
        GRect pill = GRect(w - 56, toggles[i].y + 6, 48, 22);
        graphics_context_set_fill_color(ctx, on ? BTN_GREEN : GColorDarkGray);
        graphics_fill_rect(ctx, pill, 8, GCornersAll);
        graphics_context_set_text_color(ctx, TXT_LIGHT);
        graphics_draw_text(ctx, on ? "ON" : "OFF", font_sm,
                           GRect(pill.origin.x, pill.origin.y + 4, pill.size.w, 16),
                           GTextOverflowModeTrailingEllipsis,
                           GTextAlignmentCenter, NULL);
    }

    // Apply button
    draw_rounded_btn(ctx,
                     GRect(w/2 - 55, bounds.size.h - 52, 110, 36),
                     BTN_GREEN, TXT_LIGHT, "Apply", font_med);

    // Hint
    graphics_context_set_text_color(ctx, GColorLightGray);
    graphics_draw_text(ctx, "Back to cancel", font_sm,
                       GRect(0, bounds.size.h - 14, w, 14),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);
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

static void info_layer_update(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    GFont font_sm  = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
    GFont font_med = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
    GFont font_lg  = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
    int w = bounds.size.w;

    graphics_context_set_fill_color(ctx, FORD_BLUE);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    graphics_context_set_text_color(ctx, TXT_LIGHT);
    graphics_draw_text(ctx, "Vehicle Info", font_lg,
                       GRect(0, 4, w, 26),
                       GTextOverflowModeTrailingEllipsis,
                       GTextAlignmentCenter, NULL);

    if (s_is_loading) {
        graphics_draw_text(ctx, "Loading...", font_med,
                           GRect(0, bounds.size.h/2 - 12, w, 24),
                           GTextOverflowModeTrailingEllipsis,
                           GTextAlignmentCenter, NULL);
        return;
    }

    char buf[32];
    int y = 36;

    // Fuel
    if (s_fuel >= 0) {
        snprintf(buf, sizeof(buf), "Fuel:  %d%%", s_fuel);
        graphics_context_set_text_color(ctx, TXT_LIGHT);
        graphics_draw_text(ctx, buf, font_sm, GRect(10, y, w - 20, 16),
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
        y += 16;
        draw_bar(ctx, 10, y, w - 20, 8, s_fuel,
                 s_fuel > 25 ? BTN_GREEN : BTN_RED);
        y += 14;
    }
    // Oil
    if (s_oil >= 0) {
        snprintf(buf, sizeof(buf), "Oil:   %d%%", s_oil);
        graphics_draw_text(ctx, buf, font_sm, GRect(10, y, w - 20, 16),
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
        y += 16;
        draw_bar(ctx, 10, y, w - 20, 8, s_oil,
                 s_oil > 30 ? BTN_GREEN : BTN_AMBER);
        y += 14;
    }

    // Tire pressures
    if (s_tire_fl >= 0) {
        graphics_context_set_stroke_color(ctx, GColorDarkGray);
        graphics_context_set_stroke_width(ctx, 1);
        graphics_draw_line(ctx, GPoint(10, y + 2), GPoint(w - 10, y + 2));
        y += 8;

        graphics_context_set_text_color(ctx, TXT_LIGHT);
        graphics_draw_text(ctx, "Tires (psi)", font_sm,
                           GRect(10, y, w - 20, 16),
                           GTextOverflowModeTrailingEllipsis,
                           GTextAlignmentLeft, NULL);
        y += 18;

        // 2x2 grid
        int half = w / 2;
        snprintf(buf, sizeof(buf), "FL: %d", s_tire_fl);
        graphics_draw_text(ctx, buf, font_sm, GRect(10, y, half - 10, 16),
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
        snprintf(buf, sizeof(buf), "FR: %d", s_tire_fr);
        graphics_draw_text(ctx, buf, font_sm, GRect(half, y, half - 10, 16),
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
        y += 18;
        snprintf(buf, sizeof(buf), "RL: %d", s_tire_rl);
        graphics_draw_text(ctx, buf, font_sm, GRect(10, y, half - 10, 16),
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
        snprintf(buf, sizeof(buf), "RR: %d", s_tire_rr);
        graphics_draw_text(ctx, buf, font_sm, GRect(half, y, half - 10, 16),
                           GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
    }
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
