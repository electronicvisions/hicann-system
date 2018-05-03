/**
// Company         :   kip                      
// Author          :   Johannes Schemmel, Andreas Gruebl          
// E-Mail          :   agruebl@kip.uni-heidelberg.de
//                    			
// Filename        :   common.h                
// Project Name    :   p_facets
// Subproject Name :   s_hicann1            
//                    			
// Create Date     :   Tue Jun 24 08
// Last Change     :   Mon Aug 04 08    
// by              :   agruebl        
//------------------------------------------------------------------------


Definitions of common methods for the Stage 2 Control Software / HAL.
Taken from Stage 1 software - this file originally by Johannes Schemmel,
adapted to FACETS Stage 2 software by Andreas Gruebl

*/

#ifndef _COMMON_H_
#define _COMMON_H_

#define NCSC_INCLUDE_TASK_CALLS

//standard library includes

#include <string>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <list>
#include <queue>
#include <valarray>
#include <bitset>
#include <numeric>

#include <cstdlib>
#include <cstdio>
#include <cstring>
extern "C" {
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
}


#include "s2_types.h"      // FACETS Stage2 type definitions from System Simulation
#include "sim_def.h"       // constant definitions from System Simulation
#include "s2hal_def.h"     // HAL specific constant definitions
#include "hardware_base.h" // constants created from SystemVerilog code with genheader.sv

#include "logger.h"
#ifdef NCSIM
  #ifdef HICANN_DESIGN
     #include "jtag_emulator.h"
  #else
     #include "eth_jtag_test.h"
  #endif
#else
#include "jtag_cmdbase_fpgadnchicann_full.h"
#endif

// debug output class
//dbg(5)<<"blabla"<<endl;


extern uint randomseed;
/*
class Debug {
	public:
	ostringstream empty;
	enum dbglvl {fatal=0,verbose=5,debug=10,all=99999};
	uint setlevel; //global output level
	Debug(uint il=verbose):setlevel(il){};
	void setLevel(uint l){setlevel=l;};
	std::ostream & operator() (uint ll=fatal){if(l(ll))return std::cout; else {empty.str(std::string());return empty;}};//str(std::string()) clears std::stringstream by setting empty std::string, std::flush() only commits buffer to output
	std::ostream & o(uint ll=fatal){if(l(ll))return std::cout; else {empty.str(std::string());return empty;}};
	bool l(uint l){return (l<=setlevel);}
};
*/
//Debug dbg;
typedef uint32_t uint;
//helper function that outputs binary numbers 
inline std::ostream & binout(std::ostream &in, uint64 u,uint max) {
	for(int i=max-1;i>=0;--i)in<<((u&(1ULL<<i))?"1":"0");
	return in;
}

//helper function that generates a binary mask from an msb bit definition
inline uint64 makemask(uint msb){
		return (1ULL<<(msb+1))-1;
	}
inline uint64 mmm(uint msb){return makemask(msb);}
inline uint64 mmw(uint width){return makemask(width-1);}


// T is a std::container
template<class T, class R>
R mean(T const & v) {
	return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
}

template<class T>
double mean(T const & v) {
	return mean<T, double>(v);
}

// simple statistics: standard deviation and mean of std::vector T
// aka "unbiased estimation of standard deviation"
template<class T, class R>
R sdev(T const & v){
	R tmp = std::inner_product(v.begin(), v.end(), v.begin(), 0.0);
	return std::sqrt( (tmp / v.size() - mean(v) * mean(v)) / (1.0 - 1.0/v.size()) );
}

template<class T>
double sdev(T const & v){
	return sdev<T, double>(v);
}

// convenience renameing ;)
template<class T, class R>
R stdev(T const & v) {
	return sdev(v);
}

template<class T>
double stdev(T const & v) {
	return sdev<T, double>(v);
}

//fir filter, res must have the same size than data and be initialized with zero
template<class T> void fir(std::vector<T> &res, const std::vector<T> &data,const std::vector<T> &coeff){
	T norm=accumulate(coeff.begin(),coeff.end(),0.0)/coeff.size();
	for(uint i=0;i<data.size()-coeff.size()+1;++i){
		T val=0;
		for(uint j=0;j<coeff.size();++j)val+=data[i+j]*coeff[j];
		res[i+coeff.size()/2 +1]=val/norm;
	}
}

// JTAG api
#ifdef NCSIM
  #ifdef HICANN_DESIGN
    typedef jtag_emulator myjtag_full;
  #else
    typedef eth_jtag_test myjtag_full;
  #endif
#else
typedef jtag_cmdbase_fpgadnchicann_full myjtag_full;
#endif

// unused macro
#ifdef UNUSED
#error macro UNUSED already defined
#endif
#define UNUSED(x) static_cast<void>((x))

#define STATIC_ASSERT(x) do{switch(0){case 0:case x:;}}while(false)

#endif  // _COMMON_H_

// -----------------------------------------------------------
//
// author: Stefan Philipp
// date  : Jan 2009
//
// nullstream.h
//
// dummy stream to depress outputs and inputs
//
// -----------------------------------------------------------

#ifndef _NULLSTREAM_
#define _NULLSTREAM_

#include <iostream>

struct nullostream: std::ostream
{
	struct nullbuf: std::streambuf { int overflow(int c) { return traits_type::not_eof(c); } } m_sbuf;
  nullostream(): std::ios(&m_sbuf), std::ostream(&m_sbuf) {}
};

extern nullostream NULLOS;

#endif // _NULLSTREAM_
