#pragma once

namespace Realtime {

struct __attribute__ ((__packed__)) spike {
	enum PACKET_TYPE {
		DUMMY,
		SYNC,
		SPIKES = 0xff
	};
	// for now: full-width timestamps
	uint64_t timestamp0;
	uint64_t timestamp;
	uint16_t label;
	uint16_t packet_type;
};

struct __attribute__ ((__packed__)) spike_h {
	uint32_t label; // 46

	void ntoh() {
		label = ntohl(label);
	}
	void hton() {
		label = htonl(label);
	}

	static void set_label(spike_h & sp, uint32_t const label) {
		sp.label = label;
	}

	static uint32_t get_label(spike_h & sp) {
		return sp.label;
	}
};

} // namespace
