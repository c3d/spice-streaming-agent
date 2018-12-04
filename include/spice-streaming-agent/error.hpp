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
    Error(const std::string &message) : std::runtime_error(message) {}
};

template<class T>
const T &syslog(const T &error) noexcept
{
    ::syslog(LOG_ERR, "%s\n", error.what());
    return error;
}

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_ERROR_HPP
