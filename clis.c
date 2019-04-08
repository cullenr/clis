#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jack/jack.h>

#include "clis.h"

#define DEFAULT_PORT   "output"

int is_running = 0;


void
clis_run()
{
    is_running = 1;
    while(is_running) {
        sleep(1);
    }
}

static void
signal_handler(int sig)
{
    is_running = 0;
}

static void
jack_shutdown (void *arg)
{
    (void)arg;

    fprintf(stderr, "jack server closed, quitting client\n");
    // TODO :raise a sigint or somthing so we can break out of sigsuspend
    is_running = 0;
}

clis_rc 
clis_init_client(char *client_name, char *server_name, jack_client_t **client,
        JackProcessCallback process_cb, void *process_cb_arg,
        JackSampleRateCallback srate_cb, void *srate_cb_arg)
{
    jack_status_t   status;
    jack_options_t  options = server_name
        ? JackNullOption
        : JackNullOption | JackServerName;

    *client = jack_client_open(client_name, options, &status, server_name);

    if (*client == NULL) {
        // TODO: add verbose logging mode
        // fprintf (stderr, "jack_client_open() failed, status = 0x%2.0x\n", 
        //         status);
        if (status & JackServerFailed) {
            return CLIS_E_JACK_CONNECT;
        } else {
            return CLIS_E_JACK_CLIENT_OPEN;
        }
    } else if (status & JackNameNotUnique) {
        return CLIS_E_NAME_TAKEN;
    }

    // signals server exited
    jack_on_shutdown(*client, jack_shutdown, 0);

    if(process_cb != NULL &&
        jack_set_process_callback(*client, process_cb, process_cb_arg) != 0) {
        return CLIS_E_JACK_CALLBACK;
    }

    if(srate_cb != NULL &&
        jack_set_sample_rate_callback(*client, srate_cb, srate_cb_arg) != 0) {
        return CLIS_E_JACK_CALLBACK;
    }

    // TODO: add error handlers
    signal(SIGQUIT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGINT, signal_handler);

    return CLIS_OK;
}

static void
port_registered(jack_port_id_t port_id, int is_registering, void *arg)
{
    printf("PORT %s : %p, %i\n", is_registering ? "REGISTERED" : "UNREGISTER", 
            port_id, is_registering);

    unsigned int i, j;
    clis_context *context       = (clis_context *)arg;
    parameter_arr *params_arr   = context->params;
    jack_port_t *port           = jack_port_by_id(context->client, port_id);
    const char *name            = jack_port_name(port);

    for(i = 0; i < params_arr->length; i++) {

        parameter *param = params_arr->params[i];

        for(j = 0; j < param->mods.length; j++) {

            if(strcmp(param->mods.sources[j].name, name) == 0) {

                param->mods.sources[j].port = is_registering ? port : NULL;
                printf("CALLBACK added mod source %s : %p\n", 
                        param->mods.sources[j].name, port);
            }
        }
    }

}

/**
 * Helper function to allocate a JACK name string from a client and port.
 *
 * @param client - the JACK client name
 * @param client - the JACK port name
 * @return - the JACK complete name, the caller is responsible for freeing this
 *  string
 */
static char *
cat_client_port(char *client, char *port)
{
    size_t size = strlen(client) + strlen(port) + 2; //\0 and :
    char *out   = malloc(size);

    snprintf(out, size, "%s:%s", client, port);

    return out;
}

/**
 *  Adds a new modulation source to the parameter->mods array.
 *
 *  @return - a pointer to the newly created modulation source or NULL if 
 *  allocation failed
 */
static mod_source *
add_mod_source(parameter *param)
{
    mod_source *tmp = realloc(param->mods.sources, (param->mods.length + 1) * sizeof *tmp);
    if(tmp == NULL) {
        return NULL;
    }
    param->mods.sources = tmp;
    param->mods.sources[param->mods.length].port = NULL;
    param->mods.length++;

    return &param->mods.sources[param->mods.length - 1];
}


clis_rc
clis_start(clis_context *context) {
    unsigned int i, j;

    // make sure we can add modulation parameter ports when they are created
    // in future.
    jack_set_port_registration_callback(context->client, port_registered,
            context);

    // at this point all of the jack callbacks should be registered and so it is
    // safe to 'activate' the client
    if (jack_activate(context->client)) {
        return CLIS_E_CLIENT_ACTIVATE;
    }

    // attempt to find jack ports that have been registered by other processes
    // and add them to the parameters array so they can be processed as inputs
    for(i = 0; i < context->params->length; i++) {

        parameter *param = context->params->params[i];

        for(j = 0; j < param->mods.length; j++) {

            jack_port_t *port = jack_port_by_name(context->client, 
                    param->mods.sources[j].name);

            if(port == NULL)
                continue;

            param->mods.sources[j].port = port;

            printf("INIT added mod source %s : %p\n", 
                    param->mods.sources[j].name, param->mods.sources[j].port);
        }
    }

    return CLIS_OK;
}

/** 
 * Connect the output port to the hardware ports 
 *
 * Make sure that the client is activated before calling this function. Ports
 * cannot be connected if they are not running.
 */
