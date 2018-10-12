/* A module for low-level communication over the streaming virtio port.
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#ifndef SPICE_STREAMING_AGENT_STREAM_PORT_HPP
#define SPICE_STREAMING_AGENT_STREAM_PORT_HPP

#include <spice/stream-device.h>
#include <spice/enums.h>

#include <cstddef>
#include <string>
#include <memory>
#include <mutex>
#include <set>


namespace spice {
namespace streaming_agent {

struct StartStopMessage
{
    bool start_streaming = false;
    std::set<SpiceVideoCodecType> client_codecs;
};

struct InCapabilitiesMessage {};

struct NotifyErrorMessage
{
    uint32_t error_code;
    char message[1025];
};

class InboundMessage
{
public:
    InboundMessage(const StreamDevHeader &header, std::unique_ptr<uint8_t[]> &&data);

    template<class Payload> Payload get_payload();

    const StreamDevHeader header;
private:
    std::unique_ptr<uint8_t[]> data;
};

template<>
StartStopMessage InboundMessage::get_payload<StartStopMessage>();
template<>
InCapabilitiesMessage InboundMessage::get_payload<InCapabilitiesMessage>();
template<>
NotifyErrorMessage InboundMessage::get_payload<NotifyErrorMessage>();

class StreamPort {
public:
    StreamPort(const std::string &port_name);
    ~StreamPort();

    InboundMessage receive();

    void write(const void *buf, size_t len);

    const int fd;
    std::mutex mutex;
};

void read_all(int fd, void *buf, size_t len);
void write_all(int fd, const void *buf, size_t len);

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_STREAM_PORT_HPP
