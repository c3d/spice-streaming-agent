/* A module for low-level communication over the streaming virtio port.
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#ifndef SPICE_STREAMING_AGENT_STREAM_PORT_HPP
#define SPICE_STREAMING_AGENT_STREAM_PORT_HPP

#include <cstddef>
#include <string>
#include <mutex>


namespace spice {
namespace streaming_agent {

class StreamPort {
public:
    StreamPort(const std::string &port_name);
    ~StreamPort();

    void read(void *buf, size_t len);
    void write(const void *buf, size_t len);

    int fd;
    std::mutex mutex;
};

void read_all(int fd, void *buf, size_t len);
void write_all(int fd, const void *buf, size_t len);

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_STREAM_PORT_HPP
