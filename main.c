#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <linux/input.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <signal.h>
#include <alsa/asoundlib.h>
#include <confuse.h>

#define MAX_SEQ_LEN 64

int main()
{
    struct input_event ev[64];
    int fd, rd, size = sizeof (struct input_event);
    char name[256] = "unk";
    snd_seq_t *seq_handle;
    snd_seq_event_t mev;
    int queue_id, port_out_id, seq_len;

    // set up default properties
    cfg_opt_t opts[] = {
        CFG_STR("device", "/dev/input/event1", CFGF_NONE),
        CFG_INT("pitch", 36, CFGF_NONE),
        CFG_INT("velocity", 127, CFGF_NONE),
        CFG_INT("duration", 300, CFGF_NONE),
        CFG_END()
    };

    // parse config file
    cfg_t *cfg;
    cfg = cfg_init(opts, CFGF_NONE);
    if(cfg_parse(cfg, "keycode2midi.conf") == CFG_PARSE_ERROR) {
        fprintf(stderr, "warning: could not read config file\n");
    }

    // set up alsa midi
    if (snd_seq_open(&seq_handle, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
        fprintf(stderr, "error opening ALSA sequencer\n");
        exit(1);
    }
    snd_seq_set_client_name(seq_handle, "keycode2midi");
    if ((port_out_id = snd_seq_create_simple_port(seq_handle, "midi out",
              SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ,
              SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
        fprintf(stderr, "error creating sequencer port\n");
        exit(1);
    }
    queue_id = snd_seq_alloc_queue(seq_handle);
    snd_seq_set_client_pool_output(seq_handle, (seq_len<<1) + 4);
    snd_seq_start_queue(seq_handle, queue_id, NULL);
    snd_seq_drain_output(seq_handle);

    // check for root
    if ((getuid ()) != 0) {
        printf ("must be root\n");
        return 1;
    }

    // open keyboard device
    if ((fd = open (cfg_getstr(cfg, "device"), O_RDONLY)) == -1) {
        printf ("error: cannot open device\n");
        return 1;
    }
    ioctl (fd, EVIOCGNAME (sizeof (name)), name);
    printf ("reading From input: %s\n", name);

    // loop on keyboard input
    while (1){
        if ((rd = read (fd, ev, size * 64)) < size) {
          printf("read error\n");
          return 1;
        }
        if (ev[1].type == EV_KEY && ev[1].value == 0) { // key release
            snd_seq_ev_clear(&mev);
            snd_seq_ev_set_note(&mev, 0, cfg_getint(cfg, "pitch"),
                cfg_getint(cfg, "velocity"), cfg_getint(cfg, "duration"));
            snd_seq_ev_schedule_tick(&mev, queue_id,  0, 0);
            snd_seq_ev_set_source(&mev, port_out_id);
            snd_seq_ev_set_subs(&mev);
            snd_seq_event_output_direct(seq_handle, &mev);
        }
    }
    return 0;
}
