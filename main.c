#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <jack/jack.h>
#include "OOPS/Inc/OOPS.h"

#include "clis.h"

typedef struct my_context {
    clis_context;
    bool play;
} my_context;

jack_port_t     *output_port;
parameter        freq = {
    .value = 200, 
    .mods = {0, NULL}
};
parameter_arr    params = {
    .length = 1, 
    .params = (parameter *[]){&freq}
};
my_context context = {
    .params = &params
};
// TODO : put this in the context
tSawtooth *saw;

static float frandom(void)
{
    return (float)rand() / (float)(RAND_MAX);
}

static void on_exit()
{
    clis_free_param_mods(&freq);
}

static int on_process(jack_nframes_t nframes, my_context *ctx)
{
    (void) ctx;
    jack_default_audio_sample_t *out, *mod;
    unsigned int i;

    out = (jack_default_audio_sample_t*)jack_port_get_buffer(output_port, nframes);
    mod = clis_get_mod_buffer(nframes, &freq.mods);

    // TODO: make saw a struct that contains a generic set freq function
    // and a union of waveform types so that we can support multiple 
    // waveforms more easily
    if(mod) {
        for(i = 0; i < nframes; i++) {
            tSawtoothSetFreq(saw, freq.value + mod[i]);
            out[i] = tSawtoothTick(saw);
        }
    } else {
        tSawtoothSetFreq(saw, freq.value);
        for(i = 0; i < nframes; i++) {
            out[i] = tSawtoothTick(saw);
        }
    }

    free(mod);

    return 0;      
}

static int on_sample_rate(jack_nframes_t nframes, my_context *ctx)
{
    (void) ctx;

    OOPSSetSampleRate((float)nframes);

    return 0;
}

static void on_init(my_context *ctx)
{
    srand((unsigned int)time(NULL));
    OOPSInit((float)jack_get_sample_rate(ctx->client), &frandom);

    saw = tSawtoothInit();
    
    output_port = jack_port_register (ctx->client, "output",
            JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    if (output_port == NULL) {
        fprintf(stderr, "no more JACK ports available\n");
        clis_exit(1);
    }
}

static void on_start(my_context *ctx)
{
    if (ctx->play) {
	    printf("play audio");
        clis_play_audio(ctx->client, output_port);
    }
}

int main (int argc, char *argv[])
{
    int        opt;
    bool       play = false;
    clis_rc    rc = CLIS_OK;

    clis_set_init_cb(&on_init);
    clis_set_exit_cb(&on_exit);
    clis_set_start_cb(&on_start);
    clis_set_process_cb(&on_process);
    clis_set_sample_rate_cb(&on_sample_rate);

    while ((opt = getopt(argc, argv, "f:p")) != -1) {
        switch (opt) {
            case 'f': rc = clis_parse_param_string(optarg, &freq); break;
            case 'p': context.play = true;                         break;
            default: {
                fprintf(stderr, "Usage: %s TBC \n", argv[0]);
                exit(EXIT_FAILURE);
            }
        }

        if (rc) {
            fprintf(stderr, "%s:%s", optarg, clis_rc_string(rc));
            exit(EXIT_FAILURE);
        }
    }

    clis_helper_main(argc, argv, &context);
}
