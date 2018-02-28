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

namespace spice
{
namespace streaming_agent
{

class FormatMessage : public Message<StreamMsgFormat, FormatMessage, STREAM_TYPE_FORMAT>
{
public:
    FormatMessage(unsigned w, unsigned h, uint8_t c) : Message(w, h, c) {}
    static size_t size(unsigned width, unsigned height, uint8_t codec)
    {
        return sizeof(payload_t);
    }
    void write_message_body(Stream &stream, unsigned w, unsigned h, uint8_t c)
    {
        StreamMsgFormat msg = { .width = w, .height = h, .codec = c, .padding1 = {} };
        stream.write_all("format", &msg, sizeof(msg));
    }
};

class FrameMessage : public Message<StreamMsgData, FrameMessage, STREAM_TYPE_DATA>
{
public:
    FrameMessage(const void *frame, size_t length) : Message(frame, length) {}
    static size_t size(const void *frame, size_t length)
    {
        return sizeof(payload_t) + length;
    }
    void write_message_body(Stream &stream, const void *frame, size_t length)
    {
        stream.write_all("frame", frame, length);
    }
};

}} // namespace spice::streaming_agent

bool quit_requested = false;

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


void ConcreteAgent::CaptureLoop(Stream &stream, FrameLog &frame_log)
{
    unsigned int frame_count = 0;
    while (!quit_requested) {
        while (!quit_requested && !stream.streaming_requested()) {
            if (stream.read_command(true) < 0) {
                syslog(LOG_ERR, "FAILED to read command\n");
                return;
            }
        }

        if (quit_requested) {
            return;
        }

        syslog(LOG_INFO, "streaming starts now\n");
        uint64_t time_last = 0;

        std::unique_ptr<FrameCapture> capture(GetBestFrameCapture(stream.client_codecs()));
        if (!capture) {
            throw Error("cannot find a suitable capture system");
        }

        while (!quit_requested && stream.streaming_requested()) {
            if (++frame_count % 100 == 0) {
                syslog(LOG_DEBUG, "SENT %d frames\n", frame_count);
            }
            uint64_t time_before = get_time();

            FrameInfo frame = capture->CaptureFrame();

            uint64_t time_after = get_time();
            syslog(LOG_DEBUG,
                   "got a frame -- size is %zu (%" PRIu64 " ms) "
                   "(%" PRIu64 " ms from last frame)(%" PRIu64 " us)\n",
                   frame.buffer_size, (time_after - time_before)/1000,
                   (time_after - time_last)/1000,
                   (time_before - time_last));
            time_last = time_after;

            if (frame.stream_start) {
                unsigned width, height;
                uint8_t codec;

                width = frame.size.width;
                height = frame.size.height;
                codec = capture->VideoCodecType();

                syslog(LOG_DEBUG, "wXh %uX%u  codec=%u\n", width, height, codec);

                stream.send<FormatMessage>(width, height, codec);
            }
            if (frame_log) {
                frame_log.dump(frame.buffer, frame.buffer_size);
            }
            stream.send<FrameMessage>(frame.buffer, frame.buffer_size);
            //usleep(1);
            if (stream.read_command(false) < 0) {
                syslog(LOG_ERR, "FAILED to read command\n");
                return;
            }
        }
    }
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
    while ((opt = getopt_long(argc, argv, "bhp:c:l:d", long_options, NULL)) != -1) {
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
        X11CursorThread cursor_thread(stream);
        FrameLog frame_log(log_filename, log_binary);
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
    catch (...) {
        syslog(LOG_ERR, "Unexpected exception caught (probably thronw by plugin init");
        ret = EXIT_FAILURE;
    }

    closelog();
    return ret;
}
