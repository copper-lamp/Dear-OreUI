#pragma once

#include "api/types/Error.h"
#include "api/types/Id.h"
#include "diagnostic/DiagnosticEvent.h"

#include <vector>

namespace dearoreui::diagnostic {

class DiagnosticReport {
public:
    explicit DiagnosticReport(api::DiagnosticId id) : mId(id) {}

    [[nodiscard]] api::DiagnosticId id() const { return mId; }

    [[nodiscard]] std::vector<DiagnosticEvent> const& events() const { return mEvents; }

    void addEvent(DiagnosticEvent event) { mEvents.push_back(std::move(event)); }

    [[nodiscard]] api::Error const& summaryError() const { return mSummaryError; }

    void setSummaryError(api::Error error) { mSummaryError = std::move(error); }

private:
    api::DiagnosticId            mId;
    std::vector<DiagnosticEvent> mEvents;
    api::Error                   mSummaryError;
};

} // namespace dearoreui::diagnostic
