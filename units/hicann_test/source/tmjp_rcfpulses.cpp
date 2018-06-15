// testmode for pulse playback/trace

#include <time.h>

#include "common.h"
#include "hicann_ctrl.h"

#include "neuron_control.h" //neuron control class (merger, background generators)
#include "spl1_control.h"
#include "fpga_control.h"
#include "dnc_control.h"
#include "iboardv2_ctrl.h"


#include "s2comm.h"
#include "testmode.h" 

#include "jd_mappingmaster.h"
#include "hicann_cfg.h"
#include "jd_rcf_experiment.h"
#include "pulse_float.h"

#define USENR         0
#define fpga_master   1
#define DATA_TX_NR    10000

using namespace std;
using namespace facets;

class TmJPRCFPulses : public Testmode{

public:
	// RCF backend helpers
	RcfClient<I_Experiment> * client;


	// test function
	bool test() 
	{
		initRCF("localhost");

		int previous_timeout = client->getClientStub().getRemoteCallTimeoutMs();
		client->getClientStub().setRemoteCallTimeoutMs(1000*60); // extend timeout value to 60s to avoid timeout exception during initialization

		bool successful_init = false;
		for (unsigned int nrep=0;nrep<20;++nrep)
        {
        	if (client->init())
            {
            	successful_init = true;
                break;
            }
        }
		
    	if (!successful_init)
        {
        	printf("Initialization failed. Stop.\n");
        	return false;
    	}

		client->getClientStub().setRemoteCallTimeoutMs(previous_timeout);

		pcs_cfg_t pcs_config;
        //pcs_config.enable_dnc_loopback = true;
        pcs_config.enable_hicann_loopback = true;
        client->config_layer2(pcs_config);
    
	    std::map< l2_pulse_address_t, std::vector<double> > send_pulses;

		this->readPulses("pynn_spikes.txt", send_pulses);

		client->set_input_spikes(send_pulses);

		// run simulation
		bool sim_done = client->run(1.0);


	if (sim_done)
	{
		// test get_recorded spikes
	    std::map< l2_pulse_address_t, std::vector<double> > r_spiketrains;
		client->get_recorded_spikes(r_spiketrains);
		std::cout << "tmjp_rcfpulses: received recorded pulses from " << r_spiketrains.size() << " source addresses" << std::endl;
		/*
		std::map< l2_pulse_address_t, std::vector<double> >::iterator it_st= r_spiketrains.begin();
		std::map< l2_pulse_address_t, std::vector<double> >::iterator it_st_end = r_spiketrains.end();

		for (;it_st != it_st_end; ++it_st )
		{
			l2_pulse_address_t addr = it_st->first;
			std::cout << "Source: \t";
			std::cout << "wafer_id: " << addr.wafer_id << ", ";
			std::cout << "fpga_id: " << addr.fpga_id << ", ";
			std::cout << "dnc_id: " <<  addr.dnc_id << ", ";
			std::cout << "hicann_id: " << addr.hicann_id << ", ";
			std::cout << "address: " <<  addr.address << std::endl;
			std::cout << "Nr of Spikes: " << it_st->second.size() << std::endl;
		}
		*/
		this->writePulses("trace_spikes.txt",r_spiketrains);
	}
	else
    	printf("Simulation failed.\n");	


		return true;
	}


	void initRCF(std::string const host) {
		RCF::RcfInitDeinit rcfInit;
		client = new RcfClient<I_Experiment>(RCF::TcpEndpoint(host.c_str(), server_port));

		client->getClientStub().ping();
		std::cout << "Connected to RCF server" << std::endl;

		size_t const msg_sz = 100*1024*1024; // 100MB max msg size
		client->getClientStub().getTransport().setMaxMessageLength(msg_sz);

		client->getClientStub().requestTransportFilters(RCF::FilterPtr(new RCF::ZlibStatefulCompressionFilter()));
		//client->getClientStub().disableBatching();
		//client->getClientStub().setMaxBatchMessageLength(0);

		// create session object/job handler on the server
		client->getClientStub().createRemoteSessionObject("Experiment");


		// run client->FUNCTION_NAME(PARAMETERS) here
		//client->run(cs);
		return;
	}

	unsigned int readPulses(const char *filename, std::map< l2_pulse_address_t, std::vector<double> > & spiketrains)
	{
		FILE *fin = fopen(filename, "r");
		spiketrains.clear();
      
		char curr_line[80];
		while ( fgets(curr_line, 80, fin) )
		{
			if (curr_line[0] == '#') // comment line
				continue;
			
            double curr_id = 0;
			double curr_time = 0.0;

			if ( sscanf(curr_line,"%lf %lf",&curr_time,&curr_id) != 2)
			{
				printf("readPulses warning: Ignoring undecodable line '%s'\n", curr_line);
				continue;
			}

			l2_pulse_address_t curr_addr(curr_id);
			spiketrains[curr_addr].push_back( curr_time );
		}
		fclose(fin);
      
		return spiketrains.size();
	}

	void writePulses(
			const char *filename,
			std::map< l2_pulse_address_t, std::vector<double> > & spiketrains
			)
	{
		// convert spiketrains to single list, sorted by time
		std::vector<pulse_float_t> pulselist;

		std::map< l2_pulse_address_t, std::vector<double> >::iterator it_st= spiketrains.begin();
		std::map< l2_pulse_address_t, std::vector<double> >::iterator it_st_end = spiketrains.end();

		for (;it_st != it_st_end; ++it_st )
		{
			l2_pulse_address_t curr_addr = it_st->first;
			std::vector<double> & times = it_st->second;

			// ignore wafer and FPGA ids for now
			//unsigned int curr_id = ((curr_addr.dnc_id&0x3)<<12) | ((curr_addr.hicann_id&0x7)<<9) | (curr_addr.address&0x1ff);
			unsigned int curr_id = curr_addr.data.integer & 0x3fff;

			for (unsigned int npulse=0;npulse<times.size();++npulse)
				pulselist.push_back(pulse_float_t(curr_id,times[npulse]));
		}

		std::sort(pulselist.begin(),pulselist.end());

		FILE *fout = fopen(filename, "w");
        
        for (unsigned int npulse=0;npulse<pulselist.size();++npulse)
        	fprintf(fout,"%.2f\t%d\n",pulselist[npulse].time, pulselist[npulse].id);

		fclose(fout);
    }
};

class LteeTmJPRCFPulses : public Listee<Testmode>{
public:
  LteeTmJPRCFPulses() : Listee<Testmode>(string("tmjp_rcfpulses"),string("playback/trace pulses via jd_mappingmaster from/to file")){};
  Testmode * create(){return new TmJPRCFPulses();};
};

LteeTmJPRCFPulses ListeeTmJPRCFPulses;

