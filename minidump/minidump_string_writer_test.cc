// Copyright 2014 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "minidump/minidump_string_writer.h"

#include <windows.h>
#include <dbghelp.h>
#include <sys/types.h>

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "gtest/gtest.h"
#include "minidump/test/minidump_rva_list_test_util.h"
#include "minidump/test/minidump_string_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "util/file/string_file_writer.h"

namespace crashpad {
namespace test {
namespace {

TEST(MinidumpStringWriter, MinidumpUTF16StringWriter) {
  StringFileWriter file_writer;

  {
    SCOPED_TRACE("unset");
    file_writer.Reset();
    crashpad::internal::MinidumpUTF16StringWriter string_writer;
    EXPECT_TRUE(string_writer.WriteEverything(&file_writer));
    ASSERT_EQ(6u, file_writer.string().size());

    const MINIDUMP_STRING* minidump_string =
        MinidumpStringAtRVA(file_writer.string(), 0);
    EXPECT_TRUE(minidump_string);
    EXPECT_EQ(base::string16(),
              MinidumpStringAtRVAAsString(file_writer.string(), 0));
  }

  const struct {
    size_t input_length;
    const char* input_string;
    size_t output_length;
    base::char16 output_string[10];
  } kTestData[] = {
      {0, "", 0, {}},
      {1, "a", 1, {'a'}},
      {2, "\0b", 2, {0, 'b'}},
      {3, "cde", 3, {'c', 'd', 'e'}},
      {9, "Hi world!", 9, {'H', 'i', ' ', 'w', 'o', 'r', 'l', 'd', '!'}},
      {7, "ret\nurn", 7, {'r', 'e', 't', '\n', 'u', 'r', 'n'}},
      {2, "\303\251", 1, {0x00e9}},  // é

      // oóöőo
      {8, "o\303\263\303\266\305\221o", 5, {'o', 0x00f3, 0x00f6, 0x151, 'o'}},
      {4, "\360\220\204\202", 2, {0xd800, 0xdd02}},  // 𐄂 (non-BMP)
  };

  for (size_t index = 0; index < arraysize(kTestData); ++index) {
    SCOPED_TRACE(base::StringPrintf(
        "index %" PRIuS ", input %s", index, kTestData[index].input_string));

    // Make sure that the expected output string with its NUL terminator fits in
    // the space provided.
    ASSERT_EQ(
        0,
        kTestData[index]
            .output_string[arraysize(kTestData[index].output_string) - 1]);

    file_writer.Reset();
    crashpad::internal::MinidumpUTF16StringWriter string_writer;
    string_writer.SetUTF8(std::string(kTestData[index].input_string,
                                      kTestData[index].input_length));
    EXPECT_TRUE(string_writer.WriteEverything(&file_writer));

    const size_t expected_utf16_units_with_nul =
        kTestData[index].output_length + 1;
    MINIDUMP_STRING tmp = {0};
    ALLOW_UNUSED_LOCAL(tmp);
    const size_t expected_utf16_bytes =
        expected_utf16_units_with_nul * sizeof(tmp.Buffer[0]);
    ASSERT_EQ(sizeof(MINIDUMP_STRING) + expected_utf16_bytes,
              file_writer.string().size());

    const MINIDUMP_STRING* minidump_string =
        MinidumpStringAtRVA(file_writer.string(), 0);
    EXPECT_TRUE(minidump_string);
    base::string16 expect_string = base::string16(
        kTestData[index].output_string, kTestData[index].output_length);
    EXPECT_EQ(expect_string,
              MinidumpStringAtRVAAsString(file_writer.string(), 0));
  }
}

TEST(MinidumpStringWriter, ConvertInvalidUTF8ToUTF16) {
  StringFileWriter file_writer;

  const char* kTestData[] = {
      "\200",  // continuation byte
      "\300",  // start byte followed by EOF
      "\310\177",  // start byte without continuation
      "\340\200",  // EOF in middle of 3-byte sequence
      "\340\200\115",  // invalid 3-byte sequence
      "\303\0\251",  // NUL in middle of valid sequence
  };

  for (size_t index = 0; index < arraysize(kTestData); ++index) {
    SCOPED_TRACE(base::StringPrintf(
        "index %" PRIuS ", input %s", index, kTestData[index]));
    file_writer.Reset();
    crashpad::internal::MinidumpUTF16StringWriter string_writer;
    string_writer.SetUTF8(kTestData[index]);
    EXPECT_TRUE(string_writer.WriteEverything(&file_writer));

    // The requirements for conversion of invalid UTF-8 input are lax. Make sure
    // that at least enough data was written for a string that has one unit and
    // a NUL terminator, make sure that the length field matches the length of
    // data written, and make sure that at least one U+FFFD replacement
    // character was written.
    const MINIDUMP_STRING* minidump_string =
        MinidumpStringAtRVA(file_writer.string(), 0);
    EXPECT_TRUE(minidump_string);
    MINIDUMP_STRING tmp = {0};
    ALLOW_UNUSED_LOCAL(tmp);
    EXPECT_EQ(file_writer.string().size() - sizeof(MINIDUMP_STRING) -
                  sizeof(tmp.Buffer[0]),
              minidump_string->Length);
    base::string16 output_string =
        MinidumpStringAtRVAAsString(file_writer.string(), 0);
    EXPECT_FALSE(output_string.empty());
    EXPECT_NE(base::string16::npos, output_string.find(0xfffd));
  }
}

TEST(MinidumpStringWriter, MinidumpUTF8StringWriter) {
  StringFileWriter file_writer;

  {
    SCOPED_TRACE("unset");
    file_writer.Reset();
    crashpad::internal::MinidumpUTF8StringWriter string_writer;
    EXPECT_TRUE(string_writer.WriteEverything(&file_writer));
    ASSERT_EQ(5u, file_writer.string().size());

    const MinidumpUTF8String* minidump_string =
        MinidumpUTF8StringAtRVA(file_writer.string(), 0);
    EXPECT_TRUE(minidump_string);
    EXPECT_EQ(std::string(),
              MinidumpUTF8StringAtRVAAsString(file_writer.string(), 0));
  }

  const struct {
    size_t length;
    const char* string;
  } kTestData[] = {
      {0, ""},
      {1, "a"},
      {2, "\0b"},
      {3, "cde"},
      {9, "Hi world!"},
      {7, "ret\nurn"},
      {2, "\303\251"},  // é

      // oóöőo
      {8, "o\303\263\303\266\305\221o"},
      {4, "\360\220\204\202"},  // 𐄂 (non-BMP)
  };

  for (size_t index = 0; index < arraysize(kTestData); ++index) {
    SCOPED_TRACE(base::StringPrintf(
        "index %" PRIuS ", input %s", index, kTestData[index].string));

    file_writer.Reset();
    crashpad::internal::MinidumpUTF8StringWriter string_writer;
    std::string test_string(kTestData[index].string, kTestData[index].length);
    string_writer.SetUTF8(test_string);
    EXPECT_EQ(test_string, string_writer.UTF8());
    EXPECT_TRUE(string_writer.WriteEverything(&file_writer));

    const size_t expected_utf8_bytes_with_nul = kTestData[index].length + 1;
    ASSERT_EQ(sizeof(MinidumpUTF8String) + expected_utf8_bytes_with_nul,
              file_writer.string().size());

    const MinidumpUTF8String* minidump_string =
        MinidumpUTF8StringAtRVA(file_writer.string(), 0);
    EXPECT_TRUE(minidump_string);
    EXPECT_EQ(test_string,
              MinidumpUTF8StringAtRVAAsString(file_writer.string(), 0));
  }
}

struct MinidumpUTF16StringListWriterTraits {
  using MinidumpStringListWriterType = MinidumpUTF16StringListWriter;
  static base::string16 ExpectationForUTF8(const std::string& utf8) {
    return base::UTF8ToUTF16(utf8);
  }
  static base::string16 ObservationAtRVA(const std::string& file_contents,
                                         RVA rva) {
    return MinidumpStringAtRVAAsString(file_contents, rva);
  }
};

struct MinidumpUTF8StringListWriterTraits {
  using MinidumpStringListWriterType = MinidumpUTF8StringListWriter;
  static std::string ExpectationForUTF8(const std::string& utf8) {
    return utf8;
  }
  static std::string ObservationAtRVA(const std::string& file_contents,
                                      RVA rva) {
    return MinidumpUTF8StringAtRVAAsString(file_contents, rva);
  }
};

template <typename Traits>
void MinidumpStringListTest() {
  std::vector<std::string> strings;
  strings.push_back(std::string("One"));
  strings.push_back(std::string());
  strings.push_back(std::string("3"));
  strings.push_back(std::string("\360\237\222\251"));

  typename Traits::MinidumpStringListWriterType string_list_writer;
  EXPECT_FALSE(string_list_writer.IsUseful());
  string_list_writer.InitializeFromVector(strings);
  EXPECT_TRUE(string_list_writer.IsUseful());

  StringFileWriter file_writer;

  ASSERT_TRUE(string_list_writer.WriteEverything(&file_writer));

  const MinidumpRVAList* list =
      MinidumpRVAListAtStart(file_writer.string(), strings.size());
  ASSERT_TRUE(list);

  for (size_t index = 0; index < strings.size(); ++index) {
    EXPECT_EQ(Traits::ExpectationForUTF8(strings[index]),
              Traits::ObservationAtRVA(file_writer.string(),
                                       list->children[index]));
  }
}

TEST(MinidumpStringWriter, MinidumpUTF16StringList) {
  MinidumpStringListTest<MinidumpUTF16StringListWriterTraits>();
}

TEST(MinidumpStringWriter, MinidumpUTF8StringList) {
  MinidumpStringListTest<MinidumpUTF8StringListWriterTraits>();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