clis_rc
clis_play_audio(jack_client_t *client, jack_port_t *output_port)
{
    const char **ports = jack_get_ports(client, NULL, NULL, 
            JackPortIsPhysical|JackPortIsInput);

    if (ports == NULL) {
        return CLIS_E_NO_OUTPUT_PORTS;
    }

    if (jack_connect (client, jack_port_name(output_port), ports[0])) {
        return CLIS_E_CONNECT_OUTPUT_PORT;
    }

    if (jack_connect (client, jack_port_name(output_port), ports[1])) {
        return CLIS_E_CONNECT_OUTPUT_PORT;
    }

    jack_free(ports);

    return CLIS_OK;
}

/**
 * return a human readable string for a clis_rc enum value
 * 
 * @param rc - the clis_rc enum value to translate
 * @return - a human readable response message
 */
const char *
clis_rc_string(clis_rc rc)
{
    switch(rc) {
        case CLIS_OK:
            return "status ok";
        case CLIS_E_ALLOC_MOD: 
            return "could not allocate new modulation source";
        case CLIS_E_PARSE_PARAM: 
            return "could not parse param string";
        case CLIS_E_JACK_CLIENT_OPEN:
            return "could not open jack client";
        case CLIS_E_JACK_CONNECT:
            return "unable to connect to JACK server";
        case CLIS_E_JACK_CALLBACK:
            return "unable add callback to JACK client";
        case CLIS_E_NAME_TAKEN:
            return "client name already taken";
        case CLIS_E_CLIENT_ACTIVATE:
            return "could not activate client";
        case CLIS_E_NO_OUTPUT_PORTS:
            return "no physical playback ports";
        case CLIS_E_CONNECT_OUTPUT_PORT:
            return "cannot connect output ports\n";
        default :
            return "unkown error";
    }
}


/**
 * Set the mod_source properties according to the arg string and the globally
 * defined defaults.
 * 
 * @param arg - the configuration string in the format:
 *   - value
 *   - client
 *   - client:value
 *   - client:port
 *   - client:port:value
 *  value will be cast to a float, client and port strings are limited to 32
 *  char each as per the JACK limits.
 * @param param - a new modulation source will be added accordingly. The
 *  caller is responsible for freeing the mod_source name string. And the 
 *  modulation sources array.
 * @return 0 on success or an error code according to clis_rc
 */
clis_rc 
clis_parse_param_string(char *arg, parameter *param)
{
    mod_source *mod = NULL;
    char  client[32];
    char  port[32];
    float value = 0;
  
    // NOTE that we only set the matched properties as the others will have 
    // incorrect matches from previous runs of sscanf
    if(sscanf(arg, "%32[^:]:%32[^:]:%f", client, port, &value) == 3) {
        mod = add_mod_source(param);
        if(mod == NULL) 
            return CLIS_E_ALLOC_MOD;
        mod->name   = cat_client_port(client, port);
        mod->value  = value;
    } else if(sscanf(arg, "%32[^:]:%f", client, &value) == 2) {
        mod = add_mod_source(param);
        if(mod == NULL) 
            return CLIS_E_ALLOC_MOD;
        mod->name   = cat_client_port(client, DEFAULT_PORT);
        mod->value  = value;
    } else if(sscanf(arg, "%f", &value) == 1) {
        param->value = value;
    } else if(sscanf(arg, "%32[^:]:%32[^:]", client, port) == 2) {
        mod = add_mod_source(param);
        if(mod == NULL) 
            return CLIS_E_ALLOC_MOD;
        mod->name   = cat_client_port(client, port);
        mod->value  = 1;
    } else if(sscanf(arg, "%32[^:]", client) == 1) {
        mod = add_mod_source(param);
        if(mod == NULL) 
            return CLIS_E_ALLOC_MOD;
        mod->name   = cat_client_port(client, DEFAULT_PORT);
        mod->value  = 1;
    } else {
        return CLIS_E_PARSE_PARAM;
    }
    return 0;
}

// TODO : adapt this so that it works with midi sources and constant sources
// TODO : find out if its ok to read the output-port buffers like this or if
// they should first be connected to an input-port on this client.
/**
 *  return a buffer of all the mod sources combined for a single parameters 
 *  modulation sources
 *
 *  @param nframes - the length of the requested buffer
 *  @param mods - the modulation sources to sample
 *  @return - a pointer to the sampled buffer, the caller must free the buffer
 */
jack_default_audio_sample_t *
clis_get_mod_buffer(jack_nframes_t nframes, mod_source_arr *mods)
{
    unsigned int i, j;
    jack_default_audio_sample_t *out = NULL, *mod = NULL;
    jack_port_t *port;

    if(mods->length > 0) {
        out = calloc(nframes, sizeof *out); 

        for(i = 0; i < mods->length; i++) {
            port = mods->sources[i].port;

            // ports may not be registered yet
            if (port == NULL)
                continue;

            mod = (jack_default_audio_sample_t*)
                   jack_port_get_buffer(mods->sources[i].port, nframes);

            for(j = 0; j < nframes; j++) {
                // TODO dynamic sample rate
                out[j] += mod[j] * mods->sources[i].value * 44100;
            }
        }
    }

    return out;
}

/**
 *  free the parameters dynamically allocated modulation sources but not the
 *  parameter itself.
 *
 *  @param - the parameter to free
 */
void 
clis_free_param_mods(parameter *param)
{
    unsigned int i;
    for(i = 0; i < param->mods.length; i++) {
        free(param->mods.sources[i].name);
    }
    free(param->mods.sources);
}

