/* tabata_timer.c
 *
 * Compile:  gcc -Wall -O2 -o tabata_timer tabata_timer.c -lrt
 *
 * Usage (client):
 *   tabata_timer start   <work_sec> <rest_sec> <rounds>
 *   tabata_timer stop
 *   tabata_timer status
 *   tabata_timer quit        # ask daemon to exit
 *
 *   If the daemon is not running it will be started automatically.
 *
 * The daemon runs in the background after being exec‑ed with "--daemon".
 * It ticks once per second (using timerfd) and guarantees that missed
 * ticks are accounted for.
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/timerfd.h>
#include <sys/select.h>
#include <signal.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
//For playing audio
#include "audio.h"


#define SOCK_PATH   "/tmp/tabata_timer.sock"
#define MAX_CMD_LEN 256

/* ----------------------------------------------------------------------
   Daemon state
   ---------------------------------------------------------------------- */
typedef enum { IDLE, RUNNING } daemon_state_t;

typedef struct {
    daemon_state_t state;
    int work_sec;      // length of work interval
    int rest_sec;      // length of rest interval
    int rounds;        // total number of rounds
    int cur_round;     // 0‑based index of current round
    bool in_work;      // true = work phase, false = rest phase
    int sec_remaining; // seconds left in current phase
} tabata_tabata_timer_t;

static tabata_tabata_timer_t timer = {
    .state = IDLE,
    .work_sec = 0,
    .rest_sec = 0,
    .rounds = 0,
    .cur_round = 0,
    .in_work = true,
    .sec_remaining = 0
};

/* ----------------------------------------------------------------------
   Helper: clean up the socket file on exit
   ---------------------------------------------------------------------- */
static void cleanup_socket(void)
{
    unlink(SOCK_PATH);
}


/* ----------------------------------------------------------------------
   Helper: on unusual exit, cleanup socket
   ---------------------------------------------------------------------- */
