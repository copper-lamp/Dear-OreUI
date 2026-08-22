#pragma once

#include "api/types/Id.h"
#include "diagnostic/DiagnosticEvent.h"
#include "diagnostic/IDiagnosticSink.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace dearoreui::diagnostic {

class DiagnosticLogger;

class EventBuilder {
public:
    EventBuilder(DiagnosticLogger& logger, Severity severity, std::string category, std::string event);

    EventBuilder(EventBuilder const&)            = delete;
    EventBuilder& operator=(EventBuilder const&) = delete;
    EventBuilder(EventBuilder&&)                 = default;
    EventBuilder& operator=(EventBuilder&&)      = default;

    EventBuilder& withContext(api::ContextId id) {
        mEvent.contextId = id;
        return *this;
    }

    EventBuilder& withMod(api::ModId id) {
        mEvent.modId = std::move(id);
        return *this;
    }

    EventBuilder& withPage(api::PageId id) {
        mEvent.pageId = std::move(id);
        return *this;
    }

    EventBuilder& withError(api::ErrorCode code) {
        mEvent.errorCode = code;
        return *this;
    }

    EventBuilder& withField(std::string key, std::string value) {
        mEvent.fields.emplace_back(std::move(key), std::move(value));
        return *this;
    }

    EventBuilder& withMessage(std::string message) {
        mEvent.message = std::move(message);
        return *this;
    }

    void emit();

private:
    DiagnosticLogger& mLogger;
    DiagnosticEvent   mEvent;
};

class DiagnosticLogger {
public:
    DiagnosticLogger() = default;

    void addSink(std::shared_ptr<IDiagnosticSink> sink);

    [[nodiscard]] EventBuilder debug(std::string category, std::string event);
    [[nodiscard]] EventBuilder info(std::string category, std::string event);
    [[nodiscard]] EventBuilder warning(std::string category, std::string event);
    [[nodiscard]] EventBuilder error(std::string category, std::string event);
    [[nodiscard]] EventBuilder critical(std::string category, std::string event);

    void emit(const DiagnosticEvent& event);
    void flush();
    void clearSinks();
    [[nodiscard]] std::vector<DiagnosticEvent> snapshot() const;

    [[nodiscard]] api::DiagnosticId nextId();

private:
    mutable std::mutex                            mMutex;
    std::vector<std::shared_ptr<IDiagnosticSink>> mSinks;
    std::vector<DiagnosticEvent>                  mEvents;
    std::atomic<std::uint64_t>                    mNextId{1};
};

[[nodiscard]] DiagnosticLogger& globalLogger();

} // namespace dearoreui::diagnostic
