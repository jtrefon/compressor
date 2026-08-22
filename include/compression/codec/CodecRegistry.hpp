#pragma once

#include <compression/ICompressor.hpp>
#include <compression/FileFormat.hpp>

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace compression {
namespace codec {

/**
 * @brief Central, self-registering factory for codecs.
 *
 * The single source of truth for codec creation (Open/Closed Principle):
 * adding an algorithm means adding one registration; factories, the CLI and
 * the UI never need to change.
 *
 * Registry contents are fixed at process start; there is no mutable global
 * state after static initialization.
 */
class CodecRegistry {
public:
  using Factory = std::unique_ptr<ICompressor> (*)();

  static CodecRegistry &instance();

  /**
   * @brief Registers a codec. Throws std::logic_error on duplicate id/name.
   */
  void registerCodec(format::AlgorithmID id, std::string_view name,
                     Factory factory);

  bool contains(format::AlgorithmID id) const;
  bool contains(std::string_view name) const;

  /**
   * @brief Resolves a name to its stable id; UNKNOWN if not registered.
   */
  format::AlgorithmID idOf(std::string_view name) const;

  /**
   * @brief Returns the registered name for an id; "unknown" if absent.
   */
  std::string_view nameOf(format::AlgorithmID id) const;

  /**
   * @brief Creates a fresh codec instance.
   * @throws ConfigurationError if the id or name is not registered.
   */
  std::unique_ptr<ICompressor> create(format::AlgorithmID id) const;
  std::unique_ptr<ICompressor> create(std::string_view name) const;

  /**
   * @brief All registered codecs as (id, name) pairs, sorted by id —
   * for the CLI --list and UI dropdowns.
   */
  std::vector<std::pair<format::AlgorithmID, std::string>> all() const;

private:
  CodecRegistry() = default;

  struct Entry {
    std::string name;
    Factory factory;
  };

  std::map<format::AlgorithmID, Entry> byId_;
  std::map<std::string, format::AlgorithmID> byName_;
};

} // namespace codec
} // namespace compression
