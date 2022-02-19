#include "RadioInterface.hpp"
#include <Radio.hpp>
#include <Random.hpp>
#include <Terminal.hpp>
#include <Timer.hpp>
#include <Nonce.hpp>
#include <hash.hpp>
#include <ieee.hpp>
#include <zb.hpp>
#include <gp.hpp>
#include <Pointer.hpp>
#include <StringOperators.hpp>
#include <util.hpp>
#include <iostream>


using EndpointType = Interface::EndpointType;

// timeout to wait for a response (e.g. default response or route reply)
constexpr SystemDuration timeout = 500ms;

// number of retries when a send fails
constexpr int MAX_RETRY = 2;

static uint8_t const za09LinkKey[] = {0x5A, 0x69, 0x67, 0x42, 0x65, 0x65, 0x41, 0x6C, 0x6C, 0x69, 0x61, 0x6E, 0x63, 0x65, 0x30, 0x39};

static AesKey const za09LinkAesKey = {{0x5a696742, 0x6565416c, 0x6c69616e, 0x63653039, 0x166d75b9, 0x730834d5, 0x1f6155bb, 0x7c046582, 0xe62066a9, 0x9528527c, 0x8a4907c7, 0xf64d6245, 0x018a08eb, 0x94a25a97, 0x1eeb5d50, 0xe8a63f15, 0x2dff5170, 0xb95d0be7, 0xa7b656b7, 0x4f1069a2, 0xf7066bf4, 0x4e5b6013, 0xe9ed36a4, 0xa6fd5f06, 0x83c904d0, 0xcd9264c3, 0x247f5267, 0x82820d61, 0xd01eebc3, 0x1d8c8f00, 0x39f3dd67, 0xbb71d006, 0xf36e8429, 0xeee20b29, 0xd711d64e, 0x6c600648, 0x3801d679, 0xd6e3dd50, 0x01f20b1e, 0x6d920d56, 0x41d66745, 0x9735ba15, 0x96c7b10b, 0xfb55bc5d}};

//static AesKey const hashedTrustCenterLinkAesKey = {{0x4bab0f17, 0x3e1434a2, 0xd572e1c1, 0xef478782, 0xeabc1cc8, 0xd4a8286a, 0x01dac9ab, 0xee9d4e29, 0xb693b9e0, 0x623b918a, 0x63e15821, 0x8d7c1608, 0xa2d489bd, 0xc0ef1837, 0xa30e4016, 0x2e72561e, 0xea65fb8c, 0x2a8ae3bb, 0x8984a3ad, 0xa7f6f5b3, 0xb88396d0, 0x9209756b, 0x1b8dd6c6, 0xbc7b2375, 0xb9a50bb5, 0x2bac7ede, 0x3021a818, 0x8c5a8b6d, 0x479837d1, 0x6c34490f, 0x5c15e117, 0xd04f6a7a, 0x439aeda1, 0x2faea4ae, 0x73bb45b9, 0xa3f42fc3, 0xe78fc3ab, 0xc8216705, 0xbb9a22bc, 0x186e0d7f, 0x4e581106, 0x86797603, 0x3de354bf, 0x258d59c0}};

static AesKey const za09KeyTransportAesKey = {{0x4bab0f17, 0x3e1434a2, 0xd572e1c1, 0xef478782, 0xeabc1cc8, 0xd4a8286a, 0x01dac9ab, 0xee9d4e29, 0xb693b9e0, 0x623b918a, 0x63e15821, 0x8d7c1608, 0xa2d489bd, 0xc0ef1837, 0xa30e4016, 0x2e72561e, 0xea65fb8c, 0x2a8ae3bb, 0x8984a3ad, 0xa7f6f5b3, 0xb88396d0, 0x9209756b, 0x1b8dd6c6, 0xbc7b2375, 0xb9a50bb5, 0x2bac7ede, 0x3021a818, 0x8c5a8b6d, 0x479837d1, 0x6c34490f, 0x5c15e117, 0xd04f6a7a, 0x439aeda1, 0x2faea4ae, 0x73bb45b9, 0xa3f42fc3, 0xe78fc3ab, 0xc8216705, 0xbb9a22bc, 0x186e0d7f, 0x4e581106, 0x86797603, 0x3de354bf, 0x258d59c0}};
static AesKey const za09KeyLoadAesKey = {{0xc5a47035, 0xc332ccbf, 0x251571d8, 0xbaded188, 0xd99ab4c1, 0x1aa8787e, 0x3fbd09a6, 0x8563d82e, 0x20fb8556, 0x3a53fd28, 0x05eef48e, 0x808d2ca0, 0x798a659b, 0x43d998b3, 0x46376c3d, 0xc6ba409d, 0x85833b2f, 0xc65aa39c, 0x806dcfa1, 0x46d78f3c, 0x9bf0d075, 0x5daa73e9, 0xddc7bc48, 0x9b103374, 0x71334261, 0x2c993188, 0xf15e8dc0, 0x6a4ebeb4, 0x1e9dcf63, 0x3204feeb, 0xc35a732b, 0xa914cd9f, 0x642014b0, 0x5624ea5b, 0x957e9970, 0x3c6a54ef, 0x7d00cb5b, 0x2b242100, 0xbe5ab870, 0x8230ec9f, 0x4fce1048, 0x64ea3148, 0xdab08938, 0x588065a7}};

constexpr zb::SecurityControl securityLevel = zb::SecurityControl::LEVEL_ENC_MIC32; // encrypted + 32 bit message integrity code



// role of a cluster
enum class Role : uint8_t {
	// client sends (cluster specific) commmands to manipulate attributes in server (is in output cluster list)
	CLIENT,

	// server (is in input cluster list)
	SERVER
};

enum class Mode : uint8_t {
	// commands are received from a cluster (needs binding) or sent to a cluster
	COMMAND,

	// attributes are reported by a cluster (needs reporting) or sent to a cluster
	ATTRIBUTE
};

struct EndpointInfo {
	// endpoint type as exposed in Interface
	EndpointType endpointType;

	// role of cluster (client/output or server/input)
	Role role;

	// cluster id
	zb::ZclCluster cluster;

	// command or attribute
	Mode mode;

	// attribute id
	uint16_t attribute;
};

// list of endpoint infos
EndpointInfo const endpointInfos[] {
	// battery percentage 0-100% in 0.5% steps, i.e. value is 0-200
	{
		EndpointType::BATTERY_LEVEL_IN,
		Role::SERVER,
		zb::ZclCluster::POWER_CONFIGURATION,
		Mode::ATTRIBUTE,
		zb::ZclPowerConfigurationAttribute::BATTERY_PERCENTAGE
	},

	// wall switch (sends on/off commands, needs binding)
	{
		EndpointType::ON_OFF_IN,
		Role::CLIENT,
		zb::ZclCluster::ON_OFF,
		Mode::COMMAND
	},

	// power on/off, e.g. light bulb, switchable socket (receives on/off commands)
	{
		EndpointType::ON_OFF_OUT,
		Role::SERVER,
		zb::ZclCluster::ON_OFF,
		Mode::COMMAND
	},

	// level, e.g. brightness of light bulb
	{
		EndpointType::LEVEL_OUT,
		Role::SERVER,
		zb::ZclCluster::LEVEL_CONTROL,
		Mode::COMMAND
	},
};


// RadioInterface

RadioInterface::RadioInterface(PersistentStateManager &stateManager)
	: securityCounter(stateManager)
{
}

Coroutine RadioInterface::start(uint16_t panId, DataBuffer<16> const &key, AesKey const &aesKey) {
	this->panId = panId;
	this->key = &key;
	this->aesKey = &aesKey;

	// restore security counter
	co_await this->securityCounter.restore();

	// set pan id of coordinator
	Radio::setPan(RADIO_ZBEE, panId);

	// set short address of coordinator
	Radio::setShortAddress(RADIO_ZBEE, 0x0000);

	// filter packets with the coordinator as destination (and broadcast pan/address)
	Radio::setFlags(RADIO_ZBEE, Radio::ContextFlags::PASS_DEST_SHORT | Radio::ContextFlags::PASS_DEST_LONG
		| Radio::ContextFlags::HANDLE_ACK);

	// start coroutines
	broadcast();
	sendBeacon();
	for (int i = 0; i < PUBLISH_COUNT; ++i)
		publish();
	for (int i = 0; i < RECEIVE_COUNT; ++i)
		receive();
}

RadioInterface::~RadioInterface() {
}

void RadioInterface::setCommissioning(bool enabled) {
	this->commissioning = enabled && getDeviceCount() < MAX_DEVICE_COUNT;
	if (this->commissioning) {
		// allocate next short address
		allocateNextAddress();
	} else {
		// cancel current association coroutine if it is still running
		this->associationCoroutine.cancel();
	}
}

int RadioInterface::getDeviceCount() {
	return this->gpDevices.count() + this->zbDevices.count();
}

DeviceId RadioInterface::getDeviceId(int index) {
	int gpDeviceCount = this->gpDevices.count();
	if (index < gpDeviceCount)
		return this->gpDevices[index]->deviceId;
	else if (index < gpDeviceCount + this->zbDevices.count())
		return this->zbDevices[index - gpDeviceCount]->longAddress;
	else
		assert(false);
	return 0;
}

Array<EndpointType const> RadioInterface::getEndpoints(DeviceId deviceId) {
	for (auto &device : this->gpDevices) {
		auto const &flash = *device;
		if (flash.deviceId == deviceId) {
			return {flash.endpointCount, flash.endpoints};
		}
	}
	for (auto &device : this->zbDevices) {
		auto const &flash = *device;
		if (flash.longAddress == deviceId) {
			return {flash.endpointCount, reinterpret_cast<EndpointType const *>(flash.endpoints)};
		}
	}
	return {};
}

void RadioInterface::addPublisher(DeviceId deviceId, uint8_t endpointIndex, Publisher &publisher) {
	for (auto &device : this->zbDevices) {
		auto const &flash = *device;
		if (flash.longAddress == deviceId && endpointIndex < flash.endpointCount) {
			publisher.remove();
			publisher.index = endpointIndex;
			publisher.event = &this->publishEvent;
			device.publishers.add(publisher);
			return;
		}
	}
}

void RadioInterface::addSubscriber(DeviceId deviceId, uint8_t endpointIndex, Subscriber &subscriber) {
	for (auto &device : this->gpDevices) {
		auto const &flash = *device;
		if (flash.deviceId == deviceId && endpointIndex < flash.endpointCount) {
			subscriber.remove();
			subscriber.index = endpointIndex;
			device.subscribers.add(subscriber);
			return;
		}
	}
	for (auto &device : this->zbDevices) {
		auto const &flash = *device;
		if (flash.longAddress == deviceId && endpointIndex < flash.endpointCount) {
			subscriber.remove();
			subscriber.index = endpointIndex;
			device.subscribers.add(subscriber);
			return;
		}
	}
}


