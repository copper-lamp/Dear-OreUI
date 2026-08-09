#pragma once

#include "diagnostic/IDiagnosticSink.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace dearoreui::diagnostic {

class FileDiagnosticSink : public IDiagnosticSink {
public:
    explicit FileDiagnosticSink(std::filesystem::path path);

    void consume(DiagnosticEvent const& event) override;
    void flush() override;

private:
    std::filesystem::path mPath;
    std::mutex            mMutex;
    std::ofstream         mOutput;
};

} // namespace dearoreui::diagnostic
