/* Encapsulation of the stream used to communicate between agent and server
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#include "stream.hpp"
#include "message.hpp"
#include "concrete-agent.hpp"

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

class CapabilitiesMessage : public OutMessage<StreamMsgData, CapabilitiesMessage,
                                              STREAM_TYPE_CAPABILITIES>
{
public:
    CapabilitiesMessage() : OutMessage() {}
    static size_t size()
    {
        return sizeof(MessagePayload);
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
        ConcreteAgent::check_if_quitting();
        throw IOError("poll failed", errno);
    }

    if (pollfd.revents == POLLIN) {
        return 1;
    }

    return 0;
}

void Stream::read_all(const char *operation, void *msg, size_t len)
{
    while (len > 0) {
        ssize_t n = read(streamfd, msg, len);
        ConcreteAgent::check_if_quitting();
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw ReadError("reading message from device failed", operation, errno);
        }

        len -= n;
        msg = (uint8_t *) msg + n;
    }
}

void Stream::write_all(const char *operation, const void *buf, size_t len)
{
    size_t written = 0;
    while (written < len) {
        int l = write(streamfd, (const char *) buf + written, len - written);
        ConcreteAgent::check_if_quitting();
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

template<>
void Stream::handle<StreamMsgStartStop>(StreamMsgStartStop &msg)
{
    is_streaming = (msg.num_codecs != 0);
    syslog(LOG_INFO, "GOT START_STOP message -- request to %s streaming\n",
           is_streaming ? "START" : "STOP");
    codecs.clear();
    uint8_t *codecs_ptr = msg.codecs;
    for (int i = 0; i < msg.num_codecs; ++i) {
        codecs.insert((SpiceVideoCodecType) codecs_ptr[i]);
    }
}

template<>
void Stream::handle<StreamMsgCapabilities>(StreamMsgCapabilities &msg)
{
    send<CapabilitiesMessage>();
}

template<>
void Stream::handle<StreamMsgNotifyError>(size_t size, const char *operation)
{
    InMessage<StreamMsgNotifyError> message(*this, size, operation);
    StreamMsgNotifyError &msg = message.payload();
    int text_length = int(size - sizeof(StreamMsgNotifyError));
    syslog(LOG_ERR, "Received NotifyError message from the server: %d - %.*s\n",
           msg.error_code, text_length, msg.msg);
}

void Stream::read_command_from_device()
{
    StreamDevHeader hdr;
    std::lock_guard<std::mutex> stream_guard(mutex);
    read_all("command", &hdr, sizeof(hdr));
    if (hdr.protocol_version != STREAM_DEVICE_PROTOCOL) {
        throw MessageDataError("bad protocol version", "command from device",
                               hdr.protocol_version, STREAM_DEVICE_PROTOCOL);
    }

    const size_t max_reasonable_message_size = 1024 * 1024;
    if (hdr.size >= max_reasonable_message_size) {
        throw MessageLengthError("incoming command", hdr.size, max_reasonable_message_size);
    }

    switch (hdr.type) {
    case STREAM_TYPE_CAPABILITIES:
        return handle<StreamMsgCapabilities>(hdr.size, "capabilities");
    case STREAM_TYPE_NOTIFY_ERROR:
        return handle<StreamMsgNotifyError>(hdr.size, "error notification");
    case STREAM_TYPE_START_STOP:
        return handle<StreamMsgStartStop>(hdr.size, "start/stop");
    }

    throw MessageDataError("unknown message type", "command from device",
                           hdr.type, STREAM_TYPE_START_STOP);
}

void Stream::read_command(bool blocking)
{
    int timeout = blocking?-1:0;
    while (!ConcreteAgent::quit_requested()) {
        if (!have_something_to_read(timeout)) {
            if (!blocking) {
                return;
            }
            sleep(1);
            continue;
        }
        read_command_from_device();
        break;
    }
}

}} // namespace spice::streaming_agent
