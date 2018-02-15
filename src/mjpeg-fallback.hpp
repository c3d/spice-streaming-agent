/* Plugin implementation for Mjpeg
 *
 * \copyright
 * Copyright 2017 Red Hat Inc. All rights reserved.
 */
#ifndef SPICE_STREAMING_AGENT_MJPEG_FALLBACK_HPP
#define SPICE_STREAMING_AGENT_MJPEG_FALLBACK_HPP

#include <spice-streaming-agent/plugin.hpp>
#include <spice-streaming-agent/frame-capture.hpp>


namespace spice {
namespace streaming_agent {

struct MjpegSettings
{
    int fps;
    int quality;
};

class MjpegPlugin final: public Plugin
{
public:
    FrameCapture *CreateCapture() override;
    unsigned Rank() override;
    void ParseOptions(const ConfigureOption *options);
    MjpegSettings Options() const;  // TODO unify on Settings vs Options
    SpiceVideoCodecType VideoCodecType() const;
private:
    MjpegSettings settings = { 10, 80 };
};

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_MJPEG_FALLBACK_HPP