static void sig_cleanup(int sig)
{
    (void)sig;               /* unused */
    unlink(SOCK_PATH);       /* remove the socket file */
    _exit(EXIT_FAILURE);    /* async‑safe exit */
}
/* Register the handler for the signals we care about */
static void setup_signal_handlers(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));   /* zero‑fill the whole struct */
    sa.sa_handler = sig_cleanup;  /* our handler */
    sigemptyset(&sa.sa_mask);     /* no additional signals blocked */
    sa.sa_flags = SA_RESTART;     /* restart interrupted syscalls */

    /* Install the handler */
    if (sigaction(SIGINT,  &sa, NULL) == -1 ||
        sigaction(SIGTERM, &sa, NULL) == -1 ||
        sigaction(SIGHUP,  &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
}
/* ----------------------------------------------------------------------
   Daemon core: timer loop + command handling
   ---------------------------------------------------------------------- */
static int make_timerfd(void)
{
    int tfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    if (tfd == -1) {
        perror("timerfd_create");
        exit(EXIT_FAILURE);
    }
    struct itimerspec its = {
        .it_interval = {.tv_sec = 1, .tv_nsec = 0},
        .it_value    = {.tv_sec = 1, .tv_nsec = 0}
    };
    if (timerfd_settime(tfd, 0, &its, NULL) == -1) {
        perror("timerfd_settime");
        exit(EXIT_FAILURE);
    }
    return tfd;
}

/* Randomly maybe play a message */
static void maybe_add_message(void)
{
    char buf[16];
    if ((rand() % 20) == 0) {
        //Randomly generate message001 through message100
        int msg_num = (rand() % 100) + 1;
        snprintf(buf, sizeof(buf), "message%03d", msg_num);
        audio_chain_add_by_name(buf);
    }
}

/* Randomly maybe play a message */
static void announce_start_of_round(
                                    int cur_round,
                                    int total_rounds,
                                    int minutes,
                                    bool is_work)
{
    char buf[16];
    audio_chain_add_by_name("round");
    //Randomly generate message001 through message100
    snprintf(buf, sizeof(buf), "num%d", cur_round);
    audio_chain_add_by_name(buf);
    audio_chain_add_by_name("of");
    snprintf(buf, sizeof(buf), "num%d", total_rounds);
    audio_chain_add_by_name(buf);


    if(is_work){
        audio_chain_add_by_name("workfor");
    } else {
        audio_chain_add_by_name("restfor");
    }
    //Convert n_min_remaining to const char*
    snprintf(buf, sizeof(buf), "num%d", minutes);
    audio_chain_add_by_name(buf);
    audio_chain_add_by_name("minutes");
    sleep(1);
    maybe_add_message();
    audio_chain_play();
    audio_chain_reset();

}

/* Called every second – advances the timer, plays sounds, etc. */
static void tick(void)
{
    //We use this for str formatting
    char buf[16];
    int n;

    if (timer.state != RUNNING)
        return;

    timer.sec_remaining--;
    if (timer.sec_remaining <= 0) {
        if (timer.in_work) {
            /* work finished → start rest */
            announce_start_of_round(
                                    timer.cur_round + 1,
                                    timer.rounds,
                                    timer.rest_sec / 60,
                                    false);
            timer.in_work = false;
            timer.sec_remaining = timer.rest_sec;
        } else {
            /* rest finished → next round or stop */
            timer.cur_round++;
            if (timer.cur_round >= timer.rounds) {
                /* all rounds finished */
                audio_chain_add_by_name("done");
                audio_chain_play();
                audio_chain_reset();
                timer.state = IDLE;
                fprintf(stderr, "Tabata complete.\n");
                return;
            }

            timer.in_work = true;
            timer.sec_remaining = timer.work_sec;

            announce_start_of_round(
                                    timer.cur_round + 1,
                                    timer.rounds,
                                    timer.work_sec / 60,
                                    true);
        }
    } else {
        //Check if timer.sec_remaining is divisible by 5 minutes:
        if (timer.sec_remaining % 300 == 0) {
            n = timer.sec_remaining / 60;
            audio_chain_add_by_name("youhave");
            //Convert n_min_remaining to const char*
            snprintf(buf, sizeof(buf), "num%d", n);
            audio_chain_add_by_name(buf);
            audio_chain_add_by_name("minutesleft");

            if(timer.in_work){
                audio_chain_add_by_name("towork");
            }
            else {
                audio_chain_add_by_name("torest");
            }

            sleep(1);

            maybe_add_message();
            audio_chain_play();
            audio_chain_reset();
        }
    }
}

/* ----------------------------------------------------------------------
   Command processing (client → daemon)
   ---------------------------------------------------------------------- */
static void handle_command(const char *cmd, int client_fd)
{
    char reply[128] = {0};

    if (strncmp(cmd, "start", 5) == 0) {
        int w, r, n;
        if (sscanf(cmd + 5, "%d %d %d", &w, &r, &n) != 3) {
            snprintf(reply, sizeof(reply), "ERR Invalid start parameters\n");
        } else if (timer.state == RUNNING) {
            snprintf(reply, sizeof(reply), "ERR Timer already running\n");
        } else {
            timer.work_sec = w;
            timer.rest_sec = r;
            timer.rounds   = n;
            timer.cur_round = 0;
            timer.in_work = true;
            timer.sec_remaining = w;
            timer.state = RUNNING;

            announce_start_of_round(
                                    1,
                                    timer.rounds,
                                    timer.work_sec / 60,
                                    true);

            //These variables are only used here
            snprintf(reply, sizeof(reply), "OK Started\n");
        }
    } else if (strcmp(cmd, "stop") == 0) {
        if (timer.state == IDLE) {
            snprintf(reply, sizeof(reply), "ERR Not running\n");
        } else {
            timer.state = IDLE;
            snprintf(reply, sizeof(reply), "OK Stopped\n");
        }
    } else if (strcmp(cmd, "status") == 0) {
        if (timer.state == IDLE) {
            snprintf(reply, sizeof(reply), "IDLE\n");
        } else {
            const char *phase = timer.in_work ? "WORK" : "REST";
            snprintf(reply, sizeof(reply),
                     "RUNNING round %d/%d %s %d sec left\n",
                     timer.cur_round + 1, timer.rounds, phase,
                     timer.sec_remaining);
        }
    } else if (strcmp(cmd, "quit") == 0) {
        snprintf(reply, sizeof(reply), "OK Bye\n");
        write(client_fd, reply, strlen(reply));
        /* Tell main loop to exit */
        exit(EXIT_SUCCESS);
    } else {
        snprintf(reply, sizeof(reply), "ERR Unknown command\n");
    }

    write(client_fd, reply, strlen(reply));
}

/* ----------------------------------------------------------------------
   Main daemon event loop
   ---------------------------------------------------------------------- */
static void daemon_loop(void)
{
    int listen_fd, timer_fd;
    struct sockaddr_un addr;

    listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);
    unlink(SOCK_PATH);               // remove stale socket, if any
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    if (listen(listen_fd, 5) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    atexit(cleanup_socket);          // ensure socket file is removed

    //Audio setup
    if (!audio_init()) exit(EXIT_FAILURE);
    atexit(audio_cleanup);
    if (!audio_chain_init()) exit(EXIT_FAILURE);
    atexit(audio_chain_cleanup);

    timer_fd = make_timerfd();

    fd_set readset;
    for (;;) {
        FD_ZERO(&readset);
        FD_SET(listen_fd, &readset);
        FD_SET(timer_fd, &readset);
        int maxfd = (listen_fd > timer_fd) ? listen_fd : timer_fd;

        int rc = select(maxfd + 1, &readset, NULL, NULL, NULL);
        if (rc == -1) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        /* ----- timer tick ----- */
        if (FD_ISSET(timer_fd, &readset)) {
            uint64_t expirations;
            if (read(timer_fd, &expirations, sizeof(expirations)) == sizeof(expirations)) {
                /* If we missed several seconds, process them one‑by‑one */
                for (uint64_t i = 0; i < expirations; ++i)
                    tick();
            }
        }

        /* ----- new client connection ----- */
        if (FD_ISSET(listen_fd, &readset)) {
            int client_fd = accept(listen_fd, NULL, NULL);
            if (client_fd == -1) {
                perror("accept");
                continue;
            }

            /* read a single line command (max MAX_CMD_LEN) */
            char cmd[MAX_CMD_LEN] = {0};
            ssize_t n = read(client_fd, cmd, sizeof(cmd) - 1);
            if (n > 0) {
                /* strip trailing newline */
                cmd[strcspn(cmd, "\r\n")] = '\0';
                handle_command(cmd, client_fd);
            }
            close(client_fd);
        }
    }

    close(timer_fd);
    close(listen_fd);
}

/* ----------------------------------------------------------------------
   Client helper – send a command to the daemon and print the reply
   ---------------------------------------------------------------------- */
static void client_send(const char *cmd, const char *my_path)
{
    struct sockaddr_un addr;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCK_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        if (errno == ENOENT || errno == ECONNREFUSED) {
            /* stale socket – delete it and try to spawn the daemon */
            unlink(SOCK_PATH);
            pid_t pid = fork();
            if (pid == 0) {
                execlp(my_path, my_path, "--daemon", (char *)NULL);
                _exit(EXIT_FAILURE);
            }
            close(fd);
            sleep(1);                 /* give daemon time to bind */
            client_send(cmd, my_path);/* retry */
            return;
        }
        perror("connect");
        close(fd);
        exit(EXIT_FAILURE);
    }

    /* ----- send the command ----- */
    write(fd, cmd, strlen(cmd));
    write(fd, "\n", 1);

    char reply[256];
    ssize_t n = read(fd, reply, sizeof(reply) - 1);
    if (n > 0) {
        reply[n] = '\0';
        fputs(reply, stdout);
    }
    close(fd);
}

