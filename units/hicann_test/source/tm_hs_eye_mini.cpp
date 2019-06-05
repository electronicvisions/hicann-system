// testmode for l2 communication measure
#include <time.h>

#include "common.h"
#include "fpga_control.h"
#include "hicann_ctrl.h"
#include "pulse_float.h"

#include "s2c_jtagphys.h"
#include "s2c_jtagphys_2fpga.h"
#include "s2comm.h"
#include "testmode.h"

using namespace std;
using namespace facets;

class TmHSEyeMini : public Testmode
{
public:
	static const uint32_t MAX_DELAY_TAPS = 32;      // physical maximum tap number of delay lines on hs-interface

	static const uint32_t link_status_mask = 0x73;  // only use these bits for link status evaluation
	static const uint32_t link_status_idle = 0x41;  // idle, and ready for pulse or config data transmission

	uint64_t jtagid;

	void printInitStates()
	{
		uint64_t fpga_state;
		unsigned char fpga_link_status;
		uint64_t hicann_link_status;

		jtag->K7FPGA_get_hicannif_status(fpga_state);
		fpga_link_status = (unsigned char) (fpga_state & 0xff);

		jtag->HICANN_read_status(hicann_link_status);

		// write statuses
		std::cout << "Status: FPGA (exp. 0x41): " << std::hex
		          << ((unsigned int) (fpga_link_status & link_status_mask))
		          << ", HICANN (exp. 0x41): "
		          << ((unsigned int) (hicann_link_status & link_status_mask)) << std::endl;
	}

	void measureHicannConnection(
	    uint32_t hicann_nr, uint32_t hicann_delay, uint32_t fpga_delay, uint32_t num_data)
	{
		uint64_t rdata64;
		uint64_t wdata64;

		uint64_t fpga_state;
		unsigned char fpga_link_status;
		uint64_t hicann_link_status;

		unsigned int fpga2hicann_error;
		unsigned int hicann2fpga_error;
		unsigned int init_error = 0;
		unsigned int data_error = 0;

		uint64_t delay_in;
		uint64_t delay_out;

		// prepare FPGA
		jtag->K7FPGA_stop_fpga_link();
		jtag->K7FPGA_set_hicannif(7 - hicann_nr);
		jtag->K7FPGA_set_hicannif_config(0x008); // stop link, manual init, master enabled
		//                              (0x00c); // stop link, auto init, master enabled
		// jtag->K7FPGA_disable_hicannif_config_output();

		// prepare HICANN
		jtag->HICANN_stop_link();
		jtag->HICANN_set_link_ctrl(0x020);
		jtag->HICANN_lvds_pads_en(0);

		jtag->HICANN_set_test(1);

		// set FPGA delay
		jtag->K7FPGA_set_hicannif_data_delay(fpga_delay);

		// set HICANN delay
		delay_in = ~(hicann_delay & 0x3f);
		jtag->HICANN_set_data_delay(delay_in, delay_out);
		std::cout << "HICANN init with data delay: " << std::hex << delay_in
		          << ", read back: " << (0x3f & delay_out) << std::endl;
		std::cout << "Original delay values FPGA: " << std::hex << fpga_delay
		          << "\t HICANN: " << (63 - hicann_delay) << std::endl;

		// Check Status
		fpga_link_status = 0;
		hicann_link_status = 0;

		while ((((fpga_link_status & link_status_mask) != link_status_idle) ||
		        ((hicann_link_status & link_status_mask) != link_status_idle)) &&
		       init_error < 1) {
			jtag->K7FPGA_stop_fpga_link();
			jtag->HICANN_stop_link();

			jtag->K7FPGA_start_fpga_link();
			jtag->HICANN_start_link();

			printInitStates();
			usleep(1000);

			for (unsigned int nrep = 0; nrep < 1; ++nrep) {
				jtag->K7FPGA_get_hicannif_status(fpga_state);
				unsigned int init_fsm_state = (fpga_state >> 30) & 0x3f;
				std::cout << "state of init FSM on K7 FPGA: " << std::hex << init_fsm_state
				          << std::endl;
				unsigned int init_rxdata = (fpga_state >> 36) & 0xff;
				std::cout << "rx data on K7 FPGA: 0x" << std::hex << init_rxdata << std::dec
				          << std::endl;
				unsigned int init_txdata = (fpga_state >> 44) & 0xff;
				std::cout << "tx data on K7 FPGA: 0x" << std::hex << init_txdata << std::dec
				          << std::endl;
				std::cout << "complete : " << std::hex << fpga_state << std::dec << std::endl;
			}

			jtag->K7FPGA_get_hicannif_status(fpga_state);
			fpga_link_status = (unsigned char) (fpga_state & 0xff);

			jtag->HICANN_read_status(hicann_link_status);

			// write statuses
			std::cout << "Status: FPGA (exp. 0x41): " << std::hex
			          << ((unsigned int) (fpga_link_status & link_status_mask))
			          << ", HICANN (exp. 0x41): "
			          << ((unsigned int) (hicann_link_status & link_status_mask)) << std::endl;

			unsigned int init_fsm_state = (fpga_state >> 30) & 0x3f;
			std::cout << "state of init FSM on K7 FPGA: " << std::hex << init_fsm_state
			          << std::endl;
			std::cout << "complete FPGA state: " << std::hex << fpga_state << std::dec << std::endl;

			uint64_t read_delay_val = 0;
			jtag->K7FPGA_get_hicannif_data_delay(read_delay_val);
			if (read_delay_val != fpga_delay)
				std::cout << "WARNING: read-back of delay value was " << read_delay_val
				          << ", but should be " << fpga_delay << "." << std::endl;

			init_error++;
		}

		// test data transmission in case of successful init
		if (((fpga_link_status & link_status_mask) == link_status_idle) &&
		    ((hicann_link_status & link_status_mask) == link_status_idle)) {
			fpga2hicann_error = 0;
			hicann2fpga_error = 0;

			for (int n = 0; n < num_data; ++n) {
				wdata64 = (((uint64)((rand() % 0xffff) + 1) << 48)) +
				          (((uint64)((rand() % 0xffffff) + 1) << 24)) +
				          (uint64)((rand() % 0xffffff) + 1);
				jtag->K7FPGA_set_hicannif_tx_data(wdata64 & 0x7fff);
				jtag->K7FPGA_set_hicannif_control(0x2);

				jtag->HICANN_get_rx_data(rdata64);

				if ((rdata64 & 0x7fff) != (wdata64 & 0x7fff)) {
					++fpga2hicann_error;
					printf(
					    "Wrote: 0x%016llX Read: 0x%016llX\n", (long long unsigned int) wdata64,
					    (long long unsigned int) rdata64);
				}
			}

			for (int n = 0; n < num_data; ++n) {
				wdata64 = (((uint64)((rand() % 0xffff) + 1) << 48)) +
				          (((uint64)((rand() % 0xffffff) + 1) << 24)) +
				          (uint64)((rand() % 0xffffff) + 1);
				jtag->HICANN_set_tx_data(wdata64);
				jtag->HICANN_start_cfg_pkg();

				jtag->K7FPGA_get_hicannif_rx_cfg(rdata64);

				if (rdata64 != wdata64) {
					++hicann2fpga_error;
					printf(
					    "Wrote: 0x%016llX Read: 0x%016llX\n",
					    static_cast<long long unsigned int>(wdata64),
					    static_cast<long long unsigned int>(rdata64));
				}
			}

			printf(
			    "Errors in HICANN-FPGA transmission : %i  FPGA-HICANN transmission : %i\n",
			    hicann2fpga_error, fpga2hicann_error);
		} else {
			fpga2hicann_error = -1;
			hicann2fpga_error = -1;
			printf(
			    "ERROR init with FPGA state: %.02X and HICANN state %.02X\n", fpga_link_status,
			    static_cast<uint32_t>(hicann_link_status));
		}

		jtag->K7FPGA_enable_hicannif_config_output();
		jtag->HICANN_set_test(0);
	}


