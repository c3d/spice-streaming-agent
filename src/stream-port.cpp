/* A module for low-level communication over the streaming virtio port.
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#include "stream-port.hpp"
#include "error.hpp"

#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <stdexcept>


namespace spice {
namespace streaming_agent {

void read_all(int fd, void *buf, size_t len)
{
    while (len > 0) {
        ssize_t n = read(fd, buf, len);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw ReadError("Reading message from device failed", errno);
        }

        len -= n;
        buf = (uint8_t *) buf + n;
    }
}

void write_all(int fd, const void *buf, size_t len)
{
    while (len > 0) {
        ssize_t n = write(fd, buf, len);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw WriteError("Writing message to device failed", errno);
        }

        len -= n;
        buf = (uint8_t *) buf + n;
    }
}

}} // namespace spice::streaming_agent