/* ----------------------------------------------------------------------
   Main – decides client vs daemon mode
   ---------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    if (argc > 1 && strcmp(argv[1], "--daemon") == 0) {
        /* ---------- Daemon mode ---------- */
        if (daemon(0, 0) == -1) {
            perror("daemon");
            exit(EXIT_FAILURE);
        }
        setup_signal_handlers();
        daemon_loop();          /* never returns */
        return 0;
    }

    /* ---------- Client mode ---------- */
    if (argc < 2) {
        fprintf(stderr,
                "Usage: %s <command> [args]\n"
                "Commands:\n"
                "  start <work_sec> <rest_sec> <rounds>\n"
                "  stop\n"
                "  status\n"
                "  quit   (stop daemon)\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    /* Build the command string that will be sent to the daemon */
    char cmd_buf[MAX_CMD_LEN] = {0};

    if (strcmp(argv[1], "start") == 0) {
        if (argc != 5) {
            fprintf(stderr, "start needs three numbers: work rest rounds\n");
            return EXIT_FAILURE;
        }
        snprintf(cmd_buf, sizeof(cmd_buf), "start %s %s %s",
                 argv[2], argv[3], argv[4]);
    } else if (strcmp(argv[1], "stop") == 0) {
        strcpy(cmd_buf, "stop");
    } else if (strcmp(argv[1], "status") == 0) {
        strcpy(cmd_buf, "status");
    } else if (strcmp(argv[1], "quit") == 0) {
        strcpy(cmd_buf, "quit");
    } else {
        fprintf(stderr, "Unknown command '%s'\n", argv[1]);
        return EXIT_FAILURE;
    }

    /* Pass argv[0] (the executable path) to client_send() */
    client_send(cmd_buf, argv[0]);

    return EXIT_SUCCESS;
}
