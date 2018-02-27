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
private:
    int saved_errno;
};

class WriteError : public IOError
{
public:
    WriteError(const char *msg, int saved_errno): IOError(msg, saved_errno) {}
};

class ReadError : public IOError
{
public:
    ReadError(const char *msg, int saved_errno): IOError(msg, saved_errno) {}
};

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_ERRORS_HPP
