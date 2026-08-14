#include "source/FileSystemSourceReader.h"

#include "api/types/Error.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_set>

namespace dearoreui::source {

namespace {

[[nodiscard]] api::Error makeError(
    api::ErrorCode code, std::string const& message, std::string detail
) {
    api::Error error;
    error.code    = code;
    error.message = message;
    error.details.push_back(std::move(detail));
    return error;
}

[[nodiscard]] std::vector<std::uint8_t> readBinaryFile(std::filesystem::path const& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    file.seekg(0, std::ios::end);
    auto size = file.tellg();
    if (size <= 0) {
        return {};
    }
    file.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> buffer(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    return buffer;
}

[[nodiscard]] std::string readTextFile(std::filesystem::path const& path) {
    std::ifstream file(path);
    if (!file) {
        return {};
    }
    std::ostringstream stream;
    stream << file.rdbuf();
    return stream.str();
}

} // namespace

[[nodiscard]] std::string_view stripHbuiPrefix(std::string_view path) {
    if (path.starts_with("/hbui/")) {
        return path.substr(6);
    }
    if (path.starts_with("hbui/")) {
        return path.substr(5);
    }
    if (path.starts_with('/')) {
        return path.substr(1);
    }
    return path;
}

FileSystemSourceReader::FileSystemSourceReader(std::filesystem::path baseDirectory)
    : mBaseDirectory(std::move(baseDirectory)) {}

api::Result<PageSourceSnapshot> FileSystemSourceReader::capture(api::PageInfo const& page) {
    PageSourceSnapshot snapshot;
    snapshot.contextId  = api::ContextId{};
    snapshot.capturedAt = std::chrono::system_clock::now();

    std::error_code code;
    if (!std::filesystem::exists(mBaseDirectory, code)) {
        snapshot.partial = true;
        snapshot.errors.push_back(makeError(
            api::ErrorCode::ResourceNotFound,
            "base directory does not exist",
            mBaseDirectory.string()
        ));
        return snapshot;
    }

    auto relativePage = stripHbuiPrefix(page.id.value());
    auto pagePath     = mBaseDirectory / relativePage;
    if (pagePath.empty() || pagePath == mBaseDirectory) {
        snapshot.partial = true;
        snapshot.errors.push_back(makeError(
            api::ErrorCode::InvalidArgument,
            "page id is empty",
            page.id.value()
        ));
        return snapshot;
    }

    // Normalize to prevent traversal out of base directory.
    auto canonicalBase = std::filesystem::canonical(mBaseDirectory, code);
    auto canonicalPage = std::filesystem::weakly_canonical(pagePath, code);
    if (code) {
        snapshot.partial = true;
        snapshot.errors.push_back(makeError(
            api::ErrorCode::ResourceNotFound,
            "failed to canonicalize page path",
            pagePath.string()
        ));
        return snapshot;
    }

    if (canonicalPage.string().find(canonicalBase.string()) != 0) {
        snapshot.partial = true;
        snapshot.errors.push_back(makeError(
            api::ErrorCode::PermissionDenied,
            "page path escapes base directory",
            pagePath.string()
        ));
        return snapshot;
    }

    auto target = std::filesystem::exists(canonicalPage, code) && std::filesystem::is_directory(canonicalPage, code)
                      ? canonicalPage
                      : canonicalPage.parent_path();

    captureDirectory(target, snapshot);
    return snapshot;
}

bool FileSystemSourceReader::isTextResource(std::filesystem::path const& path) const {
    static std::unordered_set<std::string> const textExtensions{
        ".html", ".css", ".js", ".json", ".jsonc", ".txt", ".md"
    };
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return textExtensions.find(ext) != textExtensions.end();
}

std::string FileSystemSourceReader::makeRelativePath(std::filesystem::path const& fullPath) const {
    auto relative = std::filesystem::relative(fullPath, mBaseDirectory);
    return relative.generic_string();
}

void FileSystemSourceReader::captureDirectory(
    std::filesystem::path const& directory,
    PageSourceSnapshot&          snapshot
) const {
    std::error_code code;
    auto iterator = std::filesystem::recursive_directory_iterator(directory, code);
    if (code) {
        snapshot.partial = true;
        snapshot.errors.push_back(makeError(
            api::ErrorCode::ResourceNotFound,
            "cannot iterate directory",
            directory.string()
        ));
        return;
    }

    for (auto const& entry : iterator) {
        if (!entry.is_regular_file()) {
            continue;
        }

        auto const& path = entry.path();
        auto key         = makeRelativePath(path);
        if (key.empty()) {
            continue;
        }

        if (isTextResource(path)) {
            auto content = readTextFile(path);
            if (content.empty() && std::filesystem::file_size(path, code) > 0) {
                snapshot.partial = true;
                snapshot.errors.push_back(makeError(
                    api::ErrorCode::ResourceNotFound,
                    "failed to read text resource",
                    key
                ));
                continue;
            }
            snapshot.textResources.emplace(std::move(key), std::move(content));
        } else {
            auto content = readBinaryFile(path);
            if (content.empty() && std::filesystem::file_size(path, code) > 0) {
                snapshot.partial = true;
                snapshot.errors.push_back(makeError(
                    api::ErrorCode::ResourceNotFound,
                    "failed to read binary resource",
                    key
                ));
                continue;
            }
            snapshot.binaryResources.emplace(std::move(key), std::move(content));
        }
    }
}

} // namespace dearoreui::source
