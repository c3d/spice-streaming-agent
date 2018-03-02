/* Encapsulation of the stream used to communicate between agent and server
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */
#ifndef SPICE_STREAMING_AGENT_STREAM_HPP
#define SPICE_STREAMING_AGENT_STREAM_HPP

#include <spice/enums.h>
#include <set>
#include <mutex>

#include <spice-streaming-agent/errors.hpp>

namespace spice {
namespace streaming_agent {

class Stream
{
    typedef std::set<SpiceVideoCodecType> CodecSet;

public:
    Stream(const char *name);
    ~Stream();

    const CodecSet &client_codecs() const { return codecs; }
    bool streaming_requested() const { return is_streaming; }

    template <typename Message, typename ...PayloadArgs>
    void send(PayloadArgs... payload_args)
    {
        Message message(payload_args...);
        std::lock_guard<std::mutex> stream_guard(mutex);
        message.write_header(*this);
        message.write_message_body(*this, payload_args...);
    }

    void read_command(bool blocking);
    void write_all(const char *operation, const void *buf, const size_t len);

private:
    int have_something_to_read(int timeout);
    void read_all(const char *operation, void *msg, size_t len);
    void handle_stream_start_stop(size_t len);
    void handle_stream_capabilities(size_t len);
    void handle_stream_error(size_t len);
    void read_command_from_device();

private:
    std::mutex mutex;
    CodecSet codecs;
    int streamfd = -1;
    bool is_streaming = false;
};

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_ERRORS_HPP
