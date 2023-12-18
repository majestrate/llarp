#pragma once 

namespace llarp::net 
{
    struct IPPacket{};

    enum class IPProto : uint8_t
    {
        TCP = 6,
        UDP = 17,
        ICMP = 1,
        Unknown = 0
    };

    IPProto get_ip_proto_by_name(std::string_view name);
    std::string ip_proto_to_string(IPProto proto);
}