RadioInterface::ZbDevice *RadioInterface::findZbDevice(uint16_t address) {
	for (auto &device : this->zbDevices) {
		if (device->shortAddress == address)
			return &device;
	}
	return nullptr;
}

void RadioInterface::allocateNextAddress() {
	while (true) {
		uint16_t address = (Random::int8() << 8) | Random::int8();

		// exclude some addresses at both ends of range
		if (address < 0x0100 || address >= 0xff00)
			continue;

		// check if address is already in use
		if (findZbDevice(address) != nullptr)
			continue;

		// found a new address
		this->nextShortAddress = address;
			break;
	}
}


void RadioInterface::writeNwkBroadcastCommand(PacketWriter &w, zb::NwkCommand command) {
	// ieee 802.15.4 frame control
	auto ieeeFrameControl = ieee::FrameControl::TYPE_DATA
		| ieee::FrameControl::PAN_ID_COMPRESSION
		| ieee::FrameControl::DESTINATION_ADDRESSING_SHORT
		| ieee::FrameControl::SOURCE_ADDRESSING_SHORT;
	w.e16L(ieeeFrameControl);

	// sequence number
	w.u8(this->macCounter++);

	// destination pan
	w.u16L(this->panId);

	// ieee broadcast
	w.u16L(0xffff);

	// source address
	w.u16L(0x0000);


	// set start of header for encryption
	w.setHeader();

	// network layer frame control field
	auto nwkFrameControl = zb::NwkFrameControl::TYPE_COMMAND
		| zb::NwkFrameControl::VERSION_2
		| zb::NwkFrameControl::DISCOVER_ROUTE_SUPPRESS
		| zb::NwkFrameControl::SECURITY
		| zb::NwkFrameControl::EXTENDED_SOURCE;
	w.e16L(nwkFrameControl);

	// zbee broadcast
	w.u16L(0xfffc);

	// source
	w.u16L(0x0000);

	// radius
	w.u8(1);

	// sequence number
	w.u8(this->nwkCounter++);

	// extended source
	auto longAddress = Radio::getLongAddress();
	w.u64L(longAddress);


	// security header
	if ((nwkFrameControl & zb::NwkFrameControl::SECURITY) != 0) {
		// security control and counter
		w.security(zb::SecurityControl::LEVEL_ENC_MIC32
			| zb::SecurityControl::KEY_NETWORK
			| zb::SecurityControl::EXTENDED_NONCE,
		this->securityCounter++);

		// extended source
		w.u64L(longAddress);

		// key sequence number
		w.u8(0);
	}


	// set start of message for encryption
	w.setMessage();

	// command
	w.e8(command);
}

void RadioInterface::writeNwk(PacketWriter &w, zb::NwkFrameControl nwkFrameControl, uint8_t radius, ZbDevice &device) {
	assert(device.routerAddress != 0xffff);

	// ieee 802.15.4 frame control
	auto ieeeFrameControl = ieee::FrameControl::TYPE_DATA
		| ieee::FrameControl::ACKNOWLEDGE_REQUEST
		| ieee::FrameControl::PAN_ID_COMPRESSION
		| ieee::FrameControl::DESTINATION_ADDRESSING_SHORT
		| ieee::FrameControl::SOURCE_ADDRESSING_SHORT;
	w.e16L(ieeeFrameControl);

	// sequence number
	w.u8(this->macCounter++);

	// destination pan
	w.u16L(this->panId);

	// ieee destination address (address of router for the device)
	w.u16L(device.routerAddress);

	// source address
	w.u16L(0x0000);


	// set start of header for encryption
	w.setHeader();

/*
	// set source route flag and get address of first hop
	int routeCount = device.route.size();
	if (routeCount > 0) {
		nwkFrameControl |= zb::NwkFrameControl::SOURCE_ROUTE;
	}
*/

	// network layer frame control field
	w.e16L(nwkFrameControl);

	// zbee broadcast
	w.u16L(device->shortAddress);

	// source
	w.u16L(0x0000);

	// radius
	w.u8(radius);

	// sequence number
	w.u8(this->nwkCounter++);

	// destination
	if ((nwkFrameControl & zb::NwkFrameControl::DESTINATION) != 0)
		w.u64L(device->longAddress);

	// extended source
	auto longAddress = Radio::getLongAddress();
	if ((nwkFrameControl & zb::NwkFrameControl::EXTENDED_SOURCE) != 0)
		w.u64L(longAddress);


	// security header
	if ((nwkFrameControl & zb::NwkFrameControl::SECURITY) != 0) {
		// security control and counter
		w.security(zb::SecurityControl::LEVEL_ENC_MIC32
			| zb::SecurityControl::KEY_NETWORK
			| zb::SecurityControl::EXTENDED_NONCE,
			this->securityCounter++);

		// extended source
		w.u64L(longAddress);

		// key sequence number
		w.u8(0);
	}

	// set start of message for encryption
	w.setMessage();

/*
	// add source route
	if (routeCount > 0) {
		w.int8(device.route.size());
		w.int8(0);
		for (uint16_t relay : device.route) {
			w.int16(relay);
		}
	}
*/
}

void RadioInterface::writeApsCommand(PacketWriter &w, zb::SecurityControl keyType,
	zb::ApsCommand command)
{
	// set start of header for encryption
	w.setHeader();

	// application support layer frame control field
	auto apsFrameControl = zb::ApsFrameControl::TYPE_COMMAND
		| zb::ApsFrameControl::DELIVERY_UNICAST
		| zb::ApsFrameControl::SECURITY;
	//if (requestAck)
	//	apsFrameControl |= zb::ApsFrameControl::ACK_REQUEST;
	w.e8(apsFrameControl);

	// counter
	w.u8(this->apsCounter++);


	// security header
	{
		// security control and counter
		w.security(zb::SecurityControl::LEVEL_ENC_MIC32
			| keyType
			| zb::SecurityControl::EXTENDED_NONCE,
			this->securityCounter++);

		// extended source
		w.u64L(Radio::getLongAddress());
	}


	// set start of message for encryption
	w.setMessage();

	// aps command
	w.e8(command);
}

void RadioInterface::writeApsAck(PacketWriter &w, uint8_t apsCounter) {
	// application support layer frame control field
	auto apsFrameControl = zb::ApsFrameControl::TYPE_ACK
		| zb::ApsFrameControl::DELIVERY_UNICAST
		| zb::ApsFrameControl::ACK_FORMAT;
	w.e8(apsFrameControl);

	// aps counter
	w.u8(apsCounter);
}

uint8_t RadioInterface::writeApsDataZdp(PacketWriter &w, zb::ZdpCommand request) {
	// application support layer frame control field
	w.e8(zb::ApsFrameControl::TYPE_DATA
		| zb::ApsFrameControl::DELIVERY_UNICAST);

	// destination endpoint (device)
	w.u8(0);

	// zdp command
	assert((request & zb::ZdpCommand::RESPONSE_FLAG) == 0);
	w.e16L(request);

	// device profile
	w.u16L(0x0000);

	// source endpoint (device)
	w.u8(0);

	// counter
	w.u8(this->apsCounter++);


	// device profile
	uint8_t zdpCounter = this->zdpCounter++;
	w.u8(zdpCounter);

	return zdpCounter;
}

void RadioInterface::writeApsDataZdp(PacketWriter &w, zb::ZdpCommand response, uint8_t zdpCounter) {
	// application support layer frame control field
	w.e8(zb::ApsFrameControl::TYPE_DATA
		| zb::ApsFrameControl::DELIVERY_UNICAST);

	// destination endpoint (device)
	w.u8(0);

	// zdp command
	assert((response & zb::ZdpCommand::RESPONSE_FLAG) != 0);
	w.e16L(response);

	// device profile
	w.u16L(0x0000);

	// source endpoint (device)
	w.u8(0);

	// counter
	w.u8(this->apsCounter++);


	// device profile
	w.u8(zdpCounter);
}

void RadioInterface::writeApsAckZdp(PacketWriter &w, uint8_t apsCounter, zb::ZdpCommand command) {
	// application support layer frame control field
	w.e8(zb::ApsFrameControl::TYPE_ACK
		| zb::ApsFrameControl::DELIVERY_UNICAST);

	// destination endpoint (device)
	w.u8(0);

	// zdp command
	w.e16L(command);

	// device profile
	w.u16L(0x0000);

	// source endpoint (device)
	w.u8(0);

	// counter
	w.u8(apsCounter);
}

void RadioInterface::writeApsDataZcl(PacketWriter &w, uint8_t destinationEndpoint, zb::ZclCluster clusterId,
	zb::ZclProfile profile, uint8_t sourceEndpoint)
{
	// application support layer frame control field
	w.e8(zb::ApsFrameControl::TYPE_DATA
		| zb::ApsFrameControl::DELIVERY_UNICAST);

	// destination endpoint
	w.u8(destinationEndpoint);

	// cluster id
	w.e16L(clusterId);

	// profile
	w.e16L(profile);

	// source endpoint
	w.u8(sourceEndpoint);

	// counter
	w.u8(this->apsCounter++);
}

void RadioInterface::writeApsAckZcl(PacketWriter &w, uint8_t destinationEndpoint, zb::ZclCluster clusterId,
	zb::ZclProfile profile, uint8_t sourceEndpoint)
{
	// application support layer frame control field
	w.e8(zb::ApsFrameControl::TYPE_ACK
		| zb::ApsFrameControl::DELIVERY_UNICAST);

	// destination endpoint
	w.u8(destinationEndpoint);

	// cluster id
	w.e16L(clusterId);

	// profile
	w.e16L(profile);

	// source endpoint
	w.u8(sourceEndpoint);

	// counter
	w.u8(this->apsCounter++);
}


