/*
 *  This small demo sends a simple sinusoidal wave to your speakers.
 *  COMPILE gcc -o test_sine test_sine.c -lasound -lm
 *  you need libasound2-dev in ubuntu
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
//#include "../include/asoundlib.h"
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <math.h>
 
static char *device = "plughw:0,0";         /* playback device */
static snd_pcm_format_t format = SND_PCM_FORMAT_S24;    /* sample format */
static unsigned int rate = 44100;           /* stream rate */
static unsigned int channels = 1;           /* count of channels */
static unsigned int buffer_time = 200000;       /* ring buffer length in us original setting: 5000000, changed due to clip sound...*/
static unsigned int period_time = 100000;       /* period time in us original setting 1000000, changed due to clip sound*/
/*static double freq = 110;                sinusoidal wave frequency in Hz */
static double freq;
static int verbose = 0;                 /* verbose flag */
static int resample = 0;                /* enable alsa-lib resampling */
static int period_event = 0;                /* produce poll event after each period */
 
static snd_pcm_sframes_t buffer_size;
static snd_pcm_sframes_t period_size;
static snd_output_t *output = NULL;

//make global so doesnt go back to 0 each time func called
//NOTE! doesnt matter problem is the method, not the variable...
//static double my_freq = 110;
//see comment around line 280

int counter = 0;

static void update_frequency(void)
{
   
   //scanf("%lf", &my_freq);
   
   double freq_arr[6] = {174.614, 195.998, 220, 261.626, 220, 195.998 

   
   };

   /* dont use loop for this! it is already looping by nature of 
    * how it is called!
    */

   counter ++;
   if (counter > 5)
   {
     counter = 0;
   }
     
   freq = freq_arr[counter];
   printf("sineHz\t\t%lf\n", freq);

   //freq = my_freq;
}



static void generate_sine(const snd_pcm_channel_area_t *areas, 
              snd_pcm_uframes_t offset,
              int count, double *_phase)
{
    static double max_phase = 2. * M_PI;
    double phase = *_phase;
    double step = max_phase*freq/(double)rate;
    unsigned char *samples[channels];
    int steps[channels];
    unsigned int chn;
    int format_bits = snd_pcm_format_width(format);
    unsigned int maxval = (1 << (format_bits - 1)) - 1;
    int bps = format_bits / 8;  /* bytes per sample */
    int phys_bps = snd_pcm_format_physical_width(format) / 8;
    int big_endian = snd_pcm_format_big_endian(format) == 1;
    int to_unsigned = snd_pcm_format_unsigned(format) == 1;
    int is_float = (format == SND_PCM_FORMAT_FLOAT_LE ||
            format == SND_PCM_FORMAT_FLOAT_BE);
 
    /* verify and prepare the contents of areas */
    for (chn = 0; chn < channels; chn++) {
        if ((areas[chn].first % 8) != 0) {
            printf("areas[%u].first == %u, aborting...\n", chn, areas[chn].first);
            exit(EXIT_FAILURE);
        }
        samples[chn] = /*(signed short *)*/(((unsigned char *)areas[chn].addr) + (areas[chn].first / 8));
        if ((areas[chn].step % 16) != 0) {
            printf("areas[%u].step == %u, aborting...\n", chn, areas[chn].step);
            exit(EXIT_FAILURE);
        }
        steps[chn] = areas[chn].step / 8;
        samples[chn] += offset * steps[chn];
    }
    /* fill the channel areas */
    while (count-- > 0) {
        union {
            float f;
            int i;
        } fval;
        int res, i;
        if (is_float) {
            fval.f = sin(phase);
            res = fval.i;
        } else
            res = sin(phase) * maxval;
        if (to_unsigned)
            res ^= 1U << (format_bits - 1);
        for (chn = 0; chn < channels; chn++) {
            /* Generate data in native endian format */
            if (big_endian) {
                for (i = 0; i < bps; i++)
                    *(samples[chn] + phys_bps - 1 - i) = (res >> i * 8) & 0xff;
            } else {
                for (i = 0; i < bps; i++)
                    *(samples[chn] + i) = (res >>  i * 8) & 0xff;
            }
            samples[chn] += steps[chn];
        }
        phase += step;
        if (phase >= max_phase)
            phase -= max_phase;
    }
    *_phase = phase;
}
 
