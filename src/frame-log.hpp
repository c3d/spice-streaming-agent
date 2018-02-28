/* Class to log frames as they are being streamed
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */
#ifndef SPICE_STREAMING_AGENT_FRAME_LOG_HPP
#define SPICE_STREAMING_AGENT_FRAME_LOG_HPP

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

namespace spice {
namespace streaming_agent {

/* returns current time in micro-seconds */
static inline uint64_t get_time(void)
{
    struct timeval now;

    gettimeofday(&now, NULL);

    return (uint64_t)now.tv_sec * 1000000 + (uint64_t)now.tv_usec;

}

class FrameLog
{
public:
    FrameLog(const char *filename, bool binary = false);
    ~FrameLog();

    operator bool() { return log != NULL; }
    void dump(const void *buffer, size_t length);

private:
    FILE *log;
    bool binary;
};

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_ERRORS_HPP
