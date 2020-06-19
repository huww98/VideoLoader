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
    for ([[maybe_unused]]auto &entry : huww::tar_iterator("./tests/tar/test.tar")) {
        break; // Should not have memory leak.
    }
}
