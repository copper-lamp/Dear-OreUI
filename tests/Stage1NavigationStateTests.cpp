#include "poc/Stage1NavigationState.h"

#include <cassert>

int main() {
    dearoreui::poc::Stage1NavigationState state;

    assert(state.trySchedule());
    assert(!state.trySchedule());
    assert(state.tryBeginExecution());
    assert(!state.trySchedule());
    assert(!state.tryBeginExecution());

    state.complete();

    assert(!state.trySchedule());
    assert(!state.tryBeginExecution());
}
