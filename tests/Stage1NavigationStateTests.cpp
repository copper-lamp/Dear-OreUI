#undef NDEBUG
#include "poc/Stage1NavigationState.h"
#include "tests/Tests.h"

#include <cassert>

namespace dearoreui::tests {

void runStage1NavigationStateTests() {
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

} // namespace dearoreui::tests
