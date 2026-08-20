#include <compression/core/Errors.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

using namespace compression::core;

TEST(ErrorsTest, AllDeriveFromRuntimeError) {
  EXPECT_TRUE((std::is_base_of<std::runtime_error, CompressionError>::value));
  EXPECT_TRUE((std::is_base_of<CompressionError, CorruptDataError>::value));
  EXPECT_TRUE((std::is_base_of<CompressionError, InvalidFormatError>::value));
  EXPECT_TRUE(
      (std::is_base_of<CompressionError, UnsupportedVersionError>::value));
  EXPECT_TRUE((std::is_base_of<CompressionError, ConfigurationError>::value));
  EXPECT_TRUE((std::is_base_of<CompressionError, IoError>::value));
}

TEST(ErrorsTest, CarriesMessage) {
  CorruptDataError err("truncated");
  EXPECT_STREQ(err.what(), "truncated");
}

TEST(ErrorsTest, CatchAsBaseClass) {
  try {
    throw InvalidFormatError("bad magic");
  } catch (const CompressionError &e) {
    EXPECT_STREQ(e.what(), "bad magic");
  }
}

TEST(ErrorsTest, CatchAsRuntimeError) {
  try {
    throw IoError("disk full");
  } catch (const std::runtime_error &e) {
    EXPECT_STREQ(e.what(), "disk full");
  }
}
