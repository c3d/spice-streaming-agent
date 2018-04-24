/* A module for low-level communication over the streaming virtio port.
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#include "stream-port.hpp"

#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <stdexcept>


namespace spice {
namespace streaming_agent {

void read_all(int fd, void *msg, size_t len)
{
    while (len > 0) {
        ssize_t n = read(fd, msg, len);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw std::runtime_error("Reading message from device failed: " +
                                     std::string(strerror(errno)));
        }

        len -= n;
        msg = (uint8_t *) msg + n;
    }
}

size_t write_all(int fd, const void *buf, const size_t len)
{
    size_t written = 0;
    while (written < len) {
        int l = write(fd, (const char *) buf + written, len - written);
        if (l < 0) {
            if (errno == EINTR) {
                continue;
            }
            syslog(LOG_ERR, "write failed - %m");
            return l;
        }
        written += l;
    }
    syslog(LOG_DEBUG, "write_all -- %u bytes written\n", (unsigned)written);
    return written;
}

}} // namespace spice::streaming_agent
