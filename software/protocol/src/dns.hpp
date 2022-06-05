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

	// next secure
	NSEC = 47,
};

// https://datatracker.ietf.org/doc/html/rfc1035#section-3.2.4
enum class Class : uint16_t {
	// internet
	IN = 1,
};

} // namespace dns
