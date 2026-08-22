#include "diagnostic/DiagnosticLogger.h"

namespace dearoreui::diagnostic {

EventBuilder::EventBuilder(DiagnosticLogger& logger, Severity severity, std::string category, std::string event)
: mLogger(logger),
  mEvent(
      DiagnosticEvent{
          .id        = logger.nextId(),
          .timestamp = std::chrono::system_clock::now(),
          .severity  = severity,
          .category  = std::move(category),
          .event     = std::move(event),
      }
  ) {}

void EventBuilder::emit() { mLogger.emit(std::move(mEvent)); }

void DiagnosticLogger::addSink(std::shared_ptr<IDiagnosticSink> sink) {
    std::lock_guard lock(mMutex);
    mSinks.push_back(std::move(sink));
}

EventBuilder DiagnosticLogger::debug(std::string category, std::string event) {
    return EventBuilder(*this, Severity::Debug, std::move(category), std::move(event));
}

EventBuilder DiagnosticLogger::info(std::string category, std::string event) {
    return EventBuilder(*this, Severity::Info, std::move(category), std::move(event));
}

EventBuilder DiagnosticLogger::warning(std::string category, std::string event) {
    return EventBuilder(*this, Severity::Warning, std::move(category), std::move(event));
}

EventBuilder DiagnosticLogger::error(std::string category, std::string event) {
    return EventBuilder(*this, Severity::Error, std::move(category), std::move(event));
}

EventBuilder DiagnosticLogger::critical(std::string category, std::string event) {
    return EventBuilder(*this, Severity::Critical, std::move(category), std::move(event));
}

void DiagnosticLogger::emit(const DiagnosticEvent& event) {
    std::vector<std::shared_ptr<IDiagnosticSink>> sinks;
    {
        std::lock_guard lock(mMutex);
        sinks = mSinks;
        mEvents.push_back(event);
    }
    for (auto const& sink : sinks) {
        if (sink) sink->consume(event);
    }
}

void DiagnosticLogger::flush() {
    std::vector<std::shared_ptr<IDiagnosticSink>> sinks;
    {
        std::lock_guard lock(mMutex);
        sinks = mSinks;
    }
    for (auto const& sink : sinks) {
        if (sink) sink->flush();
    }
}

void DiagnosticLogger::clearSinks() {
    std::lock_guard lock(mMutex);
    mSinks.clear();
}

std::vector<DiagnosticEvent> DiagnosticLogger::snapshot() const {
    std::lock_guard lock(mMutex);
    return mEvents;
}

api::DiagnosticId DiagnosticLogger::nextId() { return api::DiagnosticId{mNextId.fetch_add(1)}; }

DiagnosticLogger& globalLogger() {
    static DiagnosticLogger logger;
    return logger;
}

} // namespace dearoreui::diagnostic
