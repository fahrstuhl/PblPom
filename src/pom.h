#pragma once
#include <pebble.h>

typedef enum {
    PomStateReady,
    PomStateWorking,
    PomStateResting,
} PomState;

typedef struct {
    uint16_t language;
    uint16_t workTicks;
    uint16_t restTicks;
    uint16_t longRestTicks;
    uint16_t pomsPerLongRest;
    uint16_t vibrateTicks;
    bool takeLongRests;
    bool vibrateWhileWorking;
    bool showClock;
    bool autoContinue;
    bool annoyAfterRestExceeded;
} PomSettings;

typedef struct {
    unsigned int completedPoms;
    int lastPomHour;
    uint64_t end;
    PomState state;
} PomTimer;

typedef struct {

    PomSettings settings;
    PomTimer timer;
    int ticksRemaining;
    int totalTicks;
    
    Window *mainWindow;
    Window *menuWindow;
    TextLayer *workingTextLayer;
    TextLayer *timeTextLayer;
    StatusBarLayer *statusBarLayer;
    Layer *batteryLayer;

} PomApplication;


void pomSetState(PomState newState);

//the shared, global, application structure
extern PomApplication app;
