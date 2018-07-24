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

static clis_init_func clis_init_cb;
static clis_exit_func clis_exit_cb;
static clis_process_func clis_process_cb;
static clis_sample_rate_func clis_sample_rate_cb;
void clis_set_init_cb(clis_init_func cb){
    clis_init_cb = cb;
};
void clis_set_exit_cb(clis_exit_func cb){
    clis_exit_cb = cb;
};
void clis_set_process_cb(clis_sample_rate_func cb){
    clis_sample_rate_cb = cb;
};
void clis_set_sample_rate_cb(clis_sample_rate_func cb){
    clis_sample_rate_cb = cb;
};

static void die(int status)
{
    // TODO : make this use the parameter_arr:
    // free_params(&params);
    clis_exit_cb();
    exit(status);
}

static void signal_handler(int sig)
{
    jack_client_close(client);
    die(sig);
}

static void jack_shutdown (void *arg)
{
    (void)arg;

    fprintf(stderr, "jack server closed, quitting client\n");
    die(1);
}

static int set_sample_rate(jack_nframes_t nframes, void *arg)
{
    return clis_sample_rate_cb(nframes, arg)
}

static int process(jack_nframes_t nframes, void *arg)
{
    return clis_process_cb(nframes, arg);
}

void clis_helper_main(int argc, char *argv[], clis_context *context)
{
    char           *client_name = "test";
    char           *server_name = NULL;
    int             opt;
    clis_rc         rc = CLIS_OK;

    // reset getopt
    optind = 1;
    while ((opt = getopt(argc, argv, "n:s:")) != -1) {
        switch (opt) {
            case 'n': client_name = optarg;                         break;
            case 's': server_name = optarg;                         break;
            default: {
                fprintf(stderr, "Usage: %s TBC \n", argv[0]);
                exit(EXIT_FAILURE);
            }
        }
    }

    rc = clis_init_client(client_name, server_name, &context.client);
    if(rc) {
        fprintf(stderr, "%s", clis_rc_string(rc));
        die(1);
    }

    clis_init_cb(context.client);

    jack_set_process_callback(context.client, process, context.client);
    jack_set_sample_rate_callback(context.client, set_sample_rate, context.client);
    jack_on_shutdown(context.client, jack_shutdown, context.client); // signals server exited

    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGINT, signal_handler);

    clis_start(&context);

    clis_start_cb(context.client);

    while(1) {
        sleep(1);
    }

    // how did you get here?
}
