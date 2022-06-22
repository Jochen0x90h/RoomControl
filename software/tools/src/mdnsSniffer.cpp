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

using Packet = pcap::Packet<4096>;

class DnsReader : public MessageReader {
public:

	DnsReader(int length, uint8_t *message) : MessageReader(length, message), begin(message) {}
	DnsReader(DnsReader &r, int offset) : MessageReader(r.begin + offset, r.end), begin(r.begin) {}

	uint8_t *begin;
};

void handleName(DnsReader &r, bool first = true) {
	while (!r.atEnd()) {
		int len = r.u8();

		// check for zero termination
		if (len == 0)
			break;

		// check for pointer
		if ((len & 0xc0) == 0xc0) {
			int offset = ((len & 0x3f) << 8) | r.u8();
			DnsReader r2(r, offset);
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

void handleAddress6(DnsReader &r) {
	for (int i = 0; i < 8; ++i) {
		if (i != 0)
			Terminal::out << ':';
		Terminal::out << hex(r.u16B());
	}
}

void handleServer(DnsReader &r) {
	auto priority = r.u16B();
	auto weight = r.u16B();
	auto port = r.u16B();
	Terminal::out << "priority=" << dec(priority) << " weight=" << dec(weight) << " port=" << dec(port) << ' ';

	handleName(r);
}

void printTypeAndFlags2(dns::Type type, dns::Flags2 flags2) {
	switch (type) {
	case dns::Type::A:
		Terminal::out << " A ";
		break;
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
	case dns::Type::OPT:
		Terminal::out << " OPT ";
		break;
	case dns::Type::NSEC:
		Terminal::out << " NSEC ";
		break;
	case dns::Type::ANY:
		Terminal::out << " ANY ";
		break;
	default:
		Terminal::out << " Unknown";
	}
	if ((flags2 & dns::Flags2::FLUSH) != 0)
		Terminal::out << "flush ";
}

// handle resource record (RR)
bool handleResourceRecord(DnsReader &r) {
	// domain name
	handleName(r);

	// type, class, ttl and length
	if (r.getRemaining() < 2 + 2 + 4 + 2) {
		Terminal::out << " Unexpected End";
		return false;
	}
	auto type = r.e16B<dns::Type>();
	auto flags2 = r.e16B<dns::Flags2>();
	printTypeAndFlags2(type, flags2);
	auto ttl = r.u32B();
	auto length = r.u16B();

	// type dependent data
	if (r.getRemaining() < length) {
		Terminal::out << " Unexpected End";
		return false;
	}
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
    default:;
	}
	r.current = end;
	return true;
}

void handleDns(DnsReader &r) {
	uint16_t transactionId = r.u16B();
	auto flags = r.e16B<dns::Flags>();
	int questionCount = r.u16B();
	int answerCount = r.u16B();
	int authorityCount = r.u16B();
	int additionalCount = r.u16B();

	bool isQuery = (flags & dns::Flags::QR_MASK) == dns::Flags::QUERY;
	Terminal::out << (isQuery ? "Query" : "Response") << '\n';

	Terminal::out << "\tquestionCount " << dec(questionCount) << '\n';
	for (int i = 0; i < questionCount; ++i) {
		Terminal::out << "\t\t";

		// domain name
		handleName(r);

		// type, class, ttl and length
		if (r.getRemaining() < 2 + 2) {
			Terminal::out << " Unexpected End";
			return;
		}

		auto type = r.e16B<dns::Type>();
		auto flags2 = r.e16B<dns::Flags2>();
		printTypeAndFlags2(type, flags2);
		Terminal::out << '\n';
	}

	Terminal::out << "\tanswerCount " << dec(answerCount) << '\n';
	for (int i = 0; i < answerCount; ++i) {
		Terminal::out << "\t\t";
		bool ok = handleResourceRecord(r);
		Terminal::out << '\n';
		if (!ok)
			return;
	}

	Terminal::out << "\tauthorityCount " << dec(authorityCount) << '\n';
	for (int i = 0; i < authorityCount; ++i) {
		Terminal::out << "\t\t";
		bool ok = handleResourceRecord(r);
		Terminal::out << '\n';
		if (!ok)
			return;
	}

	Terminal::out << "\tadditionalCount " << dec(additionalCount) << '\n';
	for (int i = 0; i < additionalCount; ++i) {
		Terminal::out << "\t\t";
		bool ok = handleResourceRecord(r);
		Terminal::out << '\n';
		if (!ok)
			return;
	}
}


Coroutine sniffer(FILE *file) {
	Network::open(0, port);

	// join multicast group
	auto address = Network::Address::fromString("ff02::fb");
	Network::join(0, address);

	while (true) {

		while (true) {
			Network::Endpoint source;
			Packet packet;
			int length = sizeof(packet.data);
			co_await Network::receive(0, source, length, packet.data);
			Terminal::out << "from " << dec(source.port) << ' ';

			packet.header.ts_sec = 0;
			packet.header.ts_usec = 0;
			packet.header.incl_len = length;
			packet.header.orig_len = length;
			fwrite(&packet, 1, sizeof(pcap::PacketHeader) + packet.header.incl_len, file);

			// flush file so that we can interrupt any time
			fflush(file);

			// print hex
			//for (int i = 0; i < length; ++i)
			//	Terminal::out << hex(packet.data[i]);
			//Terminal::out << '\n';

			// dissect
			DnsReader r(length, packet.data);
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

		if (arg == "-i" || arg == "--input") {
			// input pcap file
			++i;
			inputFile = argv[i];
		} else {
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

			Terminal::out << "waiting for mDNS packets on port 5353 ...\n";
			Loop::run();
		}
	} else {
		// read from file

		// open input pcap file
		std::string name = inputFile.string();
		FILE *file = fopen(name.c_str(), "rb");
		if (file == nullptr) {
			Terminal::err << "error: can't read pcap file " << str(name.c_str()) << "\n";
		} else {
			// read pcap header
			pcap::Header header;
			if (fread(&header, sizeof(header), 1, file) < 1) {
				Terminal::err << "error: pcap header incomplete!\n";
			} else if (header.network != pcap::Network::USER0) {
				Terminal::err << "error: protocol not supported!\n";
			} else {
				// read packets
				Packet packet;
				while (fread(&packet.header, sizeof(pcap::PacketHeader), 1, file) == 1) {
					// read packet data
					int len = min(packet.header.incl_len, int(sizeof(packet.data)));
					if (fread(packet.data, 1, len, file) < packet.header.incl_len)
						break;

					// dissect packet
					DnsReader r(packet.header.incl_len, packet.data);
					handleDns(r);
				}
			}
			fclose(file);
		}
	}

	return 0;
}