bool writeZclClusterSpecific(RadioInterface::PacketWriter &w, uint8_t zclCounter, EndpointInfo const &info,
	MessageType srcType, void const *srcMessage)
{
	// zbee cluster library frame
	w.e8(zb::ZclFrameControl::TYPE_CLUSTER_SPECIFIC
		| zb::ZclFrameControl::DIRECTION_CLIENT_TO_SERVER);
	w.u8(zclCounter);

	Message const &src = *reinterpret_cast<Message const *>(srcMessage);

	switch (info.cluster) {
	case zb::ZclCluster::ON_OFF:
		switch (srcType) {
		case MessageType::ON_OFF:
			w.u8(src.onOff);
			break;
		case MessageType::ON_OFF2:
			// invert on/off (0, 1, 2 -> 1, 0, 2)
			w.u8(src.onOff ^ 1 ^ (src.onOff >> 1));
			break;
		case MessageType::TRIGGER:
			// trigger (e.g.button) toggles on/off
			if (src.trigger == 0)
				return false;
			w.u8(2);
			break;
		case MessageType::UP_DOWN:
			// rocker switches off/on (1, 2 -> 0, 1)
			if (src.upDown == 0)
				return false;
			w.u8(src.upDown - 1);
			break;
		default:
			// conversion failed
			return false;
		}
		break;
	case zb::ZclCluster::LEVEL_CONTROL:
		switch (srcType) {
		case MessageType::LEVEL:
		case MessageType::MOVE_TO_LEVEL:
			if (!src.level.getFlag()) {
				// absolute level
				w.e8(zb::ZclLevelControlCommand::MOVE_TO_LEVEL_WITH_ON_OFF);
			} else {
				// increase/decrease level
				w.e8(zb::ZclLevelControlCommand::STEP_WITH_ON_OFF);
				w.u8(src.level >= 0 ? 0x00 : 0x01);
			}
			w.u8(clamp(abs(int(src.level * 255.0f)), 0, 254));
			if (srcType == MessageType::LEVEL)
				w.u16L(0);
			else if (!src.moveToLevel.move.getFlag())
				w.u16L(clamp(int(src.moveToLevel.move * 10.0f), 0, 65534));
			else
				return false; // conversion failed
			break;
		default:
			// conversion failed
			return false;
		}
		break;
	default:
		// conversion failed
		return false;
	}

	// conversion successful
	return true;
}

void RadioInterface::writeFooter(PacketWriter &w, Radio::SendFlags sendFlags) {
	if (w.hasSecurity()) {
		auto securityControl = w.getSecurityControl();
		auto securityCounter = w.getSecurityCounter();

		// length of message integrity code
		int micLength = 4;

		// nonce (4.5.2.2)
		Nonce nonce(Radio::getLongAddress(), securityCounter, securityControl);

		// select key
		AesKey const *key;
		switch (securityControl & zb::SecurityControl::KEY_MASK) {
		case zb::SecurityControl::KEY_LINK:
			key = &za09LinkAesKey;
			break;
		case zb::SecurityControl::KEY_KEY_TRANSPORT:
			key = &za09KeyTransportAesKey;
			break;
		case zb::SecurityControl::KEY_KEY_LOAD:
			key = &za09KeyLoadAesKey;
			break;
		default:
			// KEY_NETWORK
			key = this->aesKey;
		}

		// encrypt
		w.encrypt(micLength, nonce, *key);

		//bool ok = decrypt(w.message, nonce, w.header, headerLength, w.message, payloadLength, micLength, key);

		// clear security level according to 4.3.1.1 step 8.
		w.clearSecurityLevel();
	}
	w.finish(sendFlags);
}

Coroutine RadioInterface::broadcast() {
	uint8_t packet[52];
	uint8_t sendResult;
	auto linkTime = Timer::now() + 1s;
	auto routeTime = linkTime + 1s;
	while (true) {
		// wait until the next timeout
		auto time = linkTime;//min(linkTime, routeTime);
		co_await Timer::sleep(time);

		if (time == linkTime) {
			// broadcast link status every 15 seconds
			linkTime += 15s;
			{
				PacketWriter w(packet);

				// link status command
				writeNwkBroadcastCommand(w, zb::NwkCommand::LINK_STATUS);
				w.e8(zb::NwkCommandLinkStatusOptions(0) // link status count
					| zb::NwkCommandLinkStatusOptions::FIRST_FRAME
					| zb::NwkCommandLinkStatusOptions::LAST_FRAME);

				writeFooter(w, Radio::SendFlags::NONE);
			}
			co_await Radio::send(RADIO_ZBEE, packet, sendResult);
		}
		/*if (time == routeTime) {
			// broadcast many-to-one route request every 100 seconds
			routeTime += 100s;
			{
				PacketWriter w(packet);

				// many-to-one route request command
				writeNwkBroadcastCommand(w, zb::NwkCommand::ROUTE_REQUEST);
				w.e8(zb::NwkCommandRouteRequestOptions::DISCOVERY_MANY_TO_ONE_WITH_SOURCE_ROUTING);

				// route id
				w.int8(this->routeCounter++);

				// destination
				w.int16(0xfffc);

				// path cost
				w.int8(0);

				writeFooter(w, radio::SendFlags::NONE);
			}
			co_await radio::send(RADIO_ZB, packet, sendResult);
		}*/
	}
}

Coroutine RadioInterface::sendBeacon() {
	uint8_t packet[32];
	uint8_t sendResult;
	while (true) {
		// wait until a beacon request was received
		co_await this->beaconBarrier.wait();

		// send beacon
		{
			// get global configuration
			//auto const &configuration = *this->configuration;

			PacketWriter w(packet);

			// ieee 802.15.4 frame control
			w.e16L(ieee::FrameControl::TYPE_BEACON
				| ieee::FrameControl::SOURCE_ADDRESSING_SHORT);

			// sequence number
			w.u8(this->macCounter++);

			// source pan
			w.u16L(this->panId);

			// source address
			w.u16L(0x0000);

			// superframe specification
			w.u16L(15 // beacon interval
				| (15 << 4) // superframe interval
				| (15 << 8) // final cap slot
				| (0 << 12) // battery extension
				| 0x4000 // pan coordinator
				| (this->commissioning ? 0x8000 : 0)); // association permit

			// gts
			w.u8(0);

			// pending addresses, short and long
			w.u8(0x00);

			// beacon

			// protocol id
			w.u8(0);

			// stack profile
			w.u16L(2 // zbee pro
				| (2 << 4) // protocol version
				| (1 << 10) // router capacity
				| (0 << 11) // device depth (4 bit)
				| (1 << 15)); // end device capacity

			// extended pan id
			w.u64L(UINT64_C(0xdddddddddddddddd));

			// tx offset
			w.u16L(0xffff);
			w.u8(0xff);

			// update id
			w.u8(0);

			w.finish(Radio::SendFlags::NONE);
		}
		co_await Radio::send(RADIO_ZBEE, packet, sendResult);

		// "cool down" before a new beacon can be sent
		co_await Timer::sleep(100ms);
	}
}

static bool handleZclClusterSpecific(MessageType dstType, void *dstMessage, EndpointInfo const &info, MessageReader r) {
	// get command
	uint8_t command = r.u8();

	Message &dst = *reinterpret_cast<Message *>(dstMessage);
	switch (dstType) {
	case MessageType::ON_OFF:
		switch (info.cluster) {
		case zb::ZclCluster::ON_OFF:
			if (command <= 2)
				dst.onOff = command;
			else
				return false; // conversion failed
			break;
		default:
			// conversion failed
			return false;
		}
		break;
	case MessageType::MOVE_TO_LEVEL:
		switch (info.cluster) {
		case zb::ZclCluster::LEVEL_CONTROL:
			switch (zb::ZclLevelControlCommand(command)) {
			case zb::ZclLevelControlCommand::MOVE_TO_LEVEL:
			case zb::ZclLevelControlCommand::MOVE_TO_LEVEL_WITH_ON_OFF:
				{
					uint8_t level = r.u8();
					uint16_t transitionTime = r.u16L();

					dst.moveToLevel.level = float(level) / 254.0f;
					dst.moveToLevel.move = float(transitionTime) / 10.0f;
				}
				break;
			case zb::ZclLevelControlCommand::STEP:
			case zb::ZclLevelControlCommand::STEP_WITH_ON_OFF:
				{
					bool up = r.u8() == 0;
					int diff = r.u8();
					uint16_t transitionTime = r.u16L();

					dst.moveToLevel.level.set(float(up ? diff : -diff) / 254.0f, true);
					dst.moveToLevel.move = float(transitionTime) / 10.0f;
				}
				break;
			}
		default:
			// conversion failed
			return false;
		}
		break;
	default:
		// conversion failed
		return false;
	}
	return true;
}

