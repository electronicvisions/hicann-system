#ifndef __PULSE_FLOAT_H
#define __PULSE_FLOAT_H

// needed for writePulses()
struct pulse_float_t
{
    unsigned int id;
    double time;

    pulse_float_t() :
    	id(0),
    	time(0.0)
    {};

    pulse_float_t(unsigned int set_id, double set_time) :
    	id(set_id)
    	, time(set_time)
    {};

    bool operator< (const pulse_float_t &p) const { return ((this->time<p.time)||((this->time==p.time)&&(this->id<p.id)));}
    bool operator==(const pulse_float_t &p) const { return ((this->time==p.time)&&(this->id==p.id));}
};


#endif // __PULSE_FLOAT_H
