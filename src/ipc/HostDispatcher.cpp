#include "ipc/HostDispatcher.h"

#include "api/types/Error.h"
#include "diagnostic/Stage5IpcTelemetry.h"

#include <algorithm>
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

    auto entry = mRegistry.findByName(request.method);
    diagnostic::recordStage5HostCall(
        request.contextId,
        request.id,
        request.method,
        request.payload,
        entry ? std::optional<api::ModId>{entry->owner} : std::nullopt
    );

    {
        std::lock_guard lock(mMutex);
        if (mInvalidated.find(request.contextId) != mInvalidated.end()) {
            diagnostic::recordStage5HostInvalidContext(request.contextId, request.id, request.method);
            return api::Error{api::ErrorCode::InvalidContext, "context has been invalidated"};
        }
        if (mCancelled.find(request.id) != mCancelled.end()) {
            diagnostic::recordStage5HostCancelled(request.contextId, request.id, request.method);
            return api::Error{api::ErrorCode::HostCallCancelled, "request was cancelled"};
        }
    }

    auto context = mPageManager.find(request.contextId);
    if (!context) {
        diagnostic::recordStage5HostInvalidContext(request.contextId, request.id, request.method);
        return api::Error{api::ErrorCode::InvalidContext, "context not found"};
    }

    if (request.payload.size() > 1024 * 1024) {
        return api::Error{api::ErrorCode::InvalidFormat, "host request payload exceeds safety limit"};
    }

    if (!entry) {

        diagnostic::recordStage5HostError(
            request.contextId,
            request.id,
            request.method,
            api::ErrorCode::HostMethodNotFound,
            "host method not found"
        );
        return api::Error{api::ErrorCode::HostMethodNotFound, "host method not found"};
    }

    auto const& manifest = entry->manifest;
    if (request.payload.size() > manifest.maxRequestBytes) {
        return api::Error{api::ErrorCode::InvalidFormat, "host request payload exceeds method limit"};
    }
    if (!manifest.pageScopes.empty()) {
        auto allowed = std::find(manifest.pageScopes.begin(), manifest.pageScopes.end(), context->page.scope) != manifest.pageScopes.end();
        if (!allowed && std::find(manifest.pageScopes.begin(), manifest.pageScopes.end(), api::PageScope::Any) == manifest.pageScopes.end()) {
            return api::Error{api::ErrorCode::PermissionDenied, "host method is not available on this page scope"};
        }
    }
    auto start   = std::chrono::steady_clock::now();
    auto result  = entry->method->execute(request.contextId, request.payload);
    auto elapsed = std::chrono::steady_clock::now() - start;
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);

    {
        std::lock_guard lock(mMutex);
        if (mCancelled.find(request.id) != mCancelled.end()) {
            diagnostic::recordStage5HostCancelled(request.contextId, request.id, request.method);
            return api::Error{api::ErrorCode::HostCallCancelled, "request was cancelled during execution"};
        }
    }

    auto effectiveTimeout = std::min(timeout, manifest.timeout);
    if (elapsed > effectiveTimeout) {
        diagnostic::recordStage5HostError(
            request.contextId,
            request.id,
            request.method,
            api::ErrorCode::HostCallTimeout,
            "host call timed out"
        );
        return api::Error{api::ErrorCode::HostCallTimeout, "host call timed out"};
    }

    IpcMessage response;
    response.type      = IpcMessageType::Response;
    response.id        = request.id;
    response.contextId = request.contextId;

    if (mReportCallback) mReportCallback(request.contextId, request.id, request.method, elapsedMs, request.payload.size(), result.isOk() ? result.value().size() : 0, result.isErr() ? result.error().code : api::ErrorCode::None);

    if (result.isErr()) {
        response.error   = result.error().code;
        response.payload = result.error().message;
        diagnostic::recordStage5HostError(
            request.contextId,
            request.id,
            request.method,
            result.error().code,
            result.error().message
        );
    } else {
        response.payload = std::move(result.value());
        if (response.payload.size() > manifest.maxResponseBytes) {
            return api::Error{api::ErrorCode::InvalidFormat, "host response payload exceeds method limit"};
        }
        diagnostic::recordStage5HostResponse(request.contextId, request.id, request.method, false);
    }

    return response;
}

void HostDispatcher::cancel(api::RequestId id) {
    std::lock_guard lock(mMutex);
    mCancelled.insert(id);
}

void HostDispatcher::setReportCallback(ReportCallback callback) { mReportCallback = std::move(callback); }

void HostDispatcher::invalidateContext(api::ContextId id) {
    std::lock_guard lock(mMutex);
    mInvalidated.insert(id);
}

} // namespace dearoreui::ipc
