/* A module for low-level communication over the streaming virtio port.
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#include "stream-port.hpp"
#include "error.hpp"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <stdexcept>


namespace spice {
namespace streaming_agent {

StreamPort::StreamPort(const std::string &port_name) : fd(open(port_name.c_str(), O_RDWR | O_NONBLOCK))
{
    if (fd < 0) {
        throw IOError("Failed to open the streaming device \"" + port_name + "\"", errno);
    }
}

StreamPort::~StreamPort()
{
    close(fd);
}

void StreamPort::read(void *buf, size_t len)
{
    read_all(fd, buf, len);
}

void StreamPort::write(const void *buf, size_t len)
{
    write_all(fd, buf, len);
}

void read_all(int fd, void *buf, size_t len)
{
    while (len > 0) {
        ssize_t n = read(fd, buf, len);

        if (n == 0) {
            throw ReadError("Reading message from device failed: read() returned 0, device is closed.");
        }

        if (n < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd pollfd = {fd, POLLIN, 0};
                if (poll(&pollfd, 1, -1) < 0) {
                    if (errno == EINTR) {
                        continue;
                    }

                    throw ReadError("poll failed while reading message from device", errno);
                }

                if (pollfd.revents & POLLIN) {
                    continue;
                }

                if (pollfd.revents & POLLHUP) {
                    throw ReadError("Reading message from device failed: The device is closed.");
                }

                throw ReadError("Reading message from device failed: poll returned " +
                                std::to_string(pollfd.revents));
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
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd pollfd = {fd, POLLOUT, 0};
                if (poll(&pollfd, 1, -1) < 0) {
                    if (errno == EINTR) {
                        continue;
                    }

                    throw WriteError("poll failed while writing message to device", errno);
                }

                if (pollfd.revents & POLLOUT) {
                    continue;
                }

                if (pollfd.revents & POLLHUP) {
                    throw WriteError("Writing message to device failed: The device is closed.");
                }

                throw WriteError("Writing message to device failed: poll returned " +
                                 std::to_string(pollfd.revents));
            }
            throw WriteError("Writing message to device failed", errno);
        }

        len -= n;
        buf = (uint8_t *) buf + n;
    }
}

}} // namespace spice::streaming_agent
