#include <pebble.h>
#include <pebble_fonts.h>
#include "debugging.h"
#include "pom.h"
#include "pom_vibes.h"
#include "pom_text.h"
#include "pom_menu.h"
#include "pom_cookies.h"
#include "pom_timer.h"

// the common, global app structure
PomApplication app;

GSize windowSize = {144, 168};

// Utilities --------------------------------------------------------------

static char gTimeString[6]; // shared time format buffer
/** Formats a time into MM:SS, e.g. 04:50 */
inline void formatTime(char* str, int seconds) {
    if (seconds < 0) seconds = 0;
    snprintf(str, 6, "%02d:%02d", seconds / 60, seconds % 60);
}

//  Application  ------------------------------------------------------------

void moveTextLayers(int workingTextLayerY, int timeTextLayerY) {
    GRect frame;
    frame = layer_get_frame(text_layer_get_layer(app.workingTextLayer));
    frame.origin.y = workingTextLayerY;
    layer_set_frame(text_layer_get_layer(app.workingTextLayer), frame);
    frame = layer_get_frame(text_layer_get_layer(app.timeTextLayer));
    frame.origin.y = timeTextLayerY;
    layer_set_frame(text_layer_get_layer(app.timeTextLayer), frame);
}

void pomUpdateTextLayers() {
    switch (app.timer.state) {
        case PomStateWorking:
            text_layer_set_text(app.workingTextLayer, POM_TEXT_WORK[app.settings.language]);
            formatTime(gTimeString, app.settings.workTicks);
            text_layer_set_text(app.timeTextLayer, gTimeString);
            moveTextLayers(2, 45);
            break;
        case PomStateReady:
            text_layer_set_text(app.workingTextLayer, POM_TEXT_READY[app.settings.language]);
            static char pomCounterString[64];
            snprintf(pomCounterString, ARRAY_LENGTH(pomCounterString), POM_TEXT_POM_COUNTER[app.settings.language], app.timer.completedPoms);
            text_layer_set_text(app.timeTextLayer, pomCounterString);
            moveTextLayers(2, 45);
            break;
        case PomStateResting:
            text_layer_set_text(app.workingTextLayer, POM_TEXT_REST[app.settings.language]);
            formatTime(gTimeString, app.ticksRemaining);
            text_layer_set_text(app.timeTextLayer, gTimeString);
            moveTextLayers(windowSize.h - 65 - 2, windowSize.h - 20 - 2);
            break;
        default:
            break;
    }
    GRect frame = layer_get_frame(text_layer_get_layer(app.timeTextLayer));
    frame.size.w = (app.timer.state == PomStateReady)? 80 : 30;
    layer_set_frame(text_layer_get_layer(app.timeTextLayer), frame);
}

/** Change the state between ready, working, and resting. */
void pomSetState(PomState newState) {
    app.timer.state = newState;
    
    switch (newState) {
        case PomStateWorking:
            app.totalTicks = app.ticksRemaining = app.settings.workTicks;
            break;

        case PomStateResting:
            app.totalTicks = app.ticksRemaining =app.settings.restTicks;
            //Take recess / long rest
            if (app.settings.takeLongRests && (app.timer.completedPoms % app.settings.pomsPerLongRest) == 0) {
                app.totalTicks = app.ticksRemaining =app.settings.longRestTicks;
            }
            break;
            
        case PomStateReady:
            break;

        default:
            WARN("Unhandled state change %d", newState);
            break;
    }
    
    pomUpdateTextLayers();
    layer_mark_dirty(window_get_root_layer(app.mainWindow));
}

void pomInit() {
    switch (app.timer.state) {
        case PomStateWorking:
            app.totalTicks = app.settings.workTicks;
            app.ticksRemaining = app.timer.end - (uint64_t)time(NULL);
            break;

        case PomStateResting:
            app.totalTicks = app.settings.restTicks;
            //Take recess / long rest
            if (app.settings.takeLongRests && (app.timer.completedPoms % app.settings.pomsPerLongRest) == 0) {
                app.totalTicks = app.settings.longRestTicks;
            }
            app.ticksRemaining = app.timer.end - (uint64_t)time(NULL);
            break;
            
        case PomStateReady:
            break;

        default:
            WARN("Unhandled state change %d", newStateapp.timer.state);
            break;
    }
    
    pomUpdateTextLayers();
    layer_mark_dirty(window_get_root_layer(app.mainWindow));
}

