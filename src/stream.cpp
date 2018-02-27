/* Encapsulation of the stream used to communicate between agent and server
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#include "stream.hpp"
#include "message.hpp"

#include <spice/stream-device.h>

#include <spice-streaming-agent/errors.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>
#include <syslog.h>
#include <unistd.h>

namespace spice
{
namespace streaming_agent
{

class CapabilitiesMessage : public Message<StreamMsgData, CapabilitiesMessage,
                                           STREAM_TYPE_CAPABILITIES>
{
public:
    CapabilitiesMessage() : Message() {}
    static size_t size()
    {
        return sizeof(payload_t);
    }
    void write_message_body(Stream &stream)
    {
        /* No body for capabilities message */
    }
};

Stream::Stream(const char *name)
    : codecs()
{
    streamfd = open(name, O_RDWR);
    if (streamfd < 0) {
        throw IOError("failed to open streaming device", errno);
    }
}

Stream::~Stream()
{
    close(streamfd);
}

int Stream::have_something_to_read(int timeout)
{
    struct pollfd pollfd = {streamfd, POLLIN, 0};

    if (poll(&pollfd, 1, timeout) < 0) {
        syslog(LOG_ERR, "poll FAILED\n");
        return -1;
    }

    if (pollfd.revents == POLLIN) {
        return 1;
    }

    return 0;
}

void Stream::handle_stream_start_stop(uint32_t len)
{
    uint8_t msg[256];

    if (len >= sizeof(msg)) {
        throw MessageDataError("message is too long", len, sizeof(msg));
    }
    int n = read(streamfd, &msg, len);
    if (n != (int) len) {
        throw MessageDataError("read start/stop command from device failed", n, len, errno);
    }
    is_streaming = (msg[0] != 0); /* num_codecs */
    syslog(LOG_INFO, "GOT START_STOP message -- request to %s streaming\n",
           is_streaming ? "START" : "STOP");
    codecs.clear();
    for (int i = 1; i <= msg[0]; ++i) {
        codecs.insert((SpiceVideoCodecType) msg[i]);
    }
}

void Stream::handle_stream_capabilities(uint32_t len)
{
    uint8_t caps[STREAM_MSG_CAPABILITIES_MAX_BYTES];

    if (len > sizeof(caps)) {
        throw MessageDataError("capability message too long", len, sizeof(caps));
    }
    int n = read(streamfd, caps, len);
    if (n != (int) len) {
        throw MessageDataError("read capabilities from device failed", n, len, errno);
    }

    // we currently do not support extensions so just reply so
    send<CapabilitiesMessage>();
}

void Stream::handle_stream_error(uint32_t len)
{
    // TODO read message and use it
    throw ProtocolError("got an error message from server");
}

void Stream::read_command_from_device()
{
    StreamDevHeader hdr;
    int n;

    std::lock_guard<std::mutex> stream_guard(mutex);
    n = read(streamfd, &hdr, sizeof(hdr));
    if (n != sizeof(hdr)) {
        throw MessageDataError("read command from device failed", n, sizeof(hdr), errno);
    }
    if (hdr.protocol_version != STREAM_DEVICE_PROTOCOL) {
        throw MessageDataError("bad protocol version", hdr.protocol_version, STREAM_DEVICE_PROTOCOL);
    }

    switch (hdr.type) {
    case STREAM_TYPE_CAPABILITIES:
        return handle_stream_capabilities(hdr.size);
    case STREAM_TYPE_NOTIFY_ERROR:
        return handle_stream_error(hdr.size);
    case STREAM_TYPE_START_STOP:
        return handle_stream_start_stop(hdr.size);
    }
    throw MessageDataError("unknown message type", hdr.type, 0);
}

int Stream::read_command(bool blocking)
{
    int timeout = blocking?-1:0;
    while (!quit_requested) {
        if (!have_something_to_read(timeout)) {
            if (!blocking) {
                return 0;
            }
            sleep(1);
            continue;
        }
        read_command_from_device();
        break;
    }

    return 1;
}

void Stream::write_all(const char *operation, const void *buf, const size_t len)
{
    size_t written = 0;
    while (written < len) {
        int l = write(streamfd, (const char *) buf + written, len - written);
        if (l < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw WriteError("write failed", operation, errno).syslog();
        }
        written += l;
    }
    syslog(LOG_DEBUG, "write_all -- %zu bytes written\n", written);
}

}} // namespace spice::streaming_agent
