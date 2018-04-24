/* The errors module.
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#include "error.hpp"

#include <string.h>


namespace spice {
namespace streaming_agent {

Error::Error(const std::string &message) : std::runtime_error(message) {}

IOError::IOError(const std::string &msg, int sys_errno) :
    Error(msg + ": " + std::to_string(sys_errno) + " - " + strerror(sys_errno))
{}

}} // namespace spice::streaming_agent
