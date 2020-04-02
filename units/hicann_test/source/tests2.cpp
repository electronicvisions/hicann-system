/**
// Company         :   kip
// Author          :   Andreas Gruebl
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//
// Filename        :   tests2.cpp
// Project Name    :   p_facets
// Subproject Name :   s_hicann1
//
// Create Date     :   Fri Jun 20 08:12:24 2008
// Last Change     :   Mon Aug 04 08
// by              :   agruebl
//------------------------------------------------------------------------


This file contains the main routine of the Stage 2 test software.

Changelog:

* 04.08.08: (AG): completely moved multi-threading parts to Stage2Comm child (e.g.
S2C_TcpIp->Stage2Comm)
*/

#include <sstream>

#include <boost/program_options.hpp>

#include "common.h" // common library includes etc.
#include "s2_types.h"
#include "s2c_jtagphys.h"
#include "s2c_jtagphys_2fpga.h"
#include "s2c_jtagphys_2fpga_arq.h"
#include "s2comm.h" // Stage 2 communication model base class
#include "stage2_conf.h"

#include "ctrlmod.h"
#include "dnc_control.h"
#include "fpga_control.h"
#include "hicann_ctrl.h"
#include "s2ctrl.h"
#include "testmode.h"

#include "HandleCommFreezeError.h"

extern "C" {
#include <netdb.h>
#include <arpa/inet.h>
}

#include "git_version.h"

using namespace facets;
using namespace std;
namespace bpo = boost::program_options;

// TODO: boostify all option parsing...
// clang-format off
const char usagestr[]="usage: tests2 [-option [Argument]]\n"\
        "options: -bt                     use tcpip communication model\n"\
        "         -btp                    use tcpip communication model with playback memory\n"\
        "         -bo                     use direct ocp communication model\n"\
        "          Comm. technique for:   |   FPGA       |  DNC  | HICANN \n"\
        "          -----------------------|--------------|-------|--------\n"\
        "         -bj                     |   JTAG       |  --   |  --     -> e.g. for testing I2C access only\n"\
        "         -bj                     |    --        |  --   | JTAG   \n"\
        "         -bjw                    | Eth/JTAG     | JTAG  | JTAG    -> Running on a wafer with JTAG\n"\
        "         -bjw2f                  | Eth/JTAG     | GBit  | Gbit    -> Running on a wafer with Gbit\n"\
        "         -bj2f   <nh>  <h>       |   JTAG       | GBit  | Gbit    -> <nh> HICANNs in chain, using HICANN <h>\n"\
        "         -bje2f  <nh>  <h>       | Eth/JTAG     | GBit  | Gbit    -> <nh> HICANNs in chain, using HICANN <h>\n"\
        "         -bje2fa <nh>  <h>       | Eth/JTAG/ARQ | GBit  | Gbit    -> <nh> HICANNs in chain, using HICANN <h>\n"\
        "         -bje    <nh>  <h>       | Eth/JTAG     | JTAG  | JTAG    -> <nh> HICANNs in chain, using HICANN <h>\n"\
        "         -bja    <nh>  <h>       |   JTAG       | JTAG  | JTAG    ->                -- \" --\n"\
        "         -bjac   <nh>  <h>       |   JTAG       |  --   | JTAG    ->                -- \" --\n"\
        "         -r      <reticle_nr>    running on wafer reticle (affects dnc addrs and timing)\n"\
        "         -s      <starttime>     starttime for testbench (used to sync chips)\n"\
        "         -sr     <seed>          seed value for random number generation\n"\
        "         -js     <speed>         set speed of JTAG interface in kHz (750, 1500, 3000, 6000, 12000, 24000)\n"\
        "         -c      <filename>      specify xml-file to be loaded - default is hicann_conf.xml\n"\
        "         -log    <loglevel>      specify loglevel [0-ERROR,1-WARNING,2-INFO(default),3-DEBUG0,4-DEBUG1,5-DEBUG2]\n"\
        "         -m      <name>          uses testmode\n"\
        "         -label  <label>         label your test\n"\
        "         -dc     <hc_nr> <chnl>  HICANN no. <hc_nr> is connected to DNC channel <chnl> on test setup\n"\
        "         -k      <keysequence>   CSV sequence of testmode-option-keys\n"\
        "         -ip     <x.x.x.x>       Set the FPGA's IP address for Ethernet communication\n"\
        "         -jp     <port>          Set port for JTAG-over-Ethernet communication\n"\
        "         -l                      list testmodes\n"\
        "         -k7                     use Kintex7 setup\n"\
        "         -kill                   kill testmode upon \"recvfrom error 11\"\n"\
        "         -bulksize               choose bulk size of ARQ stuff (DONT PUSH THIS)\n"\
        "         -shm_name               shared memory name used by HostARQ (cf. program start_core)\n"\
        "         -bitmask                bitmask of connected hicanns\n"\
		"options following \"--\" will be added to testmodes (std::vector<std::string>) argv_options\n"