Coroutine RadioInterface::receive() {
	while (true) {
		// wait until we receive a packet
		Radio::Packet packet;
		co_await Radio::receive(RADIO_ZBEE, packet);
		PacketReader r(packet);

		// ieee 802.15.4 mac
		uint8_t const *mac = r.current;

		// frame control
		auto ieeeFrameControl = r.e16L<ieee::FrameControl>();
		auto ieeeFrameType = ieeeFrameControl & ieee::FrameControl::TYPE_MASK;

		// the radio filter flags ensure that a sequence number is present
		uint8_t macCounter = r.u8();

		// the radio filter flags ensure that a destination pan id is present that is either broadcast or our pan
		uint16_t destinationPanId = r.u16L();
		bool destinationBroadcast = destinationPanId == 0xffff;

		// the radio filter flags ensure that a destination address is present hat is either boradcast or our address
		if ((ieeeFrameControl & ieee::FrameControl::DESTINATION_ADDRESSING_LONG_FLAG) == 0) {
			// short destination address
			uint16_t destAddress = r.u16L();
			destinationBroadcast |= destAddress == 0xffff;
		} else {
			// long destination address is always our address
			r.skip(8);
		}

		if ((ieeeFrameControl & ieee::FrameControl::SOURCE_ADDRESSING_FLAG) == 0) {
			// handle ieee frames with no source address separately
			if (ieeeFrameType == ieee::FrameControl::TYPE_COMMAND) {
				auto command = r.e8<ieee::Command>();
				if (command == ieee::Command::BEACON_REQUEST) {
					// handle beacon request by sending a beacon
Terminal::out << ("Beacon Request\n");
					this->beaconBarrier.resumeAll();
				}
			} else if (ieeeFrameType == ieee::FrameControl::TYPE_DATA) {
				// network layer: handle dependent on protocol version
				auto version = r.peekE8<gp::NwkFrameControl>() & gp::NwkFrameControl::VERSION_MASK;
				if (version == gp::NwkFrameControl::VERSION_3_GP)
					handleGp(mac, r);
			}
			continue;
		}

		// get source pan id
		uint16_t sourcePanId = destinationPanId;
		if ((ieeeFrameControl & ieee::FrameControl::PAN_ID_COMPRESSION) == 0) {
			sourcePanId = r.u16L();
		}

		if (ieeeFrameType == ieee::FrameControl::TYPE_COMMAND) {
			// ieee command frame

			// check if address short or long
			if ((ieeeFrameControl & ieee::FrameControl::SOURCE_ADDRESSING_LONG_FLAG) != 0) {
				// long source address
				uint64_t sourceAddress = r.u64L();

				// ieee command
				auto command = r.e8<ieee::Command>();

				// check for association request with no broadcast
				if (this->commissioning && !destinationBroadcast && command == ieee::Command::ASSOCIATION_REQUEST) {
					// cancel current association coroutine
					this->associationCoroutine.cancel();

					// start new association request
					auto deviceInfo = r.e8<ieee::DeviceInfo>();
					bool receiveOnWhenIdle = (deviceInfo & ieee::DeviceInfo::RX_ON_WHEN_IDLE) != 0;
					auto sendFlags = receiveOnWhenIdle ? Radio::SendFlags::NONE : Radio::SendFlags::AWAIT_DATA_REQUEST;
Terminal::out << ("Association Request Receive On When Idle: " + dec(receiveOnWhenIdle) + '\n');
					this->associationCoroutine = handleAssociationRequest(sourceAddress, sendFlags);
				}
			}
			continue;
		}
		if (ieeeFrameType != ieee::FrameControl::TYPE_DATA)
			continue;


		// ieee data frame
		// ---------------

		// check if address short or long
		ZbDevice *pDevice = nullptr;
		if ((ieeeFrameControl & ieee::FrameControl::SOURCE_ADDRESSING_LONG_FLAG) == 0) {
			// short source address
			uint16_t sourceAddress = r.u16L();

			// try to lookup device by short address
			pDevice = findZbDevice(sourceAddress);

			// check if there is an active association coroutine and address matches the temp device
			if (this->associationCoroutine.isAlive()) {
				if ((*this->tempDevice)->shortAddress == sourceAddress) {
					pDevice = this->tempDevice;
				}
			}
		} else {
			// long source address
			uint64_t sourceAddress = r.u64L();

			/*else if (frameType == ieee::FrameControl::TYPE_DATA) {
				// try to lookup device by long address
				for (auto &device : this->devices) {
					DeviceFlash const &flash = *device;
					if (flash.type == DeviceFlash::ZB && flash.deviceId == sourceAddress) {
						pDevice = &device;
						break;
					}
				}

				// check if there is an active association coroutine
				if (this->associationCoroutine.isAlive()) {
					if ((*this->tempDevice)->deviceId == sourceAddress) {
						pDevice = this->tempDevice;
					}
				}
			}*/
		}
		if (pDevice == nullptr)
			continue;

		// network layer (nwk)
		// -------------------

		auto version = r.peekE8<gp::NwkFrameControl>() & gp::NwkFrameControl::VERSION_MASK;
		if (version == gp::NwkFrameControl::VERSION_2) {
			// set start of encryption header
			r.setHeader();

			auto nwkFrameControl = r.e16L<zb::NwkFrameControl>();
			uint16_t destinationAddress = r.u16L();
			uint16_t sourceAddress = r.u16L();
			uint8_t radius = r.u8();
			uint8_t sequenceNumber = r.u8();

			// destination
			if ((nwkFrameControl & zb::NwkFrameControl::DESTINATION) != 0) {
				uint8_t const *destination = r.current;
				r.skip(8);
			}

			// extended source
			if ((nwkFrameControl & zb::NwkFrameControl::EXTENDED_SOURCE) != 0) {
				uint8_t const *extendedSource = r.current;
				r.skip(8);
			}

			// source route
			if ((nwkFrameControl & zb::NwkFrameControl::SOURCE_ROUTE) != 0) {
				uint8_t relayCount = r.u8();
				uint8_t relayIndex = r.u8();
				for (int i = 0; i < relayCount; ++i) {
					uint16_t relay = r.u16L();
				}
			}

			// security header
			// note: does in-place decryption of the payload
			uint8_t const *extendedSource = nullptr;
			if ((nwkFrameControl & zb::NwkFrameControl::SECURITY) != 0) {
				// restore security level according to 4.3.1.2 step 1.
				r.restoreSecurityLevel(securityLevel);

				// security control field (4.5.1.1)
				auto securityControl = r.e8<zb::SecurityControl>();

				// security counter
				uint32_t securityCounter = r.u32L();

				// key type
				if ((securityControl & zb::SecurityControl::KEY_MASK) != zb::SecurityControl::KEY_NETWORK) {
					// only network key supported
					continue;
				}

				// extended source
				if ((securityControl & zb::SecurityControl::EXTENDED_NONCE) == 0) {
					// only extended nonce supported
					continue;
				}
				extendedSource = r.current;
				r.skip(8);

				// key sequence number
				uint8_t keySequenceNumber = r.u8();

				// set start of message
				r.setMessage();

				// nonce (4.5.2.2)
				Nonce nonce(extendedSource, securityCounter, securityControl);

				// decrypt
				int micLength = 4;
				if (!r.decrypt(micLength, nonce, *this->aesKey)) {
					// decryption failed
					continue;
				}
			} else {
				// security is required
				continue;
			}

			// lookup actual source device at the start of the route
			ZbDevice &router = *pDevice;
			if (router->shortAddress != sourceAddress) {
				// source device is not a neighbor: lookup the source device
				pDevice = findZbDevice(sourceAddress);
				if (pDevice == nullptr)
					continue;
			}
			ZbDevice &device = *pDevice;

			// set first hop of route if not known yet (probably a hack)
			if (device.routerAddress == 0xffff) {
				device.sendFlags = router->sendFlags;
				device.routerAddress = router->shortAddress;
				device.cost = 255; // set bad costs so that route replies can later overwrite the router
			}

			auto nwkFrameType = nwkFrameControl & zb::NwkFrameControl::TYPE_MASK;
			if (nwkFrameType == zb::NwkFrameControl::TYPE_COMMAND) {
				// nwk command
				auto command = r.e8<zb::NwkCommand>();
				switch (command) {
				case zb::NwkCommand::ROUTE_REQUEST:
					// a device asks for the route to another device
					{
						auto options = r.e8<zb::NwkCommandRouteRequestOptions>();
						uint8_t routeId = r.u8();
						uint16_t destinationAddress = r.u16L();
						uint8_t cost = r.u8();

						switch (options & zb::NwkCommandRouteRequestOptions::DISCOVERY_MASK) {
						case zb::NwkCommandRouteRequestOptions::DISCOVERY_SINGLE:
							//if ((options & zb::NwkCommandRouteRequestOptions::EXTENDED_DESTINATION) != 0) {
							//}

							if (destinationAddress == 0x0000) {
								// build packet
								{
									PacketWriter w(packet);

									// route reply command
									writeNwkCommand(w, device, zb::NwkCommand::ROUTE_REPLY);

									// options
									w.e8(zb::NwkCommandRouteReplyOptions::EXTENDED_DESTINATION);

									// route id
									w.u8(routeId);

									// originator address
									w.u16L(device->shortAddress);

									// destination address
									w.u16L(0x0000);

									// cost
									w.u8(1);

									// extended destination
									w.u64L(Radio::getLongAddress());

									writeFooter(w, device.sendFlags);
								}

								// send packet
								uint8_t sendResult;
								co_await Radio::send(RADIO_ZBEE, packet, sendResult);
							}
							break;
						case zb::NwkCommandRouteRequestOptions::DISCOVERY_MANY_TO_ONE_WITH_SOURCE_ROUTING:
							break;
						default:
							break;
						}
					}
					break;
				case zb::NwkCommand::ROUTE_REPLY:
					// a device answers to a route request
					{
						auto options = r.e8<zb::NwkCommandRouteReplyOptions>();
						uint8_t routeId = r.u8();
						uint16_t originatorAddress = r.u16L();
						uint16_t destinationAddress = r.u16L(); // "Responder" in Wireshark
						uint8_t cost = r.u8();

						// check if a node has answered our own route request
						if (originatorAddress == 0x0000) {
							ZbDevice *destination = findZbDevice(destinationAddress);
							if (destination != nullptr /*&& cost < destination->cost*/) {
								destination->sendFlags = device->sendFlags;

								// set the node as the router for the destination
								destination->routerAddress = device->shortAddress;

								// set cost of route
								destination->cost = cost;

								// resume publisher
								destination->routeBarrier.resumeFirst();
							}
						}
					}
					break;
				case zb::NwkCommand::LEAVE:
					// device leaves the network
					for (int i = 0; i < this->zbDevices.count(); ++i) {
						if (&this->zbDevices[i] == &device) {
							this->zbDevices.erase(i);
							break;
						}
					}
					break;
				case zb::NwkCommand::ROUTE_RECORD:
					// we need to store the route to the device
					// https://www.digi.com/resources/documentation/Digidocs/90001942-13/concepts/c_source_routing.htm
					/*{
						uint8_t relayCount = r.int8();
						device.route.resize(relayCount);
						for (int i = relayCount - 1; i >= 0; --i) {
							device.route[i] = r.int16();
						}

						// set send flags for first hop
						if (relayCount > 0) {
							// a relay is always on
							device.sendFlags = radio::SendFlags::NONE;

							//ZbDevice *firstHop = findZbDevice(device.route[0]);
							//if (firstHop != nullptr)
							//	device.sendFlags = (*firstHop)->sendFlags;
						}
					}*/
					break;
				case zb::NwkCommand::REJOIN_REQUEST:
					// device wants to rejoin e.g. after a relay disappeared
					{
						uint8_t capability = r.u8();

						// delegate to publisher coroutine
						//device.rejoinPending = true;
						//this->publishEvent.set();

						// build packet
						{
							PacketWriter w(packet);

							// rejoin response command
							writeNwkCommand(w, device, zb::NwkCommand::REJOIN_RESPONSE);

							// address
							w.u16L(device->shortAddress);

							// status
							w.u8(0);

							writeFooter(w, device.sendFlags);
						}

						// send packet
						uint8_t sendResult;
						co_await Radio::send(RADIO_ZBEE, packet, sendResult);
					}
				default:
					;
				}
				continue;
			}
			if (nwkFrameType != zb::NwkFrameControl::TYPE_DATA)
				continue;

			// nwk data frame
			// --------------

			// aps (application support layer)
			// -------------------------------

			// set start of header
			r.setHeader();

			// frame control
			auto apsFrameControl = r.e8<zb::ApsFrameControl>();
			auto apsFrameType = apsFrameControl & zb::ApsFrameControl::TYPE_MASK;
			if (apsFrameType == zb::ApsFrameControl::TYPE_COMMAND) {
				// aps command
				uint8_t apsCounter = r.u8();

				// security header
				// note: does in-place decryption of the payload
				bool apsSecurity = (apsFrameControl & zb::ApsFrameControl::SECURITY) != 0;
				if (apsSecurity) {
					// restore security level according to 4.4.1.2 step 5.
					r.restoreSecurityLevel(securityLevel);

					// security control field (4.5.1.1)
					auto securityControl = r.e8<zb::SecurityControl>();

					// security counter
					uint32_t securityCounter = r.u32L();

					// nonce (4.5.2.2)
					if ((securityControl & zb::SecurityControl::EXTENDED_NONCE) == 0) {
						if (extendedSource == nullptr) {
							continue;
						}
					} else {
						extendedSource = r.current;
						r.skip(8);
					}
					Nonce nonce(extendedSource, securityCounter, securityControl);

					// select key
					auto keyType = securityControl & zb::SecurityControl::KEY_MASK;
					AesKey const *key;
					switch (keyType) {
					case zb::SecurityControl::KEY_LINK:
						key = &za09LinkAesKey;
						break;
					case zb::SecurityControl::KEY_KEY_TRANSPORT:
						key = &za09KeyTransportAesKey;
						break;
					case zb::SecurityControl::KEY_KEY_LOAD:
						key = &za09KeyLoadAesKey;
						break;
					default:
						continue;
					}

					// decrypt in-place (mic length of 4)
					r.setMessage();
					if (!r.decrypt(4, nonce, *key)) {
						continue;
					}
				}

				// acknowledge if requested
				if ((apsFrameControl & zb::ApsFrameControl::ACK_REQUEST) != 0) {
					uint8_t packet2[48];
					{
						PacketWriter w(packet2);
						writeNwkData(w, device);
						writeApsAck(w, apsCounter);
						writeFooter(w, device.sendFlags);
					}
					uint8_t sendResult;
					co_await Radio::send(RADIO_ZBEE, packet2, sendResult);

					// break on failure and rely on sender to repeat the request
					if (sendResult == 0)
						continue;
				}

				auto command = r.e8<zb::ApsCommand>();
				switch (command) {
				case zb::ApsCommand::UPDATE_DEVICE:
					{
						// device address
						uint64_t longAddress = r.u64L();

						// short device address
						uint16_t shortAddress = r.u16L();

						uint8_t status = r.u8();

						ZbDevice *d = findZbDevice(shortAddress);
						if (status == 0 && d != nullptr) {
							ZbDevice &updatedDevice = *d;
							auto const &flash = *updatedDevice;

							if (flash.longAddress == longAddress) {
		Terminal::out << ("Update Device " + hex(shortAddress));

								// clear route of updated device if we don't get the message from its first hop
								if (device->shortAddress != updatedDevice.routerAddress)
									updatedDevice.routerAddress = 0xffff;
							}
						}
					}
					break;
				case zb::ApsCommand::REQUEST_KEY:
					// a node requests a link key (packet must be aps-encrypted)
					if (apsSecurity) {
						// get key type
						auto keyType = r.e8<zb::RequestKeyType>();

						switch (keyType) {
						case zb::RequestKeyType::APPLICATION_LINK:
							// Application Link Key (0x02) -> section 4.7.3.8.
							break;
						case zb::RequestKeyType::TRUST_CENTER_LINK:
							// Trust Center Link Key (0x04) -> section 4.7.3.6.
							{
								{
									PacketWriter w(packet);

									// nwk data
									writeNwkData(w, device);
									auto state = w.push();

									// aps command: transport key
									writeApsCommand(w, zb::SecurityControl::KEY_KEY_LOAD, zb::ApsCommand::TRANSPORT_KEY);

									// key type
									w.e8(zb::StandardKeyType::TRUST_CENTER_LINK);

									// link key
									// todo: generate a link key per device
									w.data(Array<uint8_t const, 16>(za09LinkKey));

									// extended destination address
									w.u64L(device->longAddress);

									// extended source address
									w.u64L(Radio::getLongAddress()); // extended source address

									writeFooter(w, Radio::SendFlags::NONE);
									w.pop(state);
									writeFooter(w, device.sendFlags);
								}
								uint8_t sendResult;
								co_await Radio::send(RADIO_ZBEE, packet, sendResult);
							}
							break;
						default:
							// silently drop request according to section 4.4.5.2.3.
							;
						}
					}
					break;
				case zb::ApsCommand::VERIFY_KEY:
					// a node wants to verify its link key
					{
						// key type
						auto keyType = r.e8<zb::RequestKeyType>();

						// extended source
						auto extendedSource = r.u64L();

						// key hash
						auto hash = r.data<16>();

						uint8_t sendResult;
						switch (keyType) {
						case zb::RequestKeyType::TRUST_CENTER_LINK:
							{
								// check key hash (4.4.10.7.4)
								DataBuffer<16> hashedKey;
								keyHash(hashedKey, za09LinkKey, 3);
								bool success = hash == hashedKey;

								// build packet (note: hash gets overwritten)
								PacketWriter w(packet);

								// nwk data
								writeNwkData(w, device);
								auto state = w.push();

								// aps command: confirm key (zigbee2mqtt requests an ack here, but seems unnecessary)
								writeApsCommand(w, zb::SecurityControl::KEY_LINK, zb::ApsCommand::CONFIRM_KEY);

								// status (success or security fail, table 2.27)
								w.u8(success ? 0 : 0xad);

								// key type
								w.e8(zb::StandardKeyType::TRUST_CENTER_LINK);

								// extended destination address
								w.u64L(device->longAddress);

								writeFooter(w, Radio::SendFlags::NONE);
								w.pop(state);
								writeFooter(w, device.sendFlags);
							}
							co_await Radio::send(RADIO_ZBEE, packet, sendResult);
							break;
						default:
							;
						}
					}
					break;
				default:
					;
				}
				continue;
			}
			if (apsFrameType == zb::ApsFrameControl::TYPE_ACK) {
				 // aps ack
				continue;
			}
			if (apsFrameType != zb::ApsFrameControl::TYPE_DATA)
				continue;

		    // aps data
			// --------

			// endpoint 0 is zdp, others are zcl
			uint8_t dstEndpoint = r.u8();
			if (dstEndpoint == 0) {
				// zbee device profile (zdp)
				auto command = r.e16L<zb::ZdpCommand>();
				uint16_t profile = r.u16L();
				uint8_t srcEndpoint = r.u8();
				uint8_t apsCounter = r.u8();
				uint8_t zdpCounter = r.u8();

				// acknowledge if requested
				if ((apsFrameControl & zb::ApsFrameControl::ACK_REQUEST) != 0) {
					uint8_t packet2[48];
					{
						PacketWriter w(packet2);
						writeNwkData(w, device);
						writeApsAckZdp(w, apsCounter, command);
						writeFooter(w, device.sendFlags);
					}
					uint8_t sendResult;
					co_await Radio::send(RADIO_ZBEE, packet2, sendResult);

					// break on failure and rely on sender to repeat the request
					if (sendResult == 0)
						continue;
				}

				if ((command & zb::ZdpCommand::RESPONSE_FLAG) == 0) {
					// zdp request
					switch (command) {
					case zb::ZdpCommand::NODE_DESCRIPTOR_REQUEST:
						{
							// build node desciptor response packet
							{
								PacketWriter w(packet);
								writeNwkData(w, device);
								writeApsDataZdp(w, zb::ZdpCommand::NODE_DESCRIPTOR_RESPONSE, zdpCounter);

								// status: success
								w.u8(0);

								// nwk address of interest
								w.u16L(0x0000);

								// node descriptor

								// info
								w.e16L(zb::ZdpNodeDescriptorInfo::TYPE_COORDINATOR
									| zb::ZdpNodeDescriptorInfo::BAND_2_4GHZ);
								w.e8(ieee::DeviceInfo::ALTERNATE_PAN_COORDINATOR
									| ieee::DeviceInfo::DEVICE_TYPE_FFD
									| ieee::DeviceInfo::POWER_SOURCE_MAINS
									| ieee::DeviceInfo::RX_ON_WHEN_IDLE
									| ieee::DeviceInfo::ALLOCATE_SHORT_ADDRESS);

								// manufacturer code
								w.u16L(0x0000);

								// max buffer size
								w.u8(80);

								// max incoming transfer size
								w.u16L(160);

								// server flags
								w.e16L(zb::ZdpNodeDescriptorServerFlags::PRIMARY_TRUST_CENTER
									| zb::ZdpNodeDescriptorServerFlags::REVISION_22);

								// max outgoing transfer size
								w.u16L(160);

								// descriptor capability field
								w.u8(0x00);

								writeFooter(w, device.sendFlags);
							}

							// send packet
							uint8_t sendResult;
							co_await Radio::send(RADIO_ZBEE, packet, sendResult);
						}
						break;
					default:
						;
					}
				} else {
					// zdp response: forward response to the coroutine that initiated the request
					uint8_t *response = r.current;
					int length = min(r.getRemaining(), ZbDevice::RESPONSE_LENGTH);
					device.zdpResponseBarrier.resumeFirst([command, zdpCounter, length, response](ZbDevice::ZdpResponse r) {
						if (r.command == command && r.zdpCounter == zdpCounter) {
							r.length = length;
							array::copy(length, r.response, response);
							return true;
						}
						return false;
					});
				}

				continue;
			}

			// zcl
			// ---
			//handleZcl(r, device, destinationEndpoint);

			auto cluster = r.e16L<zb::ZclCluster>();
			auto profile = r.e16L<zb::ZclProfile>();
			uint8_t srcEndpoint = r.u8();
			uint8_t apsCounter = r.u8();

			// zcl frame starts here
			auto frameControl = r.e8<zb::ZclFrameControl>();
			uint8_t zclCounter = r.u8();

			auto frameType = frameControl & zb::ZclFrameControl::TYPE_MASK;
			if (frameType == zb::ZclFrameControl::TYPE_PROFILE_WIDE) {
				// profile wide commands such as "read attribute response"
				uint8_t command = r.u8();
				uint8_t *response = r.current;
				int length = min(r.getRemaining(), ZbDevice::RESPONSE_LENGTH);

				device.zclResponseBarrier.resumeFirst([dstEndpoint, cluster, profile, srcEndpoint, zclCounter,
					command, length, response] (ZbDevice::ZclResponse &r)
				{
					if (r.dstEndpoint == dstEndpoint && r.cluster == cluster && r.profile == profile
						&& r.srcEndpoint == srcEndpoint && r.zclCounter == zclCounter && r.command == zb::ZclCommand(command))
					{
						r.length = length;
						array::copy(length, r.response, response);
						return true;
					}
					return false;
				});
			} else if (frameType == zb::ZclFrameControl::TYPE_CLUSTER_SPECIFIC) {
				// cluster specific commands such as "on", "off"
				bool sendDefaultResponse = (frameControl & zb::ZclFrameControl::DISABLE_DEFAULT_RESPONSE) == 0;

				// debug-print command
				uint8_t command = *r.current;
				int length = r.getRemaining() - 1;
				//std::cout << "Device: " << std::hex << device->shortAddress << std::dec << ", ZCL command: " << int(command) << ", length: " << length << std::endl;
		Terminal::out << ("Device: " + hex(device->shortAddress) + ", ZCL Command: " + dec(command) + ", Length: " + dec(length) + '\n');

				// publish to subscribers
				auto commandPtr = r.current;
				for (auto &subscriber : device.subscribers) {
					// get endpoint info for subscribed endpoint
					uint8_t const *p = device->getEndpointIndices() + subscriber.index;
					auto const &endpointInfo = endpointInfos[p[0]];
					uint8_t zbEndpoint = p[1];

					// check if this is the right endpoint
					if (dstEndpoint == zbEndpoint && cluster == endpointInfo.cluster) {
						// trigger subscriber
						subscriber.barrier->resumeFirst([&subscriber, &r, &endpointInfo] (Subscriber::Parameters &p) {
							p.subscriptionIndex = subscriber.subscriptionIndex;

							// convert from zbee command to message (note r is passed by value for multiple subscribers)
							return handleZclClusterSpecific(subscriber.messageType, p.message, endpointInfo, r);
						});

						// reset reader to command
						r.current = commandPtr;
					}
				}

				// trigger default response
				if (sendDefaultResponse) {
					// cluster command (e.g. 'on' or 'off' for on/off cluster)
					uint8_t command = r.u8();

					// build default response packet
					{
						PacketWriter w(packet);

						// nwk data
						writeNwkData(w, device);

						// aps data (exchange source and destination endpoints)
						writeApsDataZcl(w, srcEndpoint, cluster, profile, dstEndpoint);

						// zcl profile wide
						w.e8(zb::ZclFrameControl::TYPE_PROFILE_WIDE
							| zb::ZclFrameControl::DIRECTION_SERVER_TO_CLIENT
							| zb::ZclFrameControl::DISABLE_DEFAULT_RESPONSE);
						w.u8(zclCounter);
						w.e8(zb::ZclCommand::DEFAULT_RESPONSE);

						// response to command
						w.u8(command);

						// success
						w.u8(0);

						writeFooter(w, device.sendFlags);
					}

					// send packet
					uint8_t sendResult;
					co_await Radio::send(RADIO_ZBEE, packet, sendResult);
				}
			}
		}
	}
}

