// Company         :   kip
// Author          :   Alexander Kononov
// E-Mail          :   kononov@kip.uni-heidelberg.de
//                    			
// Filename        :   tmak_fg.cpp
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   11/2010
// Last Change     :   06/2011
//------------------------------------------------------------------------

#include "s2comm.h"
#include "s2ctrl.h"

#ifdef NCSIM
#include "systemc.h"
#endif

#include "config.h"
#include "common.h"
#include "hicann_ctrl.h"
#include "fg_control.h"
#include "neuronbuilder_control.h"
#include "linklayer.h"
#include "testmode.h" 
#include "iboardv2_ctrl.h"
#include <time.h>

//functions defined in getch.c, keyboard input
extern "C" int getch(void);
extern "C" int kbhit(void);

using namespace std;
using namespace facets;

class TmAkFg : public Testmode{

public:

	HicannCtrl *hc; 
	LinkLayer<ci_payload,ARQPacketStage2> *ll;
	FGControl *fc[4]; //array of the four floating gate controlers
	NeuronBuilderControl *nbc;

	stringstream ss;
	string filename;
	fstream file;
	stringstream ss1;
	string filename1;
	fstream file1;

	//start values
	uint maxcycle;		
	uint currentwritetime;
	uint voltagewritetime;
	uint readtime;
	uint acceleratorstep;
	uint pulselength;
	uint fg_biasn;
	uint fg_bias;	

	static const int TAG = 0;

	void setupFg(){
		for(int fg_num=0; fg_num<4; fg_num++){
			log(Logger::DEBUG1)<< "Writing controller " << fg_num << "." << endl;
			fc[fg_num]->set_maxcycle(maxcycle);		
			fc[fg_num]->set_currentwritetime(currentwritetime);
			fc[fg_num]->set_voltagewritetime(voltagewritetime);
			fc[fg_num]->set_readtime(readtime);
			fc[fg_num]->set_acceleratorstep(acceleratorstep);
			fc[fg_num]->set_pulselength(pulselength);
			fc[fg_num]->set_fg_biasn(fg_biasn);
			fc[fg_num]->set_fg_bias(fg_bias);
			fc[fg_num]->write_biasingreg();
			fc[fg_num]->write_operationreg();
		}
	}

	void displaySetup(){
		cout<<"Current Setup is:"<<endl;
		cout<<"Maxcycle = "<< maxcycle<<endl;
		cout<<"Currentwritetime = "<< currentwritetime << endl;
		cout<<"Voltagewritetime = "<< voltagewritetime << endl;
		cout<<"Readtime = "<< readtime << endl;
		cout<<"Acceleratorstep = "<< acceleratorstep << endl;
		cout<<"Pulselength = "<< pulselength << endl;
		cout<<"Fg_Biasn = " << fg_biasn << endl;
		cout<<"Fg_Bias = " << fg_bias << endl;
	}

	uint revert10bits(uint val){
		uint result=0;
		for(int i=0; i<10; i++) result |= (val & (1<<i))?(1<<(9-i)):0;
		return result;
	}
	
	double meanvec(std::vector<double> values){
		double temp=0;
		for (int i=0; i<values.size(); i++) temp+=values[i];
		return (temp/values.size());
	}
	
	double stdvec(std::vector<double> values){
		double mean=meanvec(values);
		double temp=0;
		for (int h=0; h<values.size(); h++) temp+=pow(mean-values[h],2);
		temp/=(values.size()-1);
		return sqrt(temp);
	}
	
	std::vector<int> generate_rnd(){
		std::vector<int> vec(1024);
		for (int d=0; d<1024; d++) vec[d]=d;
		std::random_shuffle(vec.begin(),vec.end());
		return vec;
	}

	void randomizeVector(std::vector<int>& rndvalues, std::vector<int>& tempvalues){
		cout << "Random vector is:" << endl;
		rndvalues.clear();
		for (int b=0; b<129; b++){
			if (tempvalues.empty()) tempvalues = generate_rnd();
			rndvalues.push_back(tempvalues.back());
			tempvalues.pop_back();
			#if HICANN_V == 1
			if (b==128) rndvalues[128]=rndvalues[0];
			#endif
			if (b!=0) cout << rndvalues[b] << "\t" << flush;
		}
		cout << endl;
	}

	void setProperMux(uint fgn, SBData::ADtype& var){
		nbc->setOutputEn(true,true);
		if (fgn==0 || fgn==1)var=SBData::aout0;
		else if (fgn==2 || fgn==3) var=SBData::aout1;
		else log(Logger::ERROR)<<"ERROR: setProperMux: Illegal controler specified!" << endl;
		nbc->setOutputMux(3+(fgn%2)*6,3+(fgn%2)*6);
	}
	
	void programFgLine(uint fgn, uint line, vector<int> values){
		if(values.size()!=129) log(Logger::ERROR)<<"ERROR: program_FG_line: Wrong vector length!" << endl;
		fc[fgn]->initArray(0, values);
		while(fc[fgn]->isBusy());
		fc[fgn]->program_cells(line, 0, 0);
		while(fc[fgn]->isBusy());
		fc[fgn]->program_cells(line, 0, 1);
		while(fc[fgn]->isBusy());
	}
	
	void write_files(std::vector < vector<double> > sums, string namefinal, string nameraw){
		double temp;
		std::vector<double> mean(1024), stdev(1024); // means of sums columns and standard deviations
		ss.str("");
		ss <<"../results/tmak_fg/" << namefinal << ".dat";
		filename = ss.str();
		file.open(filename.c_str(), fstream::out | fstream::app);
		
		for (int p=0; p<1024; p++){
			mean[p]=meanvec(sums[p]);
			if (sums[p].size()>1) stdev[p]=stdvec(sums[p]);
			file << fixed << setprecision(4) << mean[p] << " " << setprecision(5) << stdev[p] << "\n" << flush;
		}
		file << endl;
		file.flush();
		file.close();
		
		ss1.str("");
		ss1 <<"../results/tmak_fg/" << nameraw << ".dat";
		filename1 = ss1.str();
		file1.open(filename1.c_str(), fstream::out | fstream::app);
		
		for (int p=0; p<1024; p++){
			for (int g=0; g<sums[p].size(); g++) file1 << fixed << setprecision(4) << sums[p][g] << " " << flush;
			file1 <<  "\n" << flush;
		}
		file1 << endl;
		file1.flush();
		file1.close();
	}
	
