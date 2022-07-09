#include "RadioInterface.hpp"
#include <Radio.hpp>
#include <Random.hpp>
#include <Storage2.hpp>
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


// timeout to wait for a response (e.g. default response or route reply)
constexpr SystemDuration timeout = 500ms;

// number of retries when a send fails
constexpr int MAX_RETRY = 2;

constexpr zb::SecurityControl securityLevel = zb::SecurityControl::LEVEL_ENC_MIC32; // encrypted + 32 bit message integrity code


// RadioInterface

RadioInterface::RadioInterface(PersistentStateManager &stateManager)
	: securityCounter(stateManager)
{
	// load devices
	this->deviceCount = Storage2::read(STORAGE_CONFIG, STORAGE_ID_RADIO1, sizeof(this->deviceIds), this->deviceIds);
	int j = 0;
	for (int i = 0; i < this->deviceCount; ++i) {
		uint8_t id = this->deviceIds[i];

		// determine size
		int size = Storage2::getSize(STORAGE_CONFIG, STORAGE_ID_RADIO1 | id);
		if (size < sizeof(DeviceData))
			continue;

		// allocate and read
		auto data = reinterpret_cast<DeviceData *>(malloc(size));
		Storage2::read(STORAGE_CONFIG, id | STORAGE_ID_RADIO1, size, data);
		if (data->id != id) {
			// id is inconsistent, delete device
			free(data);
			continue;
		}

		if (data->deviceType != DeviceType::ZBEE) {
			// green power
			auto *deviceData = static_cast<GpDeviceData *>(data);

			// check size
			if (deviceData->size() != size) {
				free(data);
				continue;
			}

			new GpDevice(this, deviceData);
		} else {
			// zbee
			auto *endpointData = static_cast<ZbEndpointData *>(data);

			// check size
			if (endpointData->size() != size) {
				free(data);
				continue;
			}

			// get or load the device this endpoint belongs to
			auto *device = getOrLoadZbDevice(endpointData->deviceId);
			if (device == nullptr) {
				// endpoint is orphaned
				free(data);
				continue;
			}
			new ZbEndpoint(device, endpointData);
		}

		// device was correctly loaded
		this->deviceIds[j] = id;
		++j;
	}
	this->deviceCount = j;
}

RadioInterface::~RadioInterface() {
}

void RadioInterface::setConfiguration(uint16_t panId, DataBuffer<16> const &key, AesKey const &aesKey) {
	bool first = this->key == nullptr;
	this->panId = panId;
	this->key = &key;
	this->aesKey = &aesKey;

	// set pan id of coordinator
	Radio::setPan(RADIO_ZBEE, panId);

	if (first)
		start();
}

void RadioInterface::setCommissioning(bool enabled) {
	this->commissioning = enabled && getDeviceCount() < MAX_DEVICE_COUNT;
	if (this->commissioning) {
		// allocate next interface id and short address
		//allocateInterfaceId();
		allocateShortAddress();
	} else {
		// cancel current association coroutine if it is still running
		this->associationCoroutine.cancel();
	}
}

int RadioInterface::getDeviceCount() {
	return this->deviceCount;
}

Interface::Device &RadioInterface::getDeviceByIndex(int index) {
	assert(index >= 0 && index < this->deviceCount);
	uint8_t id = this->deviceIds[index];
	return *getDevice(id);

	/*int gpDeviceCount = this->gpDevices.count();
	assert(index >= 0 && index < gpDeviceCount + this->zbDevices.count());

	if (index < gpDeviceCount)
		return this->gpDevices[index];
	return this->zbDevices[index - gpDeviceCount];*/
}

Interface::Device *RadioInterface::getDevice(uint8_t id) {
	{
		auto device = this->gpDevices;
		while (device != nullptr) {
			if (device->data->id == id)
				return device;
			device = device->next;
		}
	}
	{
		auto device = this->zbDevices;
		while (device != nullptr) {
			// iterate over endpoints, each endpoint is exposed as a device
			auto endpoint = device->endpoints;
			while (endpoint != nullptr) {
				if (endpoint->data->id == id)
					return endpoint;
			}
			device = device->next;
		}
	}
	return nullptr;
}

void RadioInterface::eraseDevice(uint8_t id) {
	{
		auto d = &this->gpDevices;
		while (*d != nullptr) {
			auto device = *d;
			if (device->data->id == id) {
				// remove device from linked list
				*d = device->next;

				// erase from flash
				Storage2::erase(STORAGE_CONFIG, STORAGE_ID_RADIO1 | id);

				// delete device
				delete device;

				goto list;
			}
			d = &device->next;
		}
	}
	{
		auto d = &this->zbDevices;
		while (*d != nullptr) {
			auto device = *d;

			// iterate over endpoints
			auto e = &device->endpoints;
			while (*e != nullptr) {
				auto endpoint = *e;
				if (endpoint->data->id == id) {
					// remove endpoint from linked list
					*e = endpoint->next;

					// check if device has no endpoints left
					if (device->endpoints == nullptr) {
						// remove device from linked list
						*d = device->next;

						// erase device from flash
						Storage2::erase(STORAGE_CONFIG, STORAGE_ID_RADIO2 | device->data.id);

						// delete device
						delete device;
					}

					// erase endpoint from flash
					Storage2::erase(STORAGE_CONFIG, STORAGE_ID_RADIO1 | id);

					// delete endpoint
					delete endpoint;

					goto list;
				}
			}
			d = &device->next;
		}
	}

list:

	// erase from list of device id's
	int j = 0;
	for (int i = 0; i < this->deviceCount; ++i) {
		uint8_t id2 = this->deviceIds[i];
		if (id2 != id) {
			this->deviceIds[j] = id;
			++j;
		}
	}
	this->deviceCount = j;
	Storage2::write(STORAGE_CONFIG, STORAGE_ID_RADIO1, j, this->deviceIds);
}

// private:

// GpDevice
RadioInterface::GpDevice::~GpDevice() {
	free(data);
}

uint8_t RadioInterface::GpDevice::getId() const {
	return this->data->id;
}

String RadioInterface::GpDevice::getName() const {
	return String(this->data->name);
}

void RadioInterface::GpDevice::setName(String name) {
	assign(this->data->name, name);

	// write to flash
	Storage2::write(STORAGE_CONFIG, this->data->id | STORAGE_ID_RADIO1, this->data->size(), this->data);
}

Array<MessageType const> RadioInterface::GpDevice::getPlugs() const {
	return {this->data->plugCount, this->data->plugs};
}

