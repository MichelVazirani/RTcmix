/* RTcmix  - Copyright (C) 2001  The RTcmix Development Team
   See ``AUTHORS'' for a list of contributors. See ``LICENSE'' for
   the license to this software and for a DISCLAIMER OF ALL WARRANTIES.

   Apr 2004: Converted from C source to C++ source by DS -- see deleted .c file
   for change history.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>  /* for strerror */
#include <math.h>    /* for fabs */
#include <errno.h>
#include <globals.h>
#include <prototypes.h>
#include "AudioDevice.h"
#include "audio_devices.h"

/* --------------------------------------------------------- rtwritesamps --- */
/* Write the current output buffers to an interleaved buffer, and send it to
   the fileDevice.  Currently the AudioDevice supports 16-bit and 24-bit signed
   integer and 32-bit floating-point files, in both byte orders.  Floats have
   a scaling option that forces the normal range of values to fall between 
   -1.0 and 1.0 (set at file device creation time).
*/

static BUFTYPE	*interleavedBuf;

int
rtwritesamps(AudioDevice *fileDevice)
{
	const int nframes = RTBUFSAMPS;
	const int nchans = NCHANS;

	if (interleavedBuf == NULL) {
		interleavedBuf = new BUFTYPE[8192*nchans];
		if (interleavedBuf == NULL) {
			fprintf(stderr, "rtwritesamps: failed to allocate interleave buffer\n");
			exit(1);
		}
	}

	// Interleave output buffers before handing to outputDevice.
	
	for (int n = 0; n < nchans; n++) {
		BufPtr iBuf = &interleavedBuf[n];
		BufPtr bp = out_buffer[n];
		for (int i = 0; i < nframes; i++) {
			*iBuf = bp[i];
			iBuf += nchans;
		}
	}

   /* This catches our new case where rtoutput() failed but was ignored */
   if (rtfileit < 0) {
      fprintf(stderr, "rtwritesamps: No output file open (rtoutput failed).\n");
      exit(1);
   }
   
   int framesWritten = fileDevice->sendFrames(interleavedBuf, nframes);
   
   if (framesWritten == -1) {
      fprintf(stderr, "rtwritesamps error: %s\n", fileDevice->getLastError());
      exit(1);
   }

   return 0;
}

/* ----------------------------------------------------------- rtcloseout --- */
/* Close the output file device. */
int
rtcloseout()
{
	int result = 0;

	if (rtfileit != 1)          /* nothing to close */
		return 0;

	delete [] interleavedBuf;

	result = destroy_audio_file_device();

	if (result == -1) {
		fprintf(stderr, "rtcloseout: Error closing \"%s\" (%s)\n",
				rtoutsfname, strerror(errno));
		return -1;
	}

	return 0;
}


