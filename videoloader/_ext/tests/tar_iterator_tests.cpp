#include <gtest/gtest.h>

#include <deque>

#include "tar_iterator.h"

struct expected_tar_entry {
    std::string path;
    huww::tar_entry_type type;
    std::streampos start_pos;
    std::streamsize file_size;
};

TEST(TarIterator, Iterate) {
    std::deque<expected_tar_entry> expected_entries = {
        {"testtar/", huww::tar_entry_type::directory, 512, 0},
        {"testtar/testfile.txt", huww::tar_entry_type::file, 1024, 14},
        {"testtar/zero-length-file.txt", huww::tar_entry_type::file, 2048, 0},
        {"testtar/testfile1.txt", huww::tar_entry_type::file, 2560, 14},
        {"testtar/This-is-a-file-with-path-name-length-equals-to-100-bytes-loooooooooooooooooooong"
         "-exactly.txt",
         huww::tar_entry_type::file, 3584, 11},
        {"testtar/This-is-a-file-with-very-"
         "loooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
         "ooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo"
         "oooooong-file-name.txt",
         huww::tar_entry_type::file, 5632, 13},
    };
    for (auto &entry : huww::tar_iterator("./tests/tar/test.tar")) {
        EXPECT_GT(expected_entries.size(), 0);
        auto &expected = *expected_entries.begin();
        EXPECT_EQ(expected.path, entry.path());
        EXPECT_EQ(expected.type, entry.type());
        EXPECT_EQ(expected.start_pos, entry.content_start_position());
        EXPECT_EQ(expected.file_size, entry.file_size());
        expected_entries.pop_front();
    }
    EXPECT_EQ(expected_entries.size(), 0);
}

TEST(TarIterator, IterateBreak) {
    for ([[maybe_unused]] auto &entry : huww::tar_iterator("./tests/tar/test.tar")) {
        break; // Should not have memory leak.
    }
}

TEST(TarIterator, ReadContent) {
    for (auto &entry : huww::tar_iterator("./tests/tar/test.tar")) {
        if (entry.path() == "testtar/testfile.txt") {
            auto &stream = entry.begin_read_content();
            std::string content(entry.file_size(), '\0');
            stream.read(content.data(), entry.file_size());
            EXPECT_EQ("1234567890aaa\n", content);
            return;
        }
    }
    FAIL() << "File not found in tar";
}

TEST(TarIterator, BigFile) {
    for (auto &entry : huww::tar_iterator("./tests/tar/bigzero.tar.head")) {
        EXPECT_EQ("bigzero", entry.path());
        EXPECT_EQ(std::streamsize(1) << 34, entry.file_size()); // 16GiB
        break;
    }
}

class TarFileTooLarge : public ::testing::TestWithParam<std::string> {};

TEST_P(TarFileTooLarge, Throws) {
    if constexpr (sizeof(std::streamsize) > 8) {
        GTEST_SKIP();
    }
    try {
        for ([[maybe_unused]] auto &entry : huww::tar_iterator(GetParam())) {
            FAIL();
        }
        FAIL();
    } catch (std::runtime_error &e) {
        EXPECT_STREQ("size too large", e.what());
    }
}

INSTANTIATE_TEST_SUITE_P(Inst, TarFileTooLarge,
                         ::testing::Values("tests/tar/too-large.tar.head",
                                           "tests/tar/too-large2.tar.head"));

TEST(TarWrongChecksum, Throws) {
    try {
        for ([[maybe_unused]] auto &entry : huww::tar_iterator("tests/tar/checksum-error.tar")) {
            FAIL();
        }
        FAIL();
    } catch (std::runtime_error &e) {
        EXPECT_STREQ("Header checksum mismatch", e.what());
    }
}

TEST(TarEOF, Throws) {
    try {
        for ([[maybe_unused]] auto &entry : huww::tar_iterator("tests/tar/premature-eof.tar")) {
        }
        FAIL();
    } catch (std::runtime_error &e) {
        EXPECT_STREQ("Unexpected EOF", e.what());
    }
}

TEST(TarPaxFormat, Throws) {
    try {
        for ([[maybe_unused]] auto &entry : huww::tar_iterator("tests/tar/pax.tar")) {
            FAIL();
        }
        FAIL();
    } catch (std::runtime_error &e) {
        EXPECT_STREQ("Magic not match. Only GNU Tar format supported.", e.what());
    }
}

TEST(TarReadDirContent, Throws) {
    try {
        for (auto &entry : huww::tar_iterator("./tests/tar/test.tar")) {
            ASSERT_EQ("testtar/", entry.path());
            entry.begin_read_content();
            FAIL();
        }
        FAIL();
    } catch (std::logic_error &e) {
        EXPECT_STREQ("Can only read content of file entry.", e.what());
    }
}
