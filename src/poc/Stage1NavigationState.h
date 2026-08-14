#pragma once

#include <atomic>

namespace dearoreui::poc {

class Stage1NavigationState {
public:
    [[nodiscard]] bool trySchedule() {
        auto expected = Value::Idle;
        return mValue.compare_exchange_strong(expected, Value::Scheduled);
    }

    [[nodiscard]] bool tryBeginExecution() {
        auto expected = Value::Scheduled;
        return mValue.compare_exchange_strong(expected, Value::Executing);
    }

    [[nodiscard]] bool isScheduled() const { return mValue.load() == Value::Scheduled; }

    void complete() { mValue.store(Value::Completed); }

    void reset() { mValue.store(Value::Idle); }

private:
    enum class Value {
        Idle,
        Scheduled,
        Executing,
        Completed,
    };

    std::atomic<Value> mValue{Value::Idle};
};

} // namespace dearoreui::poc
