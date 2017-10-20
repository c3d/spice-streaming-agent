/* Utility to manage registration of plugins compiled statically
 *
 * \copyright
 * Copyright 2017 Red Hat Inc. All rights reserved.
 */
#ifndef SPICE_STREAMING_AGENT_STATIC_PLUGIN_HPP
#define SPICE_STREAMING_AGENT_STATIC_PLUGIN_HPP

#include <spice-streaming-agent/plugin.hpp>

namespace SpiceStreamingAgent {

class StaticPlugin final {
public:
    StaticPlugin(PluginInitFunc init_func):
        next(list),
        init_func(init_func)
    {
        list = this;
    }
    static void InitAll(Agent& agent);
private:
    // this should be instantiated statically
    void *operator new(size_t s);
    void *operator new[](size_t s);

    const StaticPlugin *const next;
    const PluginInitFunc* const init_func;

    static const StaticPlugin *list;
};

}

#endif // SPICE_STREAMING_AGENT_STATIC_PLUGIN_HPP
