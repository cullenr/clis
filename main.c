#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <jack/jack.h>
#include "OOPS/Inc/OOPS.h"

#include "clis.h"

typedef struct process_data {
    tSawtooth       *saw;
    tSquare         *sqr;
    tTriangle       *tri;
    tCycle          *sin;
} process_data;

jack_port_t     *saw_output_port;
jack_port_t     *sqr_output_port;
jack_port_t     *tri_output_port;
jack_port_t     *sin_output_port;

parameter        freq = {
    .value = 200, 
    .mods = {0, NULL}
};
clis_context context = {
    .params_length = 1,
    .params = &freq,
    .client = NULL
};

// helper to allow us to call clis_close with no arguments with 'atexit'
static void
cleanup() {
    clis_close(&context);
}

static float
frandom(void)
{
    return (float)rand() / (float)(RAND_MAX);
}

static int
process(jack_nframes_t nframes, void *arg)
{
    unsigned int i;
    jack_default_audio_sample_t *saw_out, *sqr_out, *tri_out, *sin_out, *mod;
    process_data *data = (process_data*)arg;
    float f = freq.value;

    saw_out = (jack_default_audio_sample_t*)jack_port_get_buffer(saw_output_port, nframes);
    sqr_out = (jack_default_audio_sample_t*)jack_port_get_buffer(sqr_output_port, nframes);
    tri_out = (jack_default_audio_sample_t*)jack_port_get_buffer(tri_output_port, nframes);
    sin_out = (jack_default_audio_sample_t*)jack_port_get_buffer(sin_output_port, nframes);

    mod = clis_get_mod_buffer(nframes, &freq.mods);

    if(mod) {
        for(i = 0; i < nframes; i++) {
            f = freq.value + mod[i];

            tSawtoothSetFreq(data->saw, f);
            tSquareSetFreq(data->sqr, f);
            tTriangleSetFreq(data->tri, f);
            tCycleSetFreq(data->sin, f);

            saw_out[i] = tSawtoothTick(data->saw);
            sqr_out[i] = tSquareTick(data->sqr);
            tri_out[i] = tTriangleTick(data->tri);
            sin_out[i] = tCycleTick(data->sin);
        }
    } else {
        // Do this every frame? - do we may have a the last mod disconnected in
        // the previous process call. That will leave us out of tune.
        tSawtoothSetFreq(data->saw, f);
        tSquareSetFreq(data->sqr, f);
        tTriangleSetFreq(data->tri, f);
        tCycleSetFreq(data->sin, f);

        for(i = 0; i < nframes; i++) {
            saw_out[i] = tSawtoothTick(data->saw);
            sqr_out[i] = tSquareTick(data->sqr);
            tri_out[i] = tTriangleTick(data->tri);
            sin_out[i] = tCycleTick(data->sin);
        }
    }

    free(mod);

    return 0;
}

static int
set_sample_rate(jack_nframes_t nframes, void *arg)
{
    (void)arg;

    OOPSSetSampleRate((float)nframes);

    return 0;
}

static int 
register_output_port(jack_client_t *client, jack_port_t **port, const char *name)
{
    *port = jack_port_register(client, name,
        JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    if (*port == NULL) {
        fprintf(stderr, "could not register %s as output\n", name);
        return 1;
    }

    return 0;
}

int main (int argc, char *argv[])
{
    char      *client_name = "test";
    char      *server_name = NULL;
    int        opt;
    bool       play = false;
    clis_rc    rc = CLIS_OK;

    atexit(&cleanup);

    while ((opt = getopt(argc, argv, "n:s:f:p")) != -1) {
        switch (opt) {
            case 'n': client_name = optarg;                         break;
            case 's': server_name = optarg;                         break;
            case 'f': rc = clis_parse_param_string(optarg, &freq);  break;
            case 'p': play = true;                                  break;
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

    process_data data = {
        .saw = tSawtoothInit(),
        .sqr = tSquareInit(),
        .tri = tTriangleInit(),
        .sin = tCycleInit()
    };
    rc = clis_init_client(client_name, server_name, &context.client, process, 
                          &data, set_sample_rate, NULL);
    if(rc) {
        fprintf(stderr, "%s", clis_rc_string(rc));
        exit(EXIT_FAILURE);
    }

    srand((unsigned int)time(NULL));
    OOPSInit((float)jack_get_sample_rate(context.client), &frandom);

    if(register_output_port(context.client, &saw_output_port, "saw") ||
       register_output_port(context.client, &sqr_output_port, "sqr") ||
       register_output_port(context.client, &tri_output_port, "tri") ||
       register_output_port(context.client, &sin_output_port, "sin")) {
        exit(EXIT_FAILURE);
    }

    clis_start(&context);

    if (play) {
        clis_play_audio(context.client, saw_output_port, sqr_output_port);
        clis_play_audio(context.client, saw_output_port, sqr_output_port);
    }

    clis_run();

    printf("exiting gracefully\n");
}
