#include "RadioInterface.hpp"
#include <radio.hpp>
#include <rng.hpp>
#include <sys.hpp>
#include <timer.hpp>
#include <crypt.hpp>
#include <Nonce.hpp>
#include <ieee.hpp>
#include <zb.hpp>
#include <gp.hpp>
#include <DataReader.hpp>
#include <DataWriter.hpp>
#include <Pointer.hpp>
#include <util.hpp>
#include <iostream>


// timeout to wait for a response (e.g. default response or route reply)
constexpr SystemDuration timeout = 500ms;

// number of retries when a send fails
constexpr int MAX_RETRY = 2;

static AesKey const trustCenterLinkAesKey = {{0x5a696742, 0x6565416c, 0x6c69616e, 0x63653039, 0x166d75b9, 0x730834d5, 0x1f6155bb, 0x7c046582, 0xe62066a9, 0x9528527c, 0x8a4907c7, 0xf64d6245, 0x018a08eb, 0x94a25a97, 0x1eeb5d50, 0xe8a63f15, 0x2dff5170, 0xb95d0be7, 0xa7b656b7, 0x4f1069a2, 0xf7066bf4, 0x4e5b6013, 0xe9ed36a4, 0xa6fd5f06, 0x83c904d0, 0xcd9264c3, 0x247f5267, 0x82820d61, 0xd01eebc3, 0x1d8c8f00, 0x39f3dd67, 0xbb71d006, 0xf36e8429, 0xeee20b29, 0xd711d64e, 0x6c600648, 0x3801d679, 0xd6e3dd50, 0x01f20b1e, 0x6d920d56, 0x41d66745, 0x9735ba15, 0x96c7b10b, 0xfb55bc5d}};
static AesKey const hashedTrustCenterLinkAesKey = {{0x4bab0f17, 0x3e1434a2, 0xd572e1c1, 0xef478782, 0xeabc1cc8, 0xd4a8286a, 0x01dac9ab, 0xee9d4e29, 0xb693b9e0, 0x623b918a, 0x63e15821, 0x8d7c1608, 0xa2d489bd, 0xc0ef1837, 0xa30e4016, 0x2e72561e, 0xea65fb8c, 0x2a8ae3bb, 0x8984a3ad, 0xa7f6f5b3, 0xb88396d0, 0x9209756b, 0x1b8dd6c6, 0xbc7b2375, 0xb9a50bb5, 0x2bac7ede, 0x3021a818, 0x8c5a8b6d, 0x479837d1, 0x6c34490f, 0x5c15e117, 0xd04f6a7a, 0x439aeda1, 0x2faea4ae, 0x73bb45b9, 0xa3f42fc3, 0xe78fc3ab, 0xc8216705, 0xbb9a22bc, 0x186e0d7f, 0x4e581106, 0x86797603, 0x3de354bf, 0x258d59c0}};
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

RadioInterface::RadioInterface(Configuration &configuration, PersistentStateManager &stateManager)
	: configuration(configuration)
	, securityCounter(configuration->radioSecurityCounterOffset)
{
	// set short address of coordinator
	radio::setShortAddress(RADIO_ZB, 0x0000);
	
	// filter packets with the coordinator as destination (and broadcast pan/address)
	radio::setFlags(RADIO_ZB, radio::ContextFlags::PASS_DEST_SHORT | radio::ContextFlags::HANDLE_ACK);

	// initialize
	init(stateManager);
	
//setCommissioning(true);
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
		return this->zbDevices[index - gpDeviceCount]->deviceId;
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
		if (flash.deviceId == deviceId) {
			return {flash.endpointCount, reinterpret_cast<EndpointType const *>(flash.endpoints)};
		}
	}
	return {};
}

void RadioInterface::addSubscriber(DeviceId deviceId, Subscriber &subscriber) {
	for (auto &device : this->gpDevices) {
		auto const &flash = *device;
		if (flash.deviceId == deviceId && subscriber.endpointIndex < flash.endpointCount) {
			device.subscribers.add(subscriber);
			return;
		}
	}
	for (auto &device : this->zbDevices) {
		auto const &flash = *device;
		if (flash.deviceId == deviceId && subscriber.endpointIndex < flash.endpointCount) {
			device.subscribers.add(subscriber);
			return;
		}
	}
}

void RadioInterface::addPublisher(DeviceId deviceId, Publisher &publisher) {
	for (auto &device : this->zbDevices) {
		auto const &flash = *device;
		if (flash.deviceId == deviceId && publisher.endpointIndex < flash.endpointCount) {
			device.publishers.add(publisher);
			publisher.event = &this->publishEvent;
			return;
		}
	}
}



RadioInterface::ZbDevice *RadioInterface::findZbDevice(uint16_t address) {
	for (auto &device : this->zbDevices) {
		auto const &flash = *device;
		if (flash.shortAddress == address)
			return &device;
	}
	return nullptr;
}

