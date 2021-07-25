#pragma once

#include "Configuration.hpp"
#include "Interface.hpp"
#include "Storage.hpp"
#include <radio.hpp>
#include <crypt.hpp>
#include <PacketReader.hpp>
#include <PacketWriter.hpp>
#include <Coroutine.hpp>
#include <appConfig.hpp>


/**
 * Devices connected by radio using IEEE 802.15.4 standard
 */
class RadioInterface : public Interface {
public:
	static constexpr int MAX_DEVICE_COUNT = 64;

	/**
	 * Constructor
	 * @param configuration global configuration
	 * @param onReceived gets called when data was reveived from an endpoint (endpointId, data, length)
	 */
	RadioInterface(Storage::Array<Configuration, void> &configuration,
		std::function<void (uint8_t, uint8_t const *, int)> const &onReceived);

	~RadioInterface() override;

	/**
	 * Set long address
	 */
	//void setLongAddress(uint64_t longAddress) {this->longAddress = longAddress;}

	/**
	 * Set pan is where the radio interface should operate. Must be called
	 * @param panId pan id
	 */
	//void setPan(uint16_t panId);

	/**
	 * Set commissioning mode
	 * @param enabled true to enable commissioning
	 */
	void setCommissioning(bool enabled) override;

	int getDeviceCount() override;
	DeviceId getDeviceId(int index) override;

	Array<EndpointType> getEndpoints(DeviceId deviceId) override;

	void subscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) override;
	void unsubscribe(uint8_t &endpointId, DeviceId deviceId, uint8_t endpointIndex) override;

	void send(uint8_t endpointId, uint8_t const *data, int length) override;

	

private:

	struct Device {
		enum Type : uint8_t {
			GP,
			ZB
		};
		
		static constexpr int MAX_ENDPOINT_COUNT = 32;
		
		// device id, either 64 bit ieee 802.15.4 long device address or 32 bit green power id
		DeviceId deviceId;

		// radio device type
		Type type;
		
		// device type from commissioning message
		uint8_t deviceType;
	
		uint8_t endpointCount;
	
		// endpoints derived from device type (zero terminated list)
		//EndpointType endpointTypes[4];

		union {
			struct {
				// device key for green power devices
				AesKey key;
			
				// endpoint types
				uint8_t endpoints[MAX_ENDPOINT_COUNT];
			} gp;

			struct {
				// short device address
				uint16_t shortAddress;

				// endpoint types followed by pairs of endpoint info index and zb endpoint
				uint8_t endpoints[MAX_ENDPOINT_COUNT * 3];
			} zb;
		} u;
		
		
		/**
		 * Returns the size in bytes needed to store the device configuration in flash
		 * @return size in bytes
		 */
		int getFlashSize() const;

		/**
		 * Returns the size in bytes needed to store the device state in ram
		 * @return size in bytes
		 */
		int getRamSize() const;
		
		/**
		 * Returns the number of endpoints
		 * @return number of endpoints
		 */
		//int getEndpointCount() const;
		
		EndpointType const *getEndpoints() const;
		
		uint8_t const *getEndpointIndices() const {return this->u.zb.endpoints + this->endpointCount;}
	};

	struct DeviceState {
		// per-device security counter (for gp)
		uint32_t securityCounter;
		uint8_t state;


		static constexpr int RESPONSE_LENGTH = 64;

		
		// the query coroutine waits on this barrier until a data request arrives
		//Barrier<> dataRequestBarrier;
		
		struct ZdpResponse {
			zb::ZdpCommand command;
			uint8_t zdpCounter;
			uint16_t& length;
			uint8_t *response;
		};
		
		// the query coroutine waits on this barrier until a zdp response arrives
		Barrier<ZdpResponse> zdpResponseBarrier;
		

		struct ZclResponse {
			uint8_t dstEndpoint;
			uint8_t srcEndpoint;
			zb::ZclProfile profile;
			zb::ZclCluster cluster;
			uint8_t zclCounter;
			zb::ZclCommand command;
			uint16_t& length;
			uint8_t *response;
		};
		
		// the query coroutine waits on this barrier until a zcl response arrives
		Barrier<ZclResponse> zclResponseBarrier;



		struct Endpoint {
			uint8_t referenceCounter = 0;

			Barrier<Interface::Parameters> barrier;
		};

		Endpoint *endpoints;
	};
	
	
	class PacketWriter2 : public PacketWriter {
	public:
		PacketWriter2(uint8_t *packet) : PacketWriter(packet) {}

		uint8_t const *header;
		uint8_t *securityControlPtr;
		uint8_t *message;
	};


	Coroutine receive();

	void handleGp(uint8_t const *mac, PacketReader &r);
	void handleGpCommission(uint32_t deviceId, PacketReader& r);

	AwaitableCoroutine handleAssociationRequest(uint64_t sourceAddress, radio::SendFlags sendFlags);
	//void handleAssociationRequest(uint64_t sourceAddress, PacketReader &r);
	void handleNwk(PacketReader &r, Device const &device, DeviceState *state);
	void handleAps(PacketReader &r, Device const &device, DeviceState *state);
	void handleZdp(PacketReader &r, Device const &device, DeviceState *state);
	void handleZcl(PacketReader &r, Device const &device, DeviceState *state, uint8_t destinationEndpoint);

	// ieee beacon
	Coroutine sendBeacon();
	
	// beacon coroutine waits on this barrier until a beacon request arrives
	Barrier<> beaconBarrier;
	
	// zb link status
	Coroutine sendLinkStatus();


	void writeNwkHeader(PacketWriter2 &p, uint16_t destinationAddress, zb::NwkFrameControl nwkFrameControl);
	void writeApsCommand(PacketWriter2 &p, zb::ApsFrameControl apsFrameControl);
	//void writeApsData(PacketWriter2 &p, uint8_t destinationEndpoint, uint16_t clusterId, uint16_t profile,
	//	uint8_t sourceEndpoint);
	uint8_t writeZdpHeader(PacketWriter2 &w, zb::ZdpCommand command);//, uint16_t addressOfInterest);
	uint8_t writeZclProfileWide(PacketWriter2 &w, uint8_t dstEndpoint, uint8_t srcEndpoint, zb::ZclProfile profile,
		zb::ZclCluster cluster, zb::ZclCommand command);
	void writeFooter(PacketWriter2 &w, radio::SendFlags sendFlags);


	// global configuration
	Storage::Array<Configuration, void> &configuration;

public:
	
	// list of commissioned devices
	Storage::Array<Device, DeviceState*> devices;
	
	//CoroutineHandle tempHandle;
	AwaitableCoroutine associationCoroutine;
	Device *tempDevice;// = nullptr;
	DeviceState *tempState;// = nullptr;

private:
	
	// callback
	std::function<void (uint8_t, uint8_t const *, int)> onReceived;

	// true for commissioning mode, joining of new devices is allowed
	bool commissioning = false;

	// endpoint reference counters
	uint8_t nextEndpointId = 1;


	// counters
	uint8_t macCounter = 0;
	uint8_t nwkCounter = 0;
	uint8_t apsCounter = 0;
	uint8_t zdpCounter = 24;//21;//0;
	uint8_t zclCounter = 11;//6;//0;
	uint32_t securityCounter = 0;
};
