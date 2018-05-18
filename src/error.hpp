/* The errors module.
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#ifndef SPICE_STREAMING_AGENT_ERROR_HPP
#define SPICE_STREAMING_AGENT_ERROR_HPP

#include <stdexcept>
#include <string>
#include <syslog.h>


namespace spice {
namespace streaming_agent {

class Error : public std::runtime_error
{
public:
    Error(const std::string &message);
};

class IOError : public Error
{
public:
    using Error::Error;

    IOError(const std::string &msg, int sys_errno);
};

class ReadError : public IOError
{
public:
    using IOError::IOError;
};

class WriteError : public IOError
{
public:
    using IOError::IOError;
};

template<class T>
const T &syslog(const T &error) noexcept
{
    ::syslog(LOG_ERR, "%s\n", error.what());
    return error;
}

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_ERROR_HPP
