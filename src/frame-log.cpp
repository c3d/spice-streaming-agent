/* A utility logger for logging frames and/or time information.
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#include "frame-log.hpp"

#include "hexdump.h"
#include <spice-streaming-agent/error.hpp>

#include <chrono>
#include <cstdarg>
#include <string.h>
#include <errno.h>


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
    va_list ap;
    va_start(ap, format);
    log_statv(format, ap);
    va_end(ap);
}

void FrameLog::log_statv(const char* format, va_list ap)
{
    if (log_file && !log_binary) {
        fprintf(log_file, "%" PRIu64 ": ", get_time());
        vfprintf(log_file, format, ap);
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
    auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
}

}} // namespace spice::streaming_agent
