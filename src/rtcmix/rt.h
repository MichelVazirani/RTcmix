#ifndef __RT_H__
#define __RT_H__

class Instrument;

struct rt_item {
	struct rt_item *rt_next;
	Instrument* (*rt_ptr)();
	char *rt_name;
	};

extern rt_item *rt_list;

void heapify(Instrument *Iptr);
int addrtInst(rt_item*);

extern "C" {
    void merror(char*);
	void rtprofile();
}

#ifndef NULL
#define NULL 0
#endif

#define RT_INTRO(flabel, func) \
	{ extern Instrument* func(); \
		static rt_item this_rt = { NULL, func, flabel }; \
		if (addrtInst(&this_rt) == -1) \
		  merror(flabel); \
	}

#endif	// __RT_H__