static int set_hwparams(snd_pcm_t *handle,
            snd_pcm_hw_params_t *params,
            snd_pcm_access_t access)
{
    unsigned int rrate;
    snd_pcm_uframes_t size;
    int err, dir;
 
    /* choose all parameters */
    err = snd_pcm_hw_params_any(handle, params);
    if (err < 0) {
        printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
        return err;
    }
    /* set hardware resampling */
    err = snd_pcm_hw_params_set_rate_resample(handle, params, resample);
    if (err < 0) {
        printf("Resampling setup failed for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the interleaved read/write format */
    err = snd_pcm_hw_params_set_access(handle, params, access);
    if (err < 0) {
        printf("Access type not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the sample format */
    err = snd_pcm_hw_params_set_format(handle, params, format);
    if (err < 0) {
        printf("Sample format not available for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* set the count of channels */
    err = snd_pcm_hw_params_set_channels(handle, params, channels);
    if (err < 0) {
        printf("Channels count (%u) not available for playbacks: %s\n", channels, snd_strerror(err));
        return err;
    }
    /* set the stream rate */
    rrate = rate;
    err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
    if (err < 0) {
        printf("Rate %uHz not available for playback: %s\n", rate, snd_strerror(err));
        return err;
    }
    if (rrate != rate) {
        printf("Rate doesn't match (requested %uHz, get %iHz)\n", rate, err);
        return -EINVAL;
    }
    /* set the buffer time */
    err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, &dir);
    if (err < 0) {
        printf("Unable to set buffer time %u for playback: %s\n", buffer_time, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_buffer_size(params, &size);
    if (err < 0) {
        printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
        return err;
    }
    buffer_size = size;
    /* set the period time */
    err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, &dir);
    if (err < 0) {
        printf("Unable to set period time %u for playback: %s\n", period_time, snd_strerror(err));
        return err;
    }
    err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
    if (err < 0) {
        printf("Unable to get period size for playback: %s\n", snd_strerror(err));
        return err;
    }
    period_size = size;
    /* write the parameters to device */
    err = snd_pcm_hw_params(handle, params);
    if (err < 0) {
        printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
        return err;
    }
    return 0;
}
 
static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
{
    int err;
 
    /* get the current swparams */
    err = snd_pcm_sw_params_current(handle, swparams);
    if (err < 0) {
        printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* start the transfer when the buffer is almost full: */
    /* (buffer_size / avail_min) * avail_min */
    err = snd_pcm_sw_params_set_start_threshold(handle, swparams, (buffer_size / period_size) * period_size);
    if (err < 0) {
        printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* allow the transfer when at least period_size samples can be processed */
    /* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
    err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_event ? buffer_size : period_size);
    if (err < 0) {
        printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
        return err;
    }
    /* enable period events when requested */
    if (period_event) {
        err = snd_pcm_sw_params_set_period_event(handle, swparams, 1);
        if (err < 0) {
            printf("Unable to set period event: %s\n", snd_strerror(err));
            return err;
        }
    }
    /* write the parameters to the playback device */
    err = snd_pcm_sw_params(handle, swparams);
    if (err < 0) {
        printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
        return err;
    }
    return 0;
}
 
/*
 *   Underrun and suspend recovery
 */
 
static int xrun_recovery(snd_pcm_t *handle, int err)
{
    if (verbose)
        printf("stream recovery\n");
    if (err == -EPIPE) {    /* under-run */
        err = snd_pcm_prepare(handle);
        if (err < 0)
            printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
        return 0;
    } else if (err == -ESTRPIPE) {
        while ((err = snd_pcm_resume(handle)) == -EAGAIN)
            sleep(1);   /* wait until the suspend flag is released */
        if (err < 0) {
            err = snd_pcm_prepare(handle);
            if (err < 0)
                printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
        }
        return 0;
    }
    return err;
}

struct async_private_data {
	signed short *samples;
	snd_pcm_channel_area_t *areas;
	double phase;
};


/*
 *   Transfer method - asynchronous notification + direct write
 */
 
static void async_direct_callback(snd_async_handler_t *ahandler)
{
    snd_pcm_t *handle = snd_async_handler_get_pcm(ahandler);
    struct async_private_data *data = snd_async_handler_get_callback_private(ahandler);
    const snd_pcm_channel_area_t *my_areas;
    snd_pcm_uframes_t offset, frames, size;
    snd_pcm_sframes_t avail, commitres;
    snd_pcm_state_t state;
    int first = 0, err;
   

    /* this works! (mostly)
     */ 
    update_frequency(); 
    
    while (1) {
        state = snd_pcm_state(handle);
        if (state == SND_PCM_STATE_XRUN) {
            err = xrun_recovery(handle, -EPIPE);
            if (err < 0) {
                printf("XRUN recovery failed: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
            }
            first = 1;
        } else if (state == SND_PCM_STATE_SUSPENDED) {
            err = xrun_recovery(handle, -ESTRPIPE);
            if (err < 0) {
                printf("SUSPEND recovery failed: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
            }
        }
        avail = snd_pcm_avail_update(handle);
        if (avail < 0) {
            err = xrun_recovery(handle, avail);
            if (err < 0) {
                printf("avail update failed: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
            }
            first = 1;
            continue;
        }
        if (avail < period_size) {
            if (first) {
                first = 0;
                err = snd_pcm_start(handle);
                if (err < 0) {
                    printf("Start error: %s\n", snd_strerror(err));
                    exit(EXIT_FAILURE);
                }
            } else {
                break;
            }
            continue;
        }
        size = period_size;
        while (size > 0) {
            frames = size;
            err = snd_pcm_mmap_begin(handle, &my_areas, &offset, &frames);
            if (err < 0) {
                if ((err = xrun_recovery(handle, err)) < 0) {
                    printf("MMAP begin avail error: %s\n", snd_strerror(err));
                    exit(EXIT_FAILURE);
                }
                first = 1;
            }
            generate_sine(my_areas, offset, frames, &data->phase);
            commitres = snd_pcm_mmap_commit(handle, offset, frames);
            if (commitres < 0 || (snd_pcm_uframes_t)commitres != frames) {
                if ((err = xrun_recovery(handle, commitres >= 0 ? -EPIPE : commitres)) < 0) {
                    printf("MMAP commit error: %s\n", snd_strerror(err));
                    exit(EXIT_FAILURE);
                }
                first = 1;
            }
            size -= frames;

        }
    }
}
//THIS IS THE ONE WE WANT TO WORK WITH 
static int async_direct_loop(snd_pcm_t *handle,
                 signed short *samples ATTRIBUTE_UNUSED,
                 snd_pcm_channel_area_t *areas ATTRIBUTE_UNUSED)
{
    struct async_private_data data;
    snd_async_handler_t *ahandler;
    const snd_pcm_channel_area_t *my_areas;
    snd_pcm_uframes_t offset, frames, size;
    snd_pcm_sframes_t commitres;
    int err, count;
 
    
    data.samples = NULL;    /* we do not require the global sample area for direct write */
    data.areas = NULL;  /* we do not require the global areas for direct write */
    data.phase = 0;
    err = snd_async_add_pcm_handler(&ahandler, handle, async_direct_callback, &data);
    if (err < 0) {
        printf("Unable to register async handler\n");
        exit(EXIT_FAILURE);
    }
    for (count = 0; count < 2; count++) {
        size = period_size;
        while (size > 0) {
            frames = size;
            err = snd_pcm_mmap_begin(handle, &my_areas, &offset, &frames);
            if (err < 0) {
                if ((err = xrun_recovery(handle, err)) < 0) {
                    printf("MMAP begin avail error: %s\n", snd_strerror(err));
                    exit(EXIT_FAILURE);
                }
            }
            generate_sine(my_areas, offset, frames, &data.phase);
            commitres = snd_pcm_mmap_commit(handle, offset, frames);
            if (commitres < 0 || (snd_pcm_uframes_t)commitres != frames) {
                if ((err = xrun_recovery(handle, commitres >= 0 ? -EPIPE : commitres)) < 0) {
                    printf("MMAP commit error: %s\n", snd_strerror(err));
                    exit(EXIT_FAILURE);
                }
            }
            size -= frames;
        }
    }
    err = snd_pcm_start(handle);
    if (err < 0) {
        printf("Start error: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
 
    /* because all other work is done in the signal handler,
       suspend the process */
    while (1) {
        sleep(1);
    }
}
 

/*
 *
 */
 
 
 
int main(int argc, char *argv[])
{
    snd_pcm_t *handle;
    int err, morehelp;
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_sw_params_t *swparams;
    int method = 0;
    signed short *samples;
    unsigned int chn;
    snd_pcm_channel_area_t *areas;

    //need to do this b/c removed case statement and function
    snd_pcm_access_t access;
    access = SND_PCM_ACCESS_MMAP_INTERLEAVED;
 
    snd_pcm_hw_params_alloca(&hwparams);
    snd_pcm_sw_params_alloca(&swparams);
 
         
    err = snd_output_stdio_attach(&output, stdout, 0);
    if (err < 0) {
        printf("Output failed: %s\n", snd_strerror(err));
        return 0;
    }
 
    //printf("Playback device is %s\n", device);
    //printf("Stream parameters are %uHz, %s, %u channels\n", rate, snd_pcm_format_name(format), channels);
    //printf("Sine wave rate is %.4fHz\n", freq);
    //printf("Using transfer method: %s\n", transfer_methods[method].name);
    //printf("Using transfer method: async_direcct");

    if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        printf("Playback open error: %s\n", snd_strerror(err));
        return 0;
    }
    
    if ((err = set_hwparams(handle, hwparams, access /*transfer_methods[method].access*/)) < 0) {
        printf("Setting of hwparams failed: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
    if ((err = set_swparams(handle, swparams)) < 0) {
        printf("Setting of swparams failed: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }
 
    if (verbose > 0)
        snd_pcm_dump(handle, output);
 
    samples = malloc((period_size * channels * snd_pcm_format_physical_width(format)) / 8);
    if (samples == NULL) {
        printf("No enough memory\n");
        exit(EXIT_FAILURE);
    }
    
    areas = calloc(channels, sizeof(snd_pcm_channel_area_t));
    if (areas == NULL) {
        printf("No enough memory\n");
        exit(EXIT_FAILURE);
    }
    for (chn = 0; chn < channels; chn++) {
        areas[chn].addr = samples;
        areas[chn].first = chn * snd_pcm_format_physical_width(format);
        areas[chn].step = channels * snd_pcm_format_physical_width(format);
    }
 
    //err = transfer_methods[method].transfer_loop(handle, samples, areas);
    /* the original code requires some consideration... it is really awesome!
     * i took out the original struct, but, basically, it had three elements
     * the last one (transfer_loop) actually held the function name, and
     * the way it was set up would allow you to pass arguments to that function,
     * by accessing it through the struct
     * in other words, a concrete example of how to set up a function as a 
     * member of a struct AND pass arguments to it! WOW!
     */ 
    err = async_direct_loop(handle, samples, areas);
    if (err < 0)
        printf("Transfer failed: %s\n", snd_strerror(err));
 
    free(areas);
    free(samples);
    snd_pcm_close(handle);
    return 0;
}