/*      "         -v / -d                 verbose output / debug output\n"*/;
// clang-format on

template <>
Listee<Testmode>* Listee<Testmode>::first = NULL; // this allows access for all Listees

uint randomseed = 42;


int main(int argc, char* argv[])
{

	enum options
	{
		none,
		opthelp,
		opttime,
		sr,
		js,
		optmode,
		listmodes,
		config,
		logger,
		tlabel,
		jthcnum,
		jthcaddr,
		dcnr,
		dcnl,
		stdiokeys,
		ipset,
		jpset,
		onret,
		bulksize,
		shared_memory_name,
		hicann_bitmask
	} actopt = none;                        ///< check for command line options
	commodels commodel = jtag;              ///< available communication models see s2_types
	uint starttime = 0;                     ///< system start time for all tests
	uint jtag_speed = 750;                  ///< default to minimum jtag speed
	uint log_level = 2;                     //< default log level is info
	vector<string> modes;                   ///< testmodes to use
	string confFileName = "hicann_cfg.xml"; ///< name of config file to be loaded
	string label = "unknown";
	istringstream keys("");
	string key = "";
	vector<string> keysequence;
	keysequence.clear();
	uint num_jtag_hicanns = 8; ///< number of HICANNs in JTAG chain
	uint tar_jtag_hicann = 0;  ///< HICANN address within num_jtag_hicanns to talk to
	bool on_reticle =
		false;       ///< determines wether running on wafer reticle (affects dnc addressing)
	uint reticle_nr; ///< reticle nr according to dan/maurice's coordinate system
	uint dnc_hc_nr;
	uint mybulksize = 63; ///< default bulk size
	uint dnc_chnl[8] = {0, 1, 2, 3,
						4, 5, 6, 7}; ///< assign dnc channels to logically constructed HICANNs
	FPGAConnectionId::IPv4::bytes_type fpga_ip = {
		{192, 168, 2, 1}}; ///< define default FPGA IP address
	uint jtag_port = 1700; ///< define default port for JTAG-over-Ethernet communication
	printf(
		"Default FPGA IP address: %d.%d.%d.%d\n", fpga_ip[0], fpga_ip[1], fpga_ip[2], fpga_ip[3]);
	bool kill_on_recvfromerror = false;
	bool k7_setup = false;
	string shm_name;
	bitset<8> active_hicanns;

	int j = 1;
	for (; j <= argc - 1; j++) {
		//		if((actopt==none) && !strcmp(argv[j],"-v")){dbg.setLevel(Debug::verbose);continue;}
		//		if((actopt==none) && !strcmp(argv[j],"-d")){dbg.setLevel(Debug::debug);continue;}
		if ((actopt == none) && !strcmp(argv[j], "-bjw")) {
			commodel = jtag_wafer;
			actopt = none;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-bjw2f")) {
			commodel = jtag_wafer_fpga;
			actopt = none;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-s")) {
			actopt = opttime;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-?")) {
			actopt = opthelp;
		}
		if ((actopt == none) && !strcmp(argv[j], "-h")) {
			actopt = opthelp;
		}
		if ((actopt == none) && !strcmp(argv[j], "-sr")) {
			actopt = sr;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-js")) {
			actopt = js;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-bj")) {
			commodel = jtag;
			actopt = jthcnum;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-bj2f")) {
			commodel = jtag_2fpga;
			actopt = jthcnum;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-bje2f")) {
			commodel = jtag_eth_fpga;
			actopt = jthcnum;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-bje2fa")) {
			commodel = jtag_eth_fpga_arq;
			actopt = jthcnum;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-bje")) {
			commodel = jtag_eth;
			actopt = jthcnum;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-bja")) {
			commodel = jtag_full;
			actopt = jthcnum;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-dryrun")) {
			commodel = dryrun;
			actopt = jthcnum;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-bjac")) {
			commodel = jtag_multi;
			actopt = jthcnum;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-m")) {
			actopt = optmode;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-c")) {
			actopt = config;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-log")) {
			actopt = logger;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-label")) {
			actopt = tlabel;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-dc")) {
			actopt = dcnr;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-kill")) {
			kill_on_recvfromerror = true;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-bulksize")) {
			actopt = bulksize;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-k")) {
			actopt = stdiokeys;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-ip")) {
			actopt = ipset;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-jp")) {
			actopt = jpset;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-k7")) {
			actopt = none;
			k7_setup = true;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-r")) {
			actopt = onret;
			on_reticle = true;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-shm_name")) {
			actopt = shared_memory_name;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-bitmask")) {
			actopt = hicann_bitmask;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "--")) {
			break;
		} // end of known options

		if (actopt == opttime) {
			sscanf(argv[j], "%ud", &starttime);
			actopt = none;
			continue;
		}
		if (actopt == optmode) {
			modes.push_back(string(argv[j]));
			actopt = none;
			continue;
		} // store testmode
		if (actopt == sr) {
			srand(atoi(argv[j]));
			randomseed = atoi(argv[j]);
			actopt = none;
			continue;
		}
		if (actopt == js) {
			sscanf(argv[j], "%ud", &jtag_speed);
			actopt = none;
			continue;
		}
		if (actopt == config) {
			confFileName = argv[j];
			actopt = none;
			continue;
		}
		if (actopt == logger) {
			sscanf(argv[j], "%ud", &log_level);
			actopt = none;
			continue;
		}
		if (actopt == tlabel) {
			label = argv[j];
			actopt = none;
			continue;
		}
		if (actopt == jthcnum) {
			sscanf(argv[j], "%ud", &num_jtag_hicanns);
			actopt = jthcaddr;
			continue;
		}
		if (actopt == jthcaddr) {
			sscanf(argv[j], "%ud", &tar_jtag_hicann);
			actopt = none;
			continue;
		}
		if (actopt == dcnr) {
			sscanf(argv[j], "%ud", &dnc_hc_nr);
			actopt = dcnl;
			continue;
		}
		if (actopt == dcnl) {
			sscanf(argv[j], "%ud", &dnc_chnl[dnc_hc_nr]);
			actopt = none;
			continue;
		}
		if (actopt == bulksize) {
			sscanf(argv[j], "%ud", &mybulksize);
			actopt = none;
			continue;
		}
		if (actopt == onret) {
			sscanf(argv[j], "%ud", &reticle_nr);
			actopt = none;
			continue;
		}
		if (actopt == shared_memory_name) {
			stringstream shm_string;
			shm_string << argv[j];
			shm_name = shm_string.str();
			actopt = none;
			continue;
		}
		if (actopt == hicann_bitmask) {
			stringstream shm_string;
			shm_string << argv[j];
			active_hicanns = bitset<8>(shm_string.str());
			actopt = none;
			continue;
		}
		if (actopt == stdiokeys) {
			keys.str(argv[j]);
			while (getline(keys, key, ','))
				keysequence.push_back(key);
			actopt = none;
			continue;
		}
		if (actopt == ipset) {
			sscanf(
				argv[j], "%hhd.%hhd.%hhd.%hhd", &fpga_ip[0], &fpga_ip[1], &fpga_ip[2], &fpga_ip[3]);
			if (shm_name.empty()) {
				stringstream shm_string;
				shm_string << argv[j];
				shm_name = shm_string.str();
			}
			printf(
				"Changing IP address to %d.%d.%d.%d\n", fpga_ip[0], fpga_ip[1], fpga_ip[2],
				fpga_ip[3]);
			actopt = none;
			continue;
		}
		if (actopt == jpset) {
			sscanf(argv[j], "%d", &jtag_port);
			printf("Changing JTAG port to %d\n", jtag_port);
			actopt = none;
			continue;
		}
		if ((actopt == none) && !strcmp(argv[j], "-l")) {
			Listee<Testmode>* t = Listee<Testmode>::first;
			cout << "Available Testmodes:" << endl;
			while (t != NULL) {
				cout << "\"" << t->name() << "\" \"" << t->comment() << "\"" << endl;
				t = t->getNext();
			}
			exit(EXIT_SUCCESS);
		}
		if (actopt == none)
			cout << "Illegal options." << endl;
		cout << usagestr << endl;
		if (actopt != opthelp)
			exit(EXIT_FAILURE);
		else
			exit(EXIT_SUCCESS);
	}
	if (actopt != none) {
		cout << "Missing argument (use -? for more information)." << endl;
		exit(EXIT_FAILURE);
	}

	// initialize logger
	Logger& log = Logger::instance("hicann-system.tests2", log_level, "");

	// stdout redirection (handling recvfrom 11 errors...)
	boost::scoped_ptr<HandleCommFreezeError> error;
	if (kill_on_recvfromerror)
		error.reset(new HandleCommFreezeError);

	// identifies connection
	FPGAConnectionId connid(fpga_ip);
	connid.set_jtag_port(jtag_port);

	// get lock before accessing hardware
	CommAccess access(connid);

	// generate config object
	stage2_conf* conf = new stage2_conf();
	conf->readFile(confFileName.c_str());

	// single pointer for all comm models neccessary due to two base classes which are
	// used in subsequent argument passing
	S2C_JtagPhys* jtag_p = NULL;
	S2C_JtagPhys2Fpga* jtag_p2f = NULL;
	S2C_JtagPhys2FpgaArq* jtag_p2fa = NULL; // FIXME: GEILOMAT

	// the base class. Pointers are assigned after generation of child classes
	Stage2Comm* comm = NULL; ///< generic Stage 2 communication interface

	myjtag_full* jtf = NULL;

	vector<Stage2Ctrl*>
		chips; ///< all Stage2Ctrl(FpgaCtrl, DncCtrl, HicannCtrl) instances in the system

	// create test objects
	switch (commodel) {
		case jtag: {
			if (active_hicanns.any())
				jtf = new myjtag_full(false, false, active_hicanns, tar_jtag_hicann);
			else
				jtf = new myjtag_full(false, false, num_jtag_hicanns, tar_jtag_hicann);
			jtag_p = new S2C_JtagPhys(access, jtf, on_reticle);
			comm = jtag_p;
			log(Logger::INFO) << "JTAG communication model selected" << flush;
			log(Logger::INFO) << "Using USB device: NOT YET SUPPORTED for JtagLibV2!" << flush;
			exit(EXIT_FAILURE);
		} break;

		case jtag_multi: {
			if (active_hicanns.any())
				jtf = new myjtag_full(true, false, active_hicanns, tar_jtag_hicann);
			else
				jtf = new myjtag_full(true, false, num_jtag_hicanns, tar_jtag_hicann);
			jtag_p = new S2C_JtagPhys(access, jtf, on_reticle);
			comm = jtag_p;
			log(Logger::INFO) << "JTAG cain communication model selected" << flush;
			log(Logger::INFO) << "Using USB device: NOT YET SUPPORTED for JtagLibV2!" << flush;
			exit(EXIT_FAILURE);
		} break;

		case jtag_full: {
			if (active_hicanns.any())
				jtf = new myjtag_full(true, !k7_setup, active_hicanns, tar_jtag_hicann, k7_setup);
			else
				jtf = new myjtag_full(true, !k7_setup, num_jtag_hicanns, tar_jtag_hicann, k7_setup);
			jtag_p = new S2C_JtagPhys(access, jtf, on_reticle);
			comm = jtag_p;
			log(Logger::INFO) << "JTAG cain communication model selected" << flush;
			log(Logger::INFO) << "Using USB device: NOT YET SUPPORTED for JtagLibV2!" << flush;
			exit(EXIT_FAILURE);
		} break;

		case jtag_2fpga: {
			if (active_hicanns.any())
				jtf = new myjtag_full(true, !k7_setup, active_hicanns, tar_jtag_hicann, k7_setup);
			else
				jtf = new myjtag_full(true, !k7_setup, num_jtag_hicanns, tar_jtag_hicann, k7_setup);
			jtag_p2f = new S2C_JtagPhys2Fpga(access, jtf, on_reticle, k7_setup);
			comm = jtag_p2f;
			log(Logger::INFO) << "JTAG chain communication model selected" << flush;
			log(Logger::INFO) << "Using USB device: NOT YET SUPPORTED for JtagLibV2!" << flush;
			exit(EXIT_FAILURE);
		} break;

		case jtag_eth: {
			if (active_hicanns.any())
				jtf = new myjtag_full(true, !k7_setup, active_hicanns, tar_jtag_hicann, k7_setup);
			else
				jtf = new myjtag_full(true, !k7_setup, num_jtag_hicanns, tar_jtag_hicann, k7_setup);
			jtag_p = new S2C_JtagPhys(access, jtf, on_reticle);
			comm = jtag_p;
			log(Logger::INFO) << "JTAG chain communication model selected" << flush;
			log(Logger::INFO) << "Using ETHERNET device..." << flush;
			log(Logger::INFO) << "Initialized application layer" << flush;
			if (!jtf->initJtag(jtag_lib_v2::JTAG_ETHERNET)) {
				log(Logger::ERROR) << "JTAG open failed!" << flush;
				exit(EXIT_FAILURE);
			}

			FPGAConnectionId::IPv4 tmp(fpga_ip);
#ifdef FPGA_BOARD_BS_K7
			std::shared_ptr<sctrltp::ARQStream<sctrltp::ParametersFcpBss1>> p_hostarq(new sctrltp::ARQStream<sctrltp::ParametersFcpBss1>(
				tmp.to_string(), "192.168.0.128", /*listen port*/ 1234, "192.168.0.1",
				/*target port*/ 1234)); // TODO: set correct host IP
#else
			std::shared_ptr<sctrltp::ARQStream<sctrltp::ParametersFcpBss1>> p_hostarq(new sctrltp::ARQStream<sctrltp::ParametersFcpBss1>(
				tmp.to_string(), "192.168.1.2", /*listen port*/ 1234, tmp.to_string().c_str(),
				/*target port*/ 1234));
#endif
			if (!jtf->initJtagV2(p_hostarq, /*speed*/ 10000)) {
				throw std::runtime_error("JTAG init failed!");
			}
		} break;

		case jtag_eth_fpga: {
			if (active_hicanns.any())
				jtf = new myjtag_full(true, !k7_setup, active_hicanns, tar_jtag_hicann, k7_setup);
			else
				jtf = new myjtag_full(true, !k7_setup, num_jtag_hicanns, tar_jtag_hicann, k7_setup);
			jtag_p2f = new S2C_JtagPhys2Fpga(
				access, jtf, on_reticle, k7_setup); // ECM: add connection parameters to comm class?
													// jtf is ugly parameter? check with andi
			comm = jtag_p2f;
			log(Logger::INFO) << "JTAG chain communication model selected" << flush;
			log(Logger::INFO) << "Using ETHERNET device..." << flush;
			log(Logger::INFO) << "Initialized application layer" << flush;
			if (!jtf->initJtag(jtag_lib_v2::JTAG_ETHERNET)) {
				log(Logger::ERROR) << "JTAG open failed!" << flush;
				exit(EXIT_FAILURE);
			}

			log(Logger::INFO) << "setting JTAG frequency to " << jtag_speed << "kHz" << flush;
			if (!jtf->initJtagV2(
					jtf->ip_number(fpga_ip[0], fpga_ip[1], fpga_ip[2], fpga_ip[3]), jtag_port,
					jtag_speed)) {
				log(Logger::ERROR) << "JTAG init failed!" << flush;
				exit(EXIT_FAILURE);
			}
		} break;

		case jtag_eth_fpga_arq: {
			unsigned int curr_ip = jtf->ip_number(fpga_ip[0], fpga_ip[1], fpga_ip[2], fpga_ip[3]);
			if (active_hicanns.any())
				jtf = new myjtag_full(true, !k7_setup, active_hicanns, tar_jtag_hicann, k7_setup);
			else
				jtf = new myjtag_full(true, !k7_setup, num_jtag_hicanns, tar_jtag_hicann, k7_setup);

			// create hostarq stream here
			FPGAConnectionId::IPv4 tmp((fpga_ip));
			std::shared_ptr<sctrltp::ARQStream<sctrltp::ParametersFcpBss1>> p_hostarq(new sctrltp::ARQStream<sctrltp::ParametersFcpBss1>(
				/* shm file name */ tmp.to_string(),
				/* unused on HW */ "192.168.1.2",
				/* unused on HW */ 1234,
				/* target ip */ tmp.to_string().c_str(),
				/* unused on HW */ 1234));

			jtag_p2fa = new S2C_JtagPhys2FpgaArq(
				connid, jtf, on_reticle, jtag_port - 1700, p_hostarq, k7_setup);
			jtag_p2fa->set_max_bulk(mybulksize);
			comm = jtag_p2fa;

			// reset fpga before doing something else
			Stage2Comm::set_fpga_reset(curr_ip, true, true, true, true, true);
			Stage2Comm::set_fpga_reset(curr_ip, false, false, false, false, false);

			log(Logger::INFO) << "JTAG chain communication model selected" << flush;
			log(Logger::INFO) << "Using ETHERNET device via ARQ..." << flush;
			log(Logger::INFO) << "Initialized application layer" << flush;
			if (!jtf->initJtag(jtag_lib_v2::JTAG_ETHERNET)) {
				log(Logger::ERROR) << "JTAG open failed!" << flush;
				delete jtag_p2fa;
				exit(EXIT_FAILURE);
			}
			log(Logger::INFO) << "setting JTAG frequency to " << jtag_speed << "kHz" << flush;
			// jtag port 0 indicates named socket to hostARQ
			if (!jtf->initJtagV2(p_hostarq, jtag_speed)) {
				log(Logger::ERROR) << "JTAG init failed!" << flush;
				delete jtag_p2fa;
				exit(EXIT_FAILURE);
			}
		} break;

		case jtag_wafer: { // testing communication, will be deleted and reinstated in
						   // ReticleControl later
			log(Logger::INFO) << "Wafer with direct JTAG communication model selected" << flush;
		} break;

		case jtag_wafer_fpga: { // testing communication, will be deleted and reinstated in
								// ReticleControl later
			log(Logger::INFO) << "Wafer with JTAG to FPGA only communication model selected"
							  << flush;
		} break;

		case dryrun: {
			log(Logger::INFO) << "Dryrun mode. Ignoring hardware calls " << flush;
		} break;

		default: {
			log(Logger::ERROR) << "Illegal / unimplemented communication model!" << flush;
			exit(EXIT_FAILURE);
		}
	}

	// relying on lazy evaluation, otherwise seggy here...
	if (commodel != dryrun && commodel != jtag_wafer && commodel != jtag_wafer_fpga)
		if (comm->getState() == Stage2Comm::openfailed) {
			log(Logger::ERROR) << "Open failed!" << flush;
			exit(EXIT_FAILURE);
		}

	log(Logger::INFO) << "Stage2Test started." << flush;
	log(Logger::INFO) << "Testing " << label << "." << flush;
	chips.clear();

	// close JTAG communication while on wafer to avoid collisions
	if (commodel != jtag_wafer && commodel != jtag_wafer_fpga) {
		// construct all FPGA objects
		for (int c = 0; c < FPGA_COUNT; c++) {
			FPGAControl* tmp = new FPGAControl(comm, c);
			chips.push_back(tmp);
		}

		// construct all DNC objects
		for (int c = 0; c < DNC_COUNT; c++) {
			DNCControl* tmp = new DNCControl(comm, c);
			chips.push_back(tmp);
		}

		// construct all HICANN objects
		for (uint c = 0; c < HICANN_COUNT; c++) {
			HicannCtrl* tmp = new HicannCtrl(comm, tar_jtag_hicann);
			// !!! correctly organize DNC and JTAG address !!!
			// -> using jtag address only, for now...
			// HicannCtrl* tmp = new HicannCtrl(comm,dnc_chnl[c]);
			chips.push_back(tmp);
		}
	} else {
		for (int c = 0; c < FPGA_COUNT; c++) {
			FPGAControl* tmp = new FPGAControl(NULL, 0);
			chips.push_back(tmp);
		}
		for (int c = 0; c < DNC_COUNT; c++) {
			DNCControl* tmp = new DNCControl(NULL, 0);
			chips.push_back(tmp);
		}
		for (uint c = 0; c < HICANN_COUNT; c++) {
			HicannCtrl* tmp = new HicannCtrl(NULL, 0);
			chips.push_back(tmp);
		}
	}

	bool result = false; // by default: fail
	// ***** start testmodes ****
	// testmodes are executed in main thread

	for (vector<string>::iterator mode = modes.begin(); mode != modes.end(); mode++) {
		Testmode* t = Listee<Testmode>::first->createNew(*mode); // try to create testmode
		if (t == NULL) {
			log(Logger::ERROR) << "Unknown testmode! Use -l for list." << flush;
			exit(EXIT_FAILURE);
		}
		// initialize testmode
		t->chip = chips;
		t->comm = comm;
		t->active_hicanns = active_hicanns;
		t->jtag = jtf;
		t->shm_name = shm_name;
		t->conf = conf;
		t->label = label;
		t->keys = keysequence;
		t->commodel = commodel;
		if (argc > j) {
			// parse rest of options
			bpo::parsed_options opts = bpo::command_line_parser(argc - j, argv + j)
			                               .options(bpo::options_description())
			                               .allow_unregistered()
			                               .run();
			t->argv_options = new bpo::parsed_options(opts);
		} else {
			t->argv_options = nullptr;
		}
		log(Logger::INFO) << "Starting testmode " << *mode << flush;
		result = t->test(); // perform tests
		log(Logger::INFO) << "Testmode: " << *mode << " result:" << result << flush;
		comm->Flush();
	}
	// ***** end test *****

	// close jtag interface
	if (jtf != NULL) {
		jtf->closeSession();
	}

	if (jtag_p2fa != NULL)
		delete jtag_p2fa;

	// Testmodes return true on success
	if (result)
		return (EXIT_SUCCESS);
	else
		return (EXIT_FAILURE);
}
