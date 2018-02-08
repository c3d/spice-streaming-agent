/* Common interface for all streaming / capture cards
 * used by SPICE streaming-agent.
 *
 * \copyright
 * Copyright 2016-2017 Red Hat Inc. All rights reserved.
 */
#ifndef SPICE_STREAMING_AGENT_FRAME_CAPTURE_HPP
#define SPICE_STREAMING_AGENT_FRAME_CAPTURE_HPP
#include <cstdio>

#include <spice/enums.h>

namespace spice {
namespace streaming_agent {

struct FrameSize
{
    unsigned width;
    unsigned height;
};

struct FrameInfo
{
    FrameSize size;
    /*! Memory buffer, valid till next frame is read */
    const void *buffer;
    size_t buffer_size;
    /*! Start of a new stream */
    bool stream_start;
};

/*!
 * Pure base class implementing the frame capture
 */
class FrameCapture
{
public:
    virtual ~FrameCapture() = default;

    /*! Grab a frame
     * This function will wait for next frame.
     * Capture is started if needed.
     */
    virtual FrameInfo CaptureFrame() = 0;

    /*! Reset capturing
     * This will reset to beginning state
     */
    virtual void Reset() = 0;

    /*!
     * Get video codec used to encode last frame
     */
    virtual SpiceVideoCodecType VideoCodecType() const = 0;
protected:
    FrameCapture() = default;
    FrameCapture(const FrameCapture&) = delete;
    void operator=(const FrameCapture&) = delete;
};

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_FRAME_CAPTURE_HPP