void RadioInterface::handleGp(uint8_t const *mac, PacketReader &r) {
	// zgp stub nwk header

	// set start of header
	r.setHeader();

	// frame control
	auto frameControl = r.e8<gp::NwkFrameControl>();

	// extended frame conrol
	auto extendedFrameControl = gp::NwkExtendedFrameControl::NONE;
	if ((frameControl & gp::NwkFrameControl::EXTENDED) != 0)
		extendedFrameControl = r.e8<gp::NwkExtendedFrameControl>();
	auto securityLevel = extendedFrameControl & gp::NwkExtendedFrameControl::SECURITY_LEVEL_MASK;

	// device id
	uint32_t deviceId = r.u32L();

	if (this->commissioning && securityLevel == gp::NwkExtendedFrameControl::SECURITY_LEVEL_NONE
		&& r.peekE8<gp::Command>() == gp::Command::COMMISSIONING)
	{
		// commissioning
		handleGpCommission(deviceId, r);
		return;
	}

	// search device
	for (auto &device : this->gpDevices) {
		if (device->deviceId == deviceId) {
			auto const &flash = *device;

			// security
			// --------
			// header: header that is not encrypted, payload is part of header for security levels 0 and 1
			// payload: payload that is encrypted, has zero length for security levels 0 and 1
			// mic: message integrity code, 2 or 4 bytes
			uint32_t securityCounter;
			int micLength;
			switch (securityLevel) {
			// todo: compare securityLevel with security level from commissioning
			/*case gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT8_MIC16:
				// security level 1: 1 byte counter, 2 byte mic

				// header starts at mac sequence number and includes also payload
				r.setHeader(mac + 2);

				// use mac sequence number as security counter
				securityCounter = mac[2];

				// only decrypt message integrity code of length 2
				micLength = 2;
				r.setMessageFromEnd(micLength);
				break;*/
			case gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT32_MIC32:
				// security level 2: 4 byte counter, 4 byte mic

				// security counter
				securityCounter = r.u32L();

				// only decrypt message integrity code of length 4
				micLength = 4;
				r.setMessageFromEnd(micLength);
				break;
			case gp::NwkExtendedFrameControl::SECURITY_LEVEL_ENC_CNT32_MIC32:
				// security level 3: 4 byte counter, encrypted message, 4 byte mic

				// security counter
				securityCounter = r.u32L();

				// decrypt message and message integrity code of length 4
				r.setMessage();
				micLength = 4;
				break;
			default:
				// security is required
				return;
			}

			// check if security counter is greater than last security counter
			if (securityCounter <= device.securityCounter)
				return;
			device.securityCounter = securityCounter;

			// check message integrity code or decrypt message, depending on security level
			Nonce nonce(deviceId, securityCounter);
			if (!r.decrypt(micLength, nonce, flash.aesKey)) {
				//printf("error while decrypting message!\n");
				return;
			}

			int endpointIndex = -1;
			uint8_t message = 0;
			switch (flash.deviceType) {
			case 0x02:
				if (r.getRemaining() >= 1) {
					uint8_t command = r.u8();
					switch (command) {
					// A
					case 0x14:
					case 0x15:
						endpointIndex = 0;
						message = 0;
						break;
					case 0x10:
						endpointIndex = 0;
						message = 1;
						break;
					case 0x11:
						endpointIndex = 0;
						message = 2;
						break;
					// B
					case 0x17:
					case 0x16:
						endpointIndex = 1;
						message = 0;
						break;
					case 0x13:
						endpointIndex = 1;
						message = 1;
						break;
					case 0x12:
						endpointIndex = 1;
						message = 2;
						break;
					// AB
					case 0x65:
					case 0x63:
						endpointIndex = 2;
						message = 0;
						break;
					case 0x64:
						endpointIndex = 2;
						message = 1;
						break;
					case 0x62:
						endpointIndex = 2;
						message = 2;
						break;
					}
				}
				break;
			case 0x07:
				if (r.getRemaining() >= 2) {
					uint8_t bar = r.u8();
					uint8_t buttons = r.u8();

					uint8_t change = (buttons ^ device.state) & 0x0f;

					// check AB0 and AB1 (A and B pressed simultaneously)
					if (change == 0x5 || change == 0xa) {
						endpointIndex = 2;
						message = buttons & 0x3;
					} else {
						// check A0 and A1
						if (change & 0x3) {
							endpointIndex = 0;
							message = buttons & 0x3;
						}

						// check B0 and B1
						if (change & 0xc) {
							endpointIndex = 1;
							message = (buttons >> 2) & 0x3;
						}
					}

					device.state = buttons;
				}
			}

			if (endpointIndex != -1) {
				// publish to subscribers
				for (auto &subscriber : device.subscribers) {
					// check if this is the right endpoint
					if (subscriber.index == endpointIndex) {
						subscriber.barrier->resumeFirst([&subscriber, &r, &message] (Subscriber::Parameters &p) {
							p.subscriptionIndex = subscriber.subscriptionIndex;
							return convert(subscriber.messageType, p.message, MessageType::UP_DOWN, &message);
						});
					}
				}
			}
			break;
		}
	}
}

