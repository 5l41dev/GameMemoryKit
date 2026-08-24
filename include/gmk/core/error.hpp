// GameMemoryKit — Error model.
//
// Errors are value types: they can be copied, stored, and inspected without
// exceptions. Every failing operation in GameMemoryKit reports its failure
// through an Error, usually wrapped in a Result<T>.
//
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace gmk {

/// Categorizes the failure of an operation.
enum class ErrorCode {
    None,              ///< No error. Only present on a default-constructed Error.
    InvalidArgument,   ///< A caller-supplied argument was invalid.
    NotFound,          ///< The requested object (process, module, symbol, ...) was not found.
    PermissionDenied,  ///< The operation was not permitted.
    Unsupported,       ///< The operation is not supported in this build/configuration.
    InvalidBinary,     ///< A binary file/image is malformed.
    InvalidAddress,    ///< An address was null, out of range, or otherwise unusable.
    PlatformError,     ///< An underlying OS call failed. See native_code().
    ParseError,        ///< A textual or structured input could not be parsed.
    IoError,           ///< A file or stream I/O operation failed.
    OutOfRange,        ///< An offset/size exceeded the bounds of the input.
    NotImplemented,    ///< Declared but not yet implemented.
};

/// Returns a stable, human-readable name for an error code (e.g. "NotFound").
[[nodiscard]] constexpr std::string_view to_string(ErrorCode code) noexcept
{
    using namespace std::string_view_literals;
    switch (code) {
        case ErrorCode::None: return "None"sv;
        case ErrorCode::InvalidArgument: return "InvalidArgument"sv;
        case ErrorCode::NotFound: return "NotFound"sv;
        case ErrorCode::PermissionDenied: return "PermissionDenied"sv;
        case ErrorCode::Unsupported: return "Unsupported"sv;
        case ErrorCode::InvalidBinary: return "InvalidBinary"sv;
        case ErrorCode::InvalidAddress: return "InvalidAddress"sv;
        case ErrorCode::PlatformError: return "PlatformError"sv;
        case ErrorCode::ParseError: return "ParseError"sv;
        case ErrorCode::IoError: return "IoError"sv;
        case ErrorCode::OutOfRange: return "OutOfRange"sv;
        case ErrorCode::NotImplemented: return "NotImplemented"sv;
    }
    return "Unknown"sv;
}

/// Streams the error code name (e.g. "NotFound").
inline std::ostream& operator<<(std::ostream& out, ErrorCode code)
{
    return out << to_string(code);
}

/// A descriptive, copyable error value.
///
/// Carries a category, a human-readable message, and — for OS-level failures —
/// the native error code (Windows `GetLastError()` / `errno` on POSIX).
class Error {
public:
    /// Constructs a "no error" value.
    Error() = default;

    Error(ErrorCode code, std::string message, int native_code = 0)
        : code_(code), message_(std::move(message)), native_(native_code)
    {
    }

    /// The error category.
    [[nodiscard]] ErrorCode code() const noexcept { return code_; }

    /// A human-readable description.
    [[nodiscard]] const std::string& message() const noexcept { return message_; }

    /// The native OS error code, or 0 when not applicable.
    [[nodiscard]] int native_code() const noexcept { return native_; }

    /// True when this Error represents no failure.
    [[nodiscard]] bool ok() const noexcept { return code_ == ErrorCode::None; }

    /// Full formatted description, e.g. "NotFound: no process with id 1234".
    [[nodiscard]] std::string what() const;

    /// Shortcut: "Code: message".
    [[nodiscard]] std::string to_string() const { return what(); }

    // Convenience factories.

    static Error invalid_argument(std::string message)
    {
        return {ErrorCode::InvalidArgument, std::move(message)};
    }

    static Error not_found(std::string message)
    {
        return {ErrorCode::NotFound, std::move(message)};
    }

    static Error permission_denied(std::string message)
    {
        return {ErrorCode::PermissionDenied, std::move(message)};
    }

    static Error unsupported(std::string message)
    {
        return {ErrorCode::Unsupported, std::move(message)};
    }

    static Error invalid_binary(std::string message)
    {
        return {ErrorCode::InvalidBinary, std::move(message)};
    }

    static Error invalid_address(std::string message)
    {
        return {ErrorCode::InvalidAddress, std::move(message)};
    }

    static Error platform(std::string message, int native_code = 0)
    {
        return {ErrorCode::PlatformError, std::move(message), native_code};
    }

    static Error parse(std::string message)
    {
        return {ErrorCode::ParseError, std::move(message)};
    }

    static Error io(std::string message, int native_code = 0)
    {
        return {ErrorCode::IoError, std::move(message), native_code};
    }

    static Error out_of_range(std::string message)
    {
        return {ErrorCode::OutOfRange, std::move(message)};
    }

private:
    ErrorCode code_{ErrorCode::None};
    std::string message_;
    int native_{0};
};

}  // namespace gmk
