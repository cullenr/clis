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

jack_port_t *output_port;
jack_client_t *client;
parameter freq = {200, {0, NULL}};

static void die(int status)
{
    free_param_mods(&freq);
    exit(status);
}

static void signal_handler(int sig)
{
    fprintf(stderr, "signal received, exiting ...\n");
    jack_client_close(client);
    die(sig);
}

void jack_shutdown (void *arg)
{
    (void)arg;

    printf("jack shutdown");
    die(1);
}

float frandom(void)
{
    return (float)rand() / (float)(RAND_MAX);
}

int process(jack_nframes_t nframes, void *arg)
{
    jack_default_audio_sample_t *out, *mod;
    tSawtooth *saw = (tSawtooth*)arg;
    unsigned int i;

    out = (jack_default_audio_sample_t*)jack_port_get_buffer(output_port, nframes);
    mod = get_mod_buffer(nframes, &freq.mods);

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

int set_sample_rate(jack_nframes_t nframes, void *arg)
{
    (void)arg;

    OOPSSetSampleRate((float)nframes);

    return 0;
}

void port_registered(jack_port_id_t port_id, int is_registering, void *arg)
{
    (void)port_id;
    (void)is_registering;
    (void)arg;
//    if(is_registering) {
//        jack_port_t *port = jack_port_by_id(client, port_id);
//        const char *name = jack_port_name(port);
//        // check if we are interested in this port
//            // look up the port and store in in the appropriate mod collection.
//    } else {
//
//    }
}

int main (int argc, char *argv[])
{
    char      *client_name = "test";
    char      *server_name = NULL;
    tSawtooth       *data;
    int             opt;
    bool            play = false;
    clis_rc         rc;

    while ((opt = getopt(argc, argv, "n:s:f:p")) != -1) {
        switch (opt) {
        case 'n':
            client_name = optarg;
            break;
        case 's':
            server_name = optarg;
            break;
        case 'f': {
            rc = parse_param_string(optarg, &freq);
            if (rc) {
                if (rc == CLIS_E_PARSE_PARAM)
                    fprintf(stderr, "%s:%s", clis_rc_string(rc), optarg);
                else
                    fprintf(stderr, "%s", clis_rc_string(rc));

                die(1);
            }
            break;
        }
        case 'p':
            play = true;
            break;
        default: 
            fprintf(stderr, "Usage: %s TBC \n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    rc = clis_init(client_name, server_name, &client);
    if(rc) {
        fprintf(stderr, "%s", clis_rc_string(rc));
        die(1);
    }

    srand((unsigned int)time(NULL));
    OOPSInit((float)jack_get_sample_rate(client), &frandom);
    
    data = tSawtoothInit();

    jack_set_process_callback(client, process, data);
    jack_set_sample_rate_callback(client, set_sample_rate, 0);
    //jack_set_port_registration_callback(client, port_registered, 0);
    jack_on_shutdown(client, jack_shutdown, 0);

    output_port = jack_port_register (client, "output",
            JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    if (output_port == NULL) {
        fprintf(stderr, "no more JACK ports available\n");
        exit (1);
    }
    
    clis_start(client, (parameter*[]){&freq}, 1);

    if (play) {
        play_audio(client, output_port);
    }
    
    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGINT, signal_handler);

    while(1) {
        sleep(1);
    }

    // how did you get here?
}