void RadioInterface::handleGpCommission(uint32_t deviceId, PacketReader& r) {
	// remove commissioning command (0xe0)
	r.u8();

	GpDeviceFlash flash;
	flash.deviceId = deviceId;

	// A.4.2.1.1 Commissioning

	// device type
	// 0x02: on/off switch
	flash.deviceType = r.u8();

	//device.endpointTypes[0] = EndpointType(0);
	switch (flash.deviceType) {
	case 0x02:
		// switch, PTM215Z ("friends of hue")
		flash.endpoints[0] = EndpointType::UP_DOWN_IN;
		flash.endpoints[1] = EndpointType::UP_DOWN_IN;
		flash.endpoints[2] = EndpointType::UP_DOWN_IN;
		flash.endpointCount = 3;
		break;
	case 0x07:
		// generic switch, PTM216Z
		flash.endpoints[0] = EndpointType::UP_DOWN_IN;
		flash.endpoints[1] = EndpointType::UP_DOWN_IN;
		flash.endpoints[2] = EndpointType::UP_DOWN_IN;
		flash.endpointCount = 3;
		break;
	default:
		// unknown device type
		return;
	}

	// options
	auto options = r.e8<gp::Options>();
	uint32_t counter = 0;
	if ((options & gp::Options::EXTENDED) != 0) {
		auto extendedOptions = r.e8<gp::ExtendedOptions>();;

		// security level capability (used in messages)
		auto securityLevel = extendedOptions & gp::ExtendedOptions::SECURITY_LEVEL_CAPABILITY_MASK;

		// check for key
		if ((extendedOptions & gp::ExtendedOptions::KEY_PRESENT) != 0) {
			uint8_t *key = r.current;
			if ((extendedOptions & gp::ExtendedOptions::KEY_ENCRYPTED) != 0) {
				// Green Power A.1.5.3.3.3

				// nonce
				Nonce nonce(deviceId, deviceId);

				// construct a header containing the device id
				DataBuffer<4> header;
				header.setU32L(0, deviceId);

				if (!decrypt(key, header.data, 4, key, 16, 4, nonce, za09LinkAesKey)) {
					//printf("error while decrypting key!\n");
					return;
				}

				// skip key and MIC
				r.skip(16 + 4);
			} else {
				// skip key
				r.skip(16);
			}

			// set key for device
			setKey(flash.aesKey, Array<uint8_t const, 16>(key));
		}
		if ((extendedOptions & gp::ExtendedOptions::COUNTER_PRESENT) != 0) {
			counter = r.u32L();
			//printf("counter: %08x\n", counter);
		}
	}

	// check if we exceeded the end
	if (r.getRemaining() < 0)
		return;

	// check if device already exists
	for (auto &device : this->gpDevices) {
		if (device->deviceId == deviceId) {
			// yes: only update security counter
			device.securityCounter = counter;
			return;
		}
	}

	// create device state
	auto* device = new GpDevice(flash);
	device->securityCounter = counter;

	// store device in flash
	this->gpDevices.write(this->gpDevices.count(), device);
}

