#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jack/jack.h>

#include "clis.h"

#define DEFAULT_PORT   "output"

clis_rc clis_init(char *client_name, char *server_name, jack_client_t **client)
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

    return CLIS_OK;
}

clis_rc clis_start(jack_client_t *client, parameter **params, size_t nparams)
{
    if (jack_activate(client)) {
        return CLIS_E_CLIENT_ACTIVATE;
    }
 
    for(unsigned int i = 0; i < nparams; i++) {
        parameter *param = params[i];
        for(unsigned int j = 0; j < param->mods.length; j++) {
            // TODO : replace find_port with jack_find_port
            param->mods.sources[j].port = jack_port_by_name(client, param->mods.sources[j].name);
        }
    }

    return CLIS_OK;
}
/** Connect the output port to the hardware ports 
 *
 *  Make sure that the client is activated before calling this function. Ports
 *  cannot be connected if they are not running.
 */
clis_rc play_audio(jack_client_t *client, jack_port_t *output_port)
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
 *  return a human readable string for a clis_rc enum value
 *  
 *  @param rc - the clis_rc enum value to translate
 *  @return - a human readable response message
 */
const char *clis_rc_string(clis_rc rc)
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
            return "Unable to connect to JACK server";
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
 * Helper function to allocate a JACK name string from a client and port.
 *
 * @param client - the JACK client name
 * @param client - the JACK port name
 * @return - the JACK complete name, the caller is responsible for freeing this
 *  string
 */
static char *cat_client_port(char *client, char *port)
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
static mod_source *add_mod_source(parameter *param)
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
clis_rc parse_param_string(char *arg, parameter *param)
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

/**
 *  free the parameters dynamically allocated modulation sources but not the
 *  parameter itself.
 *
 *  @param - the parameter to free
 */
void free_param_mods(parameter *param)
{
    unsigned int i;
    for(i = 0; i < param->mods.length; i++) {
        free(param->mods.sources[i].name);
    }
    free(param->mods.sources);
}

// TODO evaluate the usefulness of this considering we now have default port
// names provided by the parameter parser.
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
//static jack_port_t *find_port(jack_client_t *client, char *name)
//{
//    // TODO: assert(is_client_active);
//
//    jack_port_t *port = NULL;
//    const char **ports = jack_get_ports(client, name, NULL, JackPortIsOutput);
//
//    if(ports != NULL) {
//        port = jack_port_by_name(client, ports[0]);
//        if(port == NULL) {
//            fprintf(stderr, "could not aquire port :%s", name);
//        }
//    }
//
//    jack_free(ports);
//    return port;
//}

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
                // TODO dynamic sample rate
                out[j] += mod[j] * mods->sources[i].value * 44100;
            }
        }
    }

    return out;
}
