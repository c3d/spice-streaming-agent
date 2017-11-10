/* Agent implementation
 *
 * \copyright
 * Copyright 2017 Red Hat Inc. All rights reserved.
 */
#ifndef SPICE_STREAMING_AGENT_CONCRETE_AGENT_HPP
#define SPICE_STREAMING_AGENT_CONCRETE_AGENT_HPP

#include <vector>
#include <set>
#include <memory>
#include <atomic>

#include <spice-streaming-agent/plugin.hpp>

namespace spice {
namespace streaming_agent {

class Stream;
class FrameLog;

struct ConcreteConfigureOption: ConfigureOption
{
    ConcreteConfigureOption(const char *name, const char *value)
    {
        this->name = name;
        this->value = value;
    }
};

class ConcreteAgent final : public Agent
{
public:
    ConcreteAgent();
    void Register(std::shared_ptr<Plugin> plugin) override;
    const ConfigureOption* Options() const override;
    void LoadPlugins(const std::string &directory);
    void CaptureLoop(Stream &stream, FrameLog &frame_log);
    // pointer must remain valid
    void AddOption(const char *name, const char *value);
    FrameCapture *GetBestFrameCapture(const std::set<SpiceVideoCodecType>& codecs);
    // TODO: Move these to the Agent interface for use by plugins
    static void register_interrupts();
    static bool quit_requested() { return must_quit; }
    static void request_quit() { must_quit = true; }
    static void check_if_quitting();
private:
    void LoadPlugin(const std::string &plugin_filename);
    static void handle_interrupt(int intr);
    static std::atomic<bool> must_quit;
    std::vector<std::shared_ptr<Plugin>> plugins;
    std::vector<ConcreteConfigureOption> options;
};

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_CONCRETE_AGENT_HPP
