// Company         :   kip                      
// Author          :   Johannes Schemmel, Andreas Gruebl          
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//                    			
// Filename        :   testmode.h                
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   Tue Jun 24 08
// Last Change     :   Mon Aug 04 08    
// by              :   agruebl        
//------------------------------------------------------------------------

//
// common testmode interface
// 
// Original file by Johannes Schemmel. Adapted to FACETS Stage 2 software by Andreas Gruebl
// 


#ifndef _TESTMODE_H
#define _TESTMODE_H

#include <boost/program_options.hpp>

#include "common.h"
#include "stage2_conf.h"
#include "s2ctrl.h"
#include "s2comm.h"
#include "s2_types.h"

#define TESTS2_SUCCESS true
#define TESTS2_FAILURE false


class Testmode {
protected:
	Logger& log;
	virtual std::string ClassName() {std::stringstream ss; ss << __FILE__; return ss.str();}
	int debug_level;	
	std::ostream & dbg(int l) { if (debug_level>= l) return std::cout<<ClassName()<<": "; else return NULLOS; }

public:	
    std::vector<facets::Stage2Ctrl *> chip;
    facets::Stage2Comm *comm;
	stage2_conf* conf;
	std::string label;
	std::vector<std::string> keys;
	facets::commodels commodel;
	boost::program_options::parsed_options * argv_options;
	std::bitset<8> active_hicanns;

	myjtag_full  *jtag;

	Testmode() :
		log(Logger::instance("hicann-system.testmode", Logger::INFO, ""))
	{
		chip.clear();
		conf=NULL;
		comm=NULL;
		jtag=NULL;
		commodel=facets::dryrun;
		argv_options = nullptr;
	};
	virtual ~Testmode(){};
	virtual bool test()=0; //performs test, return true if passed
};

/* Listee is a registration concept to avoid recompilation after changing or adding new testmodes
each testmode is derived from the class testmode and must implement at least the 
bool test() function that does the testing
the system initializes the std::vector<Spikenet *> and SpikenetComm* elements to give the testmode access to all data
it also has access to the global debug output dbg and to the static Listee functions

to connect the testmode with the main programm, the Testmoder Listee registers itself in a linked list of Listees
each Listee contains the name and the description of the testmode together with a create function that
calls the constructor of its assigned Testmode. Therefore it acts as a factory for its accompanying Testmode class. 
This allows runtime-creation of a testmode by specifying its name.
*/

//new Listee lists itself

template<class T> class Listee {//general Listee type
	std::string myname; //name by which class ist listed
	std::string mycomment; //mandatory explanation of Listee's purpose
	Listee<T> *next;
	public:
	static Listee<T> *first;
	Listee(std::string nname,std::string ncomment){
		myname=nname;mycomment=ncomment;
		Listee<T> *p=first;
		if(p==NULL){first=this;}
		else {	while(p->next!=NULL)p=p->next;
						p->next=this;	}
		next=NULL;
	};
	virtual ~Listee(){};
	const std::string &	name(void){return myname;};
	const std::string &	comment(void){return mycomment;};
	virtual T * create()=0;//calls constructor of Listee user
	Listee<T> * getNext(){return next;};
	static Listee<T> *getListee(std::string name){
		Listee<T> *p=first;
		while(p!=NULL){
			if (p->name() == name)return p;
			p=p->next;
		}
		return NULL;
	};

	static T * createNew(std::string name){
		Listee<T> *l;
		if((l=getListee(name))) return l->create();
		else return NULL;
	};
	
};

/*example
class MyListee:Listee<MyTypeBase>{
	MyListee():Listee(std::string("name"),string("comment")){}:
	MyTypeBase * create(){return new MyTypeDerived();};
} mylistee;
*/

#endif //_TESTMODE_H
