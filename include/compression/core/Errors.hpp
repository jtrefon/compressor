#pragma once

#include <stdexcept>
#include <string>

namespace compression {
namespace core {

/**
 * @brief Base class for all library errors.
 *
 * Inherits std::runtime_error so existing 1.x catch-sites keep working.
 */
class CompressionError : public std::runtime_error {
public:
  explicit CompressionError(const std::string &message)
      : std::runtime_error(message) {}
};

/**
 * @brief The data stream is truncated or otherwise damaged at decode time.
 */
class CorruptDataError : public CompressionError {
public:
  explicit CorruptDataError(const std::string &message)
      : CompressionError(message) {}
};

/**
 * @brief The byte stream does not match the expected format (bad magic,
 * malformed framing).
 */
class InvalidFormatError : public CompressionError {
public:
  explicit InvalidFormatError(const std::string &message)
      : CompressionError(message) {}
};

/**
 * @brief The format version is newer than this library understands.
 */
class UnsupportedVersionError : public CompressionError {
public:
  explicit UnsupportedVersionError(const std::string &message)
      : CompressionError(message) {}
};

/**
 * @brief Invalid options or configuration passed by a client.
 */
class ConfigurationError : public CompressionError {
public:
  explicit ConfigurationError(const std::string &message)
      : CompressionError(message) {}
};

/**
 * @brief Underlying I/O failure (file open/read/write).
 */
class IoError : public CompressionError {
public:
  explicit IoError(const std::string &message)
      : CompressionError(message) {}
};

} // namespace core
} // namespace compression
