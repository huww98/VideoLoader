#include "tar_iterator.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace huww {

struct tar_header {
    std::array<char, 100> name;
    std::array<char, 8> mode;
    std::array<char, 8> owner;
    std::array<char, 8> group;
    std::array<char, 12> size;
    std::array<char, 12> mtime;
    std::array<char, 8> checksum;
    tar_entry_type type;
    std::array<char, 100> linkname;
    std::array<char, 8> magic;
    std::array<char, 247> _padding;
};
static_assert(sizeof(tar_header) == 512);

constexpr std::array<char, 8> guntar_magic = {'u', 's', 't', 'a', 'r', ' ', ' ', '\0'};

static std::streamsize parse_size(const decltype(tar_header::size) &size) {
    if (size[0] & char(0x80)) {
        // Binary representation
        std::streamsize result;
        if constexpr (sizeof(result) >= sizeof(size)) {
            result = static_cast<std::streamsize>(size[0] & ~char(0x80)) << ((size.size() - 1) * 8);
        } else {
            if (size[0] != char(0x80)) {
                throw std::runtime_error("size too large");
            }
            result = 0;
        }
        for (size_t i = 1; i < size.size() - sizeof(result); i++) {
            if (size[i] != 0) {
                throw std::runtime_error("size too large");
            }
        }
        for (size_t i = size.size() - sizeof(result); i < size.size(); i++) {
            result |= (static_cast<std::streamsize>(size[i]) << ((size.size() - i - 1) * 8));
        }
        return result;
    }

    std::istringstream size_str(std::string(size.data(), size.size()));
    std::streamsize result;
    size_str >> std::oct >> result;
    return result;
}

static std::string parse_path(decltype(tar_header::name) &name) {
    auto name_end = std::find(name.begin(), name.end(), '\0');
    return std::string(name.begin(), name_end);
}

static std::streamoff round_file_record_size(std::streamsize size) {
    constexpr std::streamsize record_size = sizeof(tar_header);
    return ((size + record_size - 1) / record_size) * record_size;
}

class tar_file {
    std::ifstream tar_stream;

    tar_entry _entry;
    std::streampos next_header_pos;

  public:
    bool advance() {
        constexpr auto header_size = sizeof(tar_header);
        std::array<uint8_t, header_size> next_header;
        auto read_header = [this, &next_header] {
            tar_stream.read(reinterpret_cast<char *>(next_header.data()), next_header.size());
            if (tar_stream.eof()) {
                throw std::runtime_error("Unexpected EOF");
            }
            if (!tar_stream) {
                throw std::system_error(errno, std::system_category(),
                                        "Error while reading tar entry header");
            }
        };
        auto header_all_zero = [&next_header] {
            return std::all_of(next_header.cbegin(), next_header.cend(),
                               [](auto c) { return c == 0; });
        };

        bool has_long_pathname = false;
        _entry._file_size = 0;

        while (true) {
            tar_stream.seekg(next_header_pos);
            read_header();
            tar_header &header = *reinterpret_cast<tar_header *>(next_header.data());
            if (header.magic != guntar_magic) {
                if (header_all_zero()) {
                    read_header();
                    if (header_all_zero()) {
                        // Normal end of file
                        return false;
                    }
                }
                throw std::runtime_error("Magic not match. Only GNU Tar format supported.");
            }

            // Verify header checksum
            auto checksum = header.checksum;
            std::fill(header.checksum.begin(), header.checksum.end(), ' ');
            auto calculated_checksum =
                std::accumulate(next_header.begin(), next_header.end(), uint32_t(0));
            std::istringstream checksum_str(std::string(checksum.begin(), checksum.begin() + 6));
            uint32_t parsed_checksum;
            checksum_str >> std::oct >> parsed_checksum;
            if (calculated_checksum != parsed_checksum) {
                throw std::runtime_error("Header checksum mismatch");
            }

            switch (header.type) {
            case tar_entry_type::file:
                _entry._file_size = parse_size(header.size);
                [[fallthrough]];
            case tar_entry_type::directory:
                _entry._type = header.type;
                if (!has_long_pathname) {
                    _entry._path = parse_path(header.name);
                }
                _entry._start_pos = next_header_pos + std::streamoff(header_size);
                next_header_pos = _entry._start_pos + round_file_record_size(_entry._file_size);
                return true;
            case tar_entry_type::long_pathname: {
                auto path_length = parse_size(header.size) - 1; // Cut off last '\0'
                std::string long_path(path_length, '\0');
                tar_stream.read(long_path.data(), path_length);
                _entry._path = long_path;
                has_long_pathname = true;
                next_header_pos +=
                    std::streamoff(header_size) + round_file_record_size(path_length);
                break;
            }

            default:
                std::ostringstream msg;
                msg << "Unsupported entry type " << static_cast<char>(header.type);
                throw std::runtime_error(msg.str());
            }
        }
    }

    tar_file(std::string tar_path)
        : tar_stream(tar_path, std::ifstream::in | std::ifstream::binary), _entry(*this) {
        if (!tar_stream) {
            std::ostringstream msg;
            msg << "Unable to open file \"" << tar_path << "\"";
            throw std::system_error(errno, std::system_category(), msg.str());
        }
    }

    std::ifstream &stream() { return tar_stream; }
    const tar_entry &entry() { return _entry; }
};

std::istream &tar_entry::begin_read_content() {
    if (this->type() != tar_entry_type::file) {
        throw std::logic_error("Can only read content of file entry.");
    }
    return _tar_file.stream().seekg(this->content_start_position());
}

tar_iterator::tar_iterator(std::string tar_path) {
    auto file_ptr = std::make_shared<tar_file>(tar_path);
    if (file_ptr->advance()) {
        this->_tar_file = std::move(file_ptr);
    }
}

const tar_entry &tar_iterator::operator*() const {
    if (!_tar_file) {
        throw std::logic_error("Cannot dereference end tar_iterator");
    }
    return _tar_file->entry();
}

tar_iterator &tar_iterator::operator++() {
    if (!_tar_file) {
        throw std::logic_error("Cannot advance end tar_iterator");
    }
    if (!_tar_file->advance()) {
        _tar_file.reset();
    }
    return *this;
}

} // namespace huww
