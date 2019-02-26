// Company         :   kip                      
// Author          :   Andreas Gruebl            
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//                    			
// Filename        :   s2comm.cpp                
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   Fri Jun 20 08:12:24 2008                
// Last Change     :   Mon Aug 04 08    
// by              :   agruebl        
//------------------------------------------------------------------------


#include "common.h" // library includes

#ifndef NCSIM
extern "C" {
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>
#include <utime.h>
}
#endif

#include "s2comm.h"

#ifndef HICANN_DESIGN
	#include "ethernet_software_if.h"
#endif

using namespace std;
using namespace facets;

// *** class Stage2Comm ***

// ECM: Static const members have to be defined in EXACTLY one (1) compilation unit!
const IData Stage2Comm::emptydata;
const SBData Stage2Comm::emptysbdata;
const int Stage2Comm::basedelay;
const int Stage2Comm::synramdelay;
const int Stage2Comm::l1ctrldelay;
const int Stage2Comm::cidelay;


#ifndef NCSIM
static char default_reset_lockfile[] = "/var/lock/hicann/wafer_inuse";
static char * reset_lockfile = NULL;
static bool const reset_lock_just_once = false;
#endif

Stage2Comm::Stage2Comm(CommAccess const & access, myjtag_full* j, bool on_reticle, bool use_k7fpga) :
	link_layers(8),
	rxdata(8, std::vector< queue<ci_payload> >(2)),
	state(ok),
	access(access),
	reset_fd(-1), // negative == error
	k7fpga(use_k7fpga),
	log(Logger::instance("hicann-system.Stage2Comm", Logger::INFO,"")),
	running_on_reticle(on_reticle),
	jtag(j)
{ // init_s2comm MUST be called by child class constructor in correct init order
}


Stage2Comm::~Stage2Comm(){
#ifndef NCSIM
	#pragma omp critical(LockFile)
	{
	if (reset_lockfile != default_reset_lockfile) {
		delete[] reset_lockfile;
		reset_lockfile = NULL;
	}
	if (reset_fd > -1) {
		close(reset_fd);
		reset_fd = -1;
	}
	}
#endif
}


void Stage2Comm::init_s2comm(void) {
#ifndef NCSIM
	#pragma omp critical(LockFile)
	{
	if (!(reset_lock_just_once && reset_lockfile)) {
		mode_t old_umask = umask(0); // disable umask
		if (access.getPMUIP().to_ulong() && (access.getPMUIP().to_ulong() !=
				FPGAConnectionId::IPv4::from_string("129.206.176.59").to_ulong()))
		{
			// new wafer here
			std::string reset_lockfile_str(default_reset_lockfile);
			reset_lockfile_str += "-";
			reset_lockfile_str += access.getPMUIP().to_string();
			reset_lockfile = new char[reset_lockfile_str.size() + 1];
			std::copy(reset_lockfile_str.begin(), reset_lockfile_str.end(), reset_lockfile);
			reset_lockfile[reset_lockfile_str.size()] = '\0';
		} else {
			// use old lockfile on wafer #0 only
			reset_lockfile = default_reset_lockfile;
		}
		reset_fd = open(reset_lockfile, O_RDWR | O_CREAT, 0666);
		umask(old_umask); // set to previous umask
		if (!reset_fd) {
			log(Logger::ERROR) << "FIXME: handle permission problems here... nicer error messages, etc. (report to Eric if that happens)";
			abort();
		}
		int rc = 0;
		do {
			rc = flock(reset_fd, LOCK_SH | LOCK_NB);
			if (rc && errno == EWOULDBLOCK) {
				log(Logger::INFO) << "waiting for hardware (still in reset state)";
				sleep(1);
				continue;
			} else if (rc) {
				// ECM: do not use logger here => the extra buffering would drop it
				std::cerr << "strange global reset locking error (report to Eric): " << strerror(errno) << std::endl;
				abort();
			}
		} while(false);
	}
	}
#endif

	for(uint hc=0; hc<8; hc++){
		readdata[hc] = IData();
		readdata[hc].setAddr() = hc;
		lastread[hc] = ci_packet_t();
		ciptmp[hc]   = ci_payload();
		valid[hc]    = false;
	}

	// initialize LinkLayers
	// !!!hard-coded number of 8 HICANNs per comm class and 2 tags per HICANN !!!
	for(uint hc=0; hc<8; hc++){
		for(uint tag=0; tag<2; tag++) {
			link_layers[hc].push_back(LinkLayer<ci_payload,ARQPacketStage2>(hc, tag));
		}
		for(uint tag=0; tag<2; tag++) {
			link_layers[hc][tag].Bind(this, NULL);
			link_layers[hc][tag].setConcLl(&link_layers[hc][(tag+1)%2]);
			//link_layers[hc][tag].SetDebugLevel(10);
		}
	}
	//create dnc2jtag LUT
	uint jtag_nr = 0;
	// descending for loop because of inverted numbering
	for(int dnc_nr = 7; dnc_nr > -1; dnc_nr--) {
		if (jtag->physical_available_hicanns[dnc_nr]) {
			//hicann is available update map and increase jtagchain
			dnc2jtag_lut[dnc_nr] = jtag_nr;
			//TODO: not two seperate LUTs
			jtag2dnc_lut[jtag_nr] = dnc_nr;
			jtag_nr++;
		}
	}
}


