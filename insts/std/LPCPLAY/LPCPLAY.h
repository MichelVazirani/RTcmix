#include <Instrument.h>      /* the base class for this instrument */
#include "lp.h"

#define MAXVALS 2000

class DataSet;

class LPCINST : public Instrument {
public:
	LPCINST(const char *name);
	virtual ~LPCINST();
	virtual int init(float	*, int);
	virtual int configure();
	
protected:
	class WarpFilter
	{
	public:
		WarpFilter();
		float set(float warp, float *c, int npoles);
		void run(float *sig, float warp, float *c, float *out, int nvals);
	private:
		int _npoles;
		float _cq;
		float _outold;
		float	_past[MAXPOLES*2];
	};
	WarpFilter	_warpPole;

	const char *name() { return _functionName; }
	
protected:
	virtual int 	localInit(float *, int) = 0;
	virtual void	SetupArrays(int frameCount) = 0;

	// These are set via external routines and copied in during init.
	DataSet	*_dataSet;

	// These are set and used within subclasses.
	float	_amp;
	int		_nPoles;
	float	_frames, _frameno;
	int		_frame1;
	float	_ampmlt;
	float	_warpFactor;
	float	_coeffs[MAXPOLES+4];
	float	_past[MAXPOLES*2];
	int		_arrayLen;
	float	*_alpvals,*_buzvals;	// signal arrays
	long	_jcount;
	int		_counter;
	bool	_autoCorrect;
	int		_leftOver, _savedOffset;

private:
	const char *_functionName;
};

class LPCPLAY : public LPCINST {
public:
	LPCPLAY();
	virtual ~LPCPLAY();
	virtual int run();

protected:
	virtual int 	localInit(float *, int);
	virtual void	SetupArrays(int frameCount);
	double	getVoicedAmp(float err);
	float	weight(float frame1, float frame2, float thresh);
	float	deviation(float frame1, float frame2, float weight, float thresh);
	void	adjust(float actdev, float desdev, float actweight, 
				   float *pchval, float framefirst, float framelast);
	void	readjust(float maxdev, float *pchval, 
				  	 float firstframe, float lastframe, 
				  	 float thresh, float weight);
private:
	double	_lowthresh, _highthresh;
	float	_maxdev;
	float	_perperiod;
	float	_cutoff;							// amp cutoff level
	float	_hnfactor;							// harmonic count multiplier
	float	_thresh, _randamp;
	bool	_unvoiced_rate;
	float	_risetime, _decaytime;				// enveloping

	// These are set and used within LPCPLAY.
	float	_pitch;
	float	_transposition;
	float	_cf_fact, _bw_fact;
	bool	_voiced, _reson_is_on;
	float	_evals[5];
	float	_rsnetc[9];
	float	*_pchvals;						// pitch table
	float	*_noisvals;						// signal arrays
	float	_tblvals[2];
	float	_srd2, _phs, _magic;
	float	*_sineFun, *_envFun;
	long	_jcount;
	int		_counter, _datafields;
};

class LPCIN : public LPCINST {
public:
	LPCIN();
	virtual ~LPCIN();
	virtual int run();

protected:
	virtual int		localInit(float *, int);
	virtual void	SetupArrays(int frameCount);

private:
	BUFTYPE 	*_inbuf;
	int			_inChannel;
};
