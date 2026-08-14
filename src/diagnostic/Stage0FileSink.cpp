#include "diagnostic/Stage0FileSink.h"

namespace dearoreui::diagnostic {

namespace {

std::string escape(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (char character : value) {
        switch (character) {
        case '\\':
            result += "\\\\";
            break;
        case '\t':
            result += "\\t";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        default:
            result += character;
            break;
        }
    }
    return result;
}

std::string escapeFields(std::vector<DiagnosticField> const& fields) {
    std::string result;
    for (auto const& field : fields) {
        if (!result.empty()) result += '\t';
        result += escape(field.key);
        result += '=';
        result += escape(field.value);
    }
    return result;
}

} // namespace

Stage0FileSink::Stage0FileSink(std::filesystem::path path, std::string sessionId)
: mPath(std::move(path)),
  mSessionId(std::move(sessionId)) {
    std::error_code error;
    std::filesystem::create_directories(mPath.parent_path(), error);
}

void Stage0FileSink::setSessionId(std::string sessionId) {
    std::lock_guard lock(mMutex);
    mSessionId = std::move(sessionId);
}

void Stage0FileSink::consume(DiagnosticEvent const& event) {
    std::lock_guard lock(mMutex);
    if (!mOutput.is_open()) {
        mOutput.open(mPath, std::ios::out | std::ios::app | std::ios::binary);
    }
    if (!mOutput.is_open()) return;

    std::string line = escape(event.category);
    if (!mSessionId.empty()) {
        line += "\tsession_id=";
        line += escape(mSessionId);
    }
    line += "\tevent=";
    line += escape(event.event);

    if (event.contextId) {
        line += "\tcontext_id=";
        line += std::to_string(event.contextId->value());
    }
    if (event.pageId) {
        line += "\tpage_id=";
        line += escape(event.pageId->value());
    }
    if (event.modId) {
        line += "\tmod_id=";
        line += escape(event.modId->value());
    }

    auto fields = escapeFields(event.fields);
    if (!fields.empty()) {
        line += '\t';
        line += fields;
    }

    if (!event.message.empty()) {
        line += "\tmessage=";
        line += escape(event.message);
    }

    mOutput << line << '\n';
}

void Stage0FileSink::flush() {
    std::lock_guard lock(mMutex);
    if (mOutput.is_open()) mOutput.flush();
}

} // namespace dearoreui::diagnostic
