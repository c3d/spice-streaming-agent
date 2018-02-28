/* Class to log frames as they are being streamed
 *
 * \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#include "frame-log.hpp"
#include "hexdump.h"

#include <syslog.h>
#include <inttypes.h>

namespace spice
{
namespace streaming_agent
{

FrameLog::FrameLog(const char *filename, bool binary)
    : log(filename ? fopen(filename, "wb") : NULL), binary(binary)
{
    if (filename && !log) {
        // We do not abort the program in that case, it's only a warning
        syslog(LOG_WARNING, "Failed to open hexdump log file '%s': %m\n", filename);
    }
}

FrameLog::~FrameLog()
{
    if (log)
        fclose(log);
}

void FrameLog::dump(const void *buffer, size_t length)
{
    if (binary) {
        fwrite(buffer, length, 1, log);
    } else {
        fprintf(log, "%" PRIu64 ": Frame of %zu bytes:\n", get_time(), length);
        hexdump(buffer, length, log);
    }
}

}} // namespace spice::streaming_agent