void RadioInterface::GpDevice::subscribe(uint8_t plugIndex, Subscriber &subscriber) {
	subscriber.remove();
	subscriber.source.device.plugIndex = plugIndex;
	this->subscribers.add(subscriber);
}

PublishInfo RadioInterface::GpDevice::getPublishInfo(uint8_t plugIndex) {
	if (plugIndex >= this->data->plugCount)
		return {};
	return {{.type = this->data->plugs[plugIndex], .device = {this->data->id, plugIndex}},
		&this->interface->publishBarrier};
}

// ZbEndpointDataBuilder
int RadioInterface::ZbEndpointDataBuilder::size() {
	int s1 = (this->plugCount * sizeof(MessageType) + 3) / 4;
	int s2 = ((this->serverClusterIndex + (MAX_CLUSTER_COUNT - 1 - this->clientClusterIndex)) * sizeof(ClusterInfo) + 3) / 4;
	return offsetOf(ZbEndpointData, buffer[s1 + s2]);
}

void RadioInterface::ZbEndpointDataBuilder::build(ZbEndpointData *data) {
	//array::copy(data->name, this->name);

	data->id = this->id;
	data->deviceType = DeviceType::ZBEE;

	// set counts
	data->plugCount = this->plugCount;
	data->serverClusterCount = this->serverClusterIndex;
	data->clientClusterCount = MAX_CLUSTER_COUNT - 1 - this->clientClusterIndex;

	// copy plugs
	auto plugs = reinterpret_cast<MessageType *>(data->buffer);
	array::copy(data->plugCount, plugs, this->plugs);

	// get clusterInfos array
	int s1 = (this->plugCount * sizeof(MessageType) + 3) / 4;
	auto clusterInfos = reinterpret_cast<ClusterInfo *>(&data->buffer[s1]);

	// copy server plugs
	array::copy(data->serverClusterCount, clusterInfos, this->clusterInfos);
	clusterInfos += data->serverClusterCount;

	// copy client plugs
	for (int i = 0; i < data->clientClusterCount; ++i) {
		*clusterInfos = this->clusterInfos[MAX_CLUSTER_COUNT - 1 - i];
		++clusterInfos;
	}
}


// ZbEndpoint
RadioInterface::ZbEndpoint::~ZbEndpoint() {
	free(data);
}

uint8_t RadioInterface::ZbEndpoint::getId() const {
	return this->data->id;
}

String RadioInterface::ZbEndpoint::getName() const {
	return String(this->data->name);
}

void RadioInterface::ZbEndpoint::setName(String name) {
	assign(this->data->name, name);

	// write to flash
	Storage2::write(STORAGE_CONFIG, this->data->id | STORAGE_ID_RADIO1, this->data->size(), this->data);
}

Array<MessageType const> RadioInterface::ZbEndpoint::getPlugs() const {
	auto plugs = reinterpret_cast<MessageType *>(this->data->buffer);
	return {this->data->plugCount, plugs};
}

void RadioInterface::ZbEndpoint::subscribe(uint8_t plugIndex, Subscriber &subscriber) {
	subscriber.remove();
	subscriber.source.device.plugIndex = plugIndex;
	this->subscribers.add(subscriber);
}

PublishInfo RadioInterface::ZbEndpoint::getPublishInfo(uint8_t plugIndex) {
	if (plugIndex >= this->data->plugCount)
		return {};
	auto plugs = reinterpret_cast<MessageType *>(this->data->buffer);
	return {{.type = plugs[plugIndex], .device = {this->data->id, plugIndex}},
		&this->device->interface->publishBarrier};
}

RadioInterface::PlugRange RadioInterface::ZbEndpoint::findServerCluster(zb::ZclCluster cluster) const {
	// get clusterInfos array
	int s1 = (this->data->plugCount * sizeof(MessageType) + 3) / 4;
	auto clusterInfos = reinterpret_cast<ClusterInfo *>(&this->data->buffer[s1]);

	for (int i = 0; i < this->data->serverClusterCount; ++i) {
		if (clusterInfos->cluster == cluster)
			return clusterInfos->plugs;
		++clusterInfos;
	}

	// not found
	return {0, 0};
}

RadioInterface::ClusterInfo RadioInterface::ZbEndpoint::getClientCluster(int plugIndex) const {
	// get clusterInfos array
	int s1 = (this->data->plugCount * sizeof(MessageType) + 3) / 4;
	auto clusterInfos = reinterpret_cast<ClusterInfo *>(&this->data->buffer[s1]) + this->data->serverClusterCount;

	for (int i = 0; i < this->data->clientClusterCount; ++i) {
		int o = clusterInfos->plugs.offset;
		if (plugIndex >= o && plugIndex < o + clusterInfos->plugs.count)
			return *clusterInfos;
		++clusterInfos;
	}

	// not found
	return {zb::ZclCluster::BASIC, {0, 0}};
}


RadioInterface::ZbDevice *RadioInterface::getOrLoadZbDevice(uint8_t id) {
	auto device = this->zbDevices;
	while (device != nullptr) {
		if (device->data.id == id)
			return device;
		device = device->next;
	}
	ZbDeviceData data;
	if (Storage2::read(STORAGE_CONFIG, STORAGE_ID_RADIO2 | id, sizeof(data), &data) != sizeof(data))
		return nullptr;
	return new ZbDevice(this, data);
}

