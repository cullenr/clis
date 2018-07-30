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
#include "clishelper.h"

static clis_init_func clis_init_cb;
static clis_exit_func clis_exit_cb;
static clis_start_func clis_start_cb;
static clis_process_func clis_process_cb;
static clis_sample_rate_func clis_sample_rate_cb;
void clis_set_init_cb(clis_init_func cb){
    clis_init_cb = cb;
};
void clis_set_start_cb(clis_start_func cb){
    clis_start_cb = cb;
};
void clis_set_exit_cb(clis_exit_func cb){
    clis_exit_cb = cb;
};
void clis_set_process_cb(clis_sample_rate_func cb){
    clis_process_cb = cb;
};
void clis_set_sample_rate_cb(clis_sample_rate_func cb){
    clis_sample_rate_cb = cb;
};

void clis_exit(int status)
{
    // TODO : make this use the parameter_arr:
    // free_params(&params);
    clis_exit_cb();
    exit(status);
}

static void signal_handler(int sig)
{
    // TODO : re implement this!
    //jack_client_close(client);
    clis_exit(sig);
}

static void jack_shutdown (void *arg)
{
    (void)arg;

    fprintf(stderr, "jack server closed, quitting client\n");
    clis_exit(1);
}

void clis_helper_main(int argc, char *argv[], clis_context *context)
{
    char           *client_name = "test";
    char           *server_name = NULL;
    int             opt;
    clis_rc         rc = CLIS_OK;

// TODO: remove name as a param, and make server name ENV_VAR
    // reset getopt
/*    optind = 1;
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
*/
    rc = clis_init_client(client_name, server_name, &context->client);
    if(rc) {
        fprintf(stderr, "%s", clis_rc_string(rc));
        clis_exit(1);
    }

    clis_init_cb(context);

    jack_set_process_callback(context->client, clis_process_cb, context);
    jack_set_sample_rate_callback(context->client, set_sample_rate, context);
    jack_on_shutdown(context->client, jack_shutdown, context); // signals server exited

    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGINT, signal_handler);

    clis_start(context);

    clis_start_cb(context);

    while(1) {
        sleep(1);
    }

    // how did you get here?
}
