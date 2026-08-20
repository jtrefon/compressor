#include <compression/core/ByteView.hpp>

#include <gtest/gtest.h>

#include <array>
#include <numeric>
#include <string>
#include <vector>

using namespace compression::core;

TEST(ByteViewTest, DefaultConstructedIsEmpty) {
  ByteView view;
  EXPECT_TRUE(view.empty());
  EXPECT_EQ(view.size(), 0u);
  EXPECT_EQ(view.data(), nullptr);
}

TEST(ByteViewTest, FromVector) {
  std::vector<uint8_t> data = {1, 2, 3, 4};
  ByteView view(data);
  EXPECT_EQ(view.size(), data.size());
  EXPECT_EQ(view.data(), data.data());
}

TEST(ByteViewTest, FromString) {
  std::string s = "hello";
  ByteView view(s);
  EXPECT_EQ(view.size(), 5u);
  EXPECT_EQ(view[0], static_cast<uint8_t>('h'));
}

TEST(ByteViewTest, FromArray) {
  std::array<uint8_t, 3> data = {9, 8, 7};
  ByteView view(data);
  EXPECT_EQ(view.size(), 3u);
  EXPECT_EQ(view[2], 7u);
}

TEST(ByteViewTest, FromPointerAndSize) {
  const uint8_t bytes[] = {0xAA, 0xBB, 0xCC};
  ByteView view(bytes, 2);
  EXPECT_EQ(view.size(), 2u);
  EXPECT_EQ(view[0], 0xAA);
  EXPECT_EQ(view[1], 0xBB);
}

TEST(ByteViewTest, IndexingAndAt) {
  std::vector<uint8_t> data = {10, 20, 30};
  ByteView view(data);
  EXPECT_EQ(view[1], 20u);
  EXPECT_EQ(view.at(2), 30u);
  EXPECT_THROW(view.at(3), std::out_of_range);
}

TEST(ByteViewTest, Iteration) {
  std::vector<uint8_t> data = {1, 2, 3};
  ByteView view(data);
  uint8_t sum = 0;
  for (uint8_t b : view) {
    sum += b;
  }
  EXPECT_EQ(sum, 6u);
  EXPECT_EQ(view.end() - view.begin(), 3);
}

TEST(ByteViewTest, SubspanValid) {
  std::vector<uint8_t> data = {0, 1, 2, 3, 4};
  ByteView view(data);
  ByteView middle = view.subspan(1, 3);
  EXPECT_EQ(middle.size(), 3u);
  EXPECT_EQ(middle[0], 1u);
  EXPECT_EQ(middle[2], 3u);

  ByteView tail = view.subspan(3);
  EXPECT_EQ(tail.size(), 2u);
  EXPECT_EQ(tail[0], 3u);

  ByteView full = view.subspan(0);
  EXPECT_EQ(full.size(), 5u);

  ByteView emptyAtEnd = view.subspan(5);
  EXPECT_TRUE(emptyAtEnd.empty());
}

TEST(ByteViewTest, SubspanOutOfRangeThrows) {
  std::vector<uint8_t> data = {0, 1, 2};
  ByteView view(data);
  EXPECT_THROW(view.subspan(4), std::out_of_range);
  EXPECT_THROW(view.subspan(1, 5), std::out_of_range);
}

TEST(ByteViewTest, ViewIsNonOwning) {
  const uint8_t stack_bytes[] = {0x11, 0x22};
  ByteView view(stack_bytes, sizeof(stack_bytes));
  EXPECT_EQ(view[1], 0x22);
}
