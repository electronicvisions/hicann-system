#pragma once

#include "host_al_controller.h"
#include "s2c_jtagphys_2fpga.h"

#ifdef NCSIM
#include "ARQStreamWrap.h"
#else
#include "sctrltp/ARQStream.h"
#endif // NCSIM

// for packet
#include "sctrltp/ARQFrame.h"

// for JTAG: named socket
#include <boost/thread/thread.hpp>

namespace facets {

class S2C_JtagPhys2FpgaArq : public S2C_JtagPhys2Fpga
{
public:
	typedef unsigned int dncid_t;
	S2C_JtagPhys2FpgaArq(
		CommAccess const& access,
		myjtag_full* j,
		bool on_reticle,
		dncid_t dncid,
		std::shared_ptr<sctrltp::ARQStream> arq_ptr,
		bool use_k7fpga = false);
	~S2C_JtagPhys2FpgaArq();

	// override some S2C_JtagPhys2Fpga functions
	Commstate Init(
		std::bitset<8> hicann,
		bool silent = false,
		bool force_highspeed_init = false,
		bool return_on_error = false); // added hostal init
	Commstate Init(
		int hicann_nr,
		bool silent = false,
		bool force_highspeed_init = false,
		bool return_on_error = false);                    // deprecated stuff
	Commstate Send(IData data = emptydata, uint del = 0); // disabled (sw-arq stuff)
	Commstate Receive(IData& data);                       // disabled  (sw-arq stuff)
	Commstate Flush(void);
	Commstate Clear(void);

	/// synchronized systime counter start in FPGA and HICANNs
	/// enable: false -> prime systime counter
	/// enable: true  -> start systime counter
	/// listen_global: false -> listen to host signal
	/// listen_global: true  -> listen to master fpga signal
	void trigger_systime(
		unsigned int const fpga_ip,
		bool const enable = true,
		bool const listen_global = false,
		uint const sysstart_port = 1800);

	/// synchronized experiment start (i.e. start of pm and trace memory) in FPGA
	/// enable: false -> prime systime counter
	/// enable: true  -> start systime counter
	/// listen_global: false -> listen to host signal
	/// listen_global: true  -> listen to master fpga signal
	void trigger_experiment(
		unsigned int const fpga_ip,
		bool const enable = true,
		bool const listen_global = false,
		uint const sysstart_port = 1800);

	// from s2comm
	int issueCommand(uint hicann_nr, uint tagid, ci_payload* data, uint del);
	int recvData(uint hicann_nr, uint tagid, ci_payload* data);

	unsigned int get_max_bulk() const { return max_bulk; }
	void set_max_bulk(unsigned int const m) { max_bulk = m; }

	// temporary stuff, for getting access to pulse handling functionality in HostALController
	HostALController* getHostAL() { return &(this->hostalctrl); }

	// reset HostAL connection
	void initHostAL();

	// made public only for access from testmodes!
	dncid_t dncid;

private:
	// FIXME: (very ugly) boilerplate stuff -- fix awkward dbg() functionality
	int debug_level;
	virtual std::string ClassName() { return "S2C_JtagPhys2FpgaArq"; };
	virtual std::ostream& dbg() { return std::cout << ClassName() << ": "; }
	virtual std::ostream& dbg(int level)
	{
		if (this->debug_level >= level)
			return dbg();
		else
			return NULLOS;
	}

	// HostAL stuff
	std::shared_ptr<sctrltp::ARQStream> p_ARQStream;

	HostALController hostalctrl;
	unsigned int bulk;
	unsigned int max_bulk;

	// print out state of the FPGA-HICANN ARQ connection
	friend std::ostream& operator<<(std::ostream&, S2C_JtagPhys2FpgaArq const&);
};

} // facets
