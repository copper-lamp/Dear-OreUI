#pragma once

#include "api/types/Id.h"
#include "api/types/Result.h"
#include "diagnostic/DiagnosticLogger.h"
#include "ipc/HostMethodRegistry.h"
#include "ipc/IpcMessage.h"
#include "page/IPageContextManager.h"

#include <chrono>
#include <unordered_map>
#include <unordered_set>

namespace dearoreui::ipc {

class HostDispatcher {
public:
    HostDispatcher(
        HostMethodRegistry&           registry,
        page::IPageContextManager&    pageManager,
        diagnostic::DiagnosticLogger& logger
    );

    [[nodiscard]] api::Result<IpcMessage> dispatch(IpcMessage const& request, std::chrono::milliseconds timeout);

    void cancel(api::RequestId id);
    void invalidateContext(api::ContextId id);

private:
    HostMethodRegistry&           mRegistry;
    page::IPageContextManager&    mPageManager;
    diagnostic::DiagnosticLogger& mLogger;

    std::mutex                         mMutex;
    std::unordered_set<api::RequestId> mCancelled;
    std::unordered_set<api::ContextId> mInvalidated;
};

} // namespace dearoreui::ipc
