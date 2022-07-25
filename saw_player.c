
/* 
 * plays a stereo saw wave for about 15 seconds
 * based on nagorni tutorial/example, and tranter code
 *
 * compile with gcc saw_player.c -o saw_player -Wall -lasound
 * (-Wall is optional...)
 */

/* use the newer alsa api */
#define ALSA_PCM_NEW_HW_PARAMS_API

#include <alsa/asoundlib.h>


int main(){

	//long loops; //only used for timer
	int rc; //overwritten periodically with new command to check for errors
	int size; //container for frames * 4 (2 chan, 2 bytes/sample)
	
	//handle for pcm device
	snd_pcm_t *handle;
        //structure that contains hardware info
	//can be used to specify config to be used for pcm stream
	snd_pcm_hw_params_t *params;
	unsigned int val;
	int dir;
	snd_pcm_uframes_t frames; //called 'periodsize' in nagorni example
	char *buffer;


	/* open pcm device for playback 
	 * can only set to playback OR stream
	 *
	 * in this case, int rc holds the command so it can be evaluated to 
	 * true or false, and throw error if false
	 *
	 * plughw is like a general interface
	 * useful because data convered if device does not support
	 *
	 * can also create variables to hold pcm name and stream type BUT
	 * untested in other systems!
	 * 
	 * commented out method should work in all systems (Arch and ubuntu)
	 * */
	
	char *pcm_name;
	pcm_name = strdup("plughw:0,0");
	snd_pcm_stream_t pcm_stream_type = SND_PCM_STREAM_PLAYBACK;

	//rc = snd_pcm_open(&handle, "plughw:0,0", SND_PCM_STREAM_PLAYBACK, 0);

	rc = snd_pcm_open(&handle, pcm_name, pcm_stream_type, 0);

	if(rc < 0){
		fprintf(stderr, "ERR: unable to open pcm device: %s\n", snd_strerror(rc));
		exit(1);
	}

	/*Allocate a hardware parameters object 
	 * 
	 * order is reversed from Nagorni example
	 * so, apparently, can open pcm device prior to 
	 * allocating the snd_pcm_hw_params_t structure (params) on the stack
	 *
	 * */
	snd_pcm_hw_params_alloca(&params);

	/*Set the desired hardware parameters 
	 *
	 * need to specify access type, sample format, sample rate, number of channels,
	 * number of periods, and period size
	 *
	 * snd_pcm_hw params_any initializes params with full configuration space
	 * of the soundcard
	 */
	snd_pcm_hw_params_any(handle, params);

	/* Interleaved mode 
	 *
	 * snd_pcm_hw_params_set_access() specifies the way multichannel
	 * data is stored in the buffer
	 *
	 * interleaved means the buffer contains alternating "words" of
	 * sample date for the left and right channels
	 *
	 * NONINERLEAVED would mean each period contains first all sample data
	 * for first channel followed by sample data for second channel, etc.
	 *
	 * */
	snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);

	/* Signed 16-bit little edian format 
	 *
	 * NOTE:the original setting for the code was 
	 * 	16-bit. it sounds kind of cruddy,
	 * 	and could set to 24-bit, but maybe not
	 * 	needed
	 *
	 * 	BUT!! if you change this, you have to change all other
	 * 	parameters to match
	 * 	otherwise, it will be distored/chipmunked/slo-mo-tioned in one way 
	 * 	or another
	 */
	//snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S24_LE);
	snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);


	/* two channel (stereo)
	 *
	 * NOTE we only use mono, so no need for
	 * 	setting to stereo.
	 *
	 * 	again though, if you change, have to change everything else to
	 * 	match, otherwise odd distortion, etc.
	 */
	snd_pcm_hw_params_set_channels(handle, params, 2);


	/* 44100 bits/sec sampling rate (CD quality) */
	val = 44100;
	snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);

	/* set period size 
	 * (32 in original code and 8192 in nagorni tutorial) 
	 *
	 * it is my guess that this works the same way as snd_pcm_hw_params_set_rate_near()
	 * it gets as close as it can to the targer value i specify, so
	 * some room for slop on my part
	 *
	 * this function snd_pcm_hw_params_set_period_size_near() is also
	 * different than func used in nagorni example, so maybe nagorni
	 * working off older version of api
	 *
	 * nagorni uses snd_pcm_hw_params_set_periods() 
	 *           AND another function for buffer size
	 *           snd_pcm_hw_params_set_buffer_size()
	 *           or snd_pcm_hw_params_set_buffer_size_near()
	 *
	 * */
	frames = 8192; //again, called 'periodsize' in nagorni tutorial
	//int periods = 2; //from nagorni example, used for testing nagorni buffer code

	
	/* from nagorni example, wanted to see if this method also possible... 
	 *
	 * couldn't get to work that well, and i think this may be why the 
	 * api function was not used to set buffer in original 
	 * tranter code


	if (snd_pcm_hw_params_set_periods(handle, params, periods, 0) < 0) {
		fprintf(stderr, "ERR: period setting err\n");
		return(-1);
	}


	snd_pcm_uframes_t holder;
	holder = (frames * periods) >> 2 ;
	

	if (snd_pcm_hw_params_set_buffer_size_near(handle, params, &holder) < 0) {
		fprintf(stderr, "ERR: cannot set buffer size\n");
		return(-1);
	}

	if (snd_pcm_hw_params(handle, params) < 0) {
		fprintf(stderr, "ERR: cannot set hw params\n");
	}

	buffer = (char *) malloc(frames);

	*/


	//original method WORKS!/////////////////////////////////////////////////////
	snd_pcm_hw_params_set_period_size_near(handle, params, &frames, &dir);
	
	/* write these parameters to the driver */
	rc = snd_pcm_hw_params(handle, params);
	if(rc < 0) {
		fprintf(stderr, "ERR: unable to set hw params(this): %s\n", snd_strerror(rc));
		exit(1);
	}

	/* use a buffer larger enough to hold one period */
	snd_pcm_hw_params_get_period_size(params, &frames, &dir);
	size = frames * 4; // 2 bytes per sample, and 2 channel
	buffer = (char*) malloc(size);

	/* we want to loop for 5 seconds
	 * NOTE this value can be changed
	 *
	 * not used for saw player, as not timer-based
	 */
	//snd_pcm_hw_params_get_period_time(params, &val, &dir);


	//nagorni example for generating simple stereo saw wave
	int pcmreturn, l1, l2;
	short s1, s2;
	int wave_frames;

	wave_frames = size >> 2;
	//wave_frames = holder >> 2; from unstable nagorni-based method
	for (l1 = 0; l1 < 100; l1++) {
		for (l2 = 0; l2 < frames; l2++ ) {
			s1 = (l2 % 128) * 100 - 5000;
			s2 = (l2 % 256) * 100 - 5000;
			buffer[4*l2] = (unsigned char)s1;
			buffer[4*l2+1] = s1 >> 8;
			buffer[4*l2+2] = (unsigned char)s2;
			buffer[4*l2+3] = s2 >> 8;
		}
		//snd_pcm_writei() is for interleaved only
		//for non-interleaved, would be snd_pcm_writen()
		while ((pcmreturn = snd_pcm_writei(handle, buffer, wave_frames)) < 0) {
			snd_pcm_prepare(handle);
			fprintf(stderr, "buffer underrun\n");
		}
	}
	/* 5 seconds in microseconds divided by * period time */
	//loops = 10000000 /val;
/*
	while (loops > 0 ) {   //could also do while (1) for long time
		loops --;
		rc = read(0, buffer, size);
		if(rc == 0) {
			fprintf(stderr, "end of file on input\n");
			break;
		} else if (rc != size){
			fprintf(stderr, "ERR: short read. read %d bytes\n", rc);
		}
		rc = snd_pcm_writei(handle, buffer, frames);
		if(rc == -EPIPE) { //NOTE: EPIPE means underrun
			fprintf(stderr, "ERR: underrun occurred\n");
			snd_pcm_prepare(handle);
		} else if (rc < 0) {
			fprintf(stderr, "ERR: error from writei: %s\n", snd_strerror(rc));
		} else if (rc != (int)frames) {
			fprintf(stderr, "ERR: short write. write %d frames\n", rc);
		}
	}
*/
	/*stops pcm device after pending frames have been played
	 * can use snd_pcm_drop(handle) to stop immediately 
	 * and just drop remaining frames.
	 */
	 
	snd_pcm_drain(handle);
	snd_pcm_close(handle);
	free(buffer);

	return 0;
}










