/* An implementation of a SPICE streaming agent
 *
 * \copyright
 * Copyright 2016-2017 Red Hat Inc. All rights reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <poll.h>
#include <syslog.h>
#include <signal.h>
#include <exception>
#include <stdexcept>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include <spice/stream-device.h>
#include <spice/enums.h>

#include <spice-streaming-agent/frame-capture.hpp>
#include <spice-streaming-agent/plugin.hpp>

#include "hexdump.h"
#include "concrete-agent.hpp"

using namespace std;
using namespace SpiceStreamingAgent;

static ConcreteAgent agent;

struct SpiceStreamFormatMessage
{
    StreamDevHeader hdr;
    StreamMsgFormat msg;
};

struct SpiceStreamDataMessage
{
    StreamDevHeader hdr;
    StreamMsgData msg;
};

static int streaming_requested;
static bool quit;
static int streamfd = -1;
static bool stdin_ok;
static int log_binary = 0;
static std::mutex stream_mtx;

static int have_something_to_read(int *pfd, int timeout)
{
    int nfds;
    struct pollfd pollfds[2] = {
        {streamfd, POLLIN, 0},
        {0, POLLIN, 0}
    };
    *pfd = -1;
    nfds = (stdin_ok ? 2 : 1);
    if (poll(pollfds, nfds, timeout) < 0) {
        syslog(LOG_ERR, "poll FAILED\n");
        return -1;
    }
    if (pollfds[0].revents == POLLIN) {
        *pfd = streamfd;
    }
    if (pollfds[1].revents == POLLIN) {
        *pfd = 0;
    }
    return *pfd != -1;
}

static int read_command_from_stdin(void)
{
    char buffer[64], *p, *save = NULL;

    p = fgets(buffer, sizeof(buffer), stdin);
    if (p == NULL) {
        syslog(LOG_ERR, "Failed to read from stdin\n");
        return -1;
    }
    const char *cmd = strtok_r(buffer, " \t\n\r", &save);
    if (!cmd)
        return 1;
    if (strcmp(cmd, "quit") == 0) {
        quit = true;
    } else if (strcmp(cmd, "start") == 0) {
	streaming_requested = 1;
    } else if (strcmp(cmd, "stop") == 0) {
	streaming_requested = 0;
    } else {
        syslog(LOG_WARNING, "unknown command %s\n", cmd);
    }
    return 1;
}

static int read_command_from_device(void)
{
    StreamDevHeader hdr;
    uint8_t msg[64];
    int n;

    std::lock_guard<std::mutex> stream_guard(stream_mtx);
    n = read(streamfd, &hdr, sizeof(hdr));
    if (n != sizeof(hdr)) {
        syslog(LOG_WARNING,
               "read command from device FAILED -- read %d expected %lu\n",
               n, sizeof(hdr));
        return -1;
    }
    if (hdr.protocol_version != STREAM_DEVICE_PROTOCOL) {
        syslog(LOG_WARNING, "BAD VERSION %d (expected is %d)\n", hdr.protocol_version,
               STREAM_DEVICE_PROTOCOL);
        return 0; // return -1; -- fail over this ?
    }
    if (hdr.type != STREAM_TYPE_START_STOP) {
        syslog(LOG_WARNING, "UNKNOWN msg of type %d\n", hdr.type);
        return 0; // return -1;
    }
    if (hdr.size >= sizeof(msg)) {
        syslog(LOG_WARNING,
               "msg size (%d) is too long (longer than %lu)\n",
               hdr.size, sizeof(msg));
        return 0; // return -1;
    }
    n = read(streamfd, &msg, hdr.size);
    if (n != hdr.size) {
        syslog(LOG_WARNING,
               "read command from device FAILED -- read %d expected %d\n",
               n, hdr.size);
        return -1;
    }
    streaming_requested = msg[0]; /* num_codecs */
    syslog(LOG_INFO, "GOT START_STOP message -- request to %s streaming\n",
           streaming_requested ? "START" : "STOP");
    return 1;
}