AwaitableCoroutine RadioInterface::handleAssociationRequest(uint64_t sourceAddress, Radio::SendFlags sendFlags) {
Terminal::out << ("handleAssociationRequest\n");

	ZbDeviceFlash flash;
	flash.longAddress = sourceAddress;
	flash.shortAddress = this->nextShortAddress;
	flash.sendFlags = sendFlags;

	// create device
	Pointer<ZbDevice> device = new ZbDevice(flash);

	// set first hop
	device->routerAddress = flash.shortAddress;

	// set pointers so that we can forward responses
	this->tempDevice = device.ptr;


	uint8_t packet1[ZbDevice::RESPONSE_LENGTH > 80 ? ZbDevice::RESPONSE_LENGTH : 80]; // space for key transport or response
	uint8_t packet2[ZbDevice::RESPONSE_LENGTH > 48 ? ZbDevice::RESPONSE_LENGTH : 48]; // space for zdp request or response
	uint8_t sendResult;
	zb::ZdpCommand zdpCommand;
	uint16_t length;
	uint64_t longAddress = Radio::getLongAddress();

	// send association response
	{
		// get global configuration (is valid until next co_await)
		//auto const &configuration = *this->configuration;

		PacketWriter w(packet1);

		// ieee 802.15.4 frame control
		w.e16L(ieee::FrameControl::TYPE_COMMAND
			| ieee::FrameControl::ACKNOWLEDGE_REQUEST
			| ieee::FrameControl::PAN_ID_COMPRESSION
			| ieee::FrameControl::DESTINATION_ADDRESSING_LONG
			| ieee::FrameControl::SOURCE_ADDRESSING_LONG);

		// sequence number
		w.u8(this->macCounter++);

		// destination pan
		w.u16L(this->panId);

		// destination address
		w.u64L(flash.longAddress);

		// source address
		w.u64L(longAddress);

		// association response
		w.e8(ieee::Command::ASSOCIATION_RESPONSE);

		// short address of device
		w.u16L(flash.shortAddress);

		// association status
		w.u8(0);

		// always await a data request packet from the device before sending the association response
		w.finish(Radio::SendFlags::AWAIT_DATA_REQUEST);
	}
Terminal::out << ("Send Association Response\n");
	co_await Radio::send(RADIO_ZBEE, packet1, sendResult);
Terminal::out << ("Send Association Response Result " + dec(sendResult) + '\n');
	if (sendResult == 0)
		co_return;

	// send network key
	for (int retry = 0; ; ++retry) {
		{
			PacketWriter w(packet1);

			// nwk data (no security)
			writeNwkDataNoSecurity(w, *device);

			// aps command: transport key
			writeApsCommand(w, zb::SecurityControl::KEY_KEY_TRANSPORT, zb::ApsCommand::TRANSPORT_KEY);

			// key type
			w.e8(zb::StandardKeyType::NETWORK);

			// network key
			w.data(*this->key);

			// key sequence number
			w.u8(0);

			// extended destination address
			w.u64L(flash.longAddress);

			// extended source address
			w.u64L(longAddress);

			writeFooter(w, sendFlags);
		}
		co_await Radio::send(RADIO_ZBEE, packet1, sendResult);
Terminal::out << ("Send Key Result " + dec(sendResult) + '\n');
		if (sendResult != 0)
			break;

		if (retry == MAX_RETRY)
			co_return;
	}

	// get node descriptor
	for (int retry = 0; ; ++retry) {
		// send node descriptor request
		uint8_t zdpCounter;
		{
			PacketWriter w(packet1);

			// nwk data
			writeNwkData(w, *device);

			// zdp data
			zdpCounter = writeApsDataZdp(w, zb::ZdpCommand::NODE_DESCRIPTOR_REQUEST);

			// address of interest
			w.u16L(flash.shortAddress);

			writeFooter(w, sendFlags);
		}
		co_await Radio::send(RADIO_ZBEE, packet1, sendResult);
Terminal::out << ("Send Node Descriptor Request Result " + dec(sendResult) + '\n');
		if (sendResult != 0) {
			// wait for a response from the device
			int r = co_await select(device->zdpResponseBarrier.wait(zb::ZdpCommand::NODE_DESCRIPTOR_RESPONSE, zdpCounter,
				length, packet1), Timer::sleep(1s));

			// check if response was received
			if (r == 1)
				break;
		}
		if (retry == MAX_RETRY)
			co_return;
	}

	// handle node descriptor
	{
		PacketReader nodeDescriptor(length, packet1);
		uint8_t status = nodeDescriptor.u8();
		uint16_t addressOfInterest = nodeDescriptor.u16L();
Terminal::out << ("Node Descriptor Status " + dec(status) + '\n');
	}

	// get list of active endpoints
	for (int retry = 0; ; ++retry) {
		// send active endpoint request
		uint8_t zdpCounter;
		{
			PacketWriter w(packet1);

			// nwk data
			writeNwkData(w, *device);

			// zdp data
			zdpCounter = writeApsDataZdp(w, zb::ZdpCommand::ACTIVE_ENDPOINT_REQUEST);

			// address of interest
			w.u16L(flash.shortAddress);

			writeFooter(w, sendFlags);
		}
		co_await Radio::send(RADIO_ZBEE, packet1, sendResult);
		if (sendResult != 0) {
			// wait for a response from the device
			int r = co_await select(device->zdpResponseBarrier.wait(zb::ZdpCommand::ACTIVE_ENDPOINT_RESPONSE, zdpCounter,
				length, packet1), Timer::sleep(1s));

			// check if response was received
			if (r == 1)
				break;
		}
		if (retry == MAX_RETRY)
			co_return;
	}

	// handle list of active endpoints
	PacketReader endpoints(length, packet1);
	{
		uint8_t status = endpoints.u8();
		uint16_t addressOfInterest = endpoints.u16L();
Terminal::out << ("active endpoints status " + dec(status) + '\n');
	}

	// get descriptor of each endpoint
	uint8_t zbEndpointCount = endpoints.u8();
	int endpointCount = 0;
	for (int i = 0; i < zbEndpointCount; ++i) {
		// get next endpoint
		uint8_t endpoint = endpoints.u8();

		// get endpoint descriptor
		uint8_t zdpCounter;
		for (int retry = 0; ; ++retry) {
			// send simple descriptor request
			{
				PacketWriter w(packet2);

				// nwk data
				writeNwkData(w, *device);

				// zdp data
				zdpCounter = writeApsDataZdp(w, zb::ZdpCommand::SIMPLE_DESCRIPTOR_REQUEST);

				// address of interest
				w.u16L(flash.shortAddress);

				// endpoint to query
				w.u8(endpoint);

				writeFooter(w, sendFlags);
			}
			co_await Radio::send(RADIO_ZBEE, packet2, sendResult);
			if (sendResult != 0) {
				// wait for a response from the device
				int r = co_await select(device->zdpResponseBarrier.wait(zb::ZdpCommand::SIMPLE_DESCRIPTOR_RESPONSE,
					zdpCounter, length, packet2), Timer::sleep(timeout));

				// check if response was received
				if (r == 1)
					break;
			}
			if (retry == MAX_RETRY)
				co_return;
		}

		// handle endpoint descriptor
		PacketReader endpointDescriptor(length, packet2);
		{
			uint8_t status = endpointDescriptor.u8();
			uint16_t addressOfInterest = endpointDescriptor.u16L();
			uint8_t descriptorLength = endpointDescriptor.u8();
			uint8_t endpoint2 = endpointDescriptor.u8();
			zb::ZclProfile profile = endpointDescriptor.e16L<zb::ZclProfile>();
			uint16_t applicationDevice = endpointDescriptor.u16L();
			uint8_t applicationVersion = endpointDescriptor.u8();
Terminal::out << ("Endpoint Descriptor " + dec(endpoint) + " Status " + dec(status) + '\n');
		}

		// iterate over input/server clusters
		uint8_t inputClusterCount = endpointDescriptor.u8();
		for (int i = 0; i < inputClusterCount && endpointDescriptor.getRemaining() >= 3; ++i) {
			zb::ZclCluster cluster = endpointDescriptor.e16L<zb::ZclCluster>();

			// add our device endpoints based on input clusters
			for (int index = 0; index < array::count(endpointInfos); ++index) {
				auto const &info = endpointInfos[index];
				if (info.role == Role::SERVER /*&& info.profile == profile*/ && info.cluster == cluster) {
					// store endpoint type
					flash.endpoints[endpointCount] = uint8_t(info.endpointType);

					// also store index in enpointInfos and zb endpoint
					flash.endpoints[ZbDeviceFlash::MAX_ENDPOINT_COUNT + endpointCount * 2] = index;
					flash.endpoints[ZbDeviceFlash::MAX_ENDPOINT_COUNT + endpointCount * 2 + 1] = endpoint;
					++endpointCount;
					break;
				}
			}
		}

		// iterate over output/client clusters (client sends commmands to manipulate attributes in server)
		uint8_t outputClusterCount = endpointDescriptor.u8();
		for (int i = 0; i < outputClusterCount && endpointDescriptor.getRemaining() >= 2; ++i) {
			zb::ZclCluster cluster = endpointDescriptor.e16L<zb::ZclCluster>();

			// add our device endpoints based on output clusters
			for (int index = 0; index < array::count(endpointInfos); ++index) {
				auto const &info = endpointInfos[index];
				if (info.role == Role::CLIENT /*&& info.profile == profile*/ && info.cluster == cluster) {
					// store endpoint type
					flash.endpoints[endpointCount] = uint8_t(info.endpointType);

					// also store index in enpointInfos and zb endpoint
					flash.endpoints[ZbDeviceFlash::MAX_ENDPOINT_COUNT + endpointCount * 2] = index;
					flash.endpoints[ZbDeviceFlash::MAX_ENDPOINT_COUNT + endpointCount * 2 + 1] = endpoint;
					++endpointCount;
					break;
				}
			}
		}
	}

	// check some endpoints if they are available, e.g. battery percentage
	for (int i = 0; i < endpointCount; ++i) {
		auto endpointType = EndpointType(flash.endpoints[i]);
		if (endpointType == EndpointType::BATTERY_LEVEL_IN) {
			uint8_t index = flash.endpoints[ZbDeviceFlash::MAX_ENDPOINT_COUNT + i * 2];
			auto const &info = endpointInfos[index];
			uint8_t endpoint = flash.endpoints[ZbDeviceFlash::MAX_ENDPOINT_COUNT + i * 2 + 1];
			uint8_t zclCounter = this->zclCounter++;
			for (int retry = 0; ; ++retry) {
				// send read attributes
				{
					PacketWriter w(packet1);

					// nwk header
					writeNwkData(w, *device);

					// aps data
					writeApsDataZcl(w, endpoint, zb::ZclCluster::BASIC, zb::ZclProfile::HOME_AUTOMATION, endpoint);

					// zcl profile wide
					w.e8(zb::ZclFrameControl::TYPE_PROFILE_WIDE
						| zb::ZclFrameControl::DIRECTION_CLIENT_TO_SERVER
						| zb::ZclFrameControl::DISABLE_DEFAULT_RESPONSE);
					w.u8(zclCounter);
					w.e8(zb::ZclCommand::READ_ATTRIBUTES);
					w.e16L(zb::ZclBasicAttribute::POWER_SOURCE);

					writeFooter(w, sendFlags);
				}
				co_await Radio::send(RADIO_ZBEE, packet1, sendResult);
				if (sendResult != 0) {
					// wait for a response from the device
					int r = co_await select(device->zclResponseBarrier.wait(endpoint, zb::ZclCluster::BASIC,
						zb::ZclProfile::HOME_AUTOMATION, endpoint, zclCounter,
						zb::ZclCommand::READ_ATTRIBUTES_RESPONSE, length, packet2), Timer::sleep(timeout));

					// check if response was received
					if (r == 1)
						break;
				}
				if (retry == MAX_RETRY)
					co_return;
			}

			// handle read attribute response
			PacketReader attributeResponse(length, packet2);
			auto attribute = attributeResponse.e16L<zb::ZclBasicAttribute>();
			uint8_t status = attributeResponse.u8();
			auto type = attributeResponse.e8<zb::ZclDataType>();
			auto source = attributeResponse.e8<zb::ZclPowerSourceType>();
Terminal::out << ("Power Source Attribute Status " + dec(status) + " Source " + dec(source) + '\n');

			// remove endpoint if the device is not battery powered
			if (source != zb::ZclPowerSourceType::BATTERY) {
				--endpointCount;

				for (int j = i; j < endpointCount; ++j) {
					flash.endpoints[j] = flash.endpoints[j + 1];
					flash.endpoints[ZbDeviceFlash::MAX_ENDPOINT_COUNT + j * 2] = flash.endpoints[ZbDeviceFlash::MAX_ENDPOINT_COUNT + j * 2 + 2];
					flash.endpoints[ZbDeviceFlash::MAX_ENDPOINT_COUNT + j * 2 + 1] = flash.endpoints[ZbDeviceFlash::MAX_ENDPOINT_COUNT + j * 2 + 2 + 1];
				}
			}
		}
	}
	flash.endpointCount = endpointCount;

	// bind endpoints that send a command (e.g. on/off cluster of a wall switch)
	for (int i = 0; i < endpointCount; ++i) {
		auto endpointType = EndpointType(flash.endpoints[i]);
		EndpointInfo const &info = endpointInfos[flash.endpoints[ZbDeviceFlash::MAX_ENDPOINT_COUNT + i * 2]];
		uint8_t endpoint = flash.endpoints[ZbDeviceFlash::MAX_ENDPOINT_COUNT + i * 2 + 1];

		// check if we have to bind this cluster
		//if (info.bind) {
		if (info.role == Role::CLIENT && info.mode == Mode::COMMAND) {
			// bind request
Terminal::out << ("Bind Request to Endpoint " + dec(endpoint) + ", Cluster " + hex(info.cluster) + '\n');
			do {
				// wait until device is ready to receive
				//co_await deviceState->dataRequestBarrier.wait();

				// send bind request
				uint8_t zdpCounter;
				{
					// get global configuration
					//auto const &configuration = *this->configuration;

					PacketWriter w(packet1);

					// nwk data
					writeNwkData(w, *device);

					// zdp data
					zdpCounter = writeApsDataZdp(w, zb::ZdpCommand::BIND_REQUEST);

					// source
					w.u64L(flash.longAddress);

					// source endpoint
					w.u8(endpoint);

					// cluster
					w.e16L(info.cluster);

					// address mode: unicast
					w.u8(3);

					// destination
					w.u64L(longAddress);

					// destination endpoint
					w.u8(endpoint);

					writeFooter(w, sendFlags);
				}
				co_await Radio::send(RADIO_ZBEE, packet1, sendResult);

				// try again if send was not successful
				if (!sendResult)
					continue;

				// wait for a response from the device
				int r = co_await select(device->zdpResponseBarrier.wait(zb::ZdpCommand::BIND_RESPONSE, zdpCounter,
					length, packet1), Timer::sleep(300ms));
				if (r == 2)
					continue;
			} while (false);

			// handle bind response
			PacketReader bindResponse(length, packet1);
			uint8_t status = bindResponse.u8();
Terminal::out << ("bind response status " + dec(status) + '\n');
		}
	}


	// move pairs of info index and zbee endpoint
	for (int i = 0; i < endpointCount * 2; ++i) {
Terminal::out << ("endpoint info " + dec(flash.endpoints[ZbDeviceFlash::MAX_ENDPOINT_COUNT + i]) + '\n');
		flash.endpoints[endpointCount + i] = flash.endpoints[ZbDeviceFlash::MAX_ENDPOINT_COUNT + i];
	}

	// start a publisher coroutine for this device
	//device->publisher = publisher(*device);

	// check if the device already exists
	int index = this->zbDevices.count();
	for (int i = 0; i < this->zbDevices.count(); ++i) {
		if (this->zbDevices[i]->longAddress == flash.longAddress) {
			// yes: overwrite
			index = i;
			break;
		}
	}

	// write the configured device to flash
	this->zbDevices.write(index, std::move(device));

	// allocate next short address
	allocateNextAddress();
}

