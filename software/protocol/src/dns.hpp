#pragma once

#include <enum.hpp>


namespace dns {

// https://datatracker.ietf.org/doc/html/rfc1035#section-4.1.1
enum class Flags : uint16_t {
	QUERY = 0,
	RESPONSE = 0x8000,
	QR_MASK = 0x8000,

	OPCODE_QUERY = 0,
	OPCODE_INVERSE_QUERY = 1,
	OPCODE_STATUS = 2,
	OPCODE_MASK = 0x7800,

	AUTHORITATIVE_ANSWER = 0x0400,

	TRUNCATED = 0x0200,

	RECURSION_DESIRED = 0x0100,
	RECURSION_AVAILABLE = 0x0080,

	RCODE_SUCCESS = 0,
	RCODE_MASK = 0x000f
};
FLAGS_ENUM(Flags)

// https://datatracker.ietf.org/doc/html/rfc1035#section-3.2.2
enum class Type : uint16_t {
	// ipv4 address
	A = 1,

	// domain name pointer
	PTR = 12,

	// text string
	TXT = 16,

	// ipv6 address
	AAAA = 28,

	// server selection
	SRV = 33,

	// ?
	OPT = 41,

	// next secure
	NSEC = 47,

	// request for all records the server/cache has available
	ANY = 255,
};

// https://datatracker.ietf.org/doc/html/rfc1035#section-3.2.4
enum class Flags2 : uint16_t {
	// internet
	CLASS_IN = 1,
	CLASS_MASK = 0x7fff,

	// query: unicast-response desired, resource record: cache-flush of outdated records
	// https://en.wikipedia.org/wiki/Multicast_DNS
	UNICAST = 0x8000,
	FLUSH = 0x8000
};
FLAGS_ENUM(Flags2)

} // namespace dns