/** Tick handler. Called every second. Also called on the minute for "heartbeat" working reminders. */
void pomOnTick(struct tm *tick_time, TimeUnits units_changed) {
    if (app.timer.state == PomStateReady) return;
    app.ticksRemaining--;
    
    // check for time up
    // Note: because this returns, you have an extra second to see the change over. feature, not a bug.
    if (app.ticksRemaining < 0) {
        if (app.timer.state == PomStateWorking) {
            // you finished the pomodoro! congrats!
            app.timer.completedPoms++;
            if (app.timer.lastPomHour < 6 && tick_time->tm_hour >= 6) {
                app.timer.completedPoms = 0;
            }
            app.timer.lastPomHour = tick_time->tm_hour;
            vibes_short_pulse();
            light_enable_interaction();
            pomSetState(PomStateResting);
            return;
        }
        else if (app.timer.state == PomStateResting) {
            // time to start another pomodoro.
            vibes_enqueue_custom_pattern(VIBRATE_DIT_DIT_DAH);
            light_enable_interaction();
            pomSetState(app.settings.autoContinue? PomStateWorking : PomStateReady);
            return;
        }
    }
    
    bool isWorking = (app.timer.state == PomStateWorking);
//    bool isResting = (app.timer.state == PomStateResting);

    // heartbeat
    if (isWorking && app.settings.vibrateWhileWorking && (app.ticksRemaining % app.settings.vibrateTicks) == 0) {
        vibes_enqueue_custom_pattern(VIBRATE_MINIMAL);
    }
    
    // TODO resize inverter
/*
    float pctRemaining = (app.ticksRemaining + 0.0) / app.totalTicks;
    GRect inverterFrame = GRect(0, 0, windowSize.w, 0);
    if (isWorking) {
        inverterFrame.size.h = (1.0 - pctRemaining) * windowSize.h;
    }
    else if (isResting) {
        inverterFrame.size.h = pctRemaining * windowSize.h;
    }
    layer_set_frame(inverter_layer_get_layer(app.inverterLayer), inverterFrame);
*/
    
    // set timer text
    formatTime(gTimeString, app.ticksRemaining);
    text_layer_set_text(app.timeTextLayer, gTimeString);
    
    // redraw!
    layer_mark_dirty(window_get_root_layer(app.mainWindow));
}

/** Handles up or down button click while in main window. Use this click to start or restart a cycle. */
void pomOnMainWindowUpOrDownClick(ClickRecognizerRef recognizer, void *context) {
    if (app.timer.state == PomStateReady) {
        pomSetState(PomStateWorking);
    } else {
        pomSetState(PomStateReady);
    }
}

/** Select (middle button) click handler. Launches into settings menu. */
void pomOnMainWindowSelectClick(ClickRecognizerRef recognizer, void *context) {
    if (window_stack_contains_window(app.menuWindow)) {
        WARN("Window already in window stack");
        return;
    }
    window_stack_push(app.menuWindow, true);
}

/** Set up click handlers on the main window. */
void pomMainWindowClickProvider(void *context) {
    window_single_click_subscribe(BUTTON_ID_UP, pomOnMainWindowUpOrDownClick);
    window_single_click_subscribe(BUTTON_ID_DOWN, pomOnMainWindowUpOrDownClick);
    window_single_click_subscribe(BUTTON_ID_SELECT, pomOnMainWindowSelectClick);
}

void pomUpdateBatteryLayer(Layer *layer, GContext *ctx) {
  // Emulator battery meter on Aplite
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_draw_rect(ctx, GRect(126, 4, 14, 8));
  graphics_draw_line(ctx, GPoint(140, 6), GPoint(140, 9));

  BatteryChargeState state = battery_state_service_peek();
  int width = (int)(float)(((float)state.charge_percent / 100.0F) * 10.0F);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_rect(ctx, GRect(128, 6, width, 4), 0, GCornerNone);
}

