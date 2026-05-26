#ifndef WMNET_DESERIALIZE_HPP
#define WMNET_DESERIALIZE_HPP

#include <WinSock2.h>
#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <cstring>

#include "net/internals.hpp"

namespace WMNet {

    template <typename T>
    struct DeSerializer {
        static_assert(always_false_v<T>, "No serializer defined for this type :(");
    };

    template <>
    struct DeSerializer<std::uint32_t> {
        static std::uint32_t deserialize(const std::vector<byte>& data) {
            return ntohl(reinterpret_cast<std::uint32_t>(data.data()));
        }
    };

    template <>
    struct DeSerializer<std::uint64_t> {
        static std::uint64_t deserialize(const std::vector<byte>& data) {
            return ntohll(reinterpret_cast<std::uint64_t>(data.data()));
        }
    };

    template <>
    struct DeSerializer<std::string> {
        static std::string deserialize(const std::vector<byte>& data) {
            return std::string(reinterpret_cast<const char*>(data.data()), data.size());
        }
    };
}

#endif // !WMNET_DESERIALIZE_HPP