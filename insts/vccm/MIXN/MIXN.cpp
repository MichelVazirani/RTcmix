#include <iostream.h>
#include <unistd.h>
#include <stdio.h>
#include <mixerr.h>
#include <Instrument.h>
#include <notetags.h>
#include <rt.h>
#include <rtdefs.h>
#include "MIXN.h"

extern int resetval;

extern "C" {
#include <ugens.h>
  loc *aud_locs=NULL;
  pt *spk_locs=NULL;
  rfact *ratefs=NULL;
  int num_rates=0;
  int cur_rate=0;
  int cur_point=0;
  int num_points;
  float *out_chan_amp; /* Used by inst */
  double tot_dist;
  int n_spk;
  Bool use_path;
  Bool use_rates;
  double cycle;  /* Length of 1 iteration ... last path time */
  void update_amps(long);
}


MIXN::MIXN() : Instrument()
{
  in = NULL;
  my_aud_locs = aud_locs;
  my_spk_locs = spk_locs;
  my_ratefs = ratefs;
  my_num_rates = num_rates;
  my_cur_rate = cur_rate;
  my_cur_point = cur_point;
  my_num_points = num_points;
  my_out_chan_amp = new float[MAXBUS];
  my_tot_dist = tot_dist;
  my_n_spk = n_spk;
  my_use_path = use_path;
  my_use_rates = use_rates;
  my_cycle = cycle;
}

MIXN::~MIXN()
{
  delete [] in;
}

int MIXN::init(float p[], int n_args)
{
  // p0 = outsk
  // p1 = insk
  // p2 = duration (-endtime)
  // p3 = inchan
  // p4-n = channel amps
  // we're (still) stashing the setline info in gen table 1

  int i;

  Instrument::init(p, n_args);

  if (p[2] < 0.0) p[2] = -p[2] - p[1];

  outskip = p[0];
  inskip = p[1];
  dur = p[2];

  nsamps = rtsetoutput(outskip, dur, this);
  rtsetinput(inskip, this);

  inchan = p[3];
  amp = p[4];
	
  amp_count = 0;
  for (i=0; i < outputchans; i++) {
	my_out_chan_amp[i] = 0.0;
  }

  // Make sure this works with trajectories, etc...
  for (i = 0; i < n_args-5; i++) {
	my_out_chan_amp[i] = p[i+5];
	if (i > outputchans) {
	  fprintf(stderr, "You wanted output channel %d, but have only "
			  "specified %d output channels\n",
			  i, outputchans);
	  exit(-1);
	}
  }
  amp_count = i;  // Watch out for this one

  amptable = floc(1);
  if (amptable) {
	int amplen = fsize(1);
	tableset(dur, amplen, tabs);
  }
  else
	printf("Setting phrase curve to all 1's\n");

  skip = (int)(SR/(float)resetval); // how often to update amp curve, default 200/sec.

  return(this->mytag);
}

int MIXN::run()
{
  int i,j,k,outframes;
  float aamp;
  int branch;
  float *outp;
  float t_out_amp[8];  /* FIXME make this more flexible later */
  float t_dur;
  int finalsamp;

  aamp = 0;
  if (in == NULL)    /* first time, so allocate it */
    in = new float [RTBUFSAMPS * inputchans];

  Instrument::run();

  aud_locs = my_aud_locs;
  spk_locs = my_spk_locs;
  ratefs = my_ratefs;
  num_rates = my_num_rates;
  cur_rate = my_cur_rate;
  cur_point = my_cur_point;
  num_points = my_num_points;
  out_chan_amp = my_out_chan_amp;
  tot_dist = my_tot_dist;
  n_spk = my_n_spk;
  use_path = my_use_path;
  use_rates = my_use_rates;
  cycle = my_cycle;

  outframes = chunksamps*inputchans;

  rtgetin(in, this, outframes);

  outp = outbuf;  // Use private pointer to Inst::outbuf

  branch = 0;
  for (i = 0; i < outframes; i += inputchans)  {
	if (--branch < 0) {
	  if (tags_on) {
		for (j=0;j<8;j++) {
		  t_out_amp[j] = rtupdate(this->mytag,j+4);
		  if (t_out_amp[j] != NOPUPDATE) {
			out_chan_amp[j] = t_out_amp[j];
		  }
		}
	  }
	  if (amptable)
		aamp = tablei(cursamp, amptable, tabs) * amp;
	  else
		aamp = amp;
	  if (use_path) {
		update_amps(cursamp);
	  }
	  branch = skip;
	}

	float sig = in[i + (int)inchan] * aamp;

	for (j = 0; j < outputchans; j++) {
	  outp[j] = sig * out_chan_amp[j];
	}
	outp += outputchans;
	cursamp++;
  }

  my_cur_rate = cur_rate;
  my_cur_point = cur_point;

  return i;
}

Instrument*
makeMIXN()
{
  MIXN *inst;

  inst = new MIXN();
  inst->set_bus_config("MIXN");
  inst->set_instnum("MIXN");
  return inst;
}

void
rtprofile()
{
  RT_INTRO("MIXN",makeMIXN);
}

