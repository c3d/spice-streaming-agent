/* \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#ifndef SPICE_STREAMING_AGENT_UTILS_HPP
#define SPICE_STREAMING_AGENT_UTILS_HPP

#include <syslog.h>


namespace spice {
namespace streaming_agent {
namespace utils {

template<class T>
const T &syslog(const T &error) noexcept
{
    ::syslog(LOG_ERR, "%s\n", error.what());
    return error;
}

}}} // namespace spice::streaming_agent::utils

#endif // SPICE_STREAMING_AGENT_UTILS_HPP
