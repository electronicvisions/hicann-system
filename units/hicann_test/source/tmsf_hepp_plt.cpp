
#include "testmode.h"
#include "hicann_ctrl.h"
#include "s2pp_tools.h"

using namespace facets;

struct Program {
	Program(std::string const& image, std::string const& compare) {
		image_file = image;
		compare_file = compare;
	}

	std::string image_file;
	std::string compare_file;
};

class TmSFHeppPlt : public Testmode {
	bool program_test(CtrlModule* cm) {
		std::vector<Program> programs;

		programs.push_back(Program("testprogram.raw", "testprogram.raw"));

		for(std::vector<Program>::iterator it=programs.begin();
				it != programs.end();
				++it) {
			s2pp::core_reset(cm, s2pp::RESET_ON | s2pp::CLK_FORCE_OFF);

			cout << "loading program '" << it->image_file << "'" << endl;
			unsigned int const code_size = s2pp::load_program(cm, it->image_file);
			//s2pp::core_reset(cm, s2pp::RESET_ON);
			//s2pp::core_reset(cm, s2pp::RESET_OFF);

			
			ci_payload res;
			while( cm->get_data(&res, false) )
				throw std::runtime_error("unexpected read data");

			//while( !s2pp::sleeping(cm) );

			std::vector<uint32_t> dump(code_size+100);
			std::vector<uint32_t> compare_image;
			s2pp::read_block(cm, 0, dump.begin(), dump.end());

			unsigned int const compare_size = s2pp::read_program(it->compare_file, compare_image);

			std::pair<std::vector<uint32_t>::iterator,std::vector<uint32_t>::iterator> mm = std::mismatch(compare_image.begin(),
					compare_image.end(), dump.begin());

			if( mm.first != compare_image.end() ) {
				cout << "*** mismatch for program '" << it->image_file << "' at position "
					<< mm.first - compare_image.begin()
					<< " (expected: 0x" << std::hex << std::setfill('0') << std::setw(8)
					<< *(mm.first)
					<< ", got: 0x" << std::setfill('0') << std::setw(8)
					<< *(mm.second)
					<< std::dec << ")\n(only the first mismatch is reported)\n";

				std::string const dump_fn = (boost::format("dump_%s.bin") % it->image_file).str();
				std::ofstream ofs( dump_fn.c_str(), std::ios::binary );
				s2pp::write_bin(ofs, dump.begin(), dump.end());

				cout << "memory image dumped to '" << dump_fn << "'" << endl;
			}
		}
	}

	public:
	bool test() {
    	HicannCtrl *hc; 

        if ((chip.size()<FPGA_COUNT+DNC_COUNT) || !chip[FPGA_COUNT+DNC_COUNT]) {
            log << "ERROR: object 'chip' in testmode not set, abort" << endl;
            return 0;
        }
		
        // use HICANN number 0 (following FPGA and DNC in address space) for this testmode
        hc = (HicannCtrl*) chip[FPGA_COUNT+DNC_COUNT];

        log << "Try Init() ..." ;

        if (hc->GetCommObj()->Init(hc->addr()) != Stage2Comm::ok) {
            log << "ERROR: Init failed, abort" << endl;
            return 0;
        }

        log << "Init() ok" << endl;

        std::auto_ptr<CtrlModule> cm_tag_1(new CtrlModule(hc->tags[1], 0x0000, 0xffff));

		return program_test(cm_tag_1.get());
	}
};


//---------------------------------------------------------------------------
class LteeTmSFHeppPlt : public Listee<Testmode> {
public:
    LteeTmSFHeppPlt() : Listee<Testmode>(string("tmsf_hepp_plt"), string("program level testing for the embedded plasticity processor")){};
    Testmode * create(){return new TmSFHeppPlt();};
};
//---------------------------------------------------------------------------

LteeTmSFHeppPlt ListeeTmSFHeppPlt;



// vim: noexpandtab ts=4 sw=4 softtabstop=0 nosmarttab:
