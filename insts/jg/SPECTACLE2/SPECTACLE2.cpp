// Copyright (C) 2005 John Gibson.  See ``LICENSE'' for the license to this
// software and for a DISCLAIMER OF ALL WARRANTIES.

/* SPECTACLE2 - FFT-based delay processor

   p0  = output start time
   p1  = input start time
   p2  = input duration
   p3  = output amplitude multiplier, spanning entire note, including ring-down
   p4  = input amplitude multiplier, spanning just the input duration
   p5  = ring-down duration

   p6  = FFT length (power of 2, usually 1024)
   p7  = window length (power of 2, usually FFT length * 2)
   p8  = window table (or zero for internally generated Hamming window)
   p9  = overlap - how much FFT windows overlap (positive power of 2)
         1: no overlap, 2: hopsize=FFTlen/2, 4: hopsize=FFTlen/4, etc.
         2 or 4 is usually fine; 1 is fluttery; higher overlaps use more CPU.

   The following three "control" tables define the modification of FFT bins.
   The two delay tables must be the same size.  If you pass a constant instead
   of a table, then it will work as if you created a table filled with that
   constant: it will affect all frequencies, not just the ones in the ranges
   defined below (p13-16).
   
   p10 = EQ table (i.e., amplitude scaling of each band), in dB (0 dB means
         no change, + dB boost, - dB cut).
   p11 = delay time table (seconds)
   p12 = delay feedback table.  Feedback is an amplitude multiplier for signal
         reentering the delay line from its output.  Feedback values greater
         than one usually will cause clipping in a short amount of time.

   The following parameters define the range within which the EQ, delay time and
   feedback tables function.  See note (3) below for more information.

   p13 = minimum EQ frequency [optional, default is 0 Hz]
   p14 = maximum EQ frequency [optional, default is Nyquist] 
         If zero, max. freq. will be set to the Nyquist frequency.

   p15 = minimum delay frequency [optional, default is 0 Hz]
   p16 = maximum delay frequency [optional, default is Nyquist] 
         If zero, max. freq. will be set to the Nyquist frequency.

   p17 = wet/dry mix (0: dry -> 1: wet) [optional, default is 1]
   p18 = input channel [optional, default is 0]
   p19 = pan (in percent-to-left format) [optional, default is .5]


   NOTES:

   1. p4 (input amp) can be a constant or a table, but not a real-time control 
      source.  p3 (output amp), p13 (min. EQ freq.), p14 (max. EQ freq.), p15
      (min. delay freq.), p16 (max. delay freq.), p17 (wet/dry mix) and p19
      (pan) can receive dynamic updates from a table or real-time control
      source.  Note that updating min. or max. freq. can be rather expensive.

      It is also possible to update the p10 (EQ), p11 (delay time) and p12
      (feedback) tables dynamically using modtable(table, "draw", ...).  If any
      of these are not tables, then the single constant or changing value will
      apply to all FFT bins.  (Useful if you want all bins to use the same
      feedback, for example.)

   2. Output begins after a brief period of time during which internal buffers
      are filling.  This time is the duration corresponding to the following
      number of sample frames:  window length - (fft length / overlap).  This
      duration is called "latency duration" below.

      Most updateable parameters begin at the start of the note, including the
      initial latency duration when the instrument plays silence, and act until
      the end of the note, including the ring-down duration.  The exception is
      the input envelope.  It begins after the latency duration and controls the
      time span given by p2 (input duration).

   3. Frequency ranges (p13-16) for EQ, delay time and feedback control tables

      One range (p13-14) governs the EQ; another (p15-16) governs the delay
      time and feedback together.  Here's how they work...

      If both min. and max. frequency values are zero (i.e., max. is Nyquist),
      and a "control" table (EQ, delay time or feedback) is sized to half the
      FFT length (p6), then the instrument behaves similarly to SPECTACLE v1.
      That is, each table element controls one FFT bin.  If the control table
      is larger than half the FFT length, then the extra values at the end of
      the table are ignored (and a warning printed).  If the table is less
      than half the FFT length, then the scheme described below applies.

      In all other cases, the first element of a table controls all FFT bins
      below and including the minimum frequency.  Successive table elements
      control groups of bins above this frequency.  The last element of the
      table controls all FFT bins at and above the maximum frequency.  So for
      the EQ table, you can think of the first table element as a low shelf
      (brick wall) filter cutoff frequency, and the last element as a high
      shelf filter cutoff frequency.  Interior elements are like peak/notch
      filters.  If the control tables have too many elements, then the extra
      values at the end of the table are ignored.

      If a control table is smaller than the number of FFT bins it affects,
      then the table elements are mapped to FFT bins in a particular way.
      The method used creates greater resolution for lower frequencies.  For
      example, if there are 512 FFT bins (i.e., half the FFT length), but
      a control table has only 32 elements, then there is a one-to-one mapping
      from table elements to bins for the lower frequencies.  For the higher
      frequencies, one table element might control 30 or more bins.

   John Gibson <johgibso at indiana dot edu>, 6/12/05.
*/

//#define DUMP
//#define DEBUG
#include "SPECTACLE2.h"
#include <float.h>

const float kMaxDelayTime = 20.0f;


// --------------------------------------------------------------- SPECTACLE2 --
SPECTACLE2::SPECTACLE2() : SPECTACLE2_BASE()
{
	_eqconst = _deltimeconst = _feedbackconst = 0.0f;
	_eq_bin_groups = NULL;
	_eqtablen = 0;
	_eq_minfreq = -FLT_MAX;
	_eq_rawmaxfreq = -FLT_MAX;
}


// -------------------------------------------------------------- ~SPECTACLE2 --
SPECTACLE2::~SPECTACLE2()
{
	// NB: we don't own the EQ, delay time and feedback tables.

	for (int i = 0; i < _half_fftlen; i++) {
		if (_mag_delay[i])
			delete _mag_delay[i];
		if (_phase_delay[i])
			delete _phase_delay[i];
	}

	delete [] _eq_bin_groups;
}


// -------------------------------------------------------------------- usage --
int SPECTACLE2::usage()
{
	return die(NULL,
		"Usage: %s(start, inskip, indur, amp, inamp, ringdur, fftlen, "
		"windowlen, windowtype, overlap, eqtable, delaytimetable, feedbacktable, "
		"eqminfreq, eqmaxfreq, delayminfreq, delaymaxfreq, wetpct, inchan, pan)",
		instname());
}


// ------------------------------------------------------------------ getargs --
// Grab initial values of pfields that are not dynamically updateable or that
// have optional non-zero values.  outskip, inskip, dur, out amp -- as well as
// verification for all others that are not specific to this subclass -- are
// handled in base class init.  ringdur init must wait until subinit, after
// FFT parameters are set.

int SPECTACLE2::getargs(double p[], int n_args)
{
	if (n_args < 13)
		return usage();

	set_iamp_pfield_index(4);
	set_fftlen(int(p[6]));
	set_window_len(int(p[7]));
	set_window_pfield_index(8);
	set_overlap(int(p[9]));
	set_wet(1.0f);			// default, maybe overridden in doupdate
	set_inchan(int(p[18]));
	set_pan(0.5f);			// default, maybe overridden in doupdate

	return 0;
}


// ------------------------------------------------------------------ subinit --
// Perform any initialization specific to this subclass, as well as ringdur.

int SPECTACLE2::subinit(double p[], int n_args)
{
	_eqtable = (double *) getPFieldTable(10, &_eqtablen);
	if (!_eqtable)
		_eqconst = p[10];

	int deltimetablen;
	_deltimetable = (double *) getPFieldTable(11, &deltimetablen);
	if (!_deltimetable)
		_deltimeconst = p[11];		// read later in this function

	int feedbacktablen;
	_feedbacktable = (double *) getPFieldTable(12, &feedbacktablen);
	if (!_feedbacktable)
		_feedbackconst = p[12];

	// Delay and feedback tables, if they exist, must be the same size.
	int cntltablen = 0;
	if (_deltimetable)
		cntltablen = deltimetablen;
	if (_feedbacktable) {
		cntltablen = feedbacktablen;
		if (_deltimetable && (feedbacktablen != deltimetablen))
			return die(instname(), "Delay time and feedback tables must be the "
		                          "same size.");
	}
	_control_table_size = cntltablen;

	// Init delay minfreq and maxfreq, so that bin groups will be ready for use
	// below.  This calls update_bin_groups, which reads _control_table_size.
	set_freqrange(p[15], p[16]);

	// Compute maximum delay lag and create delay lines for FFT magnitude
	// and phase values.  Make ringdur at least as long as the longest
	// delay time.  Remember that these delays function at the decimation
	// rate, not at the audio rate, so the memory footprint is not as large
	// as you would expect -- about 44100 samples per second at fftlen=1024,
	// overlap=2 and SR=44100.

	// Set max delay time and bounds-check initial state of delay time array.
	// Also increase ringdur to accommodate longest delay time, if necessary.
	// Note that if user updates delay time table while running, values are
	// pinned to max delay time without notification.

	_maxdelsamps = long(kMaxDelayTime * SR / float(_decimation) + 0.5);
	float maxtime = 0.0f;
	float deltime = _deltimeconst;
	for (int i = 0; i <= _half_fftlen; i++) {
		if (_deltimetable)
			deltime = _deltimetable[_bin_groups[i]];
		if (deltime < 0.0f || deltime > kMaxDelayTime)
			return die(instname(), "Delay times must be >= 0 and <= %g.",
			                                                 kMaxDelayTime);
		if (deltime > maxtime)
			maxtime = deltime;
	}
	float ringdur = p[5];
	if (ringdur < maxtime)
		ringdur = maxtime;	// but will still cut off any trailing feedback
	set_ringdur(ringdur);

	DPRINT3("decimation=%d, _maxdelsamps=%ld, ringdur=%g\n",
											_decimation, _maxdelsamps, ringdur);
	return 0;
}


// ------------------------------------------------------------- subconfigure --
int SPECTACLE2::subconfigure()
{
	_eq_bin_groups = new int [_half_fftlen + 1];

	for (int i = 0; i <= _half_fftlen; i++) {
		Odelay *delay = new Odelay(_maxdelsamps);
		if (delay == NULL)
			goto bad_alloc;
		_mag_delay[i] = delay;
		delay = new Odelay(_maxdelsamps);
		if (delay == NULL)
			goto bad_alloc;
		_phase_delay[i] = delay;
	}
	return 0;
bad_alloc:
	return die(instname(), "Not enough memory for delay lines.");
}


// --------------------------------------------------------- set_eq_freqrange --
// This works exactly like set_freqrange in base class, but for _eq_bin_groups.
void SPECTACLE2::set_eq_freqrange(float min, float max)
{
	if (min != _eq_minfreq || max != _eq_rawmaxfreq) {
		_eq_rawmaxfreq = max;
		if (max == 0.0f)
			max = _nyquist;
		if (max < min)
			_fswap(&min, &max);
		if (max > _nyquist)
			max = _nyquist;
		_eq_minfreq = _fclamp(0.0f, min, max);
		update_bin_groups(_eq_bin_groups, _eq_minfreq, max, _eqtablen);
	}
}


// ---------------------------------------------------------------- subupdate --
void SPECTACLE2::subupdate()
{
	// NB: update EQ, deltime, fdback tables, so that live table draw will work,
	// and so that we can retrieve changing monolithic values instead of tables.
	// NB: input amp is updated in base class.
	double p[20];
	update(p, 20, 1 << 3 | 1 << 10 | 1 << 11 | 1 << 12 | 1 << 13
	                     | 1 << 14 | 1 << 15 | 1 << 16 | 1 << 17 | 1 << 19);
	set_oamp(p[3]);
	set_eq_freqrange(p[13], p[14]);
	set_freqrange(p[15], p[16]);
	if (_nargs > 17)
		set_wet(p[17]);
	if (_nargs > 19)
		set_pan(p[19]);

	if (_eqtable == NULL)
		_eqconst = p[10];
	if (_deltimetable == NULL)
		_deltimeconst = p[11];
	if (_feedbacktable == NULL)
		_feedbackconst = p[12];
}


// ----------------------------------------------------------- dump_anal_bins --
void SPECTACLE2::dump_anal_bins()
{
#ifdef DUMP
	printf("\n----------------------------------------\n");
	for (int i = 0; i <= _half_fftlen; i++) {
		int index = i << 1;
		printf("%3d [%.3f Hz]:\tmag=%f, phs=%f\n",
					i,
					i * _fund_anal_freq,
					_anal_bins[index],
					_anal_bins[index + 1]);
	}
#endif
}


// ---------------------------------------------------------- modify_analysis --
void SPECTACLE2::modify_analysis(bool reading_input)
{
	DPRINT1("modify_analysis: .............. reading_input=%d\n", reading_input);
#ifdef DUMP
	dump_anal_bins();
#endif

	for (int i = 0; i <= _half_fftlen; i++) {
		int index = i << 1;

		float eq;
		if (_eqtable) {
			int bg = _eq_bin_groups ? _eq_bin_groups[i] : i;
			eq = _eqtable[bg];
		}
		else
			eq = _eqconst;

		// Delay time and feedback use base class bin groups array.
		const int bg = _bin_groups[i];

		float deltime = _deltimetable ? _deltimetable[bg] : _deltimeconst;
		deltime = _fclamp(0.0f, deltime, kMaxDelayTime);

		float mag = reading_input ? (_anal_bins[index] * _ampdb(eq)) : 0.0f;
		float phase = _anal_bins[index + 1];

		if (deltime == 0.0f) {
			_anal_bins[index] = mag;
			_anal_bins[index + 1] = phase;
		}
		else {
			float feedback = _feedbacktable ? _feedbacktable[bg] : _feedbackconst;

			long delsamps = long((deltime * SR) + 0.5) / _decimation;

			// Not sure why this is necessary, but without it, delayed copies
			// sound distorted...

			if (_overlap > 1) {
				int remainder = delsamps % _overlap;
				if (remainder)
					delsamps -= remainder;
			}

			float newmag = _mag_delay[i]->getsamp(delsamps);
			float newphase = _phase_delay[i]->getsamp(delsamps);
			_anal_bins[index] = newmag;
			_anal_bins[index + 1] = newphase;
			_mag_delay[i]->putsamp(mag + (newmag * feedback));

			if (reading_input)
				_phase_delay[i]->putsamp(phase);
			else
				_phase_delay[i]->putsamp(newphase);
		}
	}
}


// ----------------------------------------------------------- makeSPECTACLE2 --
Instrument *makeSPECTACLE2()
{
	SPECTACLE2 *inst;

	inst = new SPECTACLE2();
	inst->set_bus_config("SPECTACLE2");

	return inst;
}


// ---------------------------------------------------------------- rtprofile --
void rtprofile()
{
	RT_INTRO("SPECTACLE2", makeSPECTACLE2);
}


