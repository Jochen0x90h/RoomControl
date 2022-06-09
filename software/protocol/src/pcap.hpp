#pragma once

#include <cstdint>


namespace pcap {

// data link types, see https://tcpdump.org/linktypes.html
enum class Network : uint32_t {
	// https://wiki.wireshark.org/HowToDissectAnything
	USER0 = 147,
	USER1 = 148,
	USER2 = 149,
	USER3 = 150,
	USER4 = 151,
	USER5 = 152,
	USER6 = 153,
	USER7 = 154,
	USER8 = 155,
	USER9 = 156,
	USER10 = 157,
	USER11 = 158,
	USER12 = 159,
	USER13 = 160,
	USER14 = 161,
	USER15 = 162,

	// IEEE 802.15.4 radio, ZigBee, Thread
	IEEE802_15_4 = 195
};

// header of pcap file
struct Header {
	uint32_t magic_number;   // magic number
	uint16_t version_major;  // major version number
	uint16_t version_minor;  // minor version number
	int32_t thiszone;        // GMT to local correction
	uint32_t sigfigs;        // accuracy of timestamps
	uint32_t snaplen;        // max length of captured packets, in octets
	Network network;        // data link type
};

// header of one packet in a pcap file
struct PacketHeader {
	uint32_t ts_sec;         // timestamp seconds
	uint32_t ts_usec;        // timestamp microseconds
	uint32_t incl_len;       // number of octets of packet saved in file
	uint32_t orig_len;       // actual length of packet
};

template <int N>
struct Packet {
	PacketHeader header;
	uint8_t data[N];
};

} // namespace pcap