Coroutine RadioInterface::publish() {
	uint8_t packet[64];
	while (true) {
		// wait until something was published
		co_await this->publishEvent.wait();

		// iterate over devices
		bool dirty = false;
		for (auto &device : this->zbDevices) {
			// iterate over publishers
			for (auto &publisher : device.publishers) {
				// check if publisher wants to publish
				if (publisher.dirty) {
					publisher.dirty = false;
					dirty = true;

					// get endpoint info
					uint8_t endpointIndex = publisher.index;
					EndpointType endpointType = EndpointType(device->endpoints[endpointIndex]);
					uint8_t const *p = device->getEndpointIndices() + endpointIndex;
					EndpointInfo const &endpointInfo = endpointInfos[p[0]];
					uint8_t zbEndpointIndex = p[1];

					if ((endpointType & EndpointType::OUT) == EndpointType::OUT) {

						// request the route if necessary
						for (int retry = 0; retry < MAX_RETRY; ++retry) {
							if (device.routerAddress != 0xffff)
								break;

							// reset cost of route
							device.cost = 255;

							// build packet
							{
								PacketWriter w(packet);
								auto const &flash = *device;

								// route request command
								writeNwkBroadcastCommand(w, zb::NwkCommand::ROUTE_REQUEST);
								w.e8(zb::NwkCommandRouteRequestOptions::DISCOVERY_SINGLE
									| zb::NwkCommandRouteRequestOptions::EXTENDED_DESTINATION);

								// route id
								w.u8(this->routeCounter++);

								// destination
								w.u16L(flash.shortAddress);

								// path cost
								w.u8(0);

								// extended destination
								w.u64L(flash.longAddress);

								writeFooter(w, Radio::SendFlags::NONE);
							}

							// send packet
							uint8_t sendResult;
							co_await Radio::send(RADIO_ZBEE, packet, sendResult);
							if (sendResult == 0)
								continue;

							// wait for first route reply or timeout (more route replies with lower cost may arrive later)
							co_await select(device.routeBarrier.wait(), Timer::sleep(timeout));

				Terminal::out << "Router for " << hex(device->shortAddress) << ": " << hex(device.routerAddress) << '\n';
						}

						// fail if route remains unknown
						if (device.routerAddress == 0xffff) {
							// todo: set device to failed state and notify failure
							continue;
						}

						// try to send
						uint8_t zclCounter = this->zclCounter++;
						for (int retry = 0; retry <= MAX_RETRY; ++retry) {
							// build packet
							{
								PacketWriter w(packet);
								auto const &flash = *device;

								// nwk data
								writeNwkData(w, device);

								// aps data
								writeApsDataZcl(w, zbEndpointIndex, endpointInfo.cluster, zb::ZclProfile::HOME_AUTOMATION,
									zbEndpointIndex);

								if (endpointInfo.mode == Mode::COMMAND) {
									// send command

									// zcl cluster specific (conversion may fail)
									if (!writeZclClusterSpecific(w, zclCounter, endpointInfo,
										publisher.messageType, publisher.message))
									{
										break;
									}
								} else {
									// send attribute request

								}

								writeFooter(w, Radio::SendFlags::NONE);
							}

							// send packet
							uint8_t sendResult;
							co_await Radio::send(RADIO_ZBEE, packet, sendResult);
							if (sendResult != 0) {
								// wait for a response from the device
								uint16_t length;
								int r = co_await select(device.zclResponseBarrier.wait(zbEndpointIndex,
									endpointInfo.cluster, zb::ZclProfile::HOME_AUTOMATION, zbEndpointIndex, zclCounter,
									zb::ZclCommand::DEFAULT_RESPONSE, length, packet), Timer::sleep(timeout));

								// check if response was received
								if (r == 1) {
									MessageReader r(length, packet);
									uint8_t responseToCommand = r.u8();
									uint8_t status = r.u8();

									// todo: check status (0 = ok)

									break;
								}
							}
						}
					}

					// forward to subscribers
					for (auto &subscriber : device.subscribers) {
						if (subscriber.index == publisher.index) {
							subscriber.barrier->resumeAll([&subscriber, &publisher] (Subscriber::Parameters &p) {
								p.subscriptionIndex = subscriber.subscriptionIndex;

								// convert to target unit and type and resume coroutine if conversion was successful
								return convert(subscriber.messageType, p.message,
									publisher.messageType, publisher.message);
							});
						}
					}
				}
			}
		}

		// clear event when no dirty publisher was found
		if (!dirty)
			this->publishEvent.clear();
	}
}


// RadioInterface::GpDeviceFlash

int RadioInterface::GpDeviceFlash::size() const {
	return getOffset(GpDeviceFlash, endpoints[this->endpointCount]);
}

RadioInterface::GpDevice *RadioInterface::GpDeviceFlash::allocate() const {
	return new GpDevice(*this);
}


// RadioInterface::ZbDeviceFlash

int RadioInterface::ZbDeviceFlash::size() const {
	return getOffset(ZbDeviceFlash, endpoints[this->endpointCount * 3]);
}

RadioInterface::ZbDevice *RadioInterface::ZbDeviceFlash::allocate() const {
	return new ZbDevice(*this);
}
