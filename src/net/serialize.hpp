#ifndef WMNET_SERIALIZE_HPP
#define WMNET_SERIALIZE_HPP

#include <WinSock2.h>
#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <cstring>

#include "net/internals.hpp"
#include "net/message.hpp"

namespace WMNet {

    template <typename T>
    struct Serializer {
        static_assert(always_false_v<T>, "No serializer defined for this type :(");
    };

    template <>
    struct Serializer<int> {
        static std::vector<byte> serialize(const Message<int>& msg) {
            std::vector<byte> serialized(sizeof(NetHeader) + sizeof(int));

            NetHeader header;
            header.id = htons(msg.id);
            header.type = htons(MessageType_UInt32);
            header.size = htons(static_cast<msg_size_t>(sizeof(int)));

            std::memcpy(serialized.data(), &header, sizeof(NetHeader));

            // Data
            int net_int = htonl(msg.data);
            std::memcpy(serialized.data() + sizeof(NetHeader), &net_int, sizeof(int));

            return serialized;
        }
    };

    template <>
    struct Serializer<uint32_t> {
        static std::vector<byte> serialize(const Message<uint32_t>& msg) {
            std::vector<byte> serialized(sizeof(NetHeader) + sizeof(std::uint32_t));

            NetHeader header;
            header.id = htons(msg.id);
            header.type = htons(MessageType_UInt32);
            header.size = htons(static_cast<msg_size_t>(sizeof(std::uint32_t)));

            std::memcpy(serialized.data(), &header, sizeof(NetHeader));

            // Data
            std::uint32_t net_int = htonl(static_cast<std::uint32_t>(msg.data));
            std::memcpy(serialized.data() + sizeof(NetHeader), &net_int, sizeof(std::uint32_t));

            return serialized;
        }
    };

    template <>
    struct Serializer<uint64_t> {
        static std::vector<byte> serialize(const Message<uint64_t>& msg) {
            std::vector<byte> serialized(sizeof(NetHeader) + sizeof(std::uint64_t));

            NetHeader header;
            header.id = htons(msg.id);
            header.type = htons(MessageType_UInt64);
            header.size = htons(static_cast<msg_size_t>(sizeof(std::uint64_t)));

            std::memcpy(serialized.data(), &header, sizeof(NetHeader));

            // Data
            std::uint64_t net_int = htonll(static_cast<std::uint64_t>(msg.data));
            std::memcpy(serialized.data() + sizeof(NetHeader), &net_int, sizeof(std::uint64_t));

            return serialized;
        }
    };

    template <>
    struct Serializer<std::string> {
        static std::vector<byte> serialize(const Message<std::string>& msg) {
            if (msg.data.size() > 0xFFFF) {
                throw std::length_error("String payload exceeds maximum 16-bit size limit (65535 bytes).");
            }

            std::vector<byte> serialized(sizeof(NetHeader) + msg.data.size());

            NetHeader header;
            header.id = htons(msg.id);
            header.type = htons(MessageType_String);
            header.size = htons(static_cast<msg_size_t>(msg.data.size()));

            std::memcpy(serialized.data(), &header, sizeof(NetHeader));

            // Data
            std::memcpy(serialized.data() + sizeof(NetHeader), msg.data.data(), msg.data.size());

            return serialized;
        }
    };

    template <>
    struct Serializer<Empty> {
        static std::vector<byte> serialize(const Message<Empty>& msg) {
            std::vector<byte> serialized(sizeof(NetHeader));

            NetHeader header;
            header.id = htons(msg.id);
            header.type = htons(MessageType_Empty);
            header.size = 0;

            std::memcpy(serialized.data(), &header, sizeof(NetHeader));

            return serialized;
        }
    };
}

#endif // !WMNET_SERIALIZE_HPP