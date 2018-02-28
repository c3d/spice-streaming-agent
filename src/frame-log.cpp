/* Class to log frames as they are being streamed
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#include "frame-log.hpp"
#include "hexdump.h"

#include <spice-streaming-agent/errors.hpp>

#include <syslog.h>
#include <inttypes.h>
#include <errno.h>

namespace spice
{
namespace streaming_agent
{

FrameLog::FrameLog(const char *filename, bool binary)
    : log(filename ? fopen(filename, "wb") : NULL), binary(binary)
{
    if (filename && !log) {
        throw OpenError("failed to open hexdump log file", filename, errno);
    }
}

FrameLog::~FrameLog()
{
    if (log) {
        fclose(log);
    }
}

void FrameLog::dump(const void *buffer, size_t length)
{
    if (log) {
        if (binary) {
            fwrite(buffer, length, 1, log);
        } else {
            fprintf(log, "%" PRIu64 ": Frame of %zu bytes:\n", get_time(), length);
            hexdump(buffer, length, log);
        }
    }
}

}} // namespace spice::streaming_agent
