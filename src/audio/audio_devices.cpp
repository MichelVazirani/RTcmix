/* RTcmix  - Copyright (C) 2000  The RTcmix Development Team
   See ``AUTHORS'' for a list of contributors. See ``LICENSE'' for
   the license to this software and for a DISCLAIMER OF ALL WARRANTIES.
*/
// audio_devices.cpp
// C++ functions for creating and configuring AudioDevice instances for input
// and output.
//

#include <globals.h>
#include <sndlibsupport.h>
#include <ugens.h>

#include "AudioFileDevice.h"
#include "audio_devices.h"

#if defined(LINUX)

#if MUS_LITTLE_ENDIAN
#define NATIVE_HW_FORMAT MUS_LSHORT
#else
#define NATIVE_HW_FORMAT MUS_BSHORT
#endif
#ifdef OSS
#include "OSSAudioDevice.h"
#elif defined(ALSA)
#include "ALSAAudioDevice.h"
#endif

#elif defined(MACOSX)

#include "OSXAudioDevice.h"
#define NATIVE_HW_FORMAT MUS_BFLOAT

#elif defined(SGI)

#include "SGIAudioDevice.h"
#define NATIVE_HW_FORMAT MUS_BFLOAT

#endif


#ifdef ALSA
#define FULL_DUPLEX_NOT_SUPPORTED
#endif

AudioDevice *globalInputDevice;			// Used by Minc/audioLoop.C
AudioDevice *globalOutputDevice;		// Used by Minc/audioLoop.C
AudioDevice *globalOutputFileDevice;

static const int numBuffers = 4;		// number of audio buffers to queue up

int
create_audio_devices(int recordAndPlay, int chans, float srate, int *buffersize)
{
	int status;
#ifdef FULL_DUPLEX_NOT_SUPPORTED
	/* Open audio input and output ports. */
	if (recordAndPlay) {
		AudioDevice *idevice = createAudioDevice();
		// Don't run thread for input device if output device is running too.
		int openMode = AudioFileDevice::Record | AudioDevice::Passive;
		if ((status = idevice->open(openMode,
									NATIVE_HW_FORMAT, 
									chans, 
									srate)) == 0)
		{
			int reqsize = *buffersize * numBuffers;
			if ((status = idevice->setQueueSize(&reqsize)) < 0) {
				die("rtsetparams",
					"Trouble setting audio input device queue size: %s",
					idevice->getLastError());
				return -1;
			}
			*buffersize = reqsize / numBuffers;
			// Passive start takes NULL callback and context.
			if (idevice->start(NULL, NULL) != 0) {
				die("rtsetparams", "Trouble starting input audio device: %s",
					idevice->getLastError());
				return -1;
			}		
		}
		else {
			die("rtsetparams", "Trouble opening input audio device: %s",
				idevice->getLastError());
			return -1;
		}
		globalInputDevice = idevice;
		recordAndPlay = false;	// reset so code below will not attempt duplex
	}
#endif	// FULL_DUPLEX_NOT_SUPPORTED

	AudioDevice *device = createAudioDevice();
	int openMode = (recordAndPlay) ?
						AudioDevice::RecordPlayback : AudioDevice::Playback;
	if ((status = device->open(openMode,
	  						   NATIVE_HW_FORMAT,
							   chans,
							   srate)) == 0)
	{
		int reqsize = *buffersize * numBuffers;
		if ((status = device->setQueueSize(&reqsize)) < 0) {
			die("rtsetparams",
				"Trouble setting audio output device queue size: %s",
				device->getLastError());
			return -1;
		}
		*buffersize = reqsize / numBuffers;
	}
	else
	{
		die("rtsetparams", "Trouble opening audio device: %s", 
			device->getLastError());
		return -1;
	}

	globalOutputDevice = device;

	if (recordAndPlay)
		globalInputDevice = device;
	return status;
}

int create_audio_file_device(const char *outfilename,
							 int header_type,
							 int sample_format,
							 int chans,
							 float srate,
							 int normalize_output_floats,
							 int check_peaks,
							 int play_audio_too)
{
	// Pass global options into the device.
	
	int fileOptions = 0;
	if (normalize_output_floats)
		fileOptions |= AudioFileDevice::NormalizeFloats;
	if (check_peaks)
		fileOptions |= AudioFileDevice::CheckPeaks;

	AudioFileDevice *fileDevice = new AudioFileDevice(outfilename,
													  header_type,
													  sample_format,
													  fileOptions);
													
	int openMode = AudioFileDevice::Playback;
	if (play_audio)
		openMode |= AudioDevice::Passive;	// Don't run thread for file device.

	int ret = fileDevice->open(openMode, MUS_LFLOAT, chans, srate);
	
	if (ret == -1) {
		rterror("rtoutput", "Can't create output for \"%s\": %s", 
				 outfilename, fileDevice->getLastError());
		return -1;
	}
	// Cheating -- should hand in queue size as argument!
	int queueSize = RTBUFSAMPS;
	ret = fileDevice->setQueueSize(&queueSize);
	if (ret == -1) {
		rterror("rtoutput", "Failed to set queue size on file device:  %s", 
				 fileDevice->getLastError());
		return -1;
	}
	if (play_audio) {
		// Passive start takes NULL callback and context.
		if (fileDevice->start(NULL, NULL) == -1) {
			rterror("rtoutput", "Can't start file device: %s", 
				 	fileDevice->getLastError());
			return -1;
		}
	}

	if (print_is_on) {
		 printf("Output file set for writing:\n");
		 printf("      name:  %s\n", outfilename);
		 printf("      type:  %s\n", mus_header_type_name(header_type));
		 printf("    format:  %s\n", mus_data_format_name(sample_format));
		 printf("     srate:  %g\n", srate);
		 printf("     chans:  %d\n", chans);
	}

	// If we are only writing to disk, we only have a single output device.  
	// When all this is finished, we should be able to "play" to file, with 
	// no distinction in the RTcmix code between to-disk and to-audio-hw.
	
	globalOutputFileDevice = fileDevice;
	if (!play_audio_too) {
		globalOutputDevice = fileDevice;
	}
	return 0;
}

int audio_input_is_initialized()
{
	return globalInputDevice != NULL;
}

void
destroy_audio_devices()
{
	// Delete input device if independent of output device.
	
 	if (globalInputDevice != globalOutputDevice) {
		delete globalInputDevice;
		globalInputDevice = NULL;
	}

	globalOutputDevice->close();
	
	if (globalOutputFileDevice == globalOutputDevice) {
		delete globalOutputDevice;
		globalOutputFileDevice = NULL;	// Dont delete elsewhere.
	}
	globalOutputDevice = NULL;
}

int
destroy_audio_file_device()
{
	int result = 0;
	if (globalOutputFileDevice) {
    	result = globalOutputFileDevice->close();
	}
                                                                                                    
	if (globalOutputFileDevice != globalOutputDevice) {
    	delete globalOutputFileDevice;
	}
	globalOutputFileDevice = NULL;

	return 0;
}