void RadioInterface::allocateNextAddress() {
	while (true) {
		uint16_t address = (rng::int8() << 8) | rng::int8();

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
	// get global configuration
	auto const &configuration = *this->configuration;

	// ieee 802.15.4 frame control
	auto ieeeFrameControl = ieee::FrameControl::TYPE_DATA
		| ieee::FrameControl::PAN_ID_COMPRESSION
		| ieee::FrameControl::DESTINATION_ADDRESSING_SHORT
		| ieee::FrameControl::SOURCE_ADDRESSING_SHORT;
	w.enum16(ieeeFrameControl);

	// sequence number
	w.uint8(this->macCounter++);
	
	// destination pan
	w.uint16(configuration.radioPanId);

	// ieee broadcast
	w.uint16(0xffff);

	// source address
	w.uint16(0x0000);
	

	// set start of header for encryption
	w.setHeader();

	// network layer frame control field
	auto nwkFrameControl = zb::NwkFrameControl::TYPE_COMMAND
		| zb::NwkFrameControl::VERSION_2
		| zb::NwkFrameControl::DISCOVER_ROUTE_SUPPRESS
		| zb::NwkFrameControl::SECURITY
		| zb::NwkFrameControl::EXTENDED_SOURCE;
	w.enum16(nwkFrameControl);

	// zbee broadcast
	w.uint16(0xfffc);
	
	// source
	w.uint16(0x0000);
	
	// radius
	w.uint8(1);
	
	// sequence number
	w.uint8(this->nwkCounter++);
	
	// extended source
	w.uint64(configuration.radioLongAddress);


	// security header
	if ((nwkFrameControl & zb::NwkFrameControl::SECURITY) != 0) {
		// security control field
		w.securityControlPtr = w.current;
		w.enum8(zb::SecurityControl::LEVEL_ENC_MIC32
			| zb::SecurityControl::KEY_NETWORK
			| zb::SecurityControl::EXTENDED_NONCE);

		// security counter
		w.uint32(this->securityCounter);

		// extended source
		w.uint64(configuration.radioLongAddress);

		// key sequence number
		w.uint8(0);
	} else {
		w.securityControlPtr = nullptr;
	}
		
	
	// set start of message for encryption
	w.setMessage();
	
	// command
	w.enum8(command);
}

void RadioInterface::writeNwk(PacketWriter &w, zb::NwkFrameControl nwkFrameControl, uint8_t radius, ZbDevice &device) {
	assert(device.firstHop != 0xffff);

	// get global configuration
	auto const &configuration = *this->configuration;

	// ieee 802.15.4 frame control
	auto ieeeFrameControl = ieee::FrameControl::TYPE_DATA
		| ieee::FrameControl::ACKNOWLEDGE_REQUEST
		| ieee::FrameControl::PAN_ID_COMPRESSION
		| ieee::FrameControl::DESTINATION_ADDRESSING_SHORT
		| ieee::FrameControl::SOURCE_ADDRESSING_SHORT;
	w.enum16(ieeeFrameControl);

	// sequence number
	w.uint8(this->macCounter++);
	
	// destination pan
	w.uint16(configuration.radioPanId);

	// ieee destination address (address of first hop)
	w.uint16(device.firstHop);

	// source address
	w.uint16(0x0000);
	
	
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
	w.enum16(nwkFrameControl);

	// zbee broadcast
	w.uint16(device->shortAddress);
	
	// source
	w.uint16(0x0000);
	
	// radius
	w.uint8(radius);
	
	// sequence number
	w.uint8(this->nwkCounter++);

	// destination
	if ((nwkFrameControl & zb::NwkFrameControl::DESTINATION) != 0)
		w.uint64(device->deviceId);

	// extended source
	if ((nwkFrameControl & zb::NwkFrameControl::EXTENDED_SOURCE) != 0)
		w.uint64(configuration.radioLongAddress);


	// security header
	if ((nwkFrameControl & zb::NwkFrameControl::SECURITY) != 0) {
		// security control field
		w.securityControlPtr = w.current;
		w.enum8(zb::SecurityControl::LEVEL_ENC_MIC32
			| zb::SecurityControl::KEY_NETWORK
			| zb::SecurityControl::EXTENDED_NONCE);

		// security counter
		w.uint32(this->securityCounter);

		// extended source
		w.uint64(configuration.radioLongAddress);

		// key sequence number
		w.uint8(0);
	} else {
		w.securityControlPtr = nullptr;
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

void RadioInterface::writeApsCommand(PacketWriter &w, zb::ApsFrameControl apsFrameControl) {
	// get global configuration
	auto const &configuration = *this->configuration;
	
	// set start of header for encryption
	w.setHeader();

	// application support layer frame control field
	w.enum8(zb::ApsFrameControl::TYPE_COMMAND | apsFrameControl);
	
	// counter
	w.uint8(this->apsCounter++);


	// security header
	if ((apsFrameControl & zb::ApsFrameControl::SECURITY) != 0) {
		// security control field
		w.securityControlPtr = w.current;
		w.enum8(zb::SecurityControl::LEVEL_ENC_MIC32
			| zb::SecurityControl::KEY_KEY_TRANSPORT
			| zb::SecurityControl::EXTENDED_NONCE);

		// security counter
		w.uint32(this->securityCounter);

		// extended source
		w.uint64(configuration.radioLongAddress);
	} else {
		w.securityControlPtr = nullptr;
	}
		
	// set start of message for encryption
	w.setMessage();
}

void RadioInterface::writeApsData(PacketWriter &w, uint8_t destinationEndpoint, zb::ZclCluster clusterId,
	zb::ZclProfile profile, uint8_t sourceEndpoint)
{
	// application support layer frame control field
	w.enum8(zb::ApsFrameControl::TYPE_DATA
		| zb::ApsFrameControl::DELIVERY_UNICAST);

	// destination endpoint
	w.uint8(destinationEndpoint);
	
	// cluster id
	w.enum16(clusterId);
	
	// profile
	w.enum16(profile);
	
	// source endpoint
	w.uint8(sourceEndpoint);

	// counter
	w.uint8(this->apsCounter++);
}

uint8_t RadioInterface::writeZdpData(PacketWriter &w, zb::ZdpCommand command) {
	// application support layer frame control field
	w.enum8(zb::ApsFrameControl::TYPE_DATA
		| zb::ApsFrameControl::DELIVERY_UNICAST);

	// destination endpoint (device)
	w.uint8(0);
	
	// cluster id
	w.enum16<zb::ZdpCommand>(command);
	
	// profile (device profile)
	w.uint16(0x0000);
	
	// source endpoint (device)
	w.uint8(0);

	// counter
	w.uint8(this->apsCounter++);
	
	
	// device profile
	uint8_t zdpCounter = this->zdpCounter++;
	w.uint8(zdpCounter);
	
	return zdpCounter;
}

bool writeZclClusterSpecific(RadioInterface::PacketWriter &w, uint8_t zclCounter, EndpointInfo const &info,
	MessageType srcType, void const *srcMessage)
{
	// zbee cluster library frame
	w.enum8(zb::ZclFrameControl::TYPE_CLUSTER_SPECIFIC
		| zb::ZclFrameControl::DIRECTION_CLIENT_TO_SERVER);
	w.uint8(zclCounter);

	Message const &src = *reinterpret_cast<Message const *>(srcMessage);

	switch (info.cluster) {
	case zb::ZclCluster::ON_OFF:
		switch (srcType) {
		case MessageType::ON_OFF:
			w.uint8(src.onOff);
			break;
		case MessageType::ON_OFF2:
			// invert on/off (0, 1, 2 -> 1, 0, 2)
			w.uint8(src.onOff ^ 1 ^ (src.onOff >> 1));
			break;
		case MessageType::TRIGGER:
			// trigger (e.g.button) toggles on/off
			if (src.trigger == 0)
				return false;
			w.uint8(2);
			break;
		case MessageType::UP_DOWN:
			// rocker switches off/on (1, 2 -> 0, 1)
			if (src.upDown == 0)
				return false;
			w.uint8(src.upDown - 1);
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
				w.enum8(zb::ZclLevelControlCommand::MOVE_TO_LEVEL_WITH_ON_OFF);
			} else {
				// increase/decrease level
				w.enum8(zb::ZclLevelControlCommand::STEP_WITH_ON_OFF);
				w.uint8(src.level >= 0 ? 0x00 : 0x01);
			}
			w.uint8(clamp(abs(int(src.level * 255.0f)), 0, 254));
			if (srcType == MessageType::LEVEL)
				w.uint16(0);
			else if (!src.moveToLevel.move.getFlag())
				w.uint16(clamp(int(src.moveToLevel.move * 10.0f), 0, 65534));
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

void RadioInterface::writeFooter(PacketWriter &w, radio::SendFlags sendFlags) {
	if (w.securityControlPtr != nullptr) {
		// get global configuration
		auto const &configuration = *this->configuration;

		auto securityControl = zb::SecurityControl(*w.securityControlPtr);
		
		// nonce (4.5.2.2)
		Nonce nonce(configuration.radioLongAddress, this->securityCounter, securityControl);

		// encrypt
		int micLength = 4;
		AesKey const &key = (securityControl & zb::SecurityControl::KEY_MASK) == zb::SecurityControl::KEY_NETWORK
			? configuration.aesKey : hashedTrustCenterLinkAesKey;
		//encrypt(w.message, nonce, w.header, headerLength, w.message, payloadLength, micLength, key);
		w.encrypt(micLength, nonce, key);

		//bool ok = decrypt(w.message, nonce, w.header, headerLength, w.message, payloadLength, micLength, key);

		// clear security level according to 4.3.1.1 step 8.
		*w.securityControlPtr &= ~zb::SecurityControl::LEVEL_MASK;

		// increment security counter
		++this->securityCounter;
	}
	w.finish(sendFlags);
}


Coroutine RadioInterface::init(PersistentStateManager &stateManager) {
	// restore security counter
	co_await this->securityCounter.restore(&stateManager);

	// start coroutines
	broadcast();
	sendBeacon();
	
	// start publish coroutines
	for (int i = 0; i < 1; ++i)
		publish();
	
	// start receiving
	receive();
}

Coroutine RadioInterface::broadcast() {
	uint8_t packet[52];
	uint8_t sendResult;
	auto linkTime = timer::now() + 1s;
	auto routeTime = linkTime + 1s;
	while (true) {
		// wait until the next timeout
		auto time = linkTime;//min(linkTime, routeTime);
		co_await timer::wait(time);

		if (time == linkTime) {
			// broadcast link status every 15 seconds
			linkTime += 15s;
			{
				PacketWriter w(packet);
				
				// link status command
				writeNwkBroadcastCommand(w, zb::NwkCommand::LINK_STATUS);
				w.enum8(zb::NwkCommandLinkStatusOptions(0) // link status count
					| zb::NwkCommandLinkStatusOptions::FIRST_FRAME
					| zb::NwkCommandLinkStatusOptions::LAST_FRAME);

				writeFooter(w, radio::SendFlags::NONE);
			}
			co_await radio::send(RADIO_ZB, packet, sendResult);
		}
		/*if (time == routeTime) {
			// broadcast many-to-one route request every 100 seconds
			routeTime += 100s;
			{
				PacketWriter w(packet);

				// many-to-one route request command
				writeNwkBroadcastCommand(w, zb::NwkCommand::ROUTE_REQUEST);
				w.enum8(zb::NwkCommandRouteRequestOptions::DISCOVERY_MANY_TO_ONE_WITH_SOURCE_ROUTING);
				
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
			auto const &configuration = *this->configuration;

			PacketWriter w(packet);

			// ieee 802.15.4 frame control
			w.enum16(ieee::FrameControl::TYPE_BEACON
				| ieee::FrameControl::SOURCE_ADDRESSING_SHORT);

			// sequence number
			w.uint8(this->macCounter++);

			// source pan
			w.uint16(configuration.radioPanId);

			// source address
			w.uint16(0x0000);

			// superframe specification
			w.uint16(15 // beacon interval
				| (15 << 4) // superframe interval
				| (15 << 8) // final cap slot
				| (0 << 12) // battery extension
				| 0x4000 // pan coordinator
				| (this->commissioning ? 0x8000 : 0)); // association permit

			// gts
			w.uint8(0);

			// pending addresses, short and long
			w.uint8(0x00);

			// beacon

			// protocol id
			w.uint8(0);

			// stack profile
			w.uint16(2 // zbee pro
				| (2 << 4) // protocol version
				| (1 << 10) // router capacity
				| (0 << 11) // device depth (4 bit)
				| (1 << 15)); // end device capacity

			// extended pan id
			w.uint64(UINT64_C(0xdddddddddddddddd));

			// tx offset
			w.uint16(0xffff);
			w.uint8(0xff);

			// update id
			w.uint8(0);
	
			w.finish(radio::SendFlags::NONE);
		}
		co_await radio::send(RADIO_ZB, packet, sendResult);
		
		// "cool down" before a new beacon can be sent
		co_await timer::sleep(100ms);
	}
}

Coroutine RadioInterface::receive() {
	while (true) {
		radio::Packet packet;
		co_await radio::receive(RADIO_ZB, packet);
		PacketReader r(packet);
		
		// ieee 802.15.4 mac
		uint8_t const *mac = r.current;

		// frame control
		auto frameControl = r.enum16<ieee::FrameControl>();

		// the radio filter flags ensure that a sequence number is present
		uint8_t macCounter = r.uint8();
		
		// the radio filter flags ensure that a destination pan id is present that is either broadcast or configuration.zbPanId
		uint16_t destinationPanId = r.uint16();
		bool destinationBroadcast = destinationPanId == 0xffff;
		
		// the radio filter flags ensure that a destination address is present hat is either boradcast or our address
		if ((frameControl & ieee::FrameControl::DESTINATION_ADDRESSING_LONG_FLAG) == 0) {
			// short destination address
			uint16_t destAddress = r.uint16();
			destinationBroadcast |= destAddress == 0xffff;
		} else {
			// long destination address is always our address
			r.skip(8);
		}

		auto frameType = frameControl & ieee::FrameControl::TYPE_MASK;
		if ((frameControl & ieee::FrameControl::SOURCE_ADDRESSING_FLAG) == 0) {
			// no source address
			if (frameType == ieee::FrameControl::TYPE_COMMAND) {
				auto command = r.enum8<ieee::Command>();
				if (command == ieee::Command::BEACON_REQUEST) {
					// handle beacon request by sending a beacon
sys::out.write("Beacon Request\n");
					this->beaconBarrier.resumeAll();
				}
			} else if (frameType == ieee::FrameControl::TYPE_DATA) {
				// network layer: handle dependent on protocol version
				auto version = r.peekEnum8<gp::NwkFrameControl>() & gp::NwkFrameControl::VERSION_MASK;
				if (version == gp::NwkFrameControl::VERSION_3_GP)
					handleGp(mac, r);
			}
		} else {
			// short or long source address

			// get source pan id
			uint16_t sourcePanId = destinationPanId;
			if ((frameControl & ieee::FrameControl::PAN_ID_COMPRESSION) == 0) {
				sourcePanId = r.uint16();
			}

			// check if address short or long
			ZbDevice *pDevice = nullptr;
			if ((frameControl & ieee::FrameControl::SOURCE_ADDRESSING_LONG_FLAG) == 0) {
				// short source address
				uint16_t sourceAddress = r.uint16();

				// check for command frame
				if (frameType == ieee::FrameControl::TYPE_COMMAND) {
					auto command = r.enum8<ieee::Command>();
				} else if (frameType == ieee::FrameControl::TYPE_DATA) {
					// try to lookup device by short address
					pDevice = findZbDevice(sourceAddress);

					// check if there is an active association coroutine
					if (this->associationCoroutine.isAlive()) {
						if ((*this->tempDevice)->shortAddress == sourceAddress) {
							pDevice = this->tempDevice;
						}
					}
				}
			} else {
				// long source address
				uint64_t sourceAddress = r.int64();

				// check for command frame
				if (frameType == ieee::FrameControl::TYPE_COMMAND) {
					auto command = r.enum8<ieee::Command>();

					// check for association request with no broadcast
					if (this->commissioning && !destinationBroadcast && command == ieee::Command::ASSOCIATION_REQUEST) {
						// cancel current association coroutine
						this->associationCoroutine.cancel();
						
						// start new association request
						auto associationRequest = r.enum8<ieee::AssociationRequest>();
						bool receiveOnWhenIdle = (associationRequest & ieee::AssociationRequest::RECEIVE_ON_WHEN_IDLE) != 0;
						auto sendFlags = receiveOnWhenIdle ? radio::SendFlags::NONE : radio::SendFlags::AWAIT_DATA_REQUEST;
sys::out.write("Association Request Receive On When Idle: " + dec(receiveOnWhenIdle) + '\n');
						this->associationCoroutine = handleAssociationRequest(sourceAddress, sendFlags);
					}
				} /*else if (frameType == ieee::FrameControl::TYPE_DATA) {
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

			// check if the device was found
			if (pDevice != nullptr) {
				auto &device = *pDevice;
				
				if (frameType == ieee::FrameControl::TYPE_DATA) {
					// network layer: handle dependent on protocol version
					auto version = r.peekEnum8<gp::NwkFrameControl>() & gp::NwkFrameControl::VERSION_MASK;
					if (version == gp::NwkFrameControl::VERSION_2) {
						handleNwk(r, device);
					}
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
	auto frameControl = r.enum8<gp::NwkFrameControl>();

	// extended frame conrol
	auto extendedFrameControl = gp::NwkExtendedFrameControl::NONE;
	if ((frameControl & gp::NwkFrameControl::EXTENDED) != 0)
		extendedFrameControl = r.enum8<gp::NwkExtendedFrameControl>();
	auto securityLevel = extendedFrameControl & gp::NwkExtendedFrameControl::SECURITY_LEVEL_MASK;
	
	// device id
	uint32_t deviceId = r.int32();

	if (this->commissioning && securityLevel == gp::NwkExtendedFrameControl::SECURITY_LEVEL_NONE
		&& r.peekEnum8<gp::Command>() == gp::Command::COMMISSIONING)
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
			int micLength = 4;
			switch (securityLevel) {
			// todo: compare securityLevel with security level from commissioning
			/*case gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT8_MIC16:
				// security level 1: 1 byte counter, 2 byte mic
			
				// header starts at mac sequence number and includes also payload
				r.setHeader(mac + 2);
				
				// use mac sequence number as security counter
				securityCounter = mac[2];
				
				micLength = 2;
				
				// encrypted message is empty, only message integrity code
				r.setMessage(r.end - micLength);
				break;*/
			case gp::NwkExtendedFrameControl::SECURITY_LEVEL_CNT32_MIC32:
				// security level 2: 4 byte counter, 4 byte mic

				// security counter
				securityCounter = r.int32();

				// encrypted message is empty, only message integrity code
				r.setMessage(r.end - micLength);
				break;
			case gp::NwkExtendedFrameControl::SECURITY_LEVEL_ENC_CNT32_MIC32:
				// security level 3: 4 byte counter, encrypted message, 4 byte mic

				// security counter
				securityCounter = r.int32();

				// set start of encrypted message
				r.setMessage();
				break;
			default:
				// error: security is required
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
					uint8_t command = r.uint8();
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
					uint8_t bar = r.uint8();
					uint8_t buttons = r.uint8();
					
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
					if (subscriber.endpointIndex == endpointIndex) {
						subscriber.barrier->resumeFirst([&subscriber, &r, &message] (Interface::Parameters &p) {
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
	r.uint8();
	
	GpDeviceFlash flash;
	flash.deviceId = deviceId;
	
	// A.4.2.1.1 Commissioning
		
	// device type
	// 0x02: on/off switch
	flash.deviceType = r.uint8();

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
	auto options = r.enum8<gp::Options>();
	uint32_t counter = 0;
	if ((options & gp::Options::EXTENDED) != 0) {
		auto extendedOptions = r.enum8<gp::ExtendedOptions>();;
		
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
				header.setLittleEndianInt32(0, deviceId);

				if (!decrypt(key, nonce, header.data, 4, key, 16, 4, trustCenterLinkAesKey)) {
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
			setKey(flash.aesKey, key);
		}
		if ((extendedOptions & gp::ExtendedOptions::COUNTER_PRESENT) != 0) {
			counter = r.int32();
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


void RadioInterface::handleNwk(PacketReader &r, ZbDevice &device) {
	auto const &configuration = *this->configuration;
	
	// set start of header
	r.setHeader();

	auto frameControl = r.enum16<zb::NwkFrameControl>();
	uint16_t destinationAddress = r.uint16();
	uint16_t sourceAddress = r.uint16();
	uint8_t radius = r.uint8();
	uint8_t sequenceNumber = r.uint8();

	// destination
	if ((frameControl & zb::NwkFrameControl::DESTINATION) != 0) {
		uint8_t const *destination = r.current;
		r.skip(8);
	}

	// extended source
	if ((frameControl & zb::NwkFrameControl::EXTENDED_SOURCE) != 0) {
		uint8_t const *extendedSource = r.current;
		r.skip(8);
	}

	// source route
	if ((frameControl & zb::NwkFrameControl::SOURCE_ROUTE) != 0) {
		uint8_t relayCount = r.uint8();
		uint8_t relayIndex = r.uint8();
		for (int i = 0; i < relayCount; ++i) {
			uint16_t relay = r.uint16();
		}
	}

	// security header
	// note: does in-place decryption of the payload
	uint8_t const *extendedSource = nullptr;
	if ((frameControl & zb::NwkFrameControl::SECURITY) != 0) {
		// restore security level according to 4.3.1.2 step 1.
		r.current[0] |= securityLevel;
		
		// security control field (4.5.1.1)
		auto securityControl = r.enum8<zb::SecurityControl>();
				
		// security counter
		uint32_t securityCounter = r.int32();

		// key type
		if ((securityControl & zb::SecurityControl::KEY_MASK) != zb::SecurityControl::KEY_NETWORK) {
			// only network key supported
			return;
		}

		// extended source
		if ((securityControl & zb::SecurityControl::EXTENDED_NONCE) == 0) {
			// only extended nonce supported
			return;
		}
		extendedSource = r.current;
		r.skip(8);
		
		// key sequence number
		uint8_t keySequenceNumber = r.uint8();

		// set start of message
		r.setMessage();

		// nonce (4.5.2.2)
		Nonce nonce(extendedSource, securityCounter, securityControl);
		
		// decrypt
		int micLength = 4;
		if (!r.decrypt(micLength, nonce, configuration.aesKey)) {
			// decryption failed
			return;
		}
	} else {
		// security is required
		return;
	}

	auto frameType = frameControl & zb::NwkFrameControl::TYPE_MASK;
	if (sourceAddress == device->shortAddress) {
		// if the device is a direct neighbor, we know the route to it
		device.firstHop = device->shortAddress;
		device.sendFlags = device->sendFlags;

		switch (frameType) {
		case zb::NwkFrameControl::TYPE_COMMAND:
			// nwk command
			handleNwkCommand(r, device);
			break;
		case zb::NwkFrameControl::TYPE_DATA:
			// nwk data
			handleAps(r, device, extendedSource);
			break;
		default:
			break;
		}
	} else {
		// try to lookup "real" source device by address (device at start of a route)
		for (auto &d : this->zbDevices) {
			auto const &flash = *d;
			if (flash.shortAddress == sourceAddress) {
				switch (frameType) {
				case zb::NwkFrameControl::TYPE_COMMAND:
					// nwk command
					handleNwkCommand(r, d);
					break;
				case zb::NwkFrameControl::TYPE_DATA:
					// nwk data
					handleAps(r, d, extendedSource);
					break;
				default:
					break;
				}
				break;
			}
		}
	}
}

void RadioInterface::handleNwkCommand(PacketReader &r, ZbDevice &device) {
	auto command = r.enum8<zb::NwkCommand>();
	switch (command) {
	case zb::NwkCommand::ROUTE_REPLY:
		{
			auto options = r.enum8<zb::NwkCommandRouteReplyOptions>();
			uint8_t id = r.uint8();
			uint16_t originator = r.uint16();
			uint16_t responder = r.uint16();
			uint8_t cost = r.uint8();
			if (originator == 0x0000) {
				ZbDevice *destination = findZbDevice(responder);
				if (destination != nullptr) {
					// set first hop of route
					destination->firstHop = device->shortAddress;
					destination->sendFlags = device->sendFlags;
					
					// resume publisher
					destination->routeBarrier.resumeFirst();
				}
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
			uint8_t capability = r.uint8();
			
			device.rejoinPending = true;
			
			this->publishEvent.set();
		}
	default:
		break;
	}
}

void RadioInterface::handleAps(PacketReader &r, ZbDevice &device, uint8_t const *extendedSource) {
	// application support layer

	// set start of header
	r.setHeader();

	// frame control
	auto frameControl = r.enum8<zb::ApsFrameControl>();
	auto frameType = frameControl & zb::ApsFrameControl::TYPE_MASK;
	if (frameType == zb::ApsFrameControl::TYPE_COMMAND) {
		// aps command
		uint8_t apsCounter = r.uint8();

		// security header
		// note: does in-place decryption of the payload
		if ((frameControl & zb::ApsFrameControl::SECURITY) != 0) {
			// restore security level according to 4.4.1.2 step 5.
			r.current[0] |= securityLevel;
			
			// security control field (4.5.1.1)
			auto securityControl = r.enum8<zb::SecurityControl>();
			
			// security counter
			uint32_t securityCounter = r.int32();

			auto keyType = securityControl & zb::SecurityControl::KEY_MASK;
			if (keyType != zb::SecurityControl::KEY_LINK && keyType != zb::SecurityControl::KEY_KEY_TRANSPORT) {
				return;
			}
			if ((securityControl & zb::SecurityControl::EXTENDED_NONCE) == 0) {
				if (extendedSource == nullptr) {
					return;
				}
			} else {
				extendedSource = r.current;
				r.skip(8);
			}

			// set start of message
			r.setMessage();

			// nonce (4.5.2.2)
			Nonce nonce(extendedSource, securityCounter, securityControl);

			// decrypt
			int micLength = 4;
			if (!r.decrypt(micLength, nonce, keyType == zb::SecurityControl::KEY_LINK ? trustCenterLinkAesKey : hashedTrustCenterLinkAesKey)) {
				return;
			}
		}

		auto command = r.enum8<zb::ApsCommand>();
		switch (command) {
		case zb::ApsCommand::UPDATE_DEVICE:
			{
				// device address
				uint64_t longAddress = r.int64();
				
				// short device address
				uint16_t shortAddress = r.uint16();
				
				uint8_t status = r.uint8();
				
				ZbDevice *d = findZbDevice(shortAddress);
				if (status == 0 && d != nullptr) {
					ZbDevice &updatedDevice = *d;
					auto const &flash = *updatedDevice;
					
					if (flash.deviceId == longAddress) {
sys::out.write("Update Device " + hex(shortAddress));
						
						// clear route of updated device if we don't get the message from its first hop
						if (device->shortAddress != updatedDevice.firstHop)
							updatedDevice.firstHop = 0xffff;
					}
				}
			}
			break;
		default:
			break;
		}
	} else if (frameType == zb::ApsFrameControl::TYPE_DATA) {
	   // aps data: zdp or zcl follow
	   uint8_t destinationEndpoint = r.uint8();
	   if (destinationEndpoint == 0)
		   handleZdp(r, device);
	   else
		   handleZcl(r, device, destinationEndpoint);
   } else if (frameType == zb::ApsFrameControl::TYPE_ACK) {
		// aps ack
   }
}

void RadioInterface::handleZdp(PacketReader &r, ZbDevice &device) {
	auto command = r.enum16<zb::ZdpCommand>();
	uint16_t profile = r.uint16();
	uint8_t sourceEndpoint = r.uint8();
	uint8_t apsCounter = r.uint8();
	uint8_t zdpCounter = r.uint8();

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

static bool handleZclClusterSpecific(MessageType dstType, void *dstMessage, EndpointInfo const &info,
	RadioInterface::PacketReader &r)
{
	// get command
	uint8_t command = r.uint8();

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
					uint8_t level = r.uint8();
					uint16_t transitionTime = r.uint16();
					
					dst.moveToLevel.level = float(level) / 254.0f;
					dst.moveToLevel.move = float(transitionTime) / 10.0f;
				}
				break;
			case zb::ZclLevelControlCommand::STEP:
			case zb::ZclLevelControlCommand::STEP_WITH_ON_OFF:
				{
					bool up = r.uint8() == 0;
					int diff = r.uint8();
					uint16_t transitionTime = r.uint16();
					
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

void RadioInterface::handleZcl(PacketReader &r, ZbDevice &device, uint8_t dstEndpointIndex) {
	auto cluster = r.enum16<zb::ZclCluster>();
	auto profile = r.enum16<zb::ZclProfile>();
	uint8_t srcEndpointIndex = r.uint8();
	uint8_t apsCounter = r.uint8();
	
	// zcl frame starts here
	auto frameControl = r.enum8<zb::ZclFrameControl>();
	uint8_t zclCounter = r.uint8();
	
	auto frameType = frameControl & zb::ZclFrameControl::TYPE_MASK;
	if (frameType == zb::ZclFrameControl::TYPE_PROFILE_WIDE) {
		// profile wide commands such as "read attribute response"
		uint8_t command = r.uint8();
		uint8_t *response = r.current;
		int length = min(r.getRemaining(), ZbDevice::RESPONSE_LENGTH);

		device.zclResponseBarrier.resumeFirst([dstEndpointIndex, cluster, profile, srcEndpointIndex, zclCounter,
			command, length, response] (ZbDevice::ZclResponse &r)
		{
			if (r.dstEndpoint == dstEndpointIndex && r.cluster == cluster && r.profile == profile
				&& r.srcEndpoint == srcEndpointIndex && r.zclCounter == zclCounter && r.command == zb::ZclCommand(command))
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
sys::out.write("Device: " + hex(device->shortAddress) + ", ZCL Command: " + dec(command) + ", Length: " + dec(length) + '\n');

		// publish to subscribers
		auto commandPtr = r.current;
		for (auto &subscriber : device.subscribers) {
			// get endpoint info for subscribed endpoint
			uint8_t const *p = device->getEndpointIndices() + subscriber.endpointIndex;
			auto const &endpointInfo = endpointInfos[p[0]];
			uint8_t zbEndpointIndex = p[1];

			// check if this is the right endpoint
			if (dstEndpointIndex == zbEndpointIndex && cluster == endpointInfo.cluster) {
				// trigger subscriber
				subscriber.barrier->resumeFirst([&subscriber, &r, &endpointInfo] (Interface::Parameters &p) {
					p.subscriptionIndex = subscriber.subscriptionIndex;

					// convert from zbee command to message
					return handleZclClusterSpecific(subscriber.messageType, p.message, endpointInfo, r);
				});

				// reset reader to command
				r.current = commandPtr;
			}
		}

		// trigger default response
		if (sendDefaultResponse) {
			uint8_t command = r.uint8();

			for (uint8_t endpointIndex = 0; endpointIndex < device->endpointCount; ++endpointIndex) {
				uint8_t const *p = device->getEndpointIndices() + endpointIndex;
				auto const &endpointInfo = endpointInfos[p[0]];
				uint8_t zbEndpointIndex = p[1];

				// check if this is the right endpoint
				if (dstEndpointIndex == zbEndpointIndex && cluster == endpointInfo.cluster) {
					device.defaultResponseFlags[endpointIndex / 32] = 1 << (endpointIndex & 31);
					device.defaultResponses[endpointIndex] = {zclCounter, command};
					this->publishEvent.set();
					break;
				}
			}
		}
	}
}


AwaitableCoroutine RadioInterface::handleAssociationRequest(uint64_t sourceAddress, radio::SendFlags sendFlags) {
sys::out.write("handleAssociationRequest\n");

	ZbDeviceFlash flash;
	flash.deviceId = sourceAddress;
	flash.shortAddress = this->nextShortAddress;// 0x1337 + this->devices.count();//0xcb36;//0x991e;
	flash.sendFlags = sendFlags;

	// create device
	Pointer<ZbDevice> device = new ZbDevice(flash);
	
	// set first hop
	device->firstHop = flash.shortAddress;

	// set pointers so that we can forward responses
	this->tempDevice = device.ptr;

	


	uint8_t packet1[ZbDevice::RESPONSE_LENGTH > 80 ? ZbDevice::RESPONSE_LENGTH : 80]; // space for key transport or response
	uint8_t packet2[ZbDevice::RESPONSE_LENGTH > 48 ? ZbDevice::RESPONSE_LENGTH : 48]; // space for zdp request or response
	uint8_t sendResult;
	zb::ZdpCommand zdpCommand;
	uint16_t length;

	// send association response
	{
		// get global configuration (is valid until next co_await)
		auto const &configuration = *this->configuration;

		PacketWriter w(packet1);
		
		// ieee 802.15.4 frame control
		w.enum16(ieee::FrameControl::TYPE_COMMAND
			| ieee::FrameControl::ACKNOWLEDGE_REQUEST
			| ieee::FrameControl::PAN_ID_COMPRESSION
			| ieee::FrameControl::DESTINATION_ADDRESSING_LONG
			| ieee::FrameControl::SOURCE_ADDRESSING_LONG);

		// sequence number
		w.uint8(this->macCounter++);

		// destination pan
		w.uint16(configuration.radioPanId);

		// destination address
		w.uint64(flash.deviceId);
		
		// source address
		w.uint64(configuration.radioLongAddress);
		
		
		// association response
		w.enum8(ieee::Command::ASSOCIATION_RESPONSE);
		
		// short address of device
		w.uint16(flash.shortAddress);
		
		// association status
		w.uint8(0);
		
		// always await data request
		w.finish(radio::SendFlags::AWAIT_DATA_REQUEST);
	}
sys::out.write("Send Association Response\n");
	co_await radio::send(RADIO_ZB, packet1, sendResult);
sys::out.write("Send Association Response Result " + dec(sendResult) + '\n');
	if (sendResult == 0)
		co_return;

	// send network key
	for (int retry = 0; ; ++retry) {
		{
			// get global configuration
			auto const &configuration = *this->configuration;

			PacketWriter w(packet1);
			
			// nwk data (no security)
			writeNwkDataNoSecurity(w, *device);
			
			// aps command
			writeApsCommand(w, zb::ApsFrameControl::DELIVERY_UNICAST
				| zb::ApsFrameControl::SECURITY);
			w.enum8(zb::ApsCommand::TRANSPORT_KEY);
			w.enum8(zb::KeyIdentifier::NETWORK); // standard network key
			w.data(configuration.key);
			w.uint8(0); // key sequence number
			w.uint64(flash.deviceId); // extended destination address
			w.uint64(configuration.radioLongAddress); // extended source address
		
			writeFooter(w, sendFlags);
		}
		co_await radio::send(RADIO_ZB, packet1, sendResult);
sys::out.write("Send Key Result " + dec(sendResult) + '\n');
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
			zdpCounter = writeZdpData(w, zb::ZdpCommand::NODE_DESCRIPTOR_REQUEST);//, device.u.zb.shortAddress);
			
			// address of interest
			w.uint16(flash.shortAddress);
			
			writeFooter(w, sendFlags);
		}
		co_await radio::send(RADIO_ZB, packet1, sendResult);
sys::out.write("Send Node Descriptor Request Result " + dec(sendResult) + '\n');
		if (sendResult != 0) {
			// wait for a response from the device
			int r = co_await select(device->zdpResponseBarrier.wait(zb::ZdpCommand::NODE_DESCRIPTOR_RESPONSE, zdpCounter,
				length, packet1), timer::sleep(1s));
			
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
		uint8_t status = nodeDescriptor.uint8();
		uint16_t addressOfInterest = nodeDescriptor.uint16();
sys::out.write("Node Descriptor Status " + dec(status) + '\n');
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
			zdpCounter = writeZdpData(w, zb::ZdpCommand::ACTIVE_ENDPOINT_REQUEST);//, device.u.zb.shortAddress);

			// address of interest
			w.uint16(flash.shortAddress);

			writeFooter(w, sendFlags);
		}
		co_await radio::send(RADIO_ZB, packet1, sendResult);
		if (sendResult != 0) {
			// wait for a response from the device
			int r = co_await select(device->zdpResponseBarrier.wait(zb::ZdpCommand::ACTIVE_ENDPOINT_RESPONSE, zdpCounter,
				length, packet1), timer::sleep(1s));

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
		uint8_t status = endpoints.uint8();
		uint16_t addressOfInterest = endpoints.uint16();
sys::out.write("active endpoints status " + dec(status) + '\n');
	}

	// get descriptor of each endpoint
	uint8_t zbEndpointCount = endpoints.uint8();
	int endpointCount = 0;
	for (int i = 0; i < zbEndpointCount; ++i) {
		// get next endpoint
		uint8_t endpoint = endpoints.uint8();
		
		// get endpoint descriptor
		uint8_t zdpCounter;
		for (int retry = 0; ; ++retry) {
			// send simple descriptor request
			{
				PacketWriter w(packet2);

				// nwk data
				writeNwkData(w, *device);

				// zdp data
				zdpCounter = writeZdpData(w, zb::ZdpCommand::SIMPLE_DESCRIPTOR_REQUEST);

				// address of interest
				w.uint16(flash.shortAddress);

				// endpoint to query
				w.uint8(endpoint);

				writeFooter(w, sendFlags);
			}
			co_await radio::send(RADIO_ZB, packet2, sendResult);
			if (sendResult != 0) {
				// wait for a response from the device
				int r = co_await select(device->zdpResponseBarrier.wait(zb::ZdpCommand::SIMPLE_DESCRIPTOR_RESPONSE,
					zdpCounter, length, packet2), timer::sleep(timeout));

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
			uint8_t status = endpointDescriptor.uint8();
			uint16_t addressOfInterest = endpointDescriptor.uint16();
			uint8_t descriptorLength = endpointDescriptor.uint8();
			uint8_t endpoint2 = endpointDescriptor.uint8();
			zb::ZclProfile profile = endpointDescriptor.enum16<zb::ZclProfile>();
			uint16_t applicationDevice = endpointDescriptor.uint16();
			uint8_t applicationVersion = endpointDescriptor.uint8();
sys::out.write("Endpoint Descriptor " + dec(endpoint) + " Status " + dec(status) + '\n');
		}

		// iterate over input/server clusters
		uint8_t inputClusterCount = endpointDescriptor.uint8();
		for (int i = 0; i < inputClusterCount && endpointDescriptor.getRemaining() >= 3; ++i) {
			zb::ZclCluster cluster = endpointDescriptor.enum16<zb::ZclCluster>();
			
			// add our device endpoints based on input clusters
			for (int index = 0; index < array::size(endpointInfos); ++index) {
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
		uint8_t outputClusterCount = endpointDescriptor.uint8();
		for (int i = 0; i < outputClusterCount && endpointDescriptor.getRemaining() >= 2; ++i) {
			zb::ZclCluster cluster = endpointDescriptor.enum16<zb::ZclCluster>();

			// add our device endpoints based on output clusters
			for (int index = 0; index < array::size(endpointInfos); ++index) {
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
					writeApsData(w, endpoint, zb::ZclCluster::BASIC, zb::ZclProfile::HOME_AUTOMATION, endpoint);

					// zcl profile wide
					w.enum8(zb::ZclFrameControl::TYPE_PROFILE_WIDE
						| zb::ZclFrameControl::DIRECTION_CLIENT_TO_SERVER
						| zb::ZclFrameControl::DISABLE_DEFAULT_RESPONSE);
					w.uint8(zclCounter);
					w.enum8(zb::ZclCommand::READ_ATTRIBUTES);
					w.enum16(zb::ZclBasicAttribute::POWER_SOURCE);
					
					writeFooter(w, sendFlags);
				}
				co_await radio::send(RADIO_ZB, packet1, sendResult);
				if (sendResult != 0) {
					// wait for a response from the device
					int r = co_await select(device->zclResponseBarrier.wait(endpoint, zb::ZclCluster::BASIC,
						zb::ZclProfile::HOME_AUTOMATION, endpoint, zclCounter,
						zb::ZclCommand::READ_ATTRIBUTES_RESPONSE, length, packet2), timer::sleep(timeout));

					// check if response was received
					if (r == 1)
						break;
				}
				if (retry == MAX_RETRY)
					co_return;
			}
			
			// handle read attribute response
			PacketReader attributeResponse(length, packet2);
			auto attribute = attributeResponse.enum16<zb::ZclBasicAttribute>();
			uint8_t status = attributeResponse.uint8();
			auto type = attributeResponse.enum8<zb::ZclDataType>();
			auto source = attributeResponse.enum8<zb::ZclPowerSourceType>();
sys::out.write("Power Source Attribute Status " + dec(status) + " Source " + dec(source) + '\n');

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
sys::out.write("Bind Request to Endpoint " + dec(endpoint) + ", Cluster " + hex(info.cluster) + '\n');
			do {
				// wait until device is ready to receive
				//co_await deviceState->dataRequestBarrier.wait();

				// send bind request
				uint8_t zdpCounter;
				{
					// get global configuration
					auto const &configuration = *this->configuration;
					
					PacketWriter w(packet1);

					// nwk data
					writeNwkData(w, *device);
						
					// zdp data
					zdpCounter = writeZdpData(w, zb::ZdpCommand::BIND_REQUEST);

					// source
					w.uint64(flash.deviceId);
					
					// source endpoint
					w.uint8(endpoint);
					
					// cluster
					w.enum16(info.cluster);
					
					// address mode: unicast
					w.uint8(3);
					
					// destination
					w.uint64(configuration.radioLongAddress);
					
					// destination endpoint
					w.uint8(endpoint);
					
					writeFooter(w, sendFlags);
				}
				co_await radio::send(RADIO_ZB, packet1, sendResult);

				// try again if send was not successful
				if (!sendResult)
					continue;

				// wait for a response from the device
				int r = co_await select(device->zdpResponseBarrier.wait(zb::ZdpCommand::BIND_RESPONSE, zdpCounter,
					length, packet1), timer::sleep(300ms));
				if (r == 2)
					continue;
			} while (false);
			
			// handle bind response
			PacketReader bindResponse(length, packet1);
			uint8_t status = bindResponse.uint8();
sys::out.write("bind response status " + dec(status) + '\n');
		}
	}
	
	
	// move pairs of info index and zbee endpoint
	for (int i = 0; i < endpointCount * 2; ++i) {
sys::out.write("endpoint info " + dec(flash.endpoints[ZbDeviceFlash::MAX_ENDPOINT_COUNT + i]) + '\n');
		flash.endpoints[endpointCount + i] = flash.endpoints[ZbDeviceFlash::MAX_ENDPOINT_COUNT + i];
	}

	// start a publisher coroutine for this device
	//device->publisher = publisher(*device);

	// check if the device already exists
	int index = this->zbDevices.count();
	for (int i = 0; i < this->zbDevices.count(); ++i) {
		if (this->zbDevices[i]->deviceId == flash.deviceId) {
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
		for (auto &device : this->zbDevices) {
			// iterate over publishers
			for (auto &publisher : device.publishers) {
				// check if publisher wants to publish
				if (publisher.dirty) {
					publisher.dirty = false;
					
					// get endpoint info
					uint8_t endpointIndex = publisher.endpointIndex;
					EndpointType endpointType = EndpointType(device->endpoints[endpointIndex]);
					uint8_t const *p = device->getEndpointIndices() + endpointIndex;
					EndpointInfo const &endpointInfo = endpointInfos[p[0]];
					uint8_t zbEndpointIndex = p[1];

					if ((endpointType & EndpointType::OUT) == EndpointType::OUT) {
					
						// request the route if necessary
						for (int retry = 0; retry < MAX_RETRY; ++retry) {
							if (device.firstHop != 0xffff)
								break;
						
							// build packet
							{
								PacketWriter w(packet);
								auto const &flash = *device;

								// route request command
								writeNwkBroadcastCommand(w, zb::NwkCommand::ROUTE_REQUEST);
								w.enum8(zb::NwkCommandRouteRequestOptions::DISCOVERY_SINGLE
									| zb::NwkCommandRouteRequestOptions::EXTENDED_DESTINATION);
								
								// route id
								w.uint8(this->routeCounter++);
								
								// destination
								w.uint16(flash.shortAddress);
								
								// path cost
								w.uint8(0);
								
								// extended destination
								w.uint64(flash.deviceId);

								writeFooter(w, radio::SendFlags::NONE);
							}
							
							// send packet
							uint8_t sendResult;
							co_await radio::send(RADIO_ZB, packet, sendResult);
							if (sendResult == 0)
								continue;
								
							// wait for route reply or timeout
							co_await select(device.routeBarrier.wait(), timer::sleep(timeout));

				sys::out.write("Route for " + hex(device->shortAddress) + ": " + hex(device.firstHop) + '\n');
						}

						// fail if route remains unknown
						if (device.firstHop == 0xffff) {
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
								writeApsData(w, zbEndpointIndex, endpointInfo.cluster, zb::ZclProfile::HOME_AUTOMATION,
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
								
								writeFooter(w, radio::SendFlags::NONE);
							}
							
							// send packet
							uint8_t sendResult;
							co_await radio::send(RADIO_ZB, packet, sendResult);
							if (sendResult != 0) {
								// wait for a response from the device
								uint16_t length;
								int r = co_await select(device.zclResponseBarrier.wait(zbEndpointIndex,
									endpointInfo.cluster, zb::ZclProfile::HOME_AUTOMATION, zbEndpointIndex, zclCounter,
									zb::ZclCommand::DEFAULT_RESPONSE, length, packet), timer::sleep(timeout));

								// check if response was received
								if (r == 1) {
									DataReader r(packet);
									uint8_t responseToCommand = r.uint8();
									uint8_t status = r.uint8();
									
									// todo: check status (0 = ok)
									
									break;
								}
							}
						}
					}
					
					// forward to subscribers
					for (auto &subscriber : device.subscribers) {
						if (subscriber.endpointIndex == publisher.endpointIndex) {
							subscriber.barrier->resumeAll([&subscriber, &publisher] (Interface::Parameters &p) {
								p.subscriptionIndex = subscriber.subscriptionIndex;
								
								// convert to target unit and type and resume coroutine if conversion was successful
								return convert(subscriber.messageType, p.message,
									publisher.messageType, publisher.message);
							});
						}
					}
				}
			}
			
			// check for pending default responses
			for (int j = 0; j < (ZbDeviceFlash::MAX_ENDPOINT_COUNT + 31) / 32; ++j) {
				uint32_t flags = device.defaultResponseFlags[j];
				int endpointIndex = j * 32;
				while (flags > 0) {
					if (flags & 1) {
						EndpointType endpointType = EndpointType(device->endpoints[endpointIndex]);
						uint8_t const *p = device->getEndpointIndices() + endpointIndex;
						EndpointInfo const &endpointInfo = endpointInfos[p[0]];
						uint8_t zbEndpointIndex = p[1];

						auto defaultResponse = device.defaultResponses[endpointIndex];
						
						for (int retry = 0; retry <= MAX_RETRY; ++retry) {
							// build packet
							{
								PacketWriter w(packet);

								// nwk data
								writeNwkData(w, device);

								// aps data
								writeApsData(w, zbEndpointIndex, endpointInfo.cluster, zb::ZclProfile::HOME_AUTOMATION,
									zbEndpointIndex);
								
								// zcl profile wide
								w.enum8(zb::ZclFrameControl::TYPE_PROFILE_WIDE
									| zb::ZclFrameControl::DIRECTION_SERVER_TO_CLIENT
									| zb::ZclFrameControl::DISABLE_DEFAULT_RESPONSE);
								w.uint8(defaultResponse.zclCounter);
								w.enum8(zb::ZclCommand::DEFAULT_RESPONSE);

								// response to command
								w.uint8(defaultResponse.command);
								
								// success
								w.uint8(0);

								writeFooter(w, device.sendFlags);
							}
													
							// send packet
							uint8_t sendResult;
							co_await radio::send(RADIO_ZB, packet, sendResult);
							if (sendResult != 0)
								break;
						}
					}
					flags >>= 1;
					++endpointIndex;
				}
				device.defaultResponseFlags[j] = 0;
			}
			
			// check for pending rejoin
			if (device.rejoinPending) {
				device.rejoinPending = false;
				
				for (int retry = 0; retry <= MAX_RETRY; ++retry) {

					// build packet
					{
						auto const &flash = *device;
						PacketWriter w(packet);

						// rejoin response command
						writeNwkCommand(w, device, zb::NwkCommand::REJOIN_RESPONSE);
						
						// address
						w.uint16(flash.shortAddress);
						
						// status
						w.uint8(0);

						writeFooter(w, device.sendFlags);
					}
						
					// send packet
					uint8_t sendResult;
					co_await radio::send(RADIO_ZB, packet, sendResult);
					if (sendResult != 0)
						break;
				}
			}
		}
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