int Stage2Comm::issueCommand(uint hicann_nr, uint tagid, ci_payload *data, uint /*del*/){
	if (link_layers[hicann_nr][tagid].Send(data) != 1) {
		auto const fpga_ip = boost::asio::ip::address_v4(jtag->get_ip());
		throw std::runtime_error(
		    "link layer (software arq) failed to transmit data for HICANN " +
		    std::to_string(hicann_nr) + " on tag " + std::to_string(tagid) +
		    " on FPGA with IP: " + fpga_ip.to_string());
	}
	// store potential read data locally to hide the atomic JTAG accesses to the user...
	if(link_layers[hicann_nr][tagid].HasReadData()) {
		ci_payload rd;
		ci_payload *rdptr = &rd;
		link_layers[hicann_nr][tagid].Read(&rdptr);
		rxdata[hicann_nr][tagid].push(rd);
	}
	return 1;
}


int Stage2Comm::recvData(uint hicann_nr, uint tagid, ci_payload *data){
	if(!rxdata[hicann_nr][tagid].empty()){
		*data = rxdata[hicann_nr][tagid].front();
		rxdata[hicann_nr][tagid].pop();
		return 1;
	} else {
		std::stringstream message;
		message << "Stage2Comm::recvData(hicann " << hicann_nr << ", tag " << tagid << "): receive queue empty!";
		throw std::runtime_error(message.str().c_str());
	}
}


Stage2Comm::Commstate Stage2Comm::Send(IData /*data*/, uint32_t /*del*/)
{
	return notimplemented;
}

Stage2Comm::Commstate Stage2Comm::Clear(void){
	for(uint hc=0; hc<jtag->num_hicanns; hc++){
		Receive(readdata[hc]);
		lastread[hc] = readdata[hc].cmd();
		valid[hc]=false;
	}
	return ok;
} 

Stage2Comm::Commstate Stage2Comm::Flush()
{
	// finish all pending software arq acks
	for(uint hc = 0; hc < jtag->num_hicanns; hc++) {
		for(uint tag = 0; tag < 2; tag++) {
			link_layers[hc][tag].Exec();
		}
	}
	return ok;
}

size_t Stage2Comm::dnc2jtag(int const dnc_channel_nr) {

	assert (dnc2jtag_lut.count(dnc_channel_nr) > 0);
	return dnc2jtag_lut[dnc_channel_nr];
}

size_t Stage2Comm::jtag2dnc(int const jtag_chain_nr) {
	assert (jtag2dnc_lut.count(jtag_chain_nr) > 0);
	return jtag2dnc_lut[jtag_chain_nr];
}

