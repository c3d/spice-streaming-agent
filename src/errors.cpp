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
    char buffer[1024];
    format_message(buffer, sizeof(buffer));
    ::syslog(LOG_ERR, "%s", buffer);
    return *this;
}

int IOError::format_message(char *buffer, size_t size) const noexcept
{
    int written = snprintf(buffer, size, "%s", what());
    return append_strerror(buffer, size, written);
}

int IOError::append_strerror(char *buffer, size_t size, int written) const noexcept
{
    // The conversion of written to size_t also deals with the case where written < 0
    if (saved_errno && (size_t) written < size) {
        written += snprintf(buffer + written, size - written, ": %s (errno %d)",
                            strerror(saved_errno), saved_errno);
    }
    return written;
}

int OpenError::format_message(char *buffer, size_t size) const noexcept
{
    int written = snprintf(buffer, size, "%s '%s'", what(), filename);
    return append_strerror(buffer, size, written);
}

int WriteError::format_message(char *buffer, size_t size) const noexcept
{
    int written = snprintf(buffer, size, "%s writing %s", what(), operation);
    return append_strerror(buffer, size, written);
}

int ReadError::format_message(char *buffer, size_t size) const noexcept
{
    int written = snprintf(buffer, size, "%s reading %s", what(), operation);
    return append_strerror(buffer, size, written);
}

int MessageDataError::format_message(char *buffer, size_t size) const noexcept
{
    int written = snprintf(buffer, size, "%s reading %s (received %zu, expected %zu)",
                           what(), operation, received, expected);
    return append_strerror(buffer, size, written);
}

int MessageLengthError::format_message(char *buffer, size_t size) const noexcept
{
    int written = snprintf(buffer, size, "%s reading %s (length was %zu, should be %s %zu)",
                           what(), operation,
                           received,
                           received < expected ? "at least" : "at most",
                           expected);
    return append_strerror(buffer, size, written);
}

int OptionError::format_message(char *buffer, size_t size) const noexcept
{
    return snprintf(buffer, size, "%s '%s'", what(), option);
}

int OptionValueError::format_message(char *buffer, size_t size) const noexcept
{
    return snprintf(buffer, size, "%s ('%s' is not a valid value for plugin option '%s')",
                    what(), value, option);
}

}} // namespace spice::streaming_agent
