#ifndef PAYLOADS_HPP
#define PAYLOADS_HPP

#include "net/wmnet.hpp"

struct ClientString {
	int client_id;
	std::string client_name;
	std::string str;
};

template<>
struct WMNet::Serializer<ClientString> {
    static std::vector<WMNet::byte> serialize(const WMNet::Message<ClientString>& msg) {

        size_t payload_size = sizeof(int)
            + sizeof(uint16_t) + msg.data.client_name.size()
            + sizeof(uint16_t) + msg.data.str.size();

        std::vector<WMNet::byte> serialized(sizeof(WMNet::NetHeader) + payload_size);

        WMNet::NetHeader header;
        header.id = htons(msg.id);
        header.type = htons(WMNet::MessageType_Custom);
        header.size = htons(static_cast<uint16_t>(payload_size));

        size_t offset = 0;
        std::memcpy(serialized.data(), &header, sizeof(WMNet::NetHeader));
        offset += sizeof(WMNet::NetHeader);

        // client_id
        int net_id = htonl(msg.data.client_id);
        std::memcpy(serialized.data() + offset, &net_id, sizeof(int));
        offset += sizeof(int);

        // client_name with prefix length
        uint16_t name_len = htons(static_cast<uint16_t>(msg.data.client_name.size()));
        std::memcpy(serialized.data() + offset, &name_len, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        std::memcpy(serialized.data() + offset, msg.data.client_name.data(), msg.data.client_name.size());
        offset += msg.data.client_name.size();

        // message with prefix length
        uint16_t msg_len = htons(static_cast<uint16_t>(msg.data.str.size()));
        std::memcpy(serialized.data() + offset, &msg_len, sizeof(uint16_t));
        offset += sizeof(uint16_t);
        std::memcpy(serialized.data() + offset, msg.data.str.data(), msg.data.str.size());

        return serialized;
    }
};

template<>
struct WMNet::DeSerializer<ClientString> {
    static ClientString deserialize(const std::vector<WMNet::byte>& data) {
        ClientString result;
        size_t offset = 0;

        // client_id
        int net_id;
        std::memcpy(&net_id, data.data() + offset, sizeof(int));
        result.client_id = ntohl(net_id);
        offset += sizeof(int);

        // client_name
        uint16_t name_len;
        std::memcpy(&name_len, data.data() + offset, sizeof(uint16_t));
        name_len = ntohs(name_len);
        offset += sizeof(uint16_t);
        result.client_name = std::string(reinterpret_cast<const char*>(data.data() + offset), name_len);
        offset += name_len;

        // message
        uint16_t msg_len;
        std::memcpy(&msg_len, data.data() + offset, sizeof(uint16_t));
        msg_len = ntohs(msg_len);
        offset += sizeof(uint16_t);
        result.str = std::string(reinterpret_cast<const char*>(data.data() + offset), msg_len);

        return result;
    }
};

#endif
