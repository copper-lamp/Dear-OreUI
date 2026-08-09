#pragma once

#include "diagnostic/IDiagnosticSink.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace dearoreui::diagnostic {

class Stage0FileSink : public IDiagnosticSink {
public:
    explicit Stage0FileSink(std::filesystem::path path, std::string sessionId);

    void consume(DiagnosticEvent const& event) override;
    void flush() override;

    void setSessionId(std::string sessionId);

private:
    std::filesystem::path mPath;
    std::string           mSessionId;
    std::mutex            mMutex;
    std::ofstream         mOutput;
};

} // namespace dearoreui::diagnostic
