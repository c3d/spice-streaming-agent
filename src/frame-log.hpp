/* A utility logger for logging frames and/or time information.
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#ifndef SPICE_STREAMING_AGENT_FRAME_LOG_HPP
#define SPICE_STREAMING_AGENT_FRAME_LOG_HPP

#include <cinttypes>
#include <stddef.h>
#include <stdio.h>


namespace spice {
namespace streaming_agent {

class FrameLog {
public:
    FrameLog(const char *log_name, bool log_binary, bool log_frames);
    FrameLog(const FrameLog &) = delete;
    FrameLog &operator=(const FrameLog &) = delete;
    ~FrameLog();

    __attribute__ ((format (printf, 2, 3)))
    void log_stat(const char* format, ...);
    void log_frame(const void* buffer, size_t buffer_size);

    static uint64_t get_time();

private:
    FILE *log_file = nullptr;
    bool log_binary = false;
    bool log_frames = false;
};

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_FRAME_LOG_HPP
