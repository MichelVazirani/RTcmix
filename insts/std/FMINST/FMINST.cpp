/* FMINST -- simple FM instrument
 
   p0 = start time
   p1 = duration
   p2 = amp *
   p3 = frequency of carrier (Hz or oct.pc **)
   p4 = frequency of modulator (Hz or oct.pct)
   p5 = FM index low point
   p6 = FM index high point
   p7 = pan (in percent-to-left form: 0-1) [optional; default is 0]
   p8 = reference to wavetable [optional; if missing, must use gen 2 ***]
   p9 = index guide [optional; if missing, must use gen 3 ****]

   p2 (amplitude), p3 (carrier freq), p4 (modulator freq), p5 (index low),
   p6 (index high), p7 (pan) and p9 (index guide) can receive dynamic updates
   from a table or real-time control source.

   * If an old-style gen table 1 is present, its values will be multiplied
   by p2 (amp), even if the latter is dynamic.

   ** oct.pc format generally will not work as you expect for p3 and p4
   (osc freq) if the pfield changes dynamically.  Use Hz instead in that case.

   *** If p8 is missing, you must use an old-style gen table 2 for the
   oscillator waveform.

   **** If p9 is missing, you must use an old-style gen table 3 for the
   index guide function.

                                                rev for v4, JGG, 7/12/04
*/
#include <stdlib.h>
#include <stdio.h>
#include <ugens.h>
#include <Instrument.h>
#include <PField.h>
#include "FMINST.h"
#include <rt.h>
#include <rtdefs.h>

#define AMP_GEN_SLOT     1
#define WAVET_GEN_SLOT   2
#define INDEX_GEN_SLOT   3


FMINST::FMINST() : Instrument()
{
	indexenv = NULL;
	branch = 0;
}

FMINST::~FMINST()
{
	delete carosc;
	delete modosc;
}

int FMINST::init(double p[], int n_args)
{
	nargs = n_args;
	float outskip = p[0];
	float dur = p[1];

	if (rtsetoutput(outskip, dur, this) == -1)
		return DONT_SCHEDULE;
	if (outputChannels() > 2)
		return die("FMINST", "Can't handle more than 2 output channels.");

	carfreqraw = p[3];
	if (carfreqraw < 15.0)
		carfreq = cpspch(carfreqraw);
	else
		carfreq = carfreqraw;

	modfreqraw = p[4];
	if (modfreqraw < 15.0)
		modfreq = cpspch(modfreqraw);
	else
		modfreq = modfreqraw;

	double *wavetable = NULL;
	int tablelen = 0;
	if (n_args > 8) {      // handle table coming in as optional p8 TablePField
		wavetable = (double *) getPFieldTable(8, &tablelen);
	}
	if (wavetable == NULL) {
		wavetable = floc(WAVET_GEN_SLOT);
		if (wavetable == NULL)
			return die("FMINST", "Either use the wavetable pfield (p8) or make "
                    "an old-style gen function in slot %d.", WAVET_GEN_SLOT);
		tablelen = fsize(WAVET_GEN_SLOT);
	}

	carosc = new Ooscili(SR, carfreq, wavetable, tablelen);
	modosc = new Ooscili(SR, modfreq, wavetable, tablelen);

	if (n_args < 10) {		// no p9 guide PField, must use gen table
		indexenv = floc(INDEX_GEN_SLOT);
		if (indexenv == NULL)
			return die("FMINST", "Either use the index guide pfield (p9) or make "
                    "an old-style gen function in slot %d.", INDEX_GEN_SLOT);
		int len = fsize(INDEX_GEN_SLOT);
		tableset(SR, dur, len, indtabs);
	}

	ampenv = floc(AMP_GEN_SLOT);
	if (ampenv) {
		int lenamp = fsize(AMP_GEN_SLOT);
		tableset(SR, dur, lenamp, amptabs);
	}

	skip = (int) (SR / (float) resetval);

	return nSamps();
}

void FMINST::doupdate()
{
   double p[10];
   update(p, 10);

	amp = p[2];
	if (ampenv)
		amp *= table(currentFrame(), ampenv, amptabs);

	if (p[3] != carfreqraw) {
		carfreqraw = p[3];
		if (carfreqraw < 15.0)
			carfreq = cpspch(carfreqraw);
		else
			carfreq = carfreqraw;
	}
	if (p[4] != modfreqraw) {
		modfreqraw = p[4];
		if (modfreqraw < 15.0)
			modfreq = cpspch(modfreqraw);
		else
			modfreq = modfreqraw;
		modosc->setfreq(modfreq);
	}

	float minindex = p[5];
	float maxindex = p[6];
	if (minindex > maxindex) {		// swap if wrong order
		float tmp = minindex;
		minindex = maxindex;
		maxindex = tmp;
	}
	float guide;
	if (nargs > 9)			// guide pfield is present
		guide = p[9];
	else                 // backward-compatible gen table
		guide = tablei(currentFrame(), indexenv, indtabs);
	float index = minindex + ((maxindex - minindex) * guide);
	peakdev = index * modfreq;

	spread = p[7];
}

int FMINST::run()
{
	for (int i = 0; i < framesToRun(); i++) {
		if (--branch <= 0) {
			doupdate();
			branch = skip;
		}

		float out[2];

		float modsig = modosc->next() * peakdev;
		carosc->setfreq(carfreq + modsig);
		out[0] = carosc->next() * amp;

		if (outputChannels() == 2) {
			out[1] = (1.0 - spread) * out[0];
			out[0] *= spread;
		}

		rtaddout(out);
		increment();
	}
	return framesToRun();
}

Instrument *makeFMINST()
{
	FMINST *inst;

	inst = new FMINST();
	inst->set_bus_config("FMINST");

	return inst;
}

void rtprofile()
{
	RT_INTRO("FMINST",makeFMINST);
}

