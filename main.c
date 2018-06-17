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

typedef struct mod_source {
    char        *name;
    jack_port_t *port;
    //float        amt;   // TODO : implement
} mod_source;

typedef struct mod_source_arr {
    unsigned int length;
    mod_source *sources;
} mod_source_arr;

typedef struct parameter {
    float value;
    mod_source_arr mods;
} parameter;

jack_port_t *input_port, *output_port;
jack_client_t *client;
float amt = 0.1;
parameter freq = {200, {0, NULL}};

static void 
die(int status)
{
    // dont do this! we are using optarg to set them, maybe we should use strdup
    // so that we can do this?
    //unsigned int i;
    //for(i = 0; i < freq.mods.length; i++) {
    //    free(freq.mods.sources[i].name);
    //}
    free(freq.mods.sources);

    exit(status);
}

static void 
signal_handler(int sig)
{
    fprintf(stderr, "signal received, exiting ...\n");
    jack_client_close(client);
    die(sig);
}

/**
 *  Find a port by name
 *
 *  This function is specific to these tools, it allows us to find ports based
 *  on partial names. For example we can specify only the client name to connect
 *  to the first output port on that client. Specifying the client and port name
 *  is also supported.
 *
 *  NOTE that this function can only be called after the client is activated
 *
 *  @param name The name of the port to look up
 *  @returns the jack port itself
 */
static jack_port_t *
find_port(char *name)
{
    // TODO: assert(is_client_active);

    jack_port_t *port = NULL;
    const char **ports = jack_get_ports(client, name, NULL, JackPortIsOutput);

    if(ports != NULL) {
        port = jack_port_by_name(client, ports[0]);
        if(port == NULL) {
            fprintf(stderr, "could not aquire port :%s", name);
        }
    }

    jack_free(ports);
    return port;
}

// TODO : adapt this so that it works with midi sources and constant sources
// TODO : find out if its ok to read the output-port buffers like this or if
// they should first be connected to an input-port on this client.
static jack_default_audio_sample_t *
get_mod_buffer(jack_nframes_t nframes, mod_source_arr *mods)
{
    unsigned int i, j;
    jack_default_audio_sample_t *out = NULL, *mod = NULL;

    if(mods->length > 0) {
        out = calloc(nframes, sizeof *out); 

        for(i = 0; i < mods->length; i++) {
            mod = (jack_default_audio_sample_t*)
                   jack_port_get_buffer(mods->sources[i].port, nframes);
            for(j = 0; j < nframes; j++) {
                out[j] += mod[j] * amt * 44100;// TODO dynamic sample rate
            }
        }
    }

    return out;
}

void 
jack_shutdown (void *arg)
{
    (void)arg;

    printf("jack shutdown");
    die(1);
}

float 
frandom(void)
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

int 
set_sample_rate(jack_nframes_t nframes, void *arg)
{
    (void)arg;

    printf("the sample rate is now %" PRIu32 "/sec %f \n", nframes, (float)nframes);

    OOPSSetSampleRate((float)nframes);

    return 0;
}

void 
port_registered(jack_port_id_t port_id, int is_registering, void *arg)
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

int 
main (int argc, char *argv[])
{
    const char      **ports;
    const char      *client_name = "test";
    const char      *server_name = NULL;
    tSawtooth       *data;
    jack_options_t  options = JackNullOption;
    jack_status_t   status;
    int             opt;
    bool            play = false;

    while ((opt = getopt(argc, argv, "n:s:f:m:pa:")) != -1) {
        switch (opt) {
        case 'n':
            client_name = optarg;
            break;
        case 's':
            server_name = optarg;
            options = JackNullOption | JackServerName;
            break;
        case 'f':
            freq.value = atoi(optarg);
            break;
        case 'm': {
            // this is where we need to add to a list of mod sources
            mod_source *tmp = realloc(freq.mods.sources, (freq.mods.length + 1) * sizeof *tmp);
            if(tmp == NULL) {
                fprintf(stderr, "could not allocate mod source");
                die(1);
            }
            freq.mods.sources = tmp;
            freq.mods.sources[freq.mods.length].name = optarg;
            freq.mods.sources[freq.mods.length].port = NULL; // too early to find_port(optarg);
            freq.mods.length++;
            printf("added mod source %s\n", optarg);
            break;
        }
        case 'p':
            play = true;
            break;
        case 'a':
            amt = atof(optarg);
            break;
        default: 
            fprintf(stderr, "Usage: %s [-f frequency] \n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    srand((unsigned int)time(NULL));

    client = jack_client_open(client_name, options, &status, server_name);
    if (client == NULL) {
        fprintf (stderr, "jack_client_open() failed, "
                "status = 0x%2.0x\n", status);
        if (status & JackServerFailed) {
            fprintf (stderr, "Unable to connect to JACK server\n");
        }
        exit (1);
    }
    if (status & JackServerStarted) {
        fprintf (stderr, "JACK server started\n");
    }
    if (status & JackNameNotUnique) {
        client_name = jack_get_client_name(client);
        fprintf (stderr, "unique name `%s' assigned\n", client_name);
    }

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

    /* Tell the JACK server that we are ready to roll.  Our
     * process() callback will start running now. */

    if (jack_activate(client)) {
        fprintf (stderr, "cannot activate client");
        exit (1);
    }

    for(unsigned int i = 0; i < freq.mods.length; i++) {
        freq.mods.sources[i].port = find_port(freq.mods.sources[i].name);
    }

    /* Connect the ports.  You can't do this before the client is
     * activated, because we can't make connections to clients
     * that aren't running.  Note the confusing (but necessary)
     * orientation of the driver backend ports: playback ports are
     * "input" to the backend, and capture ports are "output" from
     * it.
     */

    if (play) {
        ports = jack_get_ports(client, NULL, NULL,
                JackPortIsPhysical|JackPortIsInput);
        if (ports == NULL) {
            fprintf(stderr, "no physical playback ports\n");
            exit (1);
        }

        if (jack_connect (client, jack_port_name(output_port), ports[0])) {
            fprintf (stderr, "cannot connect output ports\n");
        }

        if (jack_connect (client, jack_port_name(output_port), ports[1])) {
            fprintf (stderr, "cannot connect output ports\n");
        }

        jack_free(ports);
    }
    /* install a signal handler to properly quits jack client */
    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGINT, signal_handler);

    /* keep running until the Ctrl+C */
    while(1) {
        sleep(1);
    }

    exit(0);
}
