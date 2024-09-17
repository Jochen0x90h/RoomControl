#include <Ble.hpp>
#include <Loop.hpp>
#include <Terminal.hpp>
#include <Debug.hpp>
#include <MessageReader.hpp>
#include <MessageWriter.hpp>
#include <bt.hpp>
#include <StringOperators.hpp>
#include <util.hpp>


using namespace bt;

struct Characteristic {
	uint16_t handle;
	bt::gatt::CharacteristicProperties properties;
	uint16_t valueHandle;
	Ble::Uuid uuid;
};

struct Service {
	uint16_t handle;
	uint16_t end;
	Ble::Uuid uuid;

	int characteristicCount = 0;
	Characteristic characteristics[32];
};

Service services[32];

void printUuid(Ble::Uuid const &uuid) {
	for (int i = 15; i >= 12; --i)
		Terminal::out << hex(uuid.u8[i]);
	Terminal::out << '-';
	for (int i = 11; i >= 10; --i)
		Terminal::out << hex(uuid.u8[i]);
	Terminal::out << '-';
	for (int i = 9; i >= 8; --i)
		Terminal::out << hex(uuid.u8[i]);
	Terminal::out << '-';
	for (int i = 7; i >= 6; --i)
		Terminal::out << hex(uuid.u8[i]);
	Terminal::out << '-';
	for (int i = 5; i >= 0; --i)
		Terminal::out << hex(uuid.u8[i]);
}

// 932c32bd-XXXX-47a2-835a-a8d455b859dd
bool isHue(Ble::Uuid const &uuid) {
	return uuid.u32[0] == u32L(0x55b859dd) && uuid.u32[1] == u32L(0x835aa8d4)
		&& uuid.u16[4] == u16L(0x47a2) && uuid.u32[3] == u32L(0x932c32bd);
}

void printProperties(gatt::CharacteristicProperties properties) {
	Terminal::out << ((properties & gatt::CharacteristicProperties::BROADCAST) != 0 ? 'B' : '-');
	Terminal::out << ((properties & gatt::CharacteristicProperties::READ) != 0 ? 'R' : '-');
	Terminal::out << ((properties & gatt::CharacteristicProperties::WRITE_WITHOUT_RESPONSE) != 0 ? 'w' : '-');
	Terminal::out << ((properties & gatt::CharacteristicProperties::WRITE) != 0 ? 'W' : '-');
	Terminal::out << ((properties & gatt::CharacteristicProperties::NOTIFY) != 0 ? 'N' : '-');
	Terminal::out << ((properties & gatt::CharacteristicProperties::INDICATE) != 0 ? 'I' : '-');
	Terminal::out << ((properties & gatt::CharacteristicProperties::AUTHENTHICATED_SIGNED_WRITES) != 0 ? 'A' : '-');
	Terminal::out << ((properties & gatt::CharacteristicProperties::EXTENDED_PROPERTIES) != 0 ? 'E' : '-');
}

