#include "configuration/line_configuration_file.hpp"

#include "configuration/line_configuration_json.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fstream>
#include <system_error>

#ifdef __unix__
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace yodau::core {
namespace {

[[noreturn]] void fail(const std::string& message) {
    throw line_configuration_error(message);
}

    std::filesystem::path
    temporary_path_for(const std::filesystem::path& target) {
        static std::atomic_uint64_t sequence { 0U };
        const auto tick
            = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto suffix = sequence.fetch_add(1U, std::memory_order_relaxed);
        return target.parent_path()
            / (target.filename().string() + ".tmp." + std::to_string(tick) + "."
               + std::to_string(suffix));
    }

#ifdef __unix__
    void write_private_file(
        const std::filesystem::path& path, const std::string_view contents
    ) {
        const int fd = ::open(
            path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
            S_IRUSR | S_IWUSR
        );
        if (fd < 0) {
            fail(
                "cannot create private temporary line configuration '"
                + path.string() + "': " + std::strerror(errno)
            );
        }

        size_t offset = 0U;
        while (offset < contents.size()) {
            const ssize_t written = ::write(
                fd, contents.data() + offset, contents.size() - offset
            );
            if (written < 0 && errno == EINTR) {
                continue;
            }
            if (written <= 0) {
                const int write_error = errno;
                ::close(fd);
                fail(
                    "cannot write temporary line configuration '"
                    + path.string() + "': " + std::strerror(write_error)
                );
            }
            offset += static_cast<size_t>(written);
        }

        if (::fsync(fd) < 0) {
            const int sync_error = errno;
            ::close(fd);
            fail(
                "cannot sync temporary line configuration '" + path.string()
                + "': " + std::strerror(sync_error)
            );
        }
        if (::close(fd) < 0) {
            fail(
                "cannot close temporary line configuration '" + path.string()
                + "': " + std::strerror(errno)
            );
        }
    }

    void sync_parent_directory(const std::filesystem::path& target) {
        const std::filesystem::path parent = target.parent_path().empty()
            ? std::filesystem::path(".")
            : target.parent_path();
        const int fd
            = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0) {
            fail(
                "cannot open line configuration directory '" + parent.string()
                + "' for sync: " + std::strerror(errno)
            );
        }
        const int result = ::fsync(fd);
        const int sync_error = errno;
        ::close(fd);
        if (result < 0) {
            fail(
                "cannot sync line configuration directory '" + parent.string()
                + "': " + std::strerror(sync_error)
            );
        }
    }
#endif

} // namespace

line_configuration_document
load_line_configuration_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        fail("cannot open line configuration '" + path.string() + "'");
    }
    std::string contents;
    contents.reserve(
        std::min<std::size_t>(maximum_line_configuration_bytes, 64U * 1024U)
    );
    std::array<char, 64U * 1024U> buffer {};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto bytes = input.gcount();
        if (bytes > 0) {
            const auto count = static_cast<std::size_t>(bytes);
            if (contents.size() > maximum_line_configuration_bytes - count) {
                fail("line configuration exceeds the 4 MiB size limit");
            }
            contents.append(buffer.data(), count);
        }
    }
    if (!input.eof()) {
        fail("cannot read line configuration '" + path.string() + "'");
    }
    return decode_line_configuration_json(contents);
}

void save_line_configuration_file_atomic(
    const line_configuration_document& document,
    const std::filesystem::path& path
) {
    if (path.empty() || path.filename().empty()) {
        fail("line configuration output path must name a file");
    }
    const std::string contents = encode_line_configuration_json(document);
    const std::filesystem::path temporary = temporary_path_for(path);

    try {
#ifdef __unix__
        write_private_file(temporary, contents);
        if (::rename(temporary.c_str(), path.c_str()) < 0) {
            fail(
                "cannot replace line configuration '" + path.string()
                + "': " + std::strerror(errno)
            );
        }
        sync_parent_directory(path);
#else
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) {
                fail(
                    "cannot create temporary line configuration '"
                    + temporary.string() + "'"
                );
            }
            output.write(
                contents.data(), static_cast<std::streamsize>(contents.size())
            );
            output.flush();
            if (!output) {
                fail(
                    "cannot write temporary line configuration '"
                    + temporary.string() + "'"
                );
            }
        }
        std::filesystem::rename(temporary, path);
#endif
    } catch (const line_configuration_error&) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    } catch (const std::filesystem::filesystem_error& error) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        fail(
            "cannot replace line configuration '" + path.string()
            + "': " + error.code().message()
        );
    }
}

} // namespace yodau::core
