#ifndef CLIS_INC
#define CLIS_INC

struct jack_port_t;
struct jack_nframes_t;

typedef enum clis_rc {
    CLIS_OK         = 0,
    CLIS_E_PARSE_PARAM,
    CLIS_E_ALLOC_MOD,
    CLIS_E_JACK_CLIENT_OPEN,
    CLIS_E_JACK_CONNECT,
    CLIS_E_NAME_TAKEN,
    CLIS_E_CLIENT_ACTIVATE,
    CLIS_E_NO_OUTPUT_PORTS,
    CLIS_E_CONNECT_OUTPUT_PORT
} clis_rc;

typedef struct mod_source {
    char        *name;
    jack_port_t *port;
    float        value;
} mod_source;

typedef struct mod_source_arr {
    size_t length;
    mod_source *sources;
} mod_source_arr;

typedef struct parameter {
    float value;
    mod_source_arr mods;
} parameter;

// lifecycle
clis_rc     clis_init(char *client_name, char *server_name, jack_client_t **client);
clis_rc     clis_start(jack_client_t *client, parameter **params, size_t nparams);
clis_rc     play_audio(jack_client_t *client, jack_port_t *output_port);

// parameters
clis_rc     parse_param_string(char *arg, parameter *param);
void        free_param_mods(parameter *param);
jack_default_audio_sample_t 
            *get_mod_buffer(jack_nframes_t nframes, mod_source_arr *mods);

// status codes
const char  *clis_rc_string(clis_rc rc);
#endif // CLIS_INC