/** App initialization. */
void pomStartup() {
    // setup default settings
    // after, load settings from persistent storage
    PomSettings defaultSettings = (PomSettings){
        .language = PomEnglish,
        .workTicks = 60 * 25,
        .restTicks = 60 * 5,
        .longRestTicks = 60 * 15,
        .pomsPerLongRest = 4,
        .vibrateTicks = 10,
        .takeLongRests = true,
        .vibrateWhileWorking = true,
        .showClock = false,
        .autoContinue = false,
        .annoyAfterRestExceeded = false,
    };
    PomTimer defaultTimer = (PomTimer){
        .completedPoms = 0,
        .lastPomHour = 0,
        .end = 0,
        .state = PomStateReady,
    };

    app.mainWindow = window_create();
    window_set_background_color(app.mainWindow, GColorWhite);
    window_set_click_config_provider(app.mainWindow, pomMainWindowClickProvider);
    
    app.workingTextLayer = text_layer_create(GRect(2, 2, windowSize.w, 50));
    text_layer_set_font(app.workingTextLayer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    text_layer_set_background_color(app.workingTextLayer, GColorClear);
    
    app.timeTextLayer = text_layer_create(GRect(4, 45, 80, 30));
    text_layer_set_font(app.timeTextLayer, fonts_get_system_font(FONT_KEY_FONT_FALLBACK));
    text_layer_set_background_color(app.timeTextLayer, GColorClear);

    app.statusBarLayer = status_bar_layer_create();
    app.batteryLayer = layer_create(GRect(0, 0, 144, STATUS_BAR_LAYER_HEIGHT));
    layer_set_update_proc(app.batteryLayer, pomUpdateBatteryLayer);
    
    layer_add_child(window_get_root_layer(app.mainWindow), text_layer_get_layer(app.workingTextLayer));
    layer_add_child(window_get_root_layer(app.mainWindow), text_layer_get_layer(app.timeTextLayer));

#if USE_CONSOLE
    __console_layer = text_layer_create(GRect(0, 28, 144, 140));
    text_layer_set_overflow_mode(__console_layer, GTextOverflowModeWordWrap);
    text_layer_set_font(__console_layer, fonts_get_system_font(FONT_KEY_FONT_FALLBACK));
    layer_add_child(window_get_root_layer(app.mainWindow), text_layer_get_layer(__console_layer.layer));
#endif

    pomInitMenuModule();
    pomInitCookiesModule();
    pomInitTimerModule();

    app.settings = defaultSettings;
    if (!pomLoadTimer()) {
        LOG("Timer data not found, using defaults");
        app.timer = defaultTimer;
    } else {
        time_t currentTime = time(NULL);
        struct tm *tick_time = localtime((time_t*)&currentTime);
        if (app.timer.lastPomHour < 6 && tick_time->tm_hour >= 6) {
            app.timer.completedPoms = 0;
        }
    }
    tick_timer_service_subscribe(SECOND_UNIT, pomOnTick);
    if (!pomLoadCookies()) {
        LOG("Settings not found, using defaults");
        app.settings = defaultSettings;
    }
    if (app.settings.showClock) {
        layer_add_child(window_get_root_layer(app.mainWindow), status_bar_layer_get_layer(app.statusBarLayer));
        layer_add_child(window_get_root_layer(app.mainWindow), app.batteryLayer);
    }

    pomInit();
    window_stack_push(app.mainWindow, true);
}

void pomShutdown() {
    if (app.timer.state == PomStateWorking || PomStateResting) {
        time_t wakeupTime = time(NULL) + app.ticksRemaining;
        wakeup_schedule(wakeupTime, 0, true);
    }
    pomSaveTimer();
    pomSaveCookies();
    window_destroy(app.mainWindow);
    window_destroy(app.menuWindow);
}

//  Pebble Core ------------------------------------------------------------

int main() {
    pomStartup();
    app_event_loop();
    pomShutdown();
}