void Stage2Comm::reprog_fpga(unsigned int fpga_ip, uint reset_port) {

#ifndef HICANN_DESIGN
	#pragma omp critical(EthIF)
	{
	EthernetSoftwareIF eth_if;
	eth_if.init(reset_port);

	uint32_t pck = 0xDEADE11D;

	pck = htonl(pck);
	eth_if.sendUDP(fpga_ip, reset_port, &pck, 4);

	eth_if.end();
	}
#else
	Logger& log = Logger::instance();
	log(Logger::WARNING) << "Stage2Comm::reprog_fpga - not implemented in HICANN testbench!";
#endif
}


// set soft reset signal in FPGA via Ethernet packet to port 1801
void Stage2Comm::set_fpga_reset(unsigned int fpga_ip, bool enable_core, bool enable_fpgadnc, bool enable_ddr2onboard, bool enable_ddr2sodimm,
                                           bool enable_arq, uint reset_port) {
	Logger& log = Logger::instance();

#ifndef HICANN_DESIGN
	#pragma omp critical(EthIF)
	{
	EthernetSoftwareIF eth_if;
	eth_if.init(reset_port);
	
	uint32_t pck = 0x55000000;
	if (enable_core) pck |= 0x1;
	if (enable_fpgadnc) pck |= 0x2;
	if (enable_ddr2onboard) pck |= 0x4;
	if (enable_ddr2sodimm) pck |= 0x8;
	if (enable_arq) pck |= 0x10;

	pck = htonl(pck);
	eth_if.sendUDP(fpga_ip, reset_port, &pck, 4);

	log(Logger::DEBUG0) << "FPGA soft reset packet: 0x" << hex << pck << ", IP: 0x" << fpga_ip << ", Port: 0x" << reset_port;

	eth_if.end();
	}
#else
	log(Logger::WARNING) << "Stage2Comm::set_fpga_reset - not implemented in HICANN testbench!";
#endif
}

Stage2Comm::Commstate Stage2Comm::reset_hicann_arq(std::bitset<8> hicann, bool release_reset)
{
	// pull HICANN ARQ reset in FPGA
	set_fpga_reset(jtag->get_ip(), false, false, false, false, true);

	// Reset software ARQ
	for (uint i = 0; i < link_layers.size(); i++) {
		for (uint j = 0; j < link_layers[i].size(); j++) {
			link_layers[i][j].arq.Reset();
		}
	}

	// pull HICANN ARQ reset in HICANNs
	for (size_t hicann_nr = 0; hicann_nr < hicann.size(); hicann_nr++) {
		if (!hicann[hicann_nr]) {
			continue;
		}

		jtag->set_hicann_pos(dnc2jtag(hicann_nr));

		jtag->HICANN_arq_write_ctrl(jtag->CMD3_ARQ_CTRL_RESET, jtag->CMD3_ARQ_CTRL_RESET);
		if (release_reset) {
			jtag->HICANN_arq_write_ctrl(0x0, 0x0);
		}
	}

	if (release_reset) {
		// release HICANN ARQ reset in FPGA
		set_fpga_reset(jtag->get_ip(), false, false, false, false, false);
	}

	return Stage2Comm::ok;
}

