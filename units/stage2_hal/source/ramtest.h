// -------------------------------------------
//
// Stefan Philipp, Electronic Vision(s)
//
// ramtest
//
// * Implements a random read/write testing
//   of distant memories with universal interface.
// * Able to handle large delays between read
//   functions and the arrival of read data!
//
// USAGE:
//	* instanciate object with read/write function
//  * execute Test() function
//
// see example in main()
//
// -------------------------------------------

#ifndef RAMTEST_H
#define RAMTEST_H

#include <vector>
#include <queue>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include "nullstream.h"

#define RANDOM(size) (uint)((uint)(size)*(1.0*rand()/(RAND_MAX+1.0)));


// -------------------------------------------
// ramtest interface abstract
// -------------------------------------------

template <class data_t> class RamTestIntf
{
protected:
	std::string classname;
	std::ostream & rtdbg() { if (debug) return std::cout << classname << ": "; else return NULLOS;}
	void overr(std::string fname) { rtdbg() << "'" << fname << "' not overwritten ..." << std::endl; }
public:
	bool debug;
	RamTestIntf() : classname("mem_intf"), debug(false) {};
	virtual ~RamTestIntf() {};

	void _clear() { std::cout<<"clearing memory"<<std::endl; clear(); };
	void _write(int addr, data_t value) { rtdbg()<<"writing "<<value<<" to "<<addr<<std::endl; write(addr,value); };
	void _read(int addr) { rtdbg()<<"read from "<<addr<<std::endl; read(addr); };
	int  _rdata(data_t& value) { int r= rdata(value); if(r) rtdbg()<<"read data "<<value<<std::endl; return r;}

	virtual void clear() =0;
	virtual void write(int addr, data_t value) =0;
	virtual void read(int addr) =0;
	virtual bool rdata(data_t& value) =0;

	virtual void random(data_t& value, uint dsize) { overr("random()"); };
	virtual void first(data_t& value) { overr("first()"); };
	virtual void next(data_t& value, uint dsize) { overr("next()"); };
};


// -------------------------------------------
// memory with delayed access
// -------------------------------------------

template <class data_t> class TestMemory : public virtual RamTestIntf<data_t>
{
protected:
	int size;
	std::vector<data_t> memory;
	std::queue<data_t> rqueue;
public:
	TestMemory(int mem_size) { /*classname="mem";*/ size=mem_size; memory=std::vector<data_t>(size); };
	virtual ~TestMemory() { }

	void resize(int size) { data_t f; first(f); memory.resize(size, f); }
	void dump() { for(uint i=0; i<memory.size();i++) std::cout << i <<": "<< memory[i]<<std::endl; }
	virtual void clear() { data_t f; this->first(f); memory=std::vector<data_t>(size, f); }
	virtual void write(int addr, data_t value) { memory[addr] = value; }
	virtual void read(int addr) { rqueue.push(memory[addr]); }
	virtual bool rdata(data_t& value)
	{
		if(rqueue.empty()) return false;
		value = rqueue.front();	rqueue.pop();
		return true;
	} ;
};


// -------------------------------------------
// ramtest class
// -------------------------------------------

#define llu long long unsigned