	// test function
	bool test()
	{
		uint32_t fpga_delay;
		uint32_t hicann_delay;

		uint32_t hicann_jtag_addr;

		if (!comm->is_k7fpga()) {
			dbg(0) << "Not on K7 FPGA - EXIT" << endl;
			return TESTS2_FAILURE;
		}

		FPGAControl* fpga = (FPGAControl*) chip[0];

//		while(true) {
// ----------------------------------------------------
// reset test logic and FPGA
// ----------------------------------------------------
#ifdef NCSIM
		hicann_jtag_addr = 4;
		jtag->set_hicann_pos(hicann_jtag_addr);
#else
		hicann_jtag_addr = chip[FPGA_COUNT + DNC_COUNT + 0]->addr();
#endif

		jtag->reset_jtag();
		fpga->reset();

		// disable FPGA arq
		comm->set_fpga_reset(comm->jtag->get_ip(), false, false, false, false, true);

		jtag->read_id(jtagid, jtag->pos_fpga);
		cout << "Read FPGA JTAG ID: 0x" << hex << jtagid << endl;
		if (jtagid != 0x1c56c007)
			return TESTS2_FAILURE;

		jtag->read_id(jtagid, jtag->pos_hicann);
		cout << "Read HICANN JTAG ID: 0x" << hex << jtagid << endl;
		if (jtagid != 0x14849434)
			return TESTS2_FAILURE;


#ifdef NCSIM
		fpga_delay = 10;
		hicann_delay = 10;
		measureHicannConnection(hicann_jtag_addr, hicann_delay, fpga_delay, 10);

#else
		// use this for interactive testing, together with outer while(true) loop
		// -> comment out for-loops in that case.
		// cout << "Enter FPGA delay value:   "; cin >> fpga_delay;
		// cout << "Enter HICANN delay value: "; cin >> hicann_delay;

		for (hicann_delay = 0; hicann_delay < MAX_DELAY_TAPS; hicann_delay += 2) {
			for (fpga_delay = 0; fpga_delay < MAX_DELAY_TAPS; fpga_delay += 2) {
				measureHicannConnection(hicann_jtag_addr, hicann_delay, fpga_delay, 10);
				cout << endl
				     << "--------------------------------------------------------------------"
				     << endl
				     << endl;
			}
		}
//		}
#endif

		return TESTS2_SUCCESS;
	}
};


class LteeTmHSEyeMini : public Listee<Testmode>
{
public:
	LteeTmHSEyeMini() : Listee<Testmode>(string("tm_hs_eye_mini"), string("test")){};
	Testmode* create() { return new TmHSEyeMini(); };
};

LteeTmHSEyeMini ListeeTmHSEyeMini;
