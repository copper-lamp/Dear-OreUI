#pragma once

#include "source/ISourceReader.h"

#include <filesystem>

namespace dearoreui::source {

class FileSystemSourceReader : public ISourceReader {
public:
    explicit FileSystemSourceReader(std::filesystem::path baseDirectory);

    [[nodiscard]] api::Result<PageSourceSnapshot> capture(api::PageInfo const& page) override;

private:
    [[nodiscard]] bool isTextResource(std::filesystem::path const& path) const;
    [[nodiscard]] std::string makeRelativePath(std::filesystem::path const& fullPath) const;
    void captureDirectory(
        std::filesystem::path const& directory,
        PageSourceSnapshot&          snapshot
    ) const;

    std::filesystem::path mBaseDirectory;
};

} // namespace dearoreui::source
