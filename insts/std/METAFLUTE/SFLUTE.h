#include "metaflute.h"

class SFLUTE : public Instrument {
	int dl1[2],dl2[2];
	float del1[DELSIZE],del2[DELSIZE];
	int length1,length2;
	float amp,namp,dampcoef,oldsig;
	double *amparr, *oamparr;
	float amptabs[2], oamptabs[2];
	float ampmult, spread;
	int skip;

public:
	SFLUTE();
	int init(double*, int);
	int run();
	};