Coroutine scan() {
	while (true) {
		Ble::ScanResult result;
		co_await Ble::scan(result);

		Terminal::out << hex(result.address.u8[5]) << ':' << hex(result.address.u8[4]) << ':'
			<< hex(result.address.u8[3]) << ':' << hex(result.address.u8[2]) << ':' << hex(result.address.u8[1]) << ':'
			<< hex(result.address.u8[0]) << '\n';

		debug::toggleBlue();


		Ble::open(0, result.address);

		uint8_t buffer[32];

		// exchange MTU
		{
			MessageWriter w(buffer);
			w.e8(att::Op::EXCHANGE_MTU_REQ);
			w.u16L(515); // https://stackoverflow.com/questions/63590616/what-is-the-maximum-att-mtu-allowed-by-bluetooth-le
			co_await Ble::send(0, w.getLength(), buffer);

			while (true) {
				int length = array::count(buffer);
				co_await Ble::receive(0, length, buffer);
				if (length >= 3) {
					MessageReader r(length, buffer);
					auto op = r.e8<att::Op>();
					if (op == att::Op::EXCHANGE_MTU_RSP) {
						int mtu = r.u16L();
						Terminal::out << "server mtu: " << dec(mtu) << '\n';
						break;
					} else if (op == att::Op::EXCHANGE_MTU_REQ) {
						MessageWriter w(buffer);
						w.e8(att::Op::EXCHANGE_MTU_RSP);
						w.u16L(515); // https://stackoverflow.com/questions/63590616/what-is-the-maximum-att-mtu-allowed-by-bluetooth-le
						co_await Ble::send(0, w.getLength(), buffer);
					}
				}
			}
		}

		int serviceCount = 0;
		uint16_t nextHandle = 1;
		while (true) {
			MessageWriter w(buffer);
			w.e8(att::Op::READ_BY_GROUP_TYPE_REQ);
			w.u16L(nextHandle);
			w.u16L(0xffff);
			w.e16L(gatt::Type::PRIMARY_SERVICE);
			co_await Ble::send(0, w.getLength(), buffer);

			int length = array::count(buffer);
			co_await Ble::receive(0, length, buffer);
			if (length >= 1) {
				MessageReader r(length, buffer);
				auto op = r.e8<att::Op>();
				if (op == att::Op::ERROR_RSP) {
					break;
				} else if (op == att::Op::READ_BY_GROUP_TYPE_RSP && length >= 2) {
					int elementSize = r.u8();
					int elementCount = r.getRemaining() / elementSize;
					for (int i = 0; i < elementCount; ++i) {
						auto &service = services[serviceCount++];

						service.handle = r.u16L();
						service.end = r.u16L();
						int uuidLength = elementSize - 4;
						if (uuidLength <= 2)
							r.data8(uuidLength, service.uuid.setShort());
						else
							r.data8(uuidLength, service.uuid.u8);

						Terminal::out << hex(service.handle) << ' ' << hex(service.end) << ' ';
						printUuid(service.uuid);
						Terminal::out << '\n';

						nextHandle = service.end + 1;
					}
					if (elementCount == 0 || nextHandle == 0)
						break;
				} else {
					break;
				}
			}
		}

		for (int serviceIndex = 0; serviceIndex < serviceCount; ++serviceIndex) {
			auto &service = services[serviceIndex];
			Terminal::out << "service " << hex(service.handle) << '\n';

			nextHandle = service.handle;

			while (true) {
				MessageWriter w(buffer);
				w.e8(att::Op::READ_BY_TYPE_REQ);
				w.u16L(nextHandle);
				w.u16L(service.end);
				w.e16L(gatt::Type::CHARACTERISTIC);
				co_await Ble::send(0, w.getLength(), buffer);

				int length = array::count(buffer);
				co_await Ble::receive(0, length, buffer);
				if (length >= 1) {
					MessageReader r(length, buffer);
					auto op = r.e8<att::Op>();
					if (op == att::Op::ERROR_RSP) {
						break;
					} else if (op == att::Op::READ_BY_TYPE_RSP && length >= 2) {
						int elementSize = r.u8();
						int elementCount = r.getRemaining() / elementSize;
						for (int i = 0; i < elementCount; ++i) {
							auto &characteristic = service.characteristics[service.characteristicCount++];

							characteristic.handle = r.u16L();
							characteristic.properties = r.e8<gatt::CharacteristicProperties>();
							characteristic.valueHandle = r.u16L();
							int uuidLength = elementSize - 5;
							if (uuidLength <= 2)
								r.data8(uuidLength, characteristic.uuid.setShort());
							else
								r.data8(uuidLength, characteristic.uuid.u8);

							Terminal::out << hex(characteristic.handle) << ' ';
							printProperties(characteristic.properties);
							Terminal::out << ' ' << hex(characteristic.valueHandle) << ' ';
							printUuid(characteristic.uuid);
							//Terminal::out << '(' << dec(isHue(characteristic.uuid)) << ')';
							Terminal::out << '\n';

							if (isHue(characteristic.uuid) && characteristic.uuid.u16[5] == u16L(0x0002)) {
								MessageWriter w(buffer);
								w.e8(att::Op::WRITE_REQ);
								w.u16L(characteristic.valueHandle);
								w.u8(0);
								co_await Ble::send(0, w.getLength(), buffer);

								int length = array::count(buffer);
								co_await Ble::receive(0, length, buffer);
								if (length >= 1) {
									MessageReader r(length, buffer);
									auto op = r.e8<att::Op>();
									Terminal::out << dec(int(op)) << '\n';
									if (op == att::Op::ERROR_RSP) {
										auto requestOp = r.e8<att::Op>();
										auto requestHandle = r.u16L();

										auto code = r.u8();
										Terminal::out << dec(int(code)) << '\n';
										int xx = 0;
									}
								}
							}

							nextHandle = characteristic.handle + 1;
						}
						if (elementCount == 0 || nextHandle > service.end)
							break;
					} else {
						break;
					}
				}
			}
		}

		Ble::close(0);
	}
}

int main() {
	loop::init();
	Ble::init();
	Terminal::init();
	Output::init(); // for debug led's

	scan();

	loop::run();
}
