#include <Loop.hpp>
#include <Network.hpp>
#include <Terminal.hpp>
#include <pcap.hpp>
#include <dns.hpp>
#include <StringOperators.hpp>
#include <Coroutine.hpp>
#include <filesystem>
#include <cstdio>
#include "MessageReader.hpp"

namespace fs = std::filesystem;

uint16_t port = 5353;

class PacketReader : public MessageReader {
public:

	PacketReader(int length, uint8_t *message) : MessageReader(length, message), begin(message) {}
	PacketReader(PacketReader &r, int offset) : MessageReader(r.begin + offset, r.end), begin(r.begin) {}

	uint8_t *begin;
};

void handleName(PacketReader &r, bool first = true) {
	while (true) {
		// check if at end of message
		if (r.atEnd())
			break;
		int len = r.u8();

		// check for zero termination
		if (len == 0)
			break;

		// check if it is a pointer
		if ((len & 0xc0) == 0xc0) {
			int offset = ((len & 0x3f) << 8) | r.u8();
			PacketReader r2(r, offset);
			handleName(r2, first);
			break;
		}

		// read string
		String s = r.string(len);
		if (!first)
			Terminal::out << '.';
		Terminal::out << s;
		first = false;
	}
}

void handleAddress6(PacketReader &r) {
	for (int i = 0; i < 8; ++i) {
		if (i != 0)
			Terminal::out << ':';
		Terminal::out << hex(r.u16B());
	}
}

void handleServer(PacketReader &r) {
	uint16_t priority = r.u16B();
	uint16_t weight = r.u16B();
	uint16_t port = r.u16B();
	Terminal::out << "priority=" << dec(priority) << " weight=" << dec(weight) << " port=" << dec(port) << ' ';

	handleName(r);
}

dns::Type handleNameAndType(PacketReader &r) {
	// domain name
	handleName(r);

	// type and class
	auto type = r.e16B<dns::Type>();
	auto clazz = r.e16B<dns::Class>();
	switch (type) {
	case dns::Type::PTR:
		Terminal::out << " PTR ";
		break;
	case dns::Type::TXT:
		Terminal::out << " TXT ";
		break;
	case dns::Type::AAAA:
		Terminal::out << " AAAA ";
		break;
	case dns::Type::SRV:
		Terminal::out << " SRV ";
		break;
	default:
		Terminal::out << " Unknown";
	}
	return type;
}

void handleRr(PacketReader &r) {
	auto type = handleNameAndType(r);

	uint32_t ttl = r.u32B();
	uint16_t length = r.u16B();

	auto end = r.current + length;
	switch (type) {
	case dns::Type::PTR:
		handleName(r);
		break;
	case dns::Type::AAAA:
		handleAddress6(r);
		break;
	case dns::Type::SRV:
		handleServer(r);
		break;
	case dns::Type::TXT:
		handleName(r);
		break;
	}
	r.current = end;
}

void handleDns(PacketReader &r) {
	uint16_t transactionId = r.u16B();
	auto flags = r.e16B<dns::Flags>();
	int questionCount = r.u16B();
	int answerRRs = r.u16B();
	int authorityRRs = r.u16B();
	int additionalRRs = r.u16B();

	bool isQuery = (flags & dns::Flags::QR_MASK) == dns::Flags::QUERY;
	Terminal::out << (isQuery ? 'Q' : 'R') << '\n';

	for (int i = 0; i < questionCount; ++i) {
		Terminal::out << "Q ";
		handleNameAndType(r);
		Terminal::out << '\n';
	}
	for (int i = 0; i < answerRRs; ++i) {
		Terminal::out << "A ";
		handleRr(r);
		Terminal::out << '\n';
	}
	for (int i = 0; i < additionalRRs; ++i) {
		Terminal::out << "a ";
		handleRr(r);
		Terminal::out << '\n';
	}
}


Coroutine sniffer(FILE *file) {
	Network::open(0, port);

	while (true) {
		Network::Endpoint source;

		while (true) {
			pcap::Packet<1024> packet;
			int length = 1024;
			co_await Network::receive(0, source, length, packet.data);

			packet.header.ts_sec = 0;
			packet.header.ts_usec = 0;
			packet.header.incl_len = length;
			packet.header.orig_len = length;
			fwrite(&packet, 1, sizeof(pcap::PacketHeader) + packet.header.incl_len, file);

			// flush file so that we can interrupt any time
			fflush(file);

			// print hex
			for (int i = 0; i < length; ++i)
				Terminal::out << hex(packet.data[i]);
			Terminal::out << '\n';

			// dissect
			PacketReader r(length, packet.data);
			handleDns(r);

		}
	}
}

/**
 * See https://wiki.wireshark.org/HowToDissectAnything to dissect DNS application layer in Wireshark
 */
int main(int argc, char const *argv[]) {
	Loop::init();
	Network::init();
	Terminal::init();

	fs::path inputFile;
	fs::path outputFile;
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];

		{
			// output pcap file
			if (arg == "-o" )
				++i;
			outputFile = argv[i];
		}
	}

	if (inputFile.empty()) {
		// open output pcap file
		FILE *file = fopen(outputFile.string().c_str(), "wb");

		// write pcap header
		if (file != nullptr) {
			Terminal::out << "capturing to " << str(outputFile.string().c_str()) << "\n";
			pcap::Header header;
			header.magic_number = 0xa1b2c3d4;
			header.version_major = 2;
			header.version_minor = 4;
			header.thiszone = 0;
			header.sigfigs = 0;
			header.snaplen = 128;
			header.network = pcap::Network::USER0;
			fwrite(&header, sizeof(header), 1, file);

			sniffer(file);

			Loop::run();
		}
	}

	return 0;
}
