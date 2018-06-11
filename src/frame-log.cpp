/* A utility logger for logging frames and/or time information.
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#include "frame-log.hpp"

#include "error.hpp"
#include "hexdump.h"

#include <cstdarg>
#include <string.h>
#include <sys/time.h>


namespace spice {
namespace streaming_agent {

FrameLog::FrameLog(const char *log_name, bool log_binary, bool log_frames) :
    log_binary(log_binary),
    log_frames(log_frames)
{
    if (log_name) {
        log_file = fopen(log_name, "wb");
        if (!log_file) {
            throw Error(std::string("Failed to open log file '") + log_name + "': " + strerror(errno));
        }
        if (!log_binary) {
            setlinebuf(log_file);
        }
    }
}

FrameLog::~FrameLog()
{
    if (log_file) {
        fclose(log_file);
    }
}

void FrameLog::log_stat(const char* format, ...)
{
    if (log_file && !log_binary) {
        fprintf(log_file, "%" PRIu64 ": ", get_time());
        va_list ap;
        va_start(ap, format);
        vfprintf(log_file, format, ap);
        va_end(ap);
        fputc('\n', log_file);
    }
}

void FrameLog::log_frame(const void* buffer, size_t buffer_size)
{
    if (log_file) {
        if (log_binary) {
            fwrite(buffer, buffer_size, 1, log_file);
        } else if (log_frames) {
            hexdump(buffer, buffer_size, log_file);
        }
    }
}

/**
 * Returns current time in microseconds.
 */
uint64_t FrameLog::get_time()
{
    struct timeval now;
    gettimeofday(&now, NULL);

    return (uint64_t)now.tv_sec * 1000000 + (uint64_t)now.tv_usec;
}

}} // namespace spice::streaming_agent
