#include <math.h>
#include <ugens.h>
#include <dbug.h>

void rsnset(float cf, float bw, float scl, float xinit, float a[])
{
	float c,temp;
	float tval1,tval2,tval3,tval4;

	c=temp=tval1=tval2=tval3=tval4=0;

	if(!xinit) {
		a[4] = 0;
		a[3] = 0;
		}

	a[2] = (float)exp(-PI2 * bw/SR);
	temp = 1. - a[2];
	c = a[2] + 1;
	a[1] = 4. * a[2]/c * (float)cos(PI2 * cf/SR);
	if(scl < 0) a[0] = 1;
	if(scl) a[0] = sqrt(temp/c*(c*c-a[1]*a[1]));
	if(!scl) a[0] = temp*sqrt(1.-a[1]*a[1]/(4.*a[2]));
}
