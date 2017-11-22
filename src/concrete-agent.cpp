/* Implementation of the agent
 *
 * \copyright
 * Copyright 2017 Red Hat Inc. All rights reserved.
 */

#include <config.h>
#include <algorithm>
#include <syslog.h>
#include <glob.h>
#include <dlfcn.h>

#include "concrete-agent.hpp"
#include "static-plugin.hpp"

using namespace std;
using namespace SpiceStreamingAgent;

static inline unsigned MajorVersion(unsigned version)
{
    return version >> 8;
}

static inline unsigned MinorVersion(unsigned version)
{
    return version & 0xffu;
}

ConcreteAgent::ConcreteAgent()
{
    options.push_back(ConcreteConfigureOption(nullptr, nullptr));
}

bool ConcreteAgent::PluginVersionIsCompatible(unsigned pluginVersion) const
{
    unsigned version = Version();
    return MajorVersion(version) == MajorVersion(pluginVersion) &&
        MinorVersion(version) >= MinorVersion(pluginVersion);
}

void ConcreteAgent::Register(Plugin& plugin)
{
    plugins.push_back(shared_ptr<Plugin>(&plugin));
}

const ConfigureOption* ConcreteAgent::Options() const
{
    static_assert(sizeof(ConcreteConfigureOption) == sizeof(ConfigureOption),
                  "ConcreteConfigureOption should be binary compatible with ConfigureOption");
    return static_cast<const ConfigureOption*>(&options[0]);
}

void ConcreteAgent::AddOption(const char *name, const char *value)
{
    // insert before the last {nullptr, nullptr} value
    options.insert(--options.end(), ConcreteConfigureOption(name, value));
}

void ConcreteAgent::LoadPlugins(const char *directory)
{
    StaticPlugin::InitAll(*this);

    string pattern = string(directory) + "/*.so";
    glob_t globbuf;

    int glob_result = glob(pattern.c_str(), 0, NULL, &globbuf);
    if (glob_result == GLOB_NOMATCH)
        return;
    if (glob_result != 0) {
        syslog(LOG_ERR, "glob FAILED with %d", glob_result);
        return;
    }

    for (size_t n = 0; n < globbuf.gl_pathc; ++n) {
        LoadPlugin(globbuf.gl_pathv[n]);
    }
    globfree(&globbuf);
}

void ConcreteAgent::LoadPlugin(const char *plugin_filename)
{
    void *dl = dlopen(plugin_filename, RTLD_LOCAL|RTLD_NOW);
    if (!dl) {
        syslog(LOG_ERR, "error loading plugin %s: %s",
               plugin_filename, dlerror());
        return;
    }

    try {
        PluginInitFunc* init_func =
            (PluginInitFunc *) dlsym(dl, "spice_streaming_agent_plugin_init");
        if (!init_func || !init_func(this)) {
            dlclose(dl);
        }
    }
    catch (std::runtime_error &err) {
        syslog(LOG_ERR, "%s", err.what());
        dlclose(dl);
    }
}

FrameCapture *ConcreteAgent::GetBestFrameCapture()
{
    vector<pair<unsigned, shared_ptr<Plugin>>> sorted_plugins;

    // sort plugins base on ranking, reverse order
    for (const auto& plugin: plugins) {
        sorted_plugins.push_back(make_pair(plugin->Rank(), plugin));
    }
    sort(sorted_plugins.rbegin(), sorted_plugins.rend());

    // return first not null
    for (const auto& plugin: sorted_plugins) {
        if (plugin.first == DontUse) {
            break;
        }
        FrameCapture *capture = plugin.second->CreateCapture();
        if (capture) {
            return capture;
        }
    }
    return nullptr;
}
