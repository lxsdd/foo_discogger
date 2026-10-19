#include "../discogger_artwork_noop.h"
#include <cassert>
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

using discogger_write_safety::artwork_bytes_equal;
using discogger_write_safety::artwork_stream_matches;
int main() {
    const std::string a = "jpeg data alpha";
    const std::string b = "jpeg data beta!";
    assert(artwork_bytes_equal(a.data(),a.size(),a.data(),a.size()));
    assert(!artwork_bytes_equal(a.data(),a.size(),b.data(),b.size()));
    assert(!artwork_bytes_equal(nullptr,a.size(),a.data(),a.size()));
    assert(!artwork_bytes_equal(a.data(),a.size(),nullptr,a.size()));
    assert(!artwork_bytes_equal(a.data(),0,a.data(),0));
    assert(!artwork_bytes_equal(a.data(),a.size()-1,a.data(),a.size()));
    std::size_t offset = 0, calls = 0;
    auto read = [&](void* dst, std::size_t sz) -> std::size_t {
        ++calls;
        std::memcpy(dst, a.data()+offset, sz);
        offset += sz;
        return sz;
    };
    assert(artwork_stream_matches(a.size(),a.data(),a.size(),read));
    assert(offset == a.size() && calls > 0);
    calls = 0;
    assert(!artwork_stream_matches(a.size()+1,a.data(),a.size(),read));
    assert(calls == 0);
    // Same-length but different on-disk bytes must not pass as a no-op.
    assert(b.size() == a.size());
    offset = 0;
    auto readDifferent = [&](void* dst, std::size_t sz)->std::size_t {
        std::memcpy(dst, b.data() + offset, sz); offset += sz; return sz;
    };
    assert(!artwork_stream_matches(b.size(),a.data(),a.size(),readDifferent));
    std::vector<unsigned char> big(64000);
    for (std::size_t i=0;i<big.size();++i) big[i] = static_cast<unsigned char>(i%251);
    offset = calls = 0;
    auto readBig = [&](void* dest, std::size_t n)->std::size_t {
        ++calls; std::memcpy(dest,big.data()+offset,n); offset+=n; return n;
    };
    assert(artwork_stream_matches(big.size(),big.data(),big.size(),readBig));
    assert(offset==big.size() && calls>=3);
    bool threw=false;
    try { artwork_stream_matches(a.size(),a.data(),a.size(),
        [](void*,std::size_t n)->std::size_t { return n-1; }); }
    catch(const std::runtime_error&) { threw=true; }
    assert(threw);
    // The decision must not invoke any writer for identical embedded/sidecar bytes.
    int writes=0;
    if (!artwork_bytes_equal(a.data(),a.size(),a.data(),a.size())) ++writes;
    assert(writes==0);
    if (!artwork_bytes_equal(a.data(),a.size(),b.data(),b.size())) ++writes;
    assert(writes==1);
}
