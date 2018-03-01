/* An implementation of a SPICE streaming agent
 *
 * \copyright
 * Copyright 2016-2017 Red Hat Inc. All rights reserved.
 */

#include "concrete-agent.hpp"
#include "stream.hpp"
#include "message.hpp"
#include "frame-log.hpp"
#include "x11-cursor.hpp"
#include "mjpeg-fallback.hpp"

#include <spice/stream-device.h>
#include <spice/enums.h>

#include <spice-streaming-agent/frame-capture.hpp>
#include <spice-streaming-agent/plugin.hpp>
#include <spice-streaming-agent/errors.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <syslog.h>
#include <signal.h>
#include <exception>
#include <stdexcept>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
#include <functional>

using namespace spice::streaming_agent;

volatile bool quit_requested = false;

static void handle_interrupt(int intr)
{
    syslog(LOG_INFO, "Got signal %d, exiting", intr);
    quit_requested = true;
}

static void register_interrupts(void)
{
    struct sigaction sa = { };
    sa.sa_handler = handle_interrupt;
    if ((sigaction(SIGINT, &sa, NULL) != 0) &&
        (sigaction(SIGTERM, &sa, NULL) != 0)) {
        syslog(LOG_WARNING, "failed to register signal handler %m");
    }
}

static void usage(const char *progname)
{
    printf("usage: %s <options>\n", progname);
    printf("options are:\n");
    printf("\t-p portname  -- virtio-serial port to use\n");
    printf("\t-l file -- log frames to file\n");
    printf("\t--log-binary -- log binary frames (following -l)\n");
    printf("\t--plugins-dir=path -- change plugins directory\n");
    printf("\t-d -- enable debug logs\n");
    printf("\t-c variable=value -- change settings\n");
    printf("\t\tframerate = 1-100 (check 10,20,30,40,50,60)\n");
    printf("\n");
    printf("\t-h or --help     -- print this help message\n");

    exit(1);
}

#define arg_error(...) syslog(LOG_ERR, ## __VA_ARGS__);

int main(int argc, char* argv[])
{
    const char *streamport = "/dev/virtio-ports/com.redhat.stream.0";
    int opt;
    const char *log_filename = NULL;
    bool log_binary = false;
    int logmask = LOG_UPTO(LOG_WARNING);
    const char *pluginsdir = PLUGINSDIR;
    enum {
        OPT_first = UCHAR_MAX,
        OPT_PLUGINS_DIR,
        OPT_LOG_BINARY,
    };
    static const struct option long_options[] = {
        { "plugins-dir", required_argument, NULL, OPT_PLUGINS_DIR},
        { "log-binary", no_argument, NULL, OPT_LOG_BINARY},
        { "help", no_argument, NULL, 'h'},
        { 0, 0, 0, 0}
    };

    openlog("spice-streaming-agent",
            isatty(fileno(stderr)) ? (LOG_PERROR|LOG_PID) : LOG_PID, LOG_USER);

    setlogmask(logmask);

    ConcreteAgent agent;
    while ((opt = getopt_long(argc, argv, "hp:c:l:d", long_options, NULL)) != -1) {
        switch (opt) {
        case 0:
            /* Handle long options if needed */
            break;
        case OPT_PLUGINS_DIR:
            pluginsdir = optarg;
            break;
        case 'p':
            streamport = optarg;
            break;
        case 'c': {
            char *p = strchr(optarg, '=');
            if (p == NULL) {
                arg_error("wrong 'c' argument %s\n", optarg);
                usage(argv[0]);
            }
            *p++ = '\0';
            agent.AddOption(optarg, p);
            break;
        }
        case OPT_LOG_BINARY:
            log_binary = true;
            break;
        case 'l':
            log_filename = optarg;
            break;
        case 'd':
            logmask = LOG_UPTO(LOG_DEBUG);
            setlogmask(logmask);
            break;
        case 'h':
            usage(argv[0]);
            break;
        }
    }

    register_interrupts();

    int ret = EXIT_SUCCESS;
    try {
        // register built-in plugins
        MjpegPlugin::Register(&agent);
        agent.LoadPlugins(pluginsdir);
        Stream stream(streamport);
        FrameLog frame_log(log_filename, log_binary);
        X11CursorThread cursor_thread(stream);
        agent.CaptureLoop(stream, frame_log);
    }
    catch (Error &err) {
        err.syslog();
        ret = EXIT_FAILURE;
    }
    catch (std::exception &err) {
        syslog(LOG_ERR, "%s\n", err.what());
        ret = EXIT_FAILURE;
    }

    closelog();
    return ret;
}
