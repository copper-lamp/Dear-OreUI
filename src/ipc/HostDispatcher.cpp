#include "ipc/HostDispatcher.h"

#include "api/types/Error.h"

#include <chrono>

namespace dearoreui::ipc {

HostDispatcher::HostDispatcher(
    HostMethodRegistry&           registry,
    page::IPageContextManager&    pageManager,
    diagnostic::DiagnosticLogger& logger
)
: mRegistry(registry),
  mPageManager(pageManager),
  mLogger(logger) {}

api::Result<IpcMessage> HostDispatcher::dispatch(IpcMessage const& request, std::chrono::milliseconds timeout) {
    if (request.type != IpcMessageType::Request) {
        return api::Error{api::ErrorCode::InvalidArgument, "only Request messages can be dispatched"};
    }

    {
        std::lock_guard lock(mMutex);
        if (mInvalidated.find(request.contextId) != mInvalidated.end()) {
            return api::Error{api::ErrorCode::InvalidContext, "context has been invalidated"};
        }
        if (mCancelled.find(request.id) != mCancelled.end()) {
            return api::Error{api::ErrorCode::HostCallCancelled, "request was cancelled"};
        }
    }

    auto context = mPageManager.find(request.contextId);
    if (!context) {
        return api::Error{api::ErrorCode::InvalidContext, "context not found"};
    }

    auto method = mRegistry.find(request.method);
    if (!method) {
        return api::Error{api::ErrorCode::HostMethodNotFound, "host method not found"};
    }

    auto start   = std::chrono::steady_clock::now();
    auto result  = method->execute(request.contextId, request.payload);
    auto elapsed = std::chrono::steady_clock::now() - start;

    {
        std::lock_guard lock(mMutex);
        if (mCancelled.find(request.id) != mCancelled.end()) {
            return api::Error{api::ErrorCode::HostCallCancelled, "request was cancelled during execution"};
        }
    }

    if (elapsed > timeout) {
        return api::Error{api::ErrorCode::HostCallTimeout, "host call timed out"};
    }

    IpcMessage response;
    response.type      = IpcMessageType::Response;
    response.id        = request.id;
    response.contextId = request.contextId;

    if (result.isErr()) {
        response.error   = result.error().code;
        response.payload = result.error().message;
    } else {
        response.payload = std::move(result.value());
    }

    return response;
}

void HostDispatcher::cancel(api::RequestId id) {
    std::lock_guard lock(mMutex);
    mCancelled.insert(id);
}

void HostDispatcher::invalidateContext(api::ContextId id) {
    std::lock_guard lock(mMutex);
    mInvalidated.insert(id);
}

} // namespace dearoreui::ipc
