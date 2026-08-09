#pragma once

#include "diagnostic/IDiagnosticSink.h"

#include <mutex>
#include <vector>

namespace dearoreui::diagnostic {

class MemoryDiagnosticSink : public IDiagnosticSink {
public:
    void consume(DiagnosticEvent const& event) override {
        std::lock_guard lock(mMutex);
        mEvents.push_back(event);
    }

    [[nodiscard]] std::vector<DiagnosticEvent> const& events() const {
        std::lock_guard lock(mMutex);
        return mEvents;
    }

    [[nodiscard]] std::size_t count() const {
        std::lock_guard lock(mMutex);
        return mEvents.size();
    }

    void clear() {
        std::lock_guard lock(mMutex);
        mEvents.clear();
    }

private:
    mutable std::mutex       mMutex;
    std::vector<DiagnosticEvent> mEvents;
};

} // namespace dearoreui::diagnostic