template <class data_t, class mem_t = TestMemory<data_t> > class RamTest
{
protected:
	RamTestIntf<data_t> *mem;
	int mem_base;
	int mem_size;
	llu data_size;
	int last_rand;
	bool debug;
public:
	mem_t compare;
	enum MODE {incremental, random};
	RamTest(RamTestIntf<data_t> *m, int mb=0, int mz=0, llu dz=0) : mem(m), compare(mz) {
		mem_base=mb; mem_size=mz; data_size=dz; debug=false;
	};
	~RamTest() { };
	void SetDebug(bool d) { mem->debug=d; debug=d; }
	void Clear(){ mem->_clear(); compare._clear();}
	void Resize(int mb, int mz, llu dz) {compare.resize(mz); mem_base=mb; mem_size=mz; data_size=dz; }

	// init target/compare memory, write only
	void Init(int count, MODE mode=random, int seed=-1) { Test(count, mode, seed, true); }

	// test memory, compare local
	int Test(int count, MODE mode=random, int seed=-1, bool initcompare=false)
	{
		int pass=0, cmd, pos=0, addr=0; data_t value, cdata, rdata; bool cvalid=false, rvalid=false;
		mem->first(value);

		if (mode==random) { if (seed>=0) srand(seed); else srand(last_rand); }
 
		while ((count==-1) || (pos<count) || cvalid)
		{
			if ((count==-1) || (pos<count)) {
				if (mode==random) {
					cmd  = RANDOM(2);
					addr = RANDOM(mem_size);
				}
				else cmd  = pass;
				if (!cmd) {
					if (mode==random) mem->random(value, data_size);
					if (!initcompare) mem->_write(mem_base+addr, value);
					compare._write(addr, value);
				}
				else if (!initcompare) {
					mem->_read(mem_base+addr);
					compare._read(addr);
					cvalid = compare._rdata(cdata);
					rvalid = mem->_rdata(rdata);
				}
			}
			if (rvalid) {
				if (cvalid) {
					std::cout << std::hex << "ramtest: verifying at addr 0x" << addr << " 0x" << cdata << " against 0x" << rdata << "...";
					if (rdata==cdata) {
						if (debug) std::cout << " OK" << std::endl;
						rvalid = cvalid = false;
					}
					else {
						std::cout << "ramtest: ERROR: read back 0x" << std::hex << rdata << ", expected 0x" << cdata << std::endl;
						return 0;
					}
				}
				else {
					std::cout << "ramtest: ERROR: read back 0x" << std::hex << rdata << " without data expected." << std::endl;
					return 0;
				}
			} else {
				std::cout << "ramtest: ERROR: memory read failure." << std::endl;
				return 0;
			}
			pos++;
			if (mode==incremental) {
				if ((pos==count) && !pass) { 
					pos=0; addr=0; mem->first(value);
					pass++;
				}
				else {
					addr=(addr+1) % mem_size;
					mem->next(value, data_size);
				}
			}
		}

		// ensure ramtests have deterministic numbers independent of other random processes
		if (mode==random) last_rand=rand();
		return 1;
	}
};


// -------------------------------------------
// pre-made instances for mem-type uint
// -------------------------------------------

class RamTestIntfUint : public virtual RamTestIntf<uint> {
public:
	void random(uint& value, uint dsize) { value=RANDOM(dsize); };
	void first(uint& value) { value=0; };
	void next(uint& value, uint dsize) { value=(value+1) % dsize; };
};

class TestMemoryUint : public TestMemory<uint>, public RamTestIntfUint {
public:
	TestMemoryUint(int mem_size) : TestMemory<uint>(mem_size) { };
};

class RamTestUint : public RamTest<uint, TestMemoryUint> {
public:
	RamTestUint(RamTestIntfUint *m, int mb, int mz, llu dz) : RamTest<uint, TestMemoryUint>(m, mb, mz, dz) {};
	RamTestUint(RamTestIntfUint *m) : RamTest<uint, TestMemoryUint>(m) { };
};


// -------------------------------------------
// pre-made instances for mem-type std::vector
// -------------------------------------------

// you need to define a std::vector debug "<<" operator in your source(!)
// this is not done here since it is nothing to do with a ramtest, but only required
//std::ostream & operator << (std::ostream &o, const std::vector<uint> &v) {
//	o<<"("; if (v.size()) o<<v[0]; for (uint i=1; i<v.size(); i++) o << "," << v[i]; return o << ")"; }

class RamTestIntfVecUint : public virtual RamTestIntf<std::vector<uint> >{
public:
	int vec_size;
	void random(std::vector<uint>& value, uint dsize) { for(uint i=0;i<value.size();i++) value[i]=RANDOM(dsize); }
	void first(std::vector<uint>& value) { value=std::vector<uint>(vec_size); for(uint i=0;i<value.size();i++) value[i]=i; }
	void next(std::vector<uint>& value, uint dsize) { for(uint i=0;i<value.size();i++) value[i]=(value[i]+vec_size)%dsize; }
};

class TestMemoryVecUint : public TestMemory<std::vector<uint> >, public RamTestIntfVecUint {
public:
	TestMemoryVecUint(int mem_size) : TestMemory<std::vector<uint> >(mem_size) { }
};

class RamTestVecUint : public RamTest<std::vector<uint>, TestMemoryVecUint>{
public:
	RamTestVecUint(RamTestIntfVecUint *m, int mb, int mz, int vz, llu dz) :
		RamTest<std::vector<uint>, TestMemoryVecUint>(m, mb, mz, dz) { m->vec_size=vz; compare.vec_size=vz; };
	RamTestVecUint(RamTestIntfVecUint *m, int vz) :
		RamTest<std::vector<uint>, TestMemoryVecUint>(m) { m->vec_size=vz; compare.vec_size=vz; };
};


// -------------------------------------------
// testbench
// -------------------------------------------

/*
int main(int argc,  char *argv[])
{
	int mem_size  = 16;
	int data_size = 10;

	TestMemory mem(mem_size);
	RamTest ramtest(&mem, 0, mem_size, data_size);
	mem.debug = true;
	ramtest.SetDebug(true);

	return ramtest.Test(25);
}
*/

#endif
