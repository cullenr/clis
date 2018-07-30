#ifndef CLISHELPER_INC
#define CLISHELPER_INC

typedef void (*clis_init_func)(void *arg);
typedef void (*clis_exit_func)(void);
typedef int (*clis_start_func)(void *arg);
typedef int (*clis_sample_rate_func)(jack_nframes_t nframes, void *arg);
typedef int (*clis_process_func)(jack_nframes_t nframes, void *arg);

void clis_set_init_cb(clis_init_func cb);
void clis_set_exit_cb(clis_exit_func cb);
void clis_set_start_cb(clis_start_func cb);
void clis_set_process_cb(clis_sample_rate_func cb);
void clis_set_sample_rate_cb(clis_sample_rate_func cb);

void clis_helper_main(int argc, char*argv[], clis_context *context);
void clis_exit();

#endif // CLISHELPER_INC
