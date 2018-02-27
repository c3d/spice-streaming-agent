/* Errors that may be thrown by the agent
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#include <spice-streaming-agent/errors.hpp>

#include <stdio.h>
#include <syslog.h>
#include <string.h>

namespace spice {
namespace streaming_agent {

const char *Error::what() const noexcept
{
    return message;
}

int Error::format_message(char *buffer, size_t size) const noexcept
{
    return snprintf(buffer, size, "%s", message);
}

const Error &Error::syslog() const noexcept
{
    char buffer[256];
    format_message(buffer, sizeof(buffer));
    ::syslog(LOG_ERR, "%s\n", buffer);
    return *this;
}

int IOError::format_message(char *buffer, size_t size) const noexcept
{
    return snprintf(buffer, size, "%s: %s (errno %d)", what(), strerror(saved_errno), saved_errno);
}

}} // namespace spice::streaming_agent
