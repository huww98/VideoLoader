#pragma once

#include <filesystem>
#include <istream>
#include <string>

namespace huww {

class tar_file;

enum class tar_entry_type : uint8_t {
    file = '0',
    hard_link = '1',
    symbolic_link = '2',
    character_device = '3',
    block_device = '4',
    directory = '5',
    fifo = '6',
    reversed = '7',
    directory_entries = 'D',
    long_linkname = 'K',
    long_pathname = 'L',
    continued = 'M',
};

class tar_entry {
    tar_file &_tar_file;

    std::string _path;
    std::streamsize _file_size;
    std::streampos _start_pos;
    tar_entry_type _type;

    friend class tar_iterator;
    friend class tar_file;

    tar_entry(tar_file &file) : _tar_file(file), _path() {}

  public:
    auto path() const { return _path; }
    auto content_start_position() const { return _start_pos; }
    auto file_size() const { return _file_size; }
    auto type() const { return _type; }

    /**
     * Return a stream that is positioned at the begin of this file. You can read up to
     * `file_size()` from this stream.
     *
     * \note You can only use this while the `tar_iterator` is still in scope. If not, please open
     * the tar file yourself and use `content_start_position()` to seek to this file.
     */
    std::istream &begin_read_content() const;

    /**
     * Call `posix_fadvise(..., POSIX_FADV_WILLNEED)` on supported platform.
     *
     * Encourage OS to read sequentially. But only 128KB data is fetched on my platform. Try also
     * `prefetch_content()` to see which is more suitable.
     */
    void will_need_content() const;

    /**
     * Read all content from this entry and ignore them.
     *
     * This will populate OS cache so that subsequent read (maybe from other thread) will be faster,
     * and will not break sequential read pattern. Eventually， this may improve overall IO
     * throughput dramatically on HDD.
     */
    void prefetch_content() const;
};

enum class tar_options : uint8_t {
    none = 0,
    advise_sequential = 1,
};

constexpr tar_options operator&(tar_options x, tar_options y) noexcept {
    using utype = typename std::underlying_type<tar_options>::type;
    return static_cast<tar_options>(static_cast<utype>(x) & static_cast<utype>(y));
}

constexpr tar_options operator|(tar_options x, tar_options y) noexcept {
    using utype = typename std::underlying_type<tar_options>::type;
    return static_cast<tar_options>(static_cast<utype>(x) | static_cast<utype>(y));
}

class tar_iterator {
    std::shared_ptr<tar_file> _tar_file;

  public:
    tar_iterator() = default;
    explicit tar_iterator(std::string file_path);
    tar_iterator(std::string file_path, tar_options options);

    const tar_entry &operator*() const;
    const tar_entry *operator->() const { return &**this; }
    tar_iterator &operator++();

    friend bool operator==(const tar_iterator &lhs, const tar_iterator &rhs) noexcept {
        return lhs._tar_file == rhs._tar_file;
    }
    friend bool operator!=(const tar_iterator &lhs, const tar_iterator &rhs) noexcept {
        return !(lhs == rhs);
    }
};

inline tar_iterator begin(tar_iterator iter) noexcept { return iter; }
inline tar_iterator end(const tar_iterator &iter) noexcept { return {}; }

} // namespace huww