Coroutine RadioInterface::start() {
	// restore security counter
	co_await this->securityCounter.restore();

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

RadioInterface::ZbDevice *RadioInterface::findZbDevice(uint16_t address) {
	auto device = this->zbDevices;
	while (device != nullptr) {
		if (device->data.shortAddress == address)
			return device;
		device = device->next;
	}
	return nullptr;
}

uint8_t RadioInterface::allocateId() {
	// find a free id
	int id;
	for (id = 1; id < 256; ++id) {
		for (int j = 0; j < this->deviceCount; ++j) {
			if (this->deviceIds[j] == id)
				goto found;
		}
		break;
	found:;
	}
	return id;
}

uint8_t RadioInterface::allocateZbDeviceId() {
	// find a free id
	int id;
	for (id = 1; id < 256; ++id) {
		auto device = this->zbDevices;
		while (device != nullptr) {
			if (device->data.id == id)
				goto found;
			device = device->next;
		}
		break;
	found:;
	}
	return id;
}

void RadioInterface::allocateShortAddress() {
	while (true) {
		uint16_t address = Random::u16();

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

	// ieee destination address (address of router/first hop for the device)
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
	w.u16L(device.data.shortAddress);

	// source
	w.u16L(0x0000);

	// radius
	w.u8(radius);

	// sequence number
	w.u8(this->nwkCounter++);

	// destination
	if ((nwkFrameControl & zb::NwkFrameControl::DESTINATION) != 0)
		w.u64L(device.data.longAddress);

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

void RadioInterface::writeApsDataZcl(PacketWriter &w, uint8_t dstEndpoint, zb::ZclCluster clusterId,
	zb::ZclProfile profile, uint8_t srcEndpoint)
{
	// application support layer frame control field
	w.e8(zb::ApsFrameControl::TYPE_DATA
		| zb::ApsFrameControl::DELIVERY_UNICAST);

	// destination endpoint
	w.u8(dstEndpoint);

	// cluster id
	w.e16L(clusterId);

	// profile
	w.e16L(profile);

	// source endpoint
	w.u8(srcEndpoint);

	// counter
	w.u8(this->apsCounter++);
}

void RadioInterface::writeApsAckZcl(PacketWriter &w, uint8_t dstEndpoint, zb::ZclCluster clusterId,
	zb::ZclProfile profile, uint8_t srcEndpoint)
{
	// application support layer frame control field
	w.e8(zb::ApsFrameControl::TYPE_ACK
		| zb::ApsFrameControl::DELIVERY_UNICAST);

	// destination endpoint
	w.u8(dstEndpoint);

	// cluster id
	w.e16L(clusterId);

	// profile
	w.e16L(profile);

	// source endpoint
	w.u8(srcEndpoint);

	// counter
	w.u8(this->apsCounter++);
}

static void writeZclCommand(RadioInterface::PacketWriter &w, uint8_t zclCounter) {
	// zbee cluster library frame
	w.e8(zb::ZclFrameControl::TYPE_CLUSTER_SPECIFIC
		| zb::ZclFrameControl::DIRECTION_CLIENT_TO_SERVER);
	w.u8(zclCounter);
}

static void writeZclAttribute(RadioInterface::PacketWriter &w, uint8_t zclCounter) {
	// zbee cluster library frame
	w.e8(zb::ZclFrameControl::TYPE_PROFILE_WIDE
		| zb::ZclFrameControl::DIRECTION_CLIENT_TO_SERVER);
	w.u8(zclCounter);
	w.e8(zb::ZclCommand::WRITE_ATTRIBUTES);
}

static bool writeZclCommand(RadioInterface::PacketWriter &w, uint8_t zclCounter, int plugIndex,
	zb::ZclCluster cluster, Message &message)
{
	writeZclCommand(w, zclCounter);
	switch (cluster) {
	case zb::ZclCluster::ON_OFF:
		w.u8(message.value.u8);
		break;
	case zb::ZclCluster::LEVEL_CONTROL: {
		if (message.command == 0) {
			// set level
			w.e8(zb::ZclLevelControlCommand::MOVE_TO_LEVEL_WITH_ON_OFF);
		} else {
			// increase/decrease level
			w.e8(zb::ZclLevelControlCommand::STEP_WITH_ON_OFF);
			w.u8(message.command == 1 ? 0x00 : 0x01);
		}
		w.u8(clamp(int(message.value.f * 254.0f + 0.5f), 0, 254));

		// transition time in 1/10s
		w.u16L(min(int(message.transition), 65534));
		break;
	}
	default:
		// not supported
		return false;
	}
	return true;
}

static bool writeZclAttribute(RadioInterface::PacketWriter &w, uint8_t zclCounter, int plugIndex,
	zb::ZclCluster cluster, Message &message)
{
	switch (cluster) {
	case zb::ZclCluster::ON_OFF:
		// on/off, do via command
		writeZclCommand(w, zclCounter);
		w.u8(message.value.u8);
		break;
	case zb::ZclCluster::LEVEL_CONTROL: {
		// set level, do via command
		writeZclCommand(w, zclCounter);
		w.e8(zb::ZclLevelControlCommand::MOVE_TO_LEVEL_WITH_ON_OFF);
		w.u8(clamp(int(message.value.f * 254.0f + 0.5f), 0, 254));

		// transition time in 1/10s
		w.u16L(0);
		break;
	}
	case zb::ZclCluster::COLOR_CONTROL: {

		break;
	}
	default:
		// not supported
		return false;
	}
	return true;
}

/*
bool writeZclClusterSpecific(RadioInterface::PacketWriter &w, uint8_t zclCounter, EndpointInfo const &info,
	MessageType messageType, Message &message)
{
	// zbee cluster library frame
	w.e8(zb::ZclFrameControl::TYPE_CLUSTER_SPECIFIC
		| zb::ZclFrameControl::DIRECTION_CLIENT_TO_SERVER);
	w.u8(zclCounter);

	//MessageType srcType = publisher.messageType;
	//auto &src = *reinterpret_cast<Message const *>(publisher.message);

	switch (info.cluster) {
	case zb::ZclCluster::ON_OFF:
		w.u8(message.command);
		break;
		//return convertCommandOut(w.u8(), srcType, src, publisher.convertOptions);
	case zb::ZclCluster::LEVEL_CONTROL: {
		//Message level;
		//if (!convertSetFloatValueOut(MessageType::MOVE_TO_LEVEL_OUT, level, srcType, src, publisher.convertOptions))
		//	return false; // conversion failed
		if (message.command == 0) {
			// set level
			w.e8(zb::ZclLevelControlCommand::MOVE_TO_LEVEL_WITH_ON_OFF);
		} else {
			// increase/decrease level
			w.e8(zb::ZclLevelControlCommand::STEP_WITH_ON_OFF);
			w.u8(message.command == 1 ? 0x00 : 0x01);
		}
		w.u8(clamp(int(message.value.f * 254.0f + 0.5f), 0, 254));

		// transition time in 1/10s
		w.u16L(min(int(message.transition), 65534));
		break;
	}
/ *
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
		break;* /
	default:
		// not supported
		return false;
	}
	return true;
}*/

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
			key = &zb::za09LinkAesKey;
			break;
		case zb::SecurityControl::KEY_KEY_TRANSPORT:
			key = &zb::za09KeyTransportAesKey;
			break;
		case zb::SecurityControl::KEY_KEY_LOAD:
			key = &zb::za09KeyLoadAesKey;
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

AwaitableCoroutine RadioInterface::readAttribute(uint8_t (&packet)[ZbDevice::MESSAGE_LENGTH], ZbDevice &device,
	uint8_t dstEndpoint, zb::ZclCluster clusterId, zb::ZclProfile profile, uint8_t srcEndpoint, uint16_t attribute)
{
	uint8_t zclCounter = this->zclCounter++;
	for (int retry = 0;; ++retry) {
		// send read attributes
		{
			PacketWriter w(packet);

			// nwk header
			writeNwkData(w, device);

			// aps data
			writeApsDataZcl(w, dstEndpoint, clusterId, profile, srcEndpoint);

			// zcl profile wide
			w.e8(zb::ZclFrameControl::TYPE_PROFILE_WIDE
				| zb::ZclFrameControl::DIRECTION_CLIENT_TO_SERVER
				| zb::ZclFrameControl::DISABLE_DEFAULT_RESPONSE);
			w.u8(zclCounter);
			w.e8(zb::ZclCommand::READ_ATTRIBUTES);
			w.u16L(attribute);

			writeFooter(w, device.sendFlags);
		}
		uint8_t sendResult;
		co_await Radio::send(RADIO_ZBEE, packet, sendResult);
		if (sendResult != 0) {
			// wait for a response from the device
			int length;
			int r = co_await select(
				this->endpointResponseBarrier.wait(uint16_t(zb::ZclCommand::READ_ATTRIBUTES_RESPONSE),
					srcEndpoint, zclCounter, length, packet), Timer::sleep(timeout));

			// check if response was received
			if (r == 1)
				break;
		}
		if (retry == MAX_RETRY) {
			// set error response for the attribute
			MessageWriter w(packet);
			w.u16L(attribute);
			w.u8(255);
			break;
		}
	}
}

template <typename T, int N>
optional<T> getEnum8(uint8_t (&packet)[N]) {
	MessageReader r(sizeof(packet), packet);
	auto attribute = r.u16L();
	uint8_t status = r.u8();
	if (status == 0) {
		auto type = r.e8<zb::ZclDataType>();
		return r.e8<T>();
	}
	return {};
}

template <int N>
optional<String> getString(uint8_t (&packet)[N]) {
	MessageReader r(sizeof(packet), packet);
	auto attribute = r.u16L();
	uint8_t status = r.u8();
	if (status == 0) {
		auto type = r.e8<zb::ZclDataType>();
		return r.string8();
	}
	return {};
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

static bool handleZclCommand(MessageType dstType, void *dstMessage, int plugIndex, zb::ZclCluster cluster,
	MessageReader r, ConvertOptions const &convertOptions)
{
	// get command
	uint8_t command = r.u8();

	auto &dst = *reinterpret_cast<Message *>(dstMessage);
	switch (cluster) {
	case zb::ZclCluster::ON_OFF:
		return convertSwitch(dstType, dst, command, convertOptions);
	case zb::ZclCluster::LEVEL_CONTROL: {
		uint8_t command = 0;
		switch (zb::ZclLevelControlCommand(command)) {
		case zb::ZclLevelControlCommand::STEP:
		case zb::ZclLevelControlCommand::STEP_WITH_ON_OFF:
			command = r.u8() == 0 ? 1 : 2; // increase/decrease
			// fall through
		case zb::ZclLevelControlCommand::MOVE_TO_LEVEL:
		case zb::ZclLevelControlCommand::MOVE_TO_LEVEL_WITH_ON_OFF: {
			float value = float(r.u8()) / 254.0f;
			uint16_t transition = r.u16L(); // transition time in 1/10 s
			return convertFloatTransition(dstType, dst, value, command, transition, convertOptions);
		}
		default:
			// conversion failed
			return false;
		}
	}
	case zb::ZclCluster::COLOR_CONTROL: {
		uint8_t command = 0;
		switch (zb::ZclColorControlCommand(command)) {
		case zb::ZclColorControlCommand::STEP_COLOR:
			command = r.u8() == 0 ? 1 : 2; // increase/decrease
			// fall through
		case zb::ZclColorControlCommand::MOVE_TO_COLOR: {
			auto x = r.u8();
			auto y = r.u8();
			float value = float(plugIndex == 0 ? x : y) / 254.0f;
			uint16_t transition = r.u16L(); // transition time in 1/10 s
			return convertFloatTransition(dstType, dst, value, command, transition, convertOptions);
		}
		default:
			// conversion failed
			return false;
		}
	}
	default:;
	}

	// conversion failed
	return false;
}
/*
static bool handleZclClusterSpecific(MessageType dstType, void *dstMessage, EndpointInfo const &info, MessageReader r,
	ConvertOptions const &convertOptions)
{
	// get command
	uint8_t command = r.u8();

	auto &dst = *reinterpret_cast<Message *>(dstMessage);
	switch (info.cluster) {
	case zb::ZclCluster::ON_OFF:
		return convertSwitch(dstType, dst, command, convertOptions);
	case zb::ZclCluster::LEVEL_CONTROL: {
		uint8_t command = 0;
		switch (zb::ZclLevelControlCommand(command)) {
		case zb::ZclLevelControlCommand::STEP:
		case zb::ZclLevelControlCommand::STEP_WITH_ON_OFF:
			command = r.u8() == 0 ? 1 : 2; // increase/decrease
			// fall through
		case zb::ZclLevelControlCommand::MOVE_TO_LEVEL:
		case zb::ZclLevelControlCommand::MOVE_TO_LEVEL_WITH_ON_OFF: {
			float value = float(r.u8()) / 254.0f;
			uint16_t transition = r.u16L(); // transition time in 1/10 s
			return convertFloatTransition(dstType, dst, value, command, transition, convertOptions);
		}
		default:
			// conversion failed
			return false;
		}
/ *
		if (dstType != MessageType::MOVE_TO_LEVEL)
			return false; // conversion failed
		switch (zb::ZclLevelControlCommand(command)) {
		case zb::ZclLevelControlCommand::MOVE_TO_LEVEL:
		case zb::ZclLevelControlCommand::MOVE_TO_LEVEL_WITH_ON_OFF: {
			uint8_t level = r.u8();
			uint16_t transitionTime = r.u16L();

			dst.moveToLevel.level = float(level) / 254.0f;
			dst.moveToLevel.move = float(transitionTime) / 10.0f;
			break;
		}
		case zb::ZclLevelControlCommand::STEP:
		case zb::ZclLevelControlCommand::STEP_WITH_ON_OFF: {
			bool up = r.u8() == 0;
			int diff = r.u8();
			uint16_t transitionTime = r.u16L();

			dst.moveToLevel.level.set(float(up ? diff : -diff) / 254.0f, true);
			dst.moveToLevel.move = float(transitionTime) / 10.0f;
			break;
		}
		default:
			// conversion failed
			return false;
		}* /
		break;
	}
		default:
			// conversion failed
			return false;
	}

	// conversion successful
	return true;
}
*/

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

		// the radio filter flags ensure that a mac sequence number is present
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
				if (this->tempDevice->data.shortAddress == sourceAddress) {
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
			if (router.data.shortAddress != sourceAddress) {
				// source device is not a neighbor: lookup the source device
				pDevice = findZbDevice(sourceAddress);
				if (pDevice == nullptr)
					continue;
			}
			ZbDevice &device = *pDevice;

			// set first hop of route if not known yet (this is probably a hack)
			if (device.routerAddress == 0xffff) {
				device.sendFlags = router.data.sendFlags;
				device.routerAddress = router.data.shortAddress;
				device.cost = 255; // set maximum costs so that route replies can later overwrite the router
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

							// check if "we" are the destination of the route
							// todo: also handle other destination addresses
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
									w.u16L(device.data.shortAddress);

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
						uint16_t originatorAddress = r.u16L(); // the node that sent a route request
						uint16_t destinationAddress = r.u16L(); // "Responder" in Wireshark
						uint8_t cost = r.u8();

						// check if a node has answered our own route request
						if (originatorAddress == 0x0000) {
							ZbDevice *destination = findZbDevice(destinationAddress);
							if (destination != nullptr /*&& cost < destination->cost*/) {
								destination->sendFlags = device.data.sendFlags;

								// set the node as the router for the destination
								destination->routerAddress = device.data.shortAddress;

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
					eraseDevice(device.data.id);
					/*for (int i = 0; i < this->zbDevices.count(); ++i) {
						if (&this->zbDevices[i] == &device) {
							this->zbDevices.erase(i);
							break;
						}
					}*/
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
							w.u16L(device.data.shortAddress);

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
						key = &zb::za09LinkAesKey;
						break;
					case zb::SecurityControl::KEY_KEY_TRANSPORT:
						key = &zb::za09KeyTransportAesKey;
						break;
					case zb::SecurityControl::KEY_KEY_LOAD:
						key = &zb::za09KeyLoadAesKey;
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

						auto updatedDevice = findZbDevice(shortAddress);
						if (status == 0 && updatedDevice != nullptr) {
							//ZbDevice &updatedDevice = *d;
							//auto const &flash = *updatedDevice;

							if (updatedDevice->data.longAddress == longAddress) {
		Terminal::out << "Update Device " << hex(shortAddress);

								// clear route of updated device if we don't get the message from its first hop
								if (device.data.shortAddress != updatedDevice->routerAddress)
									updatedDevice->routerAddress = 0xffff;
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
									w.data8(Array<uint8_t const, 16>(zb::za09LinkKey));

									// extended destination address
									w.u64L(device.data.longAddress);

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
						auto hash = r.data8<16>();

						uint8_t sendResult;
						switch (keyType) {
						case zb::RequestKeyType::TRUST_CENTER_LINK:
							{
								// check key hash (4.4.10.7.4)
								DataBuffer<16> hashedKey;
								keyHash(hashedKey, zb::za09LinkKey, 3);
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
								w.u64L(device.data.longAddress);

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
					default:;
					}
				} else {
					// zdp response: forward response to the coroutine that initiated the request (handleAssociationRequest())
					uint8_t *response = r.current;
					int length = min(r.getRemaining(), ZbDevice::MESSAGE_LENGTH);
					this->endpointResponseBarrier.resumeFirst(
						[command, zdpCounter, length, response](EndpointResponse &r)
					{
						if (r.command == command && r.dstEndpoint == 0 && r.counter == zdpCounter) {
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
				int length = min(r.getRemaining(), ZbDevice::MESSAGE_LENGTH);

				this->endpointResponseBarrier.resumeFirst(
					[command, dstEndpoint, zclCounter, length, response] (EndpointResponse &r)
				{
					if (r.command == command && r.dstEndpoint == dstEndpoint && r.counter == zclCounter) {
						r.length = length;
						array::copy(length, r.response, response);
						return true;
					}
					return false;
				});
			} else if (frameType == zb::ZclFrameControl::TYPE_CLUSTER_SPECIFIC) {
				// lookup destination endpoint
				auto endpoint = device.endpoints;
				while (endpoint != nullptr && endpoint->data->id != dstEndpoint) {
					endpoint = endpoint->next;
				}
				if (endpoint == nullptr)
					continue;

				// cluster specific commands such as "on", "off"
				bool sendDefaultResponse = (frameControl & zb::ZclFrameControl::DISABLE_DEFAULT_RESPONSE) == 0;

				// debug-print command
				uint8_t command = r.peekU8();
				int length = r.getRemaining() - 1;
				//std::cout << "Device: " << std::hex << device->shortAddress << std::dec << ", ZCL command: " << int(command) << ", length: " << length << std::endl;
		Terminal::out << "Device: " << hex(device.data.shortAddress) << ", ZCL Command: " << dec(command) << ", Length: " << dec(length) << '\n';

				// lookup cluster
				auto plugs = endpoint->findServerCluster(cluster);
				if (plugs.count == 0)
					continue;

				// publish to subscribers
				for (auto &subscriber : endpoint->subscribers) {
					int plugIndex = subscriber.source.device.plugIndex - plugs.offset;

					if (plugIndex >= 0 && plugIndex < plugs.count) {
						// trigger subscriber
						subscriber.barrier->resumeFirst([&subscriber, plugIndex, cluster, &r] (PublishInfo::Parameters &p) {
							p.info = subscriber.destination;

							// convert from zbee command to message (note r is passed by value for multiple subscribers)
							return handleZclCommand(subscriber.destination.type, p.message, plugIndex, cluster,
								r, subscriber.convertOptions);
						});
					}
				}

				// reply with default response
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
	auto device = this->gpDevices;
	while (device != nullptr) {
		if (device->data->deviceId != deviceId) {
			device = device->next;
			continue;
		}

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

		// todo: check if security counter is greater than last security counter
		if (securityCounter == device->securityCounter)
			return;
		device->securityCounter = securityCounter;

		// check message integrity code or decrypt message, depending on security level
		Nonce nonce(deviceId, securityCounter);
		if (!r.decrypt(micLength, nonce, device->data->aesKey)) {
			//printf("error while decrypting message!\n");
			return;
		}

		int endpointIndex = -1;
		uint8_t message = 0;
		switch (device->data->deviceType) {
		case DeviceType::PTM215Z:
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
		case DeviceType::PTM216Z:
			if (r.getRemaining() >= 2) {
				uint8_t bar = r.u8();
				uint8_t buttons = r.u8();

				uint8_t change = (buttons ^ device->state) & 0x0f;

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

				device->state = buttons;
			}
		}

		// publish to subscribers
		for (auto &subscriber : device->subscribers) {
			// check if this is the right endpoint
			if (subscriber.source.device.plugIndex == endpointIndex) {
				subscriber.barrier->resumeFirst([&subscriber, message] (PublishInfo::Parameters &p) {
					p.info = subscriber.destination;

					auto &dst = *reinterpret_cast<Message *>(p.message);
					return convertSwitch(subscriber.destination.type, dst, message, subscriber.convertOptions);
				});
			}
		}
		break;
	}
}

void RadioInterface::handleGpCommission(uint32_t deviceId, PacketReader& r) {
	// remove commissioning command (0xe0)
	r.u8();

	GpDeviceData data;
	data.id = allocateId();
	assign(data.name, "Switch");
	data.deviceId = deviceId;

	// A.4.2.1.1 Commissioning

	// device type
	// 0x02: on/off switch
	data.deviceType = r.e8<DeviceType>();

	switch (data.deviceType) {
	case DeviceType::PTM215Z:
		// switch, PTM215Z ("friends of hue")
		data.plugs[0] = MessageType::TERNARY_BUTTON_OUT;
		data.plugs[1] = MessageType::TERNARY_BUTTON_OUT;
		data.plugs[2] = MessageType::TERNARY_BUTTON_OUT;
		data.plugCount = 3;
		break;
	case DeviceType::PTM216Z:
		// generic switch, PTM216Z
		data.plugs[0] = MessageType::TERNARY_BUTTON_OUT;
		data.plugs[1] = MessageType::TERNARY_BUTTON_OUT;
		data.plugs[2] = MessageType::TERNARY_BUTTON_OUT;
		data.plugCount = 3;
		break;
	default:
		// unknown device type
		return;
	}

	// options
	auto options = r.e8<gp::Options>();
	uint32_t securityCounter = 0;
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

				if (!decrypt(key, header.data, 4, key, 16, 4, nonce, zb::za09LinkAesKey)) {
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
			setKey(data.aesKey, Array<uint8_t const, 16>(key));
		}
		if ((extendedOptions & gp::ExtendedOptions::COUNTER_PRESENT) != 0) {
			securityCounter = r.u32L();
			//printf("counter: %08x\n", counter);
		}
	}

	// check if we exceeded the end
	if (!r.isValid())
		return;

	// check if device already exists
	auto device = this->gpDevices;
	while (device != nullptr) {
		if (device->data->deviceId != deviceId) {
			device = device->next;
			continue;
		}

		// yes: only update security counter
		device->securityCounter = securityCounter;
		return;
	}

	// add device to list of devices
	this->deviceIds[this->deviceCount++] = data.id;
	Storage2::write(STORAGE_CONFIG, STORAGE_ID_RADIO1, this->deviceCount, this->deviceIds);

	// allocate data
	int size = data.size();
	auto d = reinterpret_cast<GpDeviceData *>(malloc(size));
	array::copy(size, reinterpret_cast<uint8_t *>(d), reinterpret_cast<uint8_t *>(&data));

	// create device
	device = new GpDevice(this, d);
	device->securityCounter = securityCounter;

	// store device in flash
	Storage2::write(STORAGE_CONFIG, STORAGE_ID_RADIO1 | data.id, size, d);
}

AwaitableCoroutine RadioInterface::handleAssociationRequest(uint64_t deviceLongAddress, Radio::SendFlags sendFlags) {
Terminal::out << "handleAssociationRequest\n";

	uint16_t deviceShortAddress = this->nextShortAddress;
	uint64_t thisLongAddress = Radio::getLongAddress();


	uint8_t packet1[ZbDevice::MESSAGE_LENGTH];
	uint8_t packet2[ZbDevice::MESSAGE_LENGTH];
	uint8_t sendResult;
	zb::ZdpCommand zdpCommand;
	int length;

	// send association response
	{
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
		w.u64L(deviceLongAddress);

		// source address
		w.u64L(thisLongAddress);

		// association response
		w.e8(ieee::Command::ASSOCIATION_RESPONSE);

		// short address of device
		w.u16L(deviceShortAddress);

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

	// find existing device if any
	auto od = &this->zbDevices;
	while (*od != nullptr) {
		auto device = *od;
		if (device->data.longAddress == deviceLongAddress)
			break;
		od = &device->next;
	}
	auto oldDevice = *od;
/*
	auto oldDevice = this->zbDevices;
	while (oldDevice != nullptr) {
		if (oldDevice->data.longAddress == deviceLongAddress)
			break;
	}*/

	// create new device
	ZbDeviceData deviceData;
	deviceData.id = oldDevice != nullptr ? oldDevice->data.id : allocateZbDeviceId();
	deviceData.sendFlags = sendFlags;
	deviceData.shortAddress = deviceShortAddress;
	deviceData.longAddress = deviceLongAddress;

	// use pointer in case the coroutine gets cancelled
	Pointer<ZbDevice> device = new ZbDevice(deviceData);

	// set first hop
	device->routerAddress = deviceData.shortAddress;

	// set pointers so that we can forward responses
	this->tempDevice = device.ptr;

	int deviceCount = this->deviceCount;

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
			w.data8(*this->key);

			// key sequence number
			w.u8(0);

			// extended destination address
			w.u64L(deviceLongAddress);

			// extended source address
			w.u64L(thisLongAddress);

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
			w.u16L(deviceShortAddress);

			writeFooter(w, sendFlags);
		}
		co_await Radio::send(RADIO_ZBEE, packet1, sendResult);
Terminal::out << ("Send Node Descriptor Request Result " + dec(sendResult) + '\n');
		if (sendResult != 0) {
			// wait for a response from the device
			int r = co_await select(this->endpointResponseBarrier.wait(uint16_t(zb::ZdpCommand::NODE_DESCRIPTOR_RESPONSE),
				uint8_t(0), zdpCounter, length, packet1), Timer::sleep(1s));

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
			w.u16L(deviceShortAddress);

			writeFooter(w, sendFlags);
		}
		co_await Radio::send(RADIO_ZBEE, packet1, sendResult);
		if (sendResult != 0) {
			// wait for a response from the device
			int r = co_await select(this->endpointResponseBarrier.wait(uint16_t(zb::ZdpCommand::ACTIVE_ENDPOINT_RESPONSE),
				uint8_t(0), zdpCounter, length, packet1), Timer::sleep(1s));

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

	// get descriptor of each endpoint (packet1)
	ZbEndpoint *oldEndpoint = oldDevice != nullptr ? oldDevice->endpoints : nullptr;
	uint8_t endpointCount = endpoints.u8();
	for (int endpointIndex = 0; endpointIndex < endpointCount; ++endpointIndex) {
		// get device endpoint (packet1)
		uint8_t deviceEndpoint = endpoints.u8();

		ZbEndpointDataBuilder builder;
		if (oldEndpoint != nullptr) {
			builder.id = oldEndpoint->data->id;
		} else {
			builder.id = allocateId();
			this->deviceIds[deviceCount++] = builder.id;
		}

		// get power source (packet2)
		co_await readAttribute(packet2, *device, deviceEndpoint, zb::ZclCluster::BASIC, zb::ZclProfile::HOME_AUTOMATION,
			builder.id, uint16_t(zb::ZclBasicAttribute::POWER_SOURCE));
		auto ps = getEnum8<zb::ZclBasicPowerSourceType>(packet2);
		auto powerSource = ps ? *ps : zb::ZclBasicPowerSourceType::MAINS;
		Terminal::out << "Power Source: Status " << dec(bool(ps)) << " Source " << dec(powerSource)
			<< '\n';

		// get endpoint descriptor (packet2)
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
				w.u16L(deviceShortAddress);

				// endpoint to query
				w.u8(deviceEndpoint);

				writeFooter(w, sendFlags);
			}
			co_await Radio::send(RADIO_ZBEE, packet2, sendResult);
			if (sendResult != 0) {
				// wait for a response from the device
				int r = co_await select(this->endpointResponseBarrier.wait(uint16_t(zb::ZdpCommand::SIMPLE_DESCRIPTOR_RESPONSE),
					uint8_t(0), zdpCounter, length, packet2), Timer::sleep(timeout));

				// check if response was received
				if (r == 1)
					break;
			}
			if (retry == MAX_RETRY)
				co_return;
		}


		// handle endpoint descriptor (packet2)
		PacketReader endpointDescriptor(length, packet2);
		{
			uint8_t status = endpointDescriptor.u8();
			uint16_t addressOfInterest = endpointDescriptor.u16L();
			uint8_t descriptorLength = endpointDescriptor.u8();
			uint8_t endpoint2 = endpointDescriptor.u8();
			zb::ZclProfile profile = endpointDescriptor.e16L<zb::ZclProfile>();
			uint16_t applicationDevice = endpointDescriptor.u16L();
			uint8_t applicationVersion = endpointDescriptor.u8();
			Terminal::out << "Endpoint Descriptor " << dec(deviceEndpoint) << " Status " << dec(status) << '\n';
		}



		// iterate over input/server clusters of device and create the counterparts (packet2)
		uint8_t inputClusterCount = endpointDescriptor.u8();
		for (int i = 0; i < inputClusterCount && endpointDescriptor.getRemaining() >= 3; ++i) {
			zb::ZclCluster cluster = endpointDescriptor.e16L<zb::ZclCluster>();

			builder.beginClientCluster(cluster);
			switch (cluster) {
			case zb::ZclCluster::BASIC:
				break;
			case zb::ZclCluster::POWER_CONFIGURATION:
				if (powerSource == zb::ZclBasicPowerSourceType::BATTERY) {
					builder.addPlug(MessageType::LEVEL_BATTERY_IN);
					builder.addPlug(MessageType::PHYSICAL_VOLTAGE_MEASURED_LOW_IN);
				}
				break;
			case zb::ZclCluster::ON_OFF:
				builder.addPlug(MessageType::BINARY_POWER_CMD_IN);
				break;
			case zb::ZclCluster::LEVEL_CONTROL:
				builder.addPlug(MessageType::LEVEL_CMD_IN);
				break;
			case zb::ZclCluster::COLOR_CONTROL:
				builder.addPlug(MessageType::LIGHTING_COLOR_PARAMETER_CHROMATICITY_X_CMD_IN);
				builder.addPlug(MessageType::LIGHTING_COLOR_PARAMETER_CHROMATICITY_Y_CMD_IN);
				break;
			default:;
			}
			builder.endClientCluster();
		}

		// iterate over output/client clusters and create counterparts (client sends commands to manipulate attributes in server) (packet2)
		uint8_t outputClusterCount = endpointDescriptor.u8();
		for (int i = 0; i < outputClusterCount && endpointDescriptor.getRemaining() >= 2; ++i) {
			zb::ZclCluster cluster = endpointDescriptor.e16L<zb::ZclCluster>();

			builder.beginServerCluster(cluster);
			switch (cluster) {
			case zb::ZclCluster::BASIC:
				break;
			case zb::ZclCluster::ON_OFF:
				builder.addPlug(MessageType::BINARY_POWER_CMD_OUT);
				break;
			case zb::ZclCluster::LEVEL_CONTROL:
				builder.addPlug(MessageType::LEVEL_CMD_OUT);
				break;
			case zb::ZclCluster::COLOR_CONTROL:
				builder.addPlug(MessageType::LIGHTING_COLOR_PARAMETER_CHROMATICITY_X_CMD_OUT);
				builder.addPlug(MessageType::LIGHTING_COLOR_PARAMETER_CHROMATICITY_Y_CMD_OUT);
				break;
			default:;
			}
			builder.endServerCluster();
		}

		auto data = reinterpret_cast<ZbEndpointData *>(malloc(builder.size()));
		builder.build(data);
		data->deviceId = device->data.id;
		data->deviceEndpoint = deviceEndpoint;

		// get model identifier (packet2)
		co_await readAttribute(packet2, *device, deviceEndpoint, zb::ZclCluster::BASIC, zb::ZclProfile::HOME_AUTOMATION,
			builder.id, uint16_t(zb::ZclBasicAttribute::MODEL_IDENTIFIER));
		auto modelIdentifier = getString(packet2).get("Device");
		assign(data->name, modelIdentifier);

		// create endpoint, device takes ownership
		auto endpoint = new ZbEndpoint(device.ptr, data);

		// bind all device client clusters to our server clusters
		for (int i = 0; i < builder.serverClusterIndex; i++) {
			auto cluster = builder.clusterInfos[i].cluster;
			for (int retry = 0;; ++retry) {
				// send bind request (packet2)
				uint8_t zdpCounter;
				{
					PacketWriter w(packet2);

					// nwk data
					writeNwkData(w, *device);

					// zdp data
					zdpCounter = writeApsDataZdp(w, zb::ZdpCommand::BIND_REQUEST);

					// source
					w.u64L(deviceLongAddress);

					// source endpoint (that later sends commands)
					w.u8(deviceEndpoint);

					// cluster
					w.e16L(cluster);

					// address mode: unicast
					w.u8(3);

					// destination
					w.u64L(thisLongAddress);

					// destination endpoint (that later receives commands)
					w.u8(builder.id);

					writeFooter(w, sendFlags);
				}
				co_await Radio::send(RADIO_ZBEE, packet2, sendResult);
				if (sendResult != 0) {
					// wait for a response from the device
					int r = co_await select(this->endpointResponseBarrier.wait(uint16_t(zb::ZdpCommand::BIND_RESPONSE),
						uint8_t(0), zdpCounter, length, packet2), Timer::sleep(timeout));

					// check if response was received
					if (r == 1)
						break;
				}
				if (retry == MAX_RETRY)
					co_return;
			}

			// handle bind response
			PacketReader bindResponse(length, packet2);
			uint8_t status = bindResponse.u8();
			Terminal::out << "bind response status " << dec(status) << '\n';
		}

		if (oldEndpoint != nullptr)
			oldEndpoint = oldEndpoint->next;
	}

	// remove old endpoints from device list
	while (oldEndpoint != nullptr) {
		int j = 0;
		for (int i = 0; i < deviceCount; ++i) {
			auto id = this->deviceIds[i];
			if (id != oldEndpoint->data->id)
				this->deviceIds[j++] = id;
		}
		deviceCount = j;
		oldEndpoint = oldEndpoint->next;
	}

	// store device list
	if (this->deviceCount != deviceCount) {
		this->deviceCount = deviceCount;
		Storage2::write(STORAGE_CONFIG, STORAGE_ID_RADIO1, deviceCount, this->deviceIds);
	}

	// store endpoints
	auto endpoint = device->endpoints;
	while (endpoint != nullptr) {
		Storage2::write(STORAGE_CONFIG, STORAGE_ID_RADIO1 | endpoint->data->id, endpoint->data->size(), endpoint->data);
		endpoint = endpoint->next;
	}

	// store device
	Storage2::write(STORAGE_CONFIG, STORAGE_ID_RADIO2 | device->data.id, sizeof(device->data), &device->data);

	// delete old device and its endpoints
	if (oldDevice != nullptr) {
		// remove device from linked list
		*od = oldDevice->next;

		// delete old endpoints
		auto endpoint = oldDevice->endpoints;
		while (endpoint != nullptr) {
			auto e = endpoint;
			endpoint = endpoint->next;
			delete e;
		}
		delete oldDevice;
	}

	// transfer ownership of device to devices list
	device->interface = this;
	device->next = this->zbDevices;
	this->zbDevices = device.ptr;
	device.ptr = nullptr;
	this->tempDevice = nullptr;

	// allocate next short address and interface id
	allocateShortAddress();
}

Coroutine RadioInterface::publish() {
	uint8_t packet[64];
	while (true) {
		// wait for message
		MessageInfo info;
		Message message;
		co_await this->publishBarrier.wait(info, &message);

		// find destination device
		auto device = this->zbDevices;
		while (device != nullptr) {
			auto endpoint = device->endpoints;
			while (endpoint != nullptr) {
				if (endpoint->data->id != info.device.id) {
					endpoint = endpoint->next;
					continue;
				}

				// get endpoint info
				auto plugIndex = info.device.plugIndex;
				if (plugIndex >= endpoint->data->plugCount)
					break;
				auto messageType = endpoint->getPlugs()[plugIndex];
				auto clusterInfo = endpoint->getClientCluster(plugIndex);

				// check if it is an input
				if (isInput(messageType)) {
					// request the route if necessary
					for (int retry = 0; retry < MAX_RETRY; ++retry) {
						if (device->routerAddress != 0xffff)
							break;

						// reset cost of route
						device->cost = 255;

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
							w.u16L(device->data.shortAddress);

							// path cost
							w.u8(0);

							// extended destination
							w.u64L(device->data.longAddress);

							writeFooter(w, Radio::SendFlags::NONE);
						}

						// send packet
						uint8_t sendResult;
						co_await Radio::send(RADIO_ZBEE, packet, sendResult);
						if (sendResult == 0)
							continue;

						// wait for first route reply or timeout (more route replies with lower cost may arrive later)
						co_await select(device->routeBarrier.wait(), Timer::sleep(timeout));

						Terminal::out << "Router for " << hex(device->data.shortAddress) << ": " << hex(device->routerAddress)
							<< '\n';
					}

					// fail if route remains unknown
					if (device->routerAddress == 0xffff) {
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
							writeNwkData(w, *device);

							// aps data
							writeApsDataZcl(w, endpoint->data->deviceEndpoint, clusterInfo.cluster,
								zb::ZclProfile::HOME_AUTOMATION, endpoint->data->id);

							if ((messageType & MessageType::CMD) == 0) {
								// write attribute (maybe using a command if the attribute is read only)
								if (!writeZclAttribute(w, zclCounter, plugIndex, clusterInfo.cluster, message))
									break;
							} else {
								// send command
								if (!writeZclCommand(w, zclCounter, plugIndex, clusterInfo.cluster, message))
									break;
							}

							writeFooter(w, Radio::SendFlags::NONE);
						}

						// send packet
						uint8_t sendResult;
						co_await Radio::send(RADIO_ZBEE, packet, sendResult);
						if (sendResult != 0) {
							// wait for a response from the device
							int length;
							int r = co_await select(this->endpointResponseBarrier.wait(uint16_t(zb::ZclCommand::DEFAULT_RESPONSE),
								endpoint->data->id, zclCounter, length, packet), Timer::sleep(timeout));

							// check if response was received
							if (r == 1) {
								MessageReader r(length, packet);
								uint8_t responseToCommand = r.u8();
								uint8_t status = r.u8();

								// check status (0 = ok)
								if (status == 0)
									break;
							}
						}

						// retry
					}
				}
				/*
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
				}*/
				break;
			}
			if (endpoint != nullptr)
				break;
			device = device->next;
		}
	}
}