static int read_command(bool blocking)
{
    int fd, n=1;
    int timeout = blocking?-1:0;
    while (!quit) {
        if (!have_something_to_read(&fd, timeout)) {
            if (!blocking) {
                return 0;
            }
            sleep(1);
            continue;
        }
        if (fd) {
            n = read_command_from_device();
        } else {
            n = read_command_from_stdin();
        }
	break;
    }
    return n;
}

static size_t
write_all(int fd, const void *buf, const size_t len)
{
    size_t written = 0;
    while (written < len) {
        int l = write(fd, (const char *) buf + written, len - written);
        if (l < 0 && errno == EINTR) {
            continue;
        }
        if (l < 0) {
            syslog(LOG_ERR, "write failed - %m");
            return l;
        }
        written += l;
    }
    syslog(LOG_DEBUG, "write_all -- %u bytes written\n", (unsigned)written);
    return written;
}

static int spice_stream_send_format(unsigned w, unsigned h, unsigned c)
{

    SpiceStreamFormatMessage msg;
    const size_t msgsize = sizeof(msg);
    const size_t hdrsize  = sizeof(msg.hdr);
    memset(&msg, 0, msgsize);
    msg.hdr.protocol_version = STREAM_DEVICE_PROTOCOL;
    msg.hdr.type = STREAM_TYPE_FORMAT;
    msg.hdr.size = msgsize - hdrsize; /* includes only the body? */
    msg.msg.width = w;
    msg.msg.height = h;
    msg.msg.codec = c;
    syslog(LOG_DEBUG, "writing format\n");
    std::lock_guard<std::mutex> stream_guard(stream_mtx);
    if (write_all(streamfd, &msg, msgsize) != msgsize) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

static int spice_stream_send_frame(const void *buf, const unsigned size)
{
    SpiceStreamDataMessage msg;
    const size_t msgsize = sizeof(msg);
    ssize_t n;

    memset(&msg, 0, msgsize);
    msg.hdr.protocol_version = STREAM_DEVICE_PROTOCOL;
    msg.hdr.type = STREAM_TYPE_DATA;
    msg.hdr.size = size; /* includes only the body? */
    std::lock_guard<std::mutex> stream_guard(stream_mtx);
    n = write_all(streamfd, &msg, msgsize);
    syslog(LOG_DEBUG,
           "wrote %ld bytes of header of data msg with frame of size %u bytes\n",
           n, msg.hdr.size);
    if (n != msgsize) {
        syslog(LOG_WARNING, "write_all header: wrote %ld expected %lu\n",
               n, msgsize);
        return EXIT_FAILURE;
    }
    n = write_all(streamfd, buf, size);
    syslog(LOG_DEBUG, "wrote data msg body of size %ld\n", n);
    if (n != size) {
        syslog(LOG_WARNING, "write_all header: wrote %ld expected %u\n",
               n, size);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/* returns current time in micro-seconds */
static uint64_t get_time(void)
{
    struct timeval now;

    gettimeofday(&now, NULL);

    return (uint64_t)now.tv_sec * 1000000 + (uint64_t)now.tv_usec;

}

static void handle_interrupt(int intr)
{
    syslog(LOG_INFO, "Got signal %d, exiting", intr);
    quit = true;
}

static void register_interrupts(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
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
    printf("\t-i accept commands from stdin\n");
    printf("\t-l file -- log frames to file\n");
    printf("\t--log-binary -- log binary frames (following -l)\n");
    printf("\t-d -- enable debug logs\n");
    printf("\t-c variable=value -- change settings\n");
    printf("\t\tprofile = [0, 1, 66, 77, 100, 244]\n");
    printf("\t\tratecontrol = constqp/vbr/cbr/2passq/2passf/2passi\n");
    printf("\t\tdwqp = 0-51\n");
    printf("\t\tframerate = 1-100 (check 10,20,30,40,50,60)\n");
    printf("\n");
    printf("\t-h or --help     -- print this help message\n");

    exit(1);
}

static void send_cursor(const XFixesCursorImage &image)
{
    if (image.width >= STREAM_MSG_CURSOR_SET_MAX_WIDTH ||
        image.height >= STREAM_MSG_CURSOR_SET_MAX_HEIGHT)
        return;

    size_t cursor_size =
        sizeof(StreamDevHeader) + sizeof(StreamMsgCursorSet) +
        image.width * image.height * sizeof(uint32_t);
    std::unique_ptr<uint8_t[]> msg(new uint8_t[cursor_size]);

    StreamDevHeader &dev_hdr(*reinterpret_cast<StreamDevHeader*>(msg.get()));
    memset(&dev_hdr, 0, sizeof(dev_hdr));
    dev_hdr.protocol_version = STREAM_DEVICE_PROTOCOL;
    dev_hdr.type = STREAM_TYPE_CURSOR_SET;
    dev_hdr.size = cursor_size - sizeof(StreamDevHeader);

    StreamMsgCursorSet &cursor_msg(*reinterpret_cast<StreamMsgCursorSet *>(msg.get() + sizeof(StreamDevHeader)));
    memset(&cursor_msg, 0, sizeof(cursor_msg));

    cursor_msg.type = SPICE_CURSOR_TYPE_ALPHA;
    cursor_msg.width = image.width;
    cursor_msg.height = image.height;
    cursor_msg.hot_spot_x = image.xhot;
    cursor_msg.hot_spot_y = image.yhot;

    uint32_t *pixels = reinterpret_cast<uint32_t *>(cursor_msg.data);
    for (unsigned i = 0; i < image.width * image.height; ++i)
        pixels[i] = image.pixels[i];

    std::lock_guard<std::mutex> stream_guard(stream_mtx);
    write_all(streamfd, msg.get(), cursor_size);
}

static void cursor_changes(Display *display, int event_base)
{
    unsigned long last_serial = 0;

    while (1) {
        XEvent event;
        XNextEvent(display, &event);
        if (event.type != event_base + 1)
            continue;

        XFixesCursorImage *cursor = XFixesGetCursorImage(display);
        if (!cursor)
            continue;

        if (cursor->cursor_serial == last_serial)
            continue;

        last_serial = cursor->cursor_serial;
        send_cursor(*cursor);
    }
}

static void
do_capture(const char *streamport, FILE *f_log)
{
    std::unique_ptr<FrameCapture> capture(agent.GetBestFrameCapture());
    if (!capture)
        throw std::runtime_error("cannot find a suitable capture system");

    streamfd = open(streamport, O_RDWR);
    if (streamfd < 0)
        // TODO was syslog(LOG_ERR, "Failed to open %s: %s\n", streamport, strerror(errno));
        throw std::runtime_error("failed to open streaming device");

    unsigned int frame_count = 0;
    while (! quit) {
        while (!quit && !streaming_requested) {
            if (read_command(1) < 0) {
                syslog(LOG_ERR, "FAILED to read command\n");
                goto done;
            }
        }

        syslog(LOG_INFO, "streaming starts now\n");
        uint64_t time_last = 0;

        while (!quit && streaming_requested) {
            if (++frame_count % 100 == 0) {
                syslog(LOG_DEBUG, "SENT %d frames\n", frame_count);
            }
            uint64_t time_before = get_time();

            FrameInfo frame = capture->CaptureFrame();

            uint64_t time_after = get_time();
            syslog(LOG_DEBUG,
                   "got a frame -- size is %zu (%lu ms) (%lu ms from last frame)(%lu us)\n",
                   frame.buffer_size, (time_after - time_before)/1000,
                   (time_after - time_last)/1000,
                   (time_before - time_last));
            time_last = time_after;

            if (frame.stream_start) {
                unsigned width, height;
                unsigned char codec;

                width = frame.size.width;
                height = frame.size.height;
                codec = capture->VideoCodecType();

                syslog(LOG_DEBUG, "wXh %uX%u  codec=%u\n", width, height, codec);

                if (spice_stream_send_format(width, height, codec) == EXIT_FAILURE)
                    throw std::runtime_error("FAILED to send format message");
            }
            if (f_log) {
                if (log_binary) {
                    fwrite(frame.buffer, frame.buffer_size, 1, f_log);
                } else {
                    fprintf(f_log, "%lu: Frame of %zu bytes:\n", get_time(), frame.buffer_size);
                    hexdump(frame.buffer, frame.buffer_size, f_log);
                }
            }
            if (spice_stream_send_frame(frame.buffer, frame.buffer_size) == EXIT_FAILURE) {
                syslog(LOG_ERR, "FAILED to send a frame\n");
                break;
            }
            //usleep(1);
            if (read_command(0) < 0) {
                syslog(LOG_ERR, "FAILED to read command\n");
                goto done;
            }
            if (!streaming_requested) {
                capture->Reset();
            }
        }
    }

done:
    if (streamfd >= 0) {
        close(streamfd);
        streamfd = -1;
    }
}

#define arg_error(...) syslog(LOG_ERR, ## __VA_ARGS__);

int main(int argc, char* argv[])
{
    const char *streamport = "/dev/virtio-ports/com.redhat.stream.0";
    char opt;
    const char *log_filename = NULL;
    int logmask = LOG_UPTO(LOG_WARNING);
    struct option long_options[] = {
        { "log-binary", no_argument, &log_binary, 1},
        { "help", no_argument, NULL, 'h'},
        { 0, 0, 0, 0}
    };

    if (isatty(fileno(stderr)) && isatty(fileno(stdin))) {
        stdin_ok = true;
    }

    openlog("spice-streaming-agent", stdin_ok? (LOG_PERROR|LOG_PID) : LOG_PID, LOG_USER);
    setlogmask(logmask);

    while ((opt = getopt_long(argc, argv, "hip:c:l:d", long_options, NULL)) != -1) {
	switch (opt) {
        case 0:
            /* Handle long options if needed */
            break;
	case 'i':
            stdin_ok = true;
            openlog("spice-streaming-agent", LOG_PERROR|LOG_PID, LOG_USER);
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

    agent.LoadPlugins(PLUGINSDIR);

    register_interrupts();

    FILE *f_log = NULL;
    if (log_filename) {
        f_log = fopen(log_filename, "wb");
        if (!f_log) {
            syslog(LOG_ERR, "Failed to open log file '%s': %s\n",
                   log_filename, strerror(errno));
            return EXIT_FAILURE;
        }
    }

    Display *display = XOpenDisplay(NULL);
    if (display == NULL) {
        syslog(LOG_ERR, "failed to open display\n");
        return EXIT_FAILURE;
    }
    int event_base, error_base;
    if (!XFixesQueryExtension(display, &event_base, &error_base)) {
        syslog(LOG_ERR, "XFixesQueryExtension failed\n");
        return EXIT_FAILURE;
    }
    Window rootwindow = DefaultRootWindow(display);
    XFixesSelectCursorInput(display, rootwindow, XFixesDisplayCursorNotifyMask);

    std::thread cursor_th(cursor_changes, display, event_base);
    cursor_th.detach();

    int ret = EXIT_SUCCESS;
    try {
        do_capture(streamport, f_log);
    }
    catch (std::runtime_error &err) {
        syslog(LOG_ERR, "%s\n", err.what());
        ret = EXIT_FAILURE;
    }

    if (f_log) {
        fclose(f_log);
        f_log = NULL;
    }
    closelog();
    return ret;
}