	void readoutFgRam(uint fgn, uint bank){
		ci_addr_t addr;
		ci_data_t data;
		uint data0, data1;
		cout << endl << "read back from RAM:" << endl;
		for (int n=0; n<65; n++){
			fc[fgn]->read_ram(bank, n);
			addr=0;
			data=0;
			fc[fgn]->get_read_data(addr, data);
			data0=(data & 0x3ff);
			data1=(data >> 10);
			if (n!=0) cout << data0 << "\t" << flush;
			if (n!=64) cout << data1 << "\t" << flush;
			}
	}

	// -----------------------------------------------------	
	// test function
	// -----------------------------------------------------	

	bool test() 
    {
		if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
		 	log(Logger::ERROR)<<"ERROR: object 'chip' in testmode not set, abort" << endl;
			return 0;
		}
		
        // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
		hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];
		nbc = &hc -> getNBC();
		fc[0]=&hc->getFC_tl();
		fc[1]=&hc->getFC_tr();
		fc[2]=&hc->getFC_bl();
		fc[3]=&hc->getFC_br();
		IBoardV2Ctrl board(conf,jtag,0); // board instance

	 	log(Logger::INFO) << "Try Init() ..." ;

		if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
		 	log(Logger::ERROR) << "ERROR: Init failed, abort" << endl;
			return 0;
		}

	 	log(Logger::INFO) << "Init() ok" << endl;
		debug_level = 4;

		//start values
		#if HICANN_V==1
		maxcycle = 255;
		currentwritetime = 10;
		voltagewritetime = 18;
		readtime = 63;
		acceleratorstep = 20;
		pulselength = 8;
		fg_biasn = 4;
		fg_bias = 4;
		#elif HICANN_V>=2
		maxcycle = 255;
		currentwritetime = 1;
		voltagewritetime = 15;
		readtime = 63;
		acceleratorstep = 9;
		pulselength = 9;
		fg_biasn = 5;
		fg_bias = 8;
		#endif

		log(Logger::INFO)<< "starting...." << endl;
		setupFg();

		char choice;
		bool cont = 1;
		do{
			displaySetup();	

			cout<<"p: Change the floating gate parameters"<<endl;
			cout<<"t: Crosstalk measurement with randomized programming"<<endl;
			cout<<"e: Crosstalk after sequent line programming"<<endl;
			cout<<"f: Crosstalk test for an FG line, multiple writings"<<endl;
			cout<<"o: Measurement error acquirement"<<endl;
			cout<<"k: Run deviation test with constant values"<<endl;
			cout<<"l: Run deviation test for an FG line, no sweeping"<<endl;
			cout<<"v: Run deviation test for an FG line, sweep one parameter"<<endl;
			cout<<"b: Run deviation test for an FG line, sweep two parameters"<<endl;
			cout<<"s: Run stress-test for an FG line"<<endl;
			cout<<"i: Run voltage drift over time test"<<endl;
			cout<<"g: Test differential writing"<<endl;
			cout<<"d: Test lower voltage boundary using error bit"<<endl;
			cout<<"z: Program all Floatinggates down to zero"<<endl;
			cout<<"m: Program all Floating to maximum"<<endl;
			cout<<"c: Program xml-file on floating gates"<<endl;
			cout<<"w: Write an FG line"<<endl;
			cout<<"r: Read out an FG line"<<endl;
			cout<<"R: Read out an FG cell and write it to HDD"<<endl;
			cout<<"x: End program"<<endl;
			cin>>choice;

			switch(choice){

			case 'p':{ //set FG parameters
				cout << "Enter maxcycle: 0 - 255" << endl;
				cin >> maxcycle;
				cout << "Enter currentwritetime: 0 - 63" << endl;
				cin >> currentwritetime;
				cout << "Enter voltagewritetime: 0 - 63" << endl;
				cin >> voltagewritetime;
				cout << "Enter readtime: 0 - 63" << endl;
				cin >> readtime;
				cout << "Enter acceleratorstep: 0 - 63" << endl;
				cin >> acceleratorstep;
				cout << "Enter pulselength: 0 - 15" << endl;
				cin >> pulselength;
				cout << "Enter biasn: 0 - 15" << endl;
				cin >> fg_biasn;
				cout << "Enter bias: 0 - 15" << endl;
				cin >> fg_bias;
				setupFg();
				break;
			} 

			case 't':{ //randomized crosstalk measurement
				int fgn, line;
				double temp;
				std::vector<double> ctrl(128), result(128);
				SBData::ADtype type;
				std::vector<int> rndvalues, tempvalues, values(129,0);
				int times_to_write=1;

				cout << "which controller? 0 - 3" << endl;
				cin >> fgn;
				cout << "which line? 0 - 23" << endl;
				cin >> line;
				
				board.setBothMux();
				setProperMux(fgn, type);
				
				for (int z=0; z < 1024; z++){
					cout << "Programming the subject line to " << z << endl;

					for (int b=0; b<129; b++) values[b]=z;
					for (int i=0; i<times_to_write; i++) programFgLine(fgn, line, values);

					cout << endl << "Control read from FGs:" << endl;
					for(int col=1; col<129; col+=1){
						fc[fgn]->readout_cell(line, col);
						temp=board.getVoltage(type);
						ctrl[col-1]=temp;
						cout << fixed << setprecision(3) << temp << "\t" << flush;
					}
					cout << endl;
					
					cout << "Programming neighbour lines at random..." << endl;
					for (int k=0; k < 24; k++){
						if (k!=line){
							randomizeVector(rndvalues, tempvalues);
							for (int w=0; w < times_to_write; w++) programFgLine(fgn, line, rndvalues);
						}
					}

					cout << endl << "Final read from FGs:" << endl;
					for(int col=1; col<129; col+=1){
						fc[fgn]->readout_cell(line, col);
						temp=board.getVoltage(type);
						result[col-1]=temp;
						cout << fixed << setprecision(3) << temp << "\t" << flush;
					}
					cout << endl;
					
					double mean = meanvec(ctrl);
					double stdev = stdvec(ctrl);
					double mean0 = meanvec(result);
					double stdev0 = stdvec(result);
									
					cout << endl <<"Mean at the beginning: " << mean << endl;
					cout << "Mean at the end : " << mean0 << endl;
					cout << "Mean difference : " << fixed << setprecision(4) << mean0-mean << endl;

					cout << "Standard deviation at the beginning : " << fixed << setprecision(4) << stdev << endl;
					cout << "Standard deviation at the end : " << fixed << setprecision(4) << stdev0 << endl;
					cout << "Standard deviation difference : " << fixed << setprecision(4) << stdev-stdev0 << endl << fixed << setprecision(3) << endl;
					
					ss.str("");
					ss <<"../results/tmak_fg/crosstalk_test.dat";
					filename = ss.str();
					file.open(filename.c_str(), fstream::app | fstream::out);
					
					file << fixed << setprecision(4) << mean << " " << setprecision(4) << mean0 << " " 
								<< setprecision(5) << stdev << " " << setprecision(5) << stdev0 << "\n" << flush;
					file.flush();
					file.close();
				}
				break;	
			}

			case 'e':{ //crosstalk dependence on sequent neighbor line programming
				bool dir;
				int fgn, line, initial_val, minchange, maxchange, startvalue_neighb, step, change;
				double temp, mean, mean0, stdev, stdev0;
				std::vector<double> ctrl(128);
				std::vector<double> result(128);
				SBData::ADtype type;
				std::vector<int> values(129);
				int times_to_write=1;
				
				cout << "Which controller? 0 - 3" << endl;
				cin >> fgn;
				cout << "Which line? 0 - 23" << endl;
				cin >> line;
				cout << "Choose direction: 0 - up, 1 - down:" << endl;
				cin >> dir; //default 0
				cout << "Choose minimum change:" << endl;
				cin >> minchange; //default 0
				cout << "Choose maximum change: (max. 900)" << endl;
				cin >> maxchange; //default 800
				cout << "Choose step:" << endl;
				cin >> step; //default 5
				
				board.setBothMux();
				setProperMux(fgn, type);
				
				if (dir) startvalue_neighb=900;
				else startvalue_neighb=100;
				initial_val=500;
				
				for (int ch=minchange; ch <= maxchange; ch+=step){
					if (dir) change=(-1)*ch;
					else change=ch;
					cout << "Run with change=" << change << endl;
					
					cout << "Programming neighbour lines to " << startvalue_neighb;
					for (int b=0; b<129; b++) values[b]=startvalue_neighb;
					for (int k=0; k < 24; k++){
						if (k!=line){
							for (int w=0; w < times_to_write; w++){
								cout << "." << flush;
								programFgLine(fgn, k, values);
							}
						}
					}
					cout << endl;
		
					cout << "Programming the subject line to " << initial_val << endl;
					for (int b=0; b<129; b++) values[b]=initial_val;
					programFgLine(fgn, line, values);

					cout << endl << "Control read from FGs:" << endl;
					for(int col=1; col<129; col+=1){
						fc[fgn]->readout_cell(line, col);
						temp=board.getVoltage(type);
						ctrl[col-1]=temp;
						cout << fixed << setprecision(3) << temp << "\t" << flush;
					}
					cout << endl;
					
					mean=meanvec(ctrl);
					stdev=stdvec(ctrl);
														
					cout << endl <<"Mean at the beginning: " << mean << endl;
					cout << "Standard deviation at the beginning : " << fixed << setprecision(5) << stdev << endl;
					
					ss.str("");
					ss <<"../results/tmak_fg/crosstalk_test.dat";
					filename = ss.str();
					file.open(filename.c_str(), fstream::app | fstream::out);
					file << change << " " << fixed << setprecision(4) << mean << " " << setprecision(5) << stdev << " " << flush;
					file.flush();
					file.close();

					for (int b=0; b<129; b++) values[b]=startvalue_neighb+change;
					for (int k=0; k < 24; k++){
						if (k!=line){
							cout << "Programming neighbour line " << k << " up/down to " << startvalue_neighb+change;
							for (int w=0; w < times_to_write; w++) programFgLine(fgn, k, values);

							cout << endl << "Final read from FGs after line " << k << ": " << endl;
							for(int col=1; col<129; col++){
								fc[fgn]->readout_cell(line, col);
								temp=board.getVoltage(type);
								result[col-1]=temp;
								cout << fixed << setprecision(3) << temp << "\t" << flush;
							}
							cout << endl;
							
							mean0=meanvec(result);
							stdev0=stdvec(result);

							cout << "Mean after line " << k <<": " << mean0 << endl;
							cout << "Mean difference : " << fixed << setprecision(4) << mean0-mean << endl;
							cout << "Standard deviation: " << fixed << setprecision(4) << stdev0 << endl;
							cout << "Standard deviation difference: " << fixed << setprecision(4) << stdev0-stdev << endl << fixed << setprecision(3) << endl;
							
							ss.str("");
							ss <<"../results/tmak_fg/crosstalk_test.dat";
							filename = ss.str();
							file.open(filename.c_str(), fstream::app | fstream::out);
							file << fixed << setprecision(4) << mean0 << " " << setprecision(5) << stdev0 << " " << flush;
							if (k==23) file << "\n" << flush;
							file.flush();
							file.close();
						}
					}
					cout << endl;
				}
				break;
			} 

			case 'f':{ //crosstalk after multiple writings
				bool dir;
				int fgn, line, initial_val, minchange, maxchange, startvalue_neighb, step, change, max_write;
				double temp, mean, mean0, stdev, stdev0;
				std::vector<double> ctrl(128);
				std::vector<double> result(128);
				SBData::ADtype type;
				std::vector<int> values(129);
				int times_to_write=1;
				
				cout << "Which controller? 0 - 3" << endl;
				cin >> fgn;
				cout << "Which line? 0 - 23" << endl;
				cin >> line;
				cout << "Choose direction: 0 - up, 1 - down:" << endl;
				cin >> dir; //default 0
				cout << "Choose minimum change:" << endl;
				cin >> minchange; //default 0
				cout << "Choose maximum change: (max. 900)" << endl;
				cin >> maxchange; //default 800
				cout << "Choose step:" << endl;
				cin >> step; //default 2
				cout << "Choose number of write cycles:" << endl;
				cin >> max_write; //default 3
				
				board.setBothMux();
				setProperMux(fgn, type);
				
				if (dir) startvalue_neighb=900;
				else startvalue_neighb=100;
				initial_val=500;
				
				for (int ch=minchange; ch <= maxchange; ch+=step){
					if (dir) change=(-1)*ch;
					else change=ch;

					cout << "Run with change=" << change << endl;
					cout << "Programming neighbour lines to " << startvalue_neighb;
					for (int b=0; b<129; b++) values[b]=startvalue_neighb;
					for (int k=0; k < 24; k++){
						if (k!=line){
							for (int w=0; w < times_to_write; w++){
								cout << "." << flush;
								programFgLine(fgn, k, values);
							}
						}
					}
					cout << endl;
				
					for(int wr=0; wr < max_write; wr++){
						cout << "Write cycle number " << wr << endl;
						cout << "Programming the subject line to " << initial_val << endl;
						for (int b=0; b<129; b++) values[b]=initial_val;
						programFgLine(fgn, line, values);

						cout << endl << "Control read from FGs:" << endl;
						for(int col=1; col<129; col+=1){
							fc[fgn]->readout_cell(line, col);
							temp=board.getVoltage(type);
							ctrl[col-1]=temp;
							cout << fixed << setprecision(3) << temp << "\t" << flush;
						}
						cout << endl;
						
						mean=meanvec(ctrl);
						stdev=stdvec(ctrl);
						
						cout << endl <<"Mean at the beginning: " << mean << endl;
						cout << "Standard deviation at the beginning : " << fixed << setprecision(4) << stdev << endl;
						
						ss.str("");
						ss <<"../results/tmak_fg/crosstalk_test.dat";
						filename = ss.str();
						file.open(filename.c_str(), fstream::app | fstream::out);
						file << change << " " << fixed << setprecision(4) << mean << " " << setprecision(5) << stdev << " " << flush;
						file.flush();
						file.close();
						
						cout << endl << "Programming neighbour lines up/down to " << startvalue_neighb+change << "...";
						for (int b=0; b<129; b++) values[b]=startvalue_neighb+change;
						for (int k=0; k < 24; k++){
							if (k!=line){
								for (int w=0; w < times_to_write; w++) programFgLine(fgn, k, values);
							}
						}
						
						cout << endl << "Final read from FGs after write " << wr << ": " << endl;
						for(int i=0; i<128; i+=1) result[i]=0; //clear array
						for(int col=1; col<129; col+=1){
							fc[fgn]->readout_cell(line, col);
							temp=board.getVoltage(type);
							result[col-1]=temp;
							cout << fixed << setprecision(3) << temp << "\t" << flush;
						}
						cout << endl;
						
						mean0=meanvec(result);
						stdev0=stdvec(result);
						
						cout << "Mean after write " << wr <<": " << mean0 << endl;
						cout << "Mean difference : " << fixed << setprecision(4) << mean0-mean << endl;
						cout << "Standard deviation: " << fixed << setprecision(4) << stdev0 << endl;
						cout << "Standard deviation difference: " << fixed << setprecision(4) << stdev0-stdev << endl << fixed << setprecision(3) << endl;
						
						ss.str("");
						ss <<"../results/tmak_fg/crosstalk_test.dat";
						filename = ss.str();
						file.open(filename.c_str(), fstream::app | fstream::out);
						file << fixed << setprecision(4) << mean0 << " " << setprecision(5) << stdev0 << " " << flush;
						if (wr==max_write-1) file << "\n" << flush;
						file.flush();
						file.close();
						cout << endl;
					}
				}
				break;
			} 

			case 'o':{ //measurement error acquirement
				int fgn, line, cell, runs;
				double mean, stdev;
				SBData::ADtype type;

				cout << "what controller? 0 - 3" << endl;
				cin >> fgn;
				cout << "what line? 0 - 23" << endl;
				cin >> line;
				cout << "what cell? 0 - 128" << endl;
				cin >> cell;
				cout << "how many readouts?" << endl;
				cin >> runs;
				
				board.setBothMux();
				setProperMux(fgn, type);
				std::vector<double> values(runs);
				
				for(int val=0; val<1024; val++){
					cout << "Programming value " << val << endl;
					std::vector<int> programval(129,val);
					programFgLine(fgn, line, programval);
					
					cout << "Reading cell " << runs << " times..." << endl;
					for(int i=0; i<runs; i++){
						fc[fgn]->readout_cell(line, cell);
						values[i]=board.getVoltage(type);
					}
					mean=meanvec(values);
					stdev=stdvec(values);
					ss.str("");
					ss <<"../results/tmak_fg/measurement_error.dat";
					filename = ss.str();
					file.open(filename.c_str(), fstream::app | fstream::out);
					file << val << "\t" << setprecision(5) << mean << "\t" << setprecision(6) << stdev << "\n" << flush;
					file.flush(); file.close();
				}
				break;
			} 

			case 'k':{ //deviation with constant values
				double temp;
				SBData::ADtype type;
				uint fgn, line, runs;
				std::vector < vector<double> > sums(1024);
				std::vector<int> rndvalues(129,0), tempvalues;
				int times_to_write=1;

				cout << "what controller? 0 - 3" << endl;
				cin >> fgn;
				cout << "what line? 0 - 23" << endl;
				cin >> line;
				cout << "how many values? (2-128)" << endl;
				cin >> runs;
				
				board.setBothMux();
				setProperMux(fgn, type);
				
				tempvalues=generate_rnd();
				for (int n=0; n<1024; n++) sums[n].clear();

				for (int s=0; s < 1024; s++){  //main counter
					cout << endl << "Run " << s << " of " << 1024 << endl << endl;
					for (int i=0; i<129; i++) rndvalues[i]=tempvalues[s];
					for (int w=0; w < times_to_write; w++) programFgLine(fgn, line, rndvalues);
					//readoutFgRam(fgn, 0);
					cout << endl << "read back from FGs:" << endl;
					for(int col=1; col<runs; col+=1){
						fc[fgn]->readout_cell(line, col);
						temp=board.getVoltage(type);
						sums[rndvalues[col]].push_back(temp);
						cout << fixed << setprecision(3) << temp << "\t" << flush;
					}
					cout << endl;
				}
				write_files(sums, "measurement", "rawdatameasurement");
				break;
			} 

			case 'l':{ //simple measurement, no sweeping
				double temp;
				SBData::ADtype type;
				uint fgn, line, runs;
				std::vector < vector<double> > sums(1024);
				std::vector<int> rndvalues, tempvalues;
				int times_to_write=1;

				cout << "what controller? 0 - 3" << endl;
				cin >> fgn;
				cout << "what line? 0 - 23" << endl;
				cin >> line;
				cout << "how many runs?" << endl;
				cin >> runs;
				cout << "how many times to write?" << endl;
				cin >> times_to_write;

				board.setBothMux();
				setProperMux(fgn, type);

				for (int n=0; n<1024; n++) sums[n].clear();

				for (int s=0; s < runs; s++){  //main counter
					cout << endl << "Run " << s << " of " << runs-1 << endl << endl;
					randomizeVector(rndvalues, tempvalues);
					for (int w=0; w < times_to_write; w++) programFgLine(fgn, line, rndvalues);
					//readoutFgRam(fgn, 0);
					cout << endl << "read back from FGs:" << endl;
					for(int col=1; col<129; col+=1){
						fc[fgn]->readout_cell(line, col);
						temp=board.getVoltage(type);
						sums[rndvalues[col]].push_back(temp);
						cout << fixed << setprecision(3) << temp << "\t" << flush;
					}
					cout << endl;
				}
				write_files(sums, "measurement", "rawdatameasurement");
				break;
			} 

			case 'v':{ //single value sweeping
				uint sw, fgn, line, runs, start, end, step;
				double temp;
				stringstream sweepparam, rawdata;
				std::vector < vector<double> > sums(1024);
				SBData::ADtype type;
				std::vector<int> rndvalues, tempvalues;
				int times_to_write=1;
				
				cout << "Which controller? 0 - 3" << endl;
				cin >> fgn;
				cout << "Which line? 0 - 23" << endl;
				cin >> line;
				cout << "How many runs per sweep?" << endl;
				cin >> runs;
				
				board.setBothMux();
				setProperMux(fgn, type);
				
				cout << "Choose parameter to sweep: " << endl;
				cout << "1: Voltagewritetime" << endl;
				cout << "2: Currentwritetime" << endl;
				cout << "3: Acceleratorstep" << endl;
				cout << "4: Pulselength" << endl;
				cout << "5: Readtime" << endl;
				cout << "6: Biasn" << endl;
				cout << "7: Biasp" << endl;
				cout << "8: Line number" << endl;
				cout << "9: FG controler" << endl;
				cin >> sw;
				cout << "Choose starting value: " << endl;
				cin >> start;
				cout << "Choose end value: " << endl;
				cin >> end;
				cout << "Choose sweep step: " << endl;
				cin >> step;
	
				for(int num=start; num <= end; num+=step){
					sweepparam.str("");
					rawdata.str("");
					if (sw == 1) {sweepparam << "volttime" << num; rawdata << "rawdatavolttime" << num; voltagewritetime=num;}
					else if (sw == 2) {sweepparam << "currtime" << num; rawdata << "rawdatacurrtime" << num; currentwritetime=num;}
					else if (sw == 3) {sweepparam << "accstep" << num; rawdata << "rawdataaccstep" << num; acceleratorstep=num;}
					else if (sw == 4) {sweepparam << "pulselength" << num; rawdata << "rawdatapulselength" << num; pulselength=num;}
					else if (sw == 5) {sweepparam << "readtime" << num; rawdata << "rawdatareadtime" << num; readtime=num;}
					else if (sw == 6) {sweepparam << "biasn" << num; rawdata << "rawdatabiasn" << num; fg_biasn=num;}
					else if (sw == 7) {sweepparam << "bias" << num; rawdata << "rawdatabias" << num; fg_bias=num;}
					else if (sw == 8) {sweepparam << "linenumber" << num; rawdata << "rawdatalinenumber" << num; line=num;}
					else {sweepparam << "fgcontroler" << num; rawdata << "rawdatafgcontroler" << num; fgn=num; setProperMux(fgn, type);}
					
					cout << endl << sweepparam.str() << endl << endl;
					setupFg();
					displaySetup();
					for (int n=0; n<1024; n++) sums[n].clear();
					
					for (int s=0; s < runs; s++){  //main routine
						cout << endl << "Run " << s << " of " << runs-1 << endl << endl;
						randomizeVector(rndvalues, tempvalues);
						for (int w=0; w < times_to_write; w++) programFgLine(fgn, line, rndvalues);
						//readoutFgRam(fgn, 0);
						
						cout << endl << "read back from FGs:" << endl;
						for(int col=1; col<129; col+=1){
							fc[fgn]->readout_cell(line, col);
							temp=board.getVoltage(type);
							sums[rndvalues[col]].push_back(temp);
							cout << fixed << setprecision(3) << temp << "\t" << flush;
						}
						cout << endl;
					}
					write_files(sums, sweepparam.str(), rawdata.str());
				}
				break;
			} 

			case 'b':{ //two values sweeping
				uint sw1, sw2, fgn, line, runs, start1, end1, step1, start2, end2, step2;
				double temp;
				stringstream sweepparam1, rawdata1, sweepparam2, rawdata2;
				std::vector < vector<double> > sums(1024);
				SBData::ADtype type;
				std::vector<int> rndvalues, tempvalues;
				int times_to_write=1;
				
				cout << "Which controller? 0 - 3" << endl;
				cin >> fgn;
				cout << "Which line? 0 - 23" << endl;
				cin >> line;
				cout << "How many runs per sweep?" << endl;
				cin >> runs;
				
				board.setBothMux();
				setProperMux(fgn, type);
				
				cout << "Choose first parameter to sweep: " << endl;
				cout << "1: Voltagewritetime" << endl;
				cout << "2: Currentwritetime" << endl;
				cout << "3: Acceleratorstep" << endl;
				cout << "4: Pulselength" << endl;
				cout << "5: Readtime" << endl;
				cout << "6: Biasn" << endl;
				cout << "7: Biasp" << endl;
				cout << "8: Line number" << endl;
				cout << "9: FG controler" << endl;
				cin >> sw1;
				cout << "Choose starting value: " << endl;
				cin >> start1;
				cout << "Choose end value: " << endl;
				cin >> end1;
				cout << "Choose sweep step: " << endl;
				cin >> step1;
				
				cout << "Choose second parameter to sweep: " << endl;
				cout << "1: Voltagewritetime" << endl;
				cout << "2: Currentwritetime" << endl;
				cout << "3: Acceleratorstep" << endl;
				cout << "4: Pulselength" << endl;
				cout << "5: Readtime" << endl;
				cout << "6: Biasn" << endl;
				cout << "7: Biasp" << endl;
				cout << "8: Line number" << endl;
				cout << "9: FG controler" << endl;
				cin >> sw2;
				cout << "Choose starting value: " << endl;
				cin >> start2;
				cout << "Choose end value: " << endl;
				cin >> end2;
				cout << "Choose sweep step: " << endl;
				cin >> step2;
	
				for(int num1=start1; num1 <= end1; num1+=step1){
					sweepparam1.str("");
					rawdata1.str("");
					if (sw1 == 1) {sweepparam1 << "volttime" << num1; rawdata1 << "rawdatavolttime" << num1; voltagewritetime=num1;}
					else if (sw1 == 2) {sweepparam1 << "currtime" << num1; rawdata1 << "rawdatacurrtime" << num1; currentwritetime=num1;}
					else if (sw1 == 3) {sweepparam1 << "accstep" << num1; rawdata1 << "rawdataaccstep" << num1; acceleratorstep=num1;}
					else if (sw1 == 4) {sweepparam1 << "pulselength" << num1; rawdata1 << "rawdatapulselength" << num1; pulselength=num1;}
					else if (sw1 == 5) {sweepparam1 << "readtime" << num1; rawdata1 << "rawdatareadtime" << num1; readtime=num1;}
					else if (sw1 == 6) {sweepparam1 << "biasn" << num1; rawdata1 << "rawdatabiasn" << num1; fg_biasn=num1;}
					else if (sw1 == 7) {sweepparam1 << "bias" << num1; rawdata1 << "rawdatabias" << num1; fg_bias=num1;}
					else if (sw1 == 8) {sweepparam1 << "linenumber" << num1; rawdata1 << "rawdatalinenumber" << num1; line=num1;}
					else {sweepparam1 << "fgcontroler" << num1; rawdata1 << "rawdatafgcontroler" << num1; fgn=num1; setProperMux(fgn, type);}
					
					for(int num2=start2; num2 <= end2; num2+=step2){
						sweepparam2.str("");
						rawdata2.str("");
						if (sw2 == 1) {sweepparam2 << "volttime" << num2; rawdata2 << "rawdatavolttime" << num2; voltagewritetime=num2;}
						else if (sw2 == 2) {sweepparam2 << "currtime" << num2; rawdata2 << "rawdatacurrtime" << num2; currentwritetime=num2;}
						else if (sw2 == 3) {sweepparam2 << "accstep" << num2; rawdata2 << "rawdataaccstep" << num2; acceleratorstep=num2;}
						else if (sw2 == 4) {sweepparam2 << "pulselength" << num2; rawdata2 << "rawdatapulselength" << num2; pulselength=num2;}
						else if (sw2 == 5) {sweepparam2 << "readtime" << num2; rawdata2 << "rawdatareadtime" << num2; readtime=num2;}
						else if (sw2 == 6) {sweepparam2 << "biasn" << num2; rawdata2 << "rawdatabiasn" << num2; fg_biasn=num2;}
						else if (sw2 == 7) {sweepparam2 << "bias" << num2; rawdata2 << "rawdatabias" << num2; fg_bias=num2;}
						else if (sw2 == 8) {sweepparam2 << "linenumber" << num2; rawdata2 << "rawdatalinenumber" << num2; line=num2;}
						else {sweepparam2 << "fgcontroler" << num2; rawdata2 << "rawdatafgcontroler" << num2; fgn=num2; setProperMux(fgn, type);}
						
						cout << endl << sweepparam1.str() << " " << sweepparam2.str() << endl << endl;
						setupFg();
						displaySetup();
						for (int n=0; n<1024; n++) sums[n].clear();
						
						for (int s=0; s < runs; s++){  //main routine
							cout << endl << "Run " << s << " of " << runs-1 << endl << endl;
							randomizeVector(rndvalues, tempvalues);
							for (int w=0; w < times_to_write; w++) programFgLine(fgn, line, rndvalues);
							//readoutFgRam(fgn, 0);
							
							cout << endl << "read back from FGs:" << endl;
							for(int col=1; col<129; col+=1){
								fc[fgn]->readout_cell(line, col);
								temp=board.getVoltage(type);
								sums[rndvalues[col]].push_back(temp);
								cout << fixed << setprecision(3) << temp << "\t" << flush;
							}
							cout << endl;
						}
						stringstream sstr1, sstr2;
						sstr1 << sweepparam1.str() << sweepparam2.str();
						sstr2 << rawdata1.str() << rawdata2.str();
						write_files(sums, sstr1.str(), sstr2.str());
					}
				}
				break;
			} 

			case 's':{ //stress test for an FG line
				int fgn, line, x;
				long unsigned int maxcycle, cycle=0;
				double temp, meanmin, meanmax, devmin, devmax;
				std::vector<double> fgvaluesmin(128); // minimum values from FGs
				std::vector<double> fgvaluesmax(128); // maximum values from FGs
				SBData::ADtype type;
				int times_to_write=49;
				
				cout << "which controller? 0 - 3" << endl;
				cin >> fgn;
				cout << "which line? 0 - 23" << endl;
				cin >> line;
				cout << "what upper value? 0 - 1023" << endl;
				cin >> x;
				cout << "how many cycles?" << endl;
				cin >> maxcycle;
				
				board.setBothMux();
				setProperMux(fgn, type);
				
				cout << "setting vdd11 to 11 Volts" << endl;
				board.setVoltage(SBData::vdd11, 11);
				
				std::vector<int> values(129,x), zeros(129,0);
				
				while (cycle < 1000){
					cycle++;
					cout << endl << "to exit, press any key and wait 60 seconds" << endl;
					cout << "Write cycle " << cycle*(times_to_write+1) << endl;
					
					for (int w=0; w < times_to_write; w++){
						programFgLine(fgn, line, zeros);
						programFgLine(fgn, line, values);
					}
					
					fc[fgn]->initArray(0, zeros);
					fc[fgn]->program_cells(line, 0, 0);
					while(fc[fgn]->isBusy());
					
					cout << endl << "read back from FGs, minimum:" << endl;
					for(int col=1; col<129; col+=1){
						fc[fgn]->readout_cell(line, col);
						temp=board.getVoltage(type);
						fgvaluesmin[col-1]=temp;
						cout << fixed << setprecision(3) << temp << "\t" << flush;
					}
					cout << endl;
					
					fc[fgn]->initArray(0, values);
					while(fc[fgn]->isBusy());
					fc[fgn]->program_cells(line, 0, 1);
					while(fc[fgn]->isBusy());
					
					cout << endl << "read back from FGs, maximum:" << endl;
					for(int col=1; col<129; col+=1){
						fc[fgn]->readout_cell(line, col);
						temp=board.getVoltage(type);
						fgvaluesmax[col-1]=temp;
						cout << fixed << setprecision(3) << temp << "\t" << flush;
					}
					cout << endl;
					
					meanmin=meanvec(fgvaluesmin);
					meanmax=meanvec(fgvaluesmax);
					devmin=stdvec(fgvaluesmin);
					devmax=stdvec(fgvaluesmax);

					ss.str("");
					ss <<"../results/tmak_fg/mean_deviation_stress.dat";
					filename = ss.str();
					file.open(filename.c_str(), fstream::app | fstream::out);
					
					file << fixed << setprecision(3) << meanmin << " " << setprecision(4) << devmin << " " 
								<< setprecision(3) << meanmax << " " << setprecision(4) << devmax << "\n" << flush;
					file.flush();
					file.close();
					
					ss1.str("");
					ss1 <<"../results/tmak_fg/rawdata_stress_min.dat";
					filename1 = ss1.str();
					file1.open(filename1.c_str(), fstream::app | fstream::out);
					
					for (int p=0; p<128; p++){
						file1 << fixed << setprecision(3) << fgvaluesmin[p] << " " << flush;
					}
					
					file1 <<  "\n" << flush;
					file1.flush();
					file1.close();
					
					ss1.str("");
					ss1 <<"../results/tmak_fg/rawdata_stress_max.dat";
					filename1 = ss1.str();
					file1.open(filename1.c_str(), fstream::app | fstream::out);
					
					for (int p=0; p<128; p++){
						file1 << fixed << setprecision(3) << fgvaluesmax[p] << " " << flush;
					}
					file1 <<  "\n" << flush;
					file1.flush();
					file1.close();

					cout << endl;
				}
				cout << "setting vdd11 back to 10.5 Volts" << endl;
				board.setVoltage(SBData::vdd11,10.5);
				break;
			} 

			case 'i':{ //drift over time test
				int fgn;
				double mean, stdev;
				SBData::ADtype type;
				std::vector<int> values1(129,0), values2(129,250), values3(129,500), values4(129,750), values5(129,1000);
				std::vector<double> readout(128,0);
				time_t starttime, currtime, timediff;
				
				cout << "Choose controller (0-3): " << endl;
				cin >> fgn;
				
				cout << "Programming lines 0-9 for the test" << endl;
				for (int i=0; i<4; i++){
					programFgLine(fgn, 0, values1);
					programFgLine(fgn, 1, values1);
					programFgLine(fgn, 2, values2);
					programFgLine(fgn, 3, values2);
					programFgLine(fgn, 4, values3);
					programFgLine(fgn, 5, values3);
					programFgLine(fgn, 6, values4);
					programFgLine(fgn, 7, values4);
					programFgLine(fgn, 8, values5);
					programFgLine(fgn, 9, values5);
				}
				
				starttime = time(0); //remember the start time
				
				ss.str("");
				ss <<"../results/tmak_fg/drift_test_mean.dat";
				filename = ss.str();
				ss1.str("");
				ss1 <<"../results/tmak_fg/drift_test_stdev.dat";
				filename1 = ss1.str();
								
				board.setBothMux();
				setProperMux(fgn, type);
				
				while(1){
					for (int line=0; line<10; line++){
						for(int col=1; col<129; col+=1){
							fc[fgn]->readout_cell(line, col);
							readout[col-1]=board.getVoltage(type);
							mean=meanvec(readout);
							stdev=stdvec(readout);
							//~ cout << fixed << setprecision(3) << readout[col-1] << "\t" << flush;
						}
						currtime = time(0); //get current time
						timediff = currtime-starttime;

						cout << endl << "Elapsed time: " << timediff << " seconds" << endl;
						cout << "Mean of line " << line << " is " << setprecision(4) << mean << endl;
						cout << "Stdev of line " << line << " is " << setprecision(5) << stdev << endl;

						file.open(filename.c_str(), fstream::app | fstream::out);
						file1.open(filename1.c_str(), fstream::app | fstream::out);
						file << setprecision(5) << timediff << "\t" << mean << "\t" << flush;
						file1 << setprecision(5) << timediff << "\t" << stdev << "\t" << flush;
						file.flush(); file.close();
						file1.flush(); file1.close();
					}
					file.open(filename.c_str(), fstream::app | fstream::out);
					file1.open(filename1.c_str(), fstream::app | fstream::out);
					file <<  "\n" << flush; file.flush(); file.close();
					file1 <<  "\n" << flush; file1.flush(); file1.close();
					
					if(kbhit()){ // if keyboard has input pending, break from loop
						getch(); // "resets" kbhit by removing a key from the buffer
						break;
					}
				}
				break;
			} 

			case 'g':{ //differential writing test
				double temp;
				SBData::ADtype type;
				uint fgn, line, runs;
				std::vector < vector<double> > sums(1024);
				std::vector<int> rndvalues, tempvalues;
				int times_to_write=1;

				cout << "what controller? 0 - 3" << endl;
				cin >> fgn;
				cout << "what line? 0 - 23" << endl;
				cin >> line;
				cout << "how many runs?" << endl;
				cin >> runs;
				
				board.setBothMux();
				setProperMux(fgn, type);

				for (int n=0; n<1024; n++) sums[n].clear();

				for (int s=0; s < runs; s++){  //main counter
					cout << endl << "Sequential writing: Run " << s << " of " << runs-1 << endl << endl;
					randomizeVector(rndvalues, tempvalues);
					///sequential writing, as formerly intended
					for (int w=0; w < times_to_write; w++){
						log(Logger::INFO)<<"tmak_fg::programming down to zero. ";
						fc[fgn]->initzeros(0);
						for(int i=0; i<24; i+=1){
							while(fc[fgn]->isBusy());
							cout<<"."<<flush;
							fc[fgn]->program_cells(i,0,0);
							while(fc[fgn]->isBusy());
						}
						cout<<endl;
						
						log(Logger::INFO)<<"tmak_fg::programming up. ";
						fc[fgn]->initArray(0, rndvalues);
						for(int i=0; i<24; i+=1){
							while(fc[fgn]->isBusy());
							cout<<"."<<flush;
							fc[fgn]->program_cells(i,0,1);
							while(fc[fgn]->isBusy());
						}
						cout<<endl;
					}

					cout << endl << "read back from FGs:" << endl;
					for(int col=1; col<129; col+=1){
						fc[fgn]->readout_cell(line, col);
						temp=board.getVoltage(type);
						sums[rndvalues[col]].push_back(temp);
						cout << fixed << setprecision(3) << temp << "\t" << flush;
					}
					cout << endl;
				}
				write_files(sums, "measurementseq", "rawdatameasurementseq");

				for (int n=0; n<1024; n++) sums[n].clear();

				for (int s=0; s < runs; s++){  //main counter
					cout << endl << "Differential writing: Run " << s << " of " << runs-1 << endl << endl;
					randomizeVector(rndvalues, tempvalues);
					for (int w=0; w < times_to_write; w++) programFgLine(fgn, line, rndvalues);
					//readoutFgRam(fgn, 0);
					cout << endl << "read back from FGs:" << endl;
					for(int col=1; col<129; col+=1){
						fc[fgn]->readout_cell(line, col);
						temp=board.getVoltage(type);
						sums[rndvalues[col]].push_back(temp);
						cout << fixed << setprecision(3) << temp << "\t" << flush;
					}
					cout << endl;
				}
				write_files(sums, "measurementdiff", "rawdatameasurementdiff");
				break;
			} 

			case 'd':{ //error bit test
				int fgn, line, cell;
				SBData::ADtype type;
				std::vector<int> values;
				int times_to_write=1;
				double temp;
				
				cout << "what controller? 0 - 3" << endl;
				cin >> fgn;
				cout << "what line? 0 - 23" << endl;
				cin >> line;
				//~ cout << "what cell? 1 - 129" << endl;
				//~ cin >> cell;
				
				board.setBothMux();
				setProperMux(fgn, type);
				
				for (int x=950; x < 970; x++){
					cout << "Run " << x << " started" << endl;
					values.clear();
					for (int b=0; b<129; b++){
						values.push_back(x);
						#if HICANN_V == 1
						if (b==128) values[128]=values[0];
						#endif
					} 
					//values[cell]=1023;

					if (fc[fgn]->isError()){
						cout << "Error bit was active before programming at following cells:" << endl;
						int counter=0;
						while(fc[fgn]->isError()){
							counter++;
							cout << "Cell " << fc[fgn]->get_next_false() << endl;
						}
						cout << "A total of " << counter <<" cells did not reach desired values" << endl;
						cout << "Error bit was reset" << endl;
						cin.ignore();
					}
					
					vector<int> max(129,1023);
					programFgLine(fgn, line, max);
					programFgLine(fgn, line, values);
					
					if (fc[fgn]->isError()){
						cout << "An error occured during programming at cells: " << x << endl;
						int counter=0;
						while(fc[fgn]->isError()){
							counter++;
							cout << "Cell " << fc[fgn]->get_next_false() << endl;
						}
						cout << "A total of " << counter <<" cells did not reach desired values" << endl;
						cin.ignore();
					}
				
					cout << endl << "read back from FGs: ";
					for(int col=1; col<2; col+=1){
						fc[fgn]->readout_cell(line, col);
						temp=board.getVoltage(type);
						cout << fixed << setprecision(3) << temp << "\t" << flush;
					}
					cout << endl;
				
				}
				break;
			} 

			case 'z':{ //program all FG to zero
				log(Logger::INFO)<<"tmak_fg::programming down to zero. ";
				for(int fg_num=0; fg_num<4; fg_num+=1) fc[fg_num]->initzeros(0);
				for(int i=0; i<24; i+=1){
					for(int fg_num=0; fg_num<4; fg_num+=1){
						while(fc[fg_num]->isBusy());
						cout<<"."<<flush;
						log(Logger::DEBUG2)<<"programing row " << i << endl;
						fc[fg_num]->program_cells(i,0,0);
					}
				}
				cout<<endl;
				break;
			}

			case 'm':{ //program all FG to maximum
				log(Logger::INFO)<<"tmak_fg::programming to maximum. ";
				for(int fg_num=0; fg_num<4; fg_num+=1) fc[fg_num]->initmax(0);
				for(int i=0; i<24; i++){
					for(int fg_num=0; fg_num<4; fg_num+=1){
						while(fc[fg_num]->isBusy());
						cout<<"."<<flush;
						log(Logger::DEBUG2)<<"programing row " << i << endl;
						fc[fg_num]->program_cells(i,0,1);
					}
				}
				cout<<endl;
				break;
			}

			case 'c':{ //program an XML-File
				log(Logger::INFO)<<"tmak_fg::programming xml-file on floating gate array. ";
				log(Logger::INFO)<<"tmak_fg::writing currents ...";
				for( uint lineNumber = 1; lineNumber < 24; lineNumber += 2){ //starting at line 1 as this is the first current!!!!!
					for(int fg_num = 0; fg_num < 4; fg_num += 1){
						programFgLine(fg_num, lineNumber, conf->getFgLine((fg_loc)fg_num, lineNumber));
					}
				}
				log(Logger::INFO)<<"tmak_fg::writing voltages ...";
				for( uint lineNumber = 0; lineNumber < 24; lineNumber += 2){
					for(int fg_num = 0; fg_num < 4; fg_num += 1){	
						programFgLine(fg_num, lineNumber, conf->getFgLine((fg_loc)fg_num, lineNumber));
					}
				}
				break;
			}

			case 'w':{ //program an FG line
				int fgn, line, value, first;

				cout << "what controller? 0 - 3" << endl;
				cin >> fgn;
				cout << "what line? 0 - 23" << endl;
				cin >> line;
				cout << "what value? 0 - 1023" << endl;
				cin >> value;
				cout << "global parameter value: " << endl;
				cin >> first;
				
				std::vector<int> values(129,value);
				values[0]=first;
				programFgLine(fgn, line, values);
				break;
			}

			case 'r':{ //read out an FG line
				int fgn, line;
				double temp;
				SBData::ADtype type;

				cout << "what controller? 0 - 3" << endl;
				cin >> fgn;
				cout << "what line? 0 - 23" << endl;
				cin >> line;
				
				board.setBothMux();
				setProperMux(fgn, type);
				
				cout << endl << "read back from FGs:" << endl;
				double mean=0;
				for(int col=0; col<129; col+=1){
					fc[fgn]->readout_cell(line, col);
					temp=board.getVoltage(type);
					mean+=temp;
					cout << fixed << setprecision(3) << temp << "\t" << flush;
				}
				cout << endl << "Mean of the line is " << mean/129 << endl;
				break;
			}

			case 'R':{ //read out an FG cell
				int fgn, line, cell, runs;
				double mean, stdev;
				SBData::ADtype type;

				cout << "what controller? 0 - 3" << endl;
				cin >> fgn;
				cout << "what line? 0 - 23" << endl;
				cin >> line;
				cout << "what cell? 0 - 128" << endl;
				cin >> cell;
				cout << "how many runs?" << endl;
				cin >> runs;
				cout << "Wait until measurement is finished..." << endl;
				
				board.setBothMux();
				setProperMux(fgn, type);
				std::vector<double> values(runs);
				
				for(int i=0; i<runs; i++){
					fc[fgn]->readout_cell(line, cell);
					values[i]=board.getVoltage(type);
				}
				mean=meanvec(values);
				stdev=stdvec(values);
				ss.str("");
				ss <<"../results/tmak_fg/adc_test.dat";
				filename = ss.str();
				file.open(filename.c_str(), fstream::app | fstream::out);
				file << setprecision(5) << mean << "\t" << setprecision(6) << stdev << "\n" << flush;
				file.flush(); file.close();
				break;
			}
			
			case 'x':{ //quit
				cont = 0;
				break;
			} 
			}
		} while (cont);
     
		log(Logger::DEBUG1)<< "ok" << endl;
		return 1;
	};
};


class LteeTmAkFg : public Listee<Testmode>{
public:
	LteeTmAkFg() : Listee<Testmode>(string("tmak_fg"), string("Testmode for fg statistics")){};
	Testmode * create(){return new TmAkFg();};
};
LteeTmAkFg ListeeTmAkFg;
