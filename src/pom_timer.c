#include "pom_timer.h"
#include "pom.h"

#define TIMER_STRUCT_KEY 973245184

void pomSaveTimer() {
    app.timer.end = (int64_t)time(NULL) + app.ticksRemaining;
    persist_write_data(TIMER_STRUCT_KEY, &app.timer, sizeof(PomTimer));
}

bool pomLoadTimer() {
    return (E_DOES_NOT_EXIST != persist_read_data(TIMER_STRUCT_KEY, &app.timer, sizeof(PomTimer)));
}

void pomClearTimer() {
    persist_delete(TIMER_STRUCT_KEY);
}

void pomInitTimerModule() {

}