// FIXME: reads iboard adc... wrong position!!!
Stage2Comm::Commstate Stage2Comm::readAdc(SBData& result) {
	double vref   = 3.3;  // adc reference voltage
	uint adcres   = 10;   // adc resolution
	uint adc1addr = 0x48, adc2addr = 0x46; // addr already shifted <<1; rw bit added below
	
	uint adcaddr, adccmd, adcval;
	double value;

	SBData::ADtype t=result.getADtype();
	switch(t){
		case SBData::ivdd:        {adcaddr = adc2addr; adccmd = 0x90; break;}
		case SBData::ivdda:       {adcaddr = adc1addr; adccmd = 0xf0; break;}
		case SBData::ivdd_dncif:  {adcaddr = adc1addr; adccmd = 0xb0; break;}
		case SBData::ivdda_dncif: {adcaddr = adc2addr; adccmd = 0xa0; break;}
		case SBData::ivdd33:      {adcaddr = adc2addr; adccmd = 0x80; break;}
		case SBData::ivol:        {adcaddr = adc1addr; adccmd = 0xa0; break;}
		case SBData::ivoh:        {adcaddr = adc1addr; adccmd = 0x90; break;}
		case SBData::ivddbus:     {adcaddr = adc1addr; adccmd = 0x80; break;}
		case SBData::ivdd25:      {adcaddr = adc1addr; adccmd = 0xd0; break;}
		case SBData::ivdd5:       {adcaddr = adc1addr; adccmd = 0xc0; break;}
		case SBData::ivdd11:      {adcaddr = adc1addr; adccmd = 0xe0; break;}
		default: {
			log(Logger::ERROR) << "Wrong ADC read type specified!";
			return parametererror;
		}
	}	

	unsigned char adcvall = 0, adcvalm = 0;
				
	// enable I2C master, just to be sure... ;-)
	jtag->FPGA_i2c_setctrl((unsigned char)jtag->ADDR_CTRL, (unsigned char)0x80);

	// send conversion command
	jtag->FPGA_i2c_byte_write(adcaddr, 1, 0, 0);
	jtag->FPGA_i2c_byte_write(adccmd,  0, 1, 0);

	// add read bit and re-send I2C address
	adcaddr |= 0x1;
	jtag->FPGA_i2c_byte_write(adcaddr, 1, 0, 0);

	// read conversion result
	jtag->FPGA_i2c_byte_read(0, 0, 0, adcvalm);
	jtag->FPGA_i2c_byte_read(0, 1, 1, adcvall);

	// perform sanity checks:
	if(adcvalm & 0x8) log(Logger::WARNING) << "ADC Alert Flag set!";
	if((adcvalm & 0x70) != (adccmd & 0x70)){
		log(Logger::ERROR) << "Wrong ADC answer channel!";
		return readfailed;
	}

	adcval = (static_cast<uint>(adcvalm)<<8) | static_cast<uint>(adcvall);
	adcval >>= 2;     // 10 Bit ADC -> D0 and D1 not used
	adcval &=  0x3ff; // 

	value = (double)adcval / (double)((1<<adcres)-1) * vref;
	result.setADvalue(value);

	log(Logger::DEBUG1) << "ADC read " << value << "V";

	return ok;	
}

Stage2Comm::Commstate Stage2Comm::setDac(SBData value) {
	double vrefdac = 2.5;
	uint dacres = 16;
	uint dac0addr = 0x98, dac1addr = 0x9a;

	uint dacaddr, daccmd;
	
	// sanity check:
	if(value.ADvalue() > vrefdac) {
		log(Logger::ERROR) << "DAC input value larger than vrefdac!";
		return parametererror;
	}
	// !!! is this correctly rounded ???
	uint dacval = value.ADvalue() / vrefdac * (double)((1<<dacres)-1);

	SBData::ADtype t=value.getADtype();
	switch(t){
		case SBData::vol:      {dacaddr = dac1addr; daccmd = 0x14; break;}
		case SBData::voh:      {dacaddr = dac1addr; daccmd = 0x10; break;}
		case SBData::vddbus:   {dacaddr = dac1addr; daccmd = 0x12; break;}
		case SBData::vdd25:    {dacaddr = dac0addr; daccmd = 0x10; break;}
		case SBData::vdd5:     {dacaddr = dac0addr; daccmd = 0x12; break;}
		case SBData::vdd11:    {dacaddr = dac0addr; daccmd = 0x14; break;}
		case SBData::vref_dac: {dacaddr = dac0addr; daccmd = 0x16; break;}
		default: {
			log(Logger::ERROR) << "Wrong DAC write type specified!";
			return parametererror;
		}
	}	

	// enable I2C master, just to be sure... ;-)
	jtag->FPGA_i2c_setctrl((unsigned char)jtag->ADDR_CTRL, (unsigned char)0x80);

	jtag->FPGA_i2c_byte_write(dacaddr, 1, 0, 0);
	jtag->FPGA_i2c_byte_write(daccmd,  0, 0, 0);
	jtag->FPGA_i2c_byte_write((dacval>>8), 0, 0, 0);
	jtag->FPGA_i2c_byte_write( dacval,     0, 1, 0);

	return ok;
}


bool Stage2Comm::is_k7fpga()
{
	return k7fpga;
}

// *** END class Stage2Comm END ***
