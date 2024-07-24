// (c) gosha20777 (github.com/gosha20777), 2019, MIT License

#include "keras2cpp/utils.h"

namespace keras2cpp {
    Stream::Stream(const std::string& filename)
    : stream_(filename, std::ios::binary) {
        //stream_.exceptions();
        if (!stream_.is_open())
        {
            kassert(false);
        }
    }

    Stream& Stream::reads(char* ptr, size_t count) {
        stream_.read(ptr, static_cast<ptrdiff_t>(count));
        if (!stream_)
        {
            kassert(false);
        }
        return *this;
    }
}
