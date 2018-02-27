/* Formatting messages
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */
#ifndef SPICE_STREAMING_AGENT_MESSAGE_HPP
#define SPICE_STREAMING_AGENT_MESSAGE_HPP

#include <spice/stream-device.h>

namespace spice
{
namespace streaming_agent
{

template <typename Payload, typename Info, unsigned Type>
class Message
{
public:
    template <typename ...PayloadArgs>
    Message(PayloadArgs... payload)
        : hdr(StreamDevHeader {
              .protocol_version = STREAM_DEVICE_PROTOCOL,
              .padding = 0,     // Workaround GCC bug "sorry: not implemented"
              .type = Type,
              .size = (uint32_t) Info::size(payload...)
          })
    { }
    void write_header(Stream &stream)
    {
        stream.write_all("header", &hdr, sizeof(hdr));
    }

protected:
    StreamDevHeader hdr;
    typedef Payload payload_t;
};

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_MESSAGE_HPP
