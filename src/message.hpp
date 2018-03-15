/* Formatting messages
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */
#ifndef SPICE_STREAMING_AGENT_MESSAGE_HPP
#define SPICE_STREAMING_AGENT_MESSAGE_HPP

#include "stream.hpp"

#include <spice/stream-device.h>
#include <vector>

namespace spice
{
namespace streaming_agent
{

template <typename Payload, typename Info, unsigned Type>
class OutMessage
{
public:
    template <typename ...PayloadArgs>
    OutMessage(PayloadArgs... payload_args)
        : hdr(StreamDevHeader {
              .protocol_version = STREAM_DEVICE_PROTOCOL,
              .padding = 0, // Required by C++ (unlike C99)
              .type = Type,
              .size = (uint32_t) Info::size(payload_args...)
          })
    { }
    void write_header(Stream &stream)
    {
        stream.write_all("header", &hdr, sizeof(hdr));
    }

protected:
    StreamDevHeader hdr;
    typedef Payload MessagePayload;
};


template <typename Payload>
class InMessage
{
    typedef std::vector<uint8_t> Storage;

public:
    InMessage(Stream &stream, size_t size, const char *operation)
        : bytes(size)
    {
        bytes.resize(size);
        stream.read_all(operation, &bytes[0], size);
    }

    Payload &payload()
    {
        return *reinterpret_cast<Payload *> (bytes.data());
    }

private:
    Storage bytes;
};

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_MESSAGE_HPP
