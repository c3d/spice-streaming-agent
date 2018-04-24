/* A module for low-level communication over the streaming virtio port.
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#ifndef SPICE_STREAMING_AGENT_STREAM_PORT_HPP
#define SPICE_STREAMING_AGENT_STREAM_PORT_HPP

#include <cstddef>


namespace spice {
namespace streaming_agent {

void read_all(int fd, void *msg, size_t len);
size_t write_all(int fd, const void *buf, const size_t len);

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_STREAM_PORT_HPP