/* Agent implementation
 *
 * \copyright
 * Copyright 2017 Red Hat Inc. All rights reserved.
 */
#ifndef SPICE_STREAMING_AGENT_CONCRETE_AGENT_HPP
#define SPICE_STREAMING_AGENT_CONCRETE_AGENT_HPP

#include <vector>
#include <memory>
#include <spice-streaming-agent/plugin.hpp>

namespace SpiceStreamingAgent {

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
    unsigned Version() const override {
        return PluginVersion;
    }
    void Register(Plugin& plugin) override;
    const ConfigureOption* Options() const override;
    void LoadPlugins(const char *directory);
    // pointer must remain valid
    void AddOption(const char *name, const char *value);
    FrameCapture *GetBestFrameCapture();
    bool PluginVersionIsCompatible(unsigned pluginVersion) const override;
private:
    void LoadPlugin(const char *plugin_filename);
    std::vector<std::shared_ptr<Plugin>> plugins;
    std::vector<ConcreteConfigureOption> options;
};

}

#endif // SPICE_STREAMING_AGENT_CONCRETE_AGENT_HPP
