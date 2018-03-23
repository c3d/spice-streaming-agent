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
#include <spice-streaming-agent/plugin.hpp>

namespace spice {
namespace streaming_agent {

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
    void Register(Plugin& plugin) override;
    const ConfigureOption* Options() const override;
    void LoadPlugins(const std::string &directory);
    // pointer must remain valid
    void AddOption(const char *name, const char *value);
    FrameCapture *GetBestFrameCapture(const std::set<SpiceVideoCodecType>& codecs);
private:
    bool PluginVersionIsCompatible(unsigned pluginVersion) const;
    void LoadPlugin(const std::string &plugin_filename);
    std::vector<std::shared_ptr<Plugin>> plugins;
    std::vector<ConcreteConfigureOption> options;
};

}} // namespace spice::streaming_agent

#endif // SPICE_STREAMING_AGENT_CONCRETE_AGENT_HPP
