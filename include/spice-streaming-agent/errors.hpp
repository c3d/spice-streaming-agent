/* Errors that may be thrown by the agent
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */
#ifndef SPICE_STREAMING_AGENT_ERRORS_HPP
#define SPICE_STREAMING_AGENT_ERRORS_HPP

#include <exception>
#include <stddef.h>

namespace spice {
namespace streaming_agent {

class Error : public std::exception
{
public:
    Error(const char *message): exception(), message(message) { }
    Error(const Error &error): exception(error), message(error.message) {}
    virtual const char *what() const noexcept override;
    virtual int format_message(char *buffer, size_t size) const noexcept;
    const Error &syslog() const noexcept;
protected:
    const char *message;
};

class IOError : public Error
{
public:
    IOError(const char *msg, int saved_errno) : Error(msg), saved_errno(saved_errno) {}
    int format_message(char *buffer, size_t size) const noexcept override;
    int append_strerror(char *buffer, size_t size, int written) const noexcept;
protected:
    int saved_errno;
};

class WriteError : public IOError
{
public:
    WriteError(const char *msg, const char *operation, int saved_errno)
        : IOError(msg, saved_errno), operation(operation) {}
    int format_message(char *buffer, size_t size) const noexcept override;
protected:
    const char *operation;
};

class ReadError : public IOError
{
public:
    ReadError(const char *msg, int saved_errno): IOError(msg, saved_errno) {}
};

class ProtocolError : public ReadError
{
public:
    ProtocolError(const char *msg, int saved_errno = 0): ReadError(msg, saved_errno) {}
};

class MessageDataError : public ProtocolError
{
public:
    MessageDataError(const char *msg, size_t received, size_t expected, int saved_errno = 0)
        : ProtocolError(msg, saved_errno), received(received), expected(expected) {}
    int format_message(char *buffer, size_t size) const noexcept override;
protected:
    size_t received;
    size_t expected;
};

class OptionError : public Error
{
public:
    OptionError(const char *msg, const char *option, const char *value)
        : Error(msg), option(option), value(value) {}
    int format_message(char *buffer, size_t size) const noexcept override;
protected:
    const char *option;
    const char *value;
};

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_ERRORS_HPP
