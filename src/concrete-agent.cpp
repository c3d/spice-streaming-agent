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
#include <string>

#include "concrete-agent.hpp"
#include <recorder/recorder.h>

using namespace spice::streaming_agent;

RECORDER_DEFINE(plugins, 64, "Plugins loading and management")
RECORDER_DEFINE(capture, 64, "Frame capture")
RECORDER_DEFINE(codecs, 64, "Frame capture")

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
    unsigned version = PluginVersion;
    return MajorVersion(version) == MajorVersion(pluginVersion) &&
        MinorVersion(version) >= MinorVersion(pluginVersion);
}

void ConcreteAgent::Register(Plugin& plugin)
{
    plugins.push_back(std::shared_ptr<Plugin>(&plugin));
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

void ConcreteAgent::LoadPlugins(const std::string &directory)
{
    std::string pattern = directory + "/*.so";
    glob_t globbuf;
    record(plugins, "Loading plugins from %s", pattern.c_str());

    int glob_result = glob(pattern.c_str(), 0, NULL, &globbuf);
    if (glob_result == GLOB_NOMATCH) {
        record(plugins, "No match");
        return;
    }
    if (glob_result != 0) {
        record(plugins, "glob failed with result %d", glob_result);
        syslog(LOG_ERR, "glob FAILED with %d", glob_result);
        return;
    }

    for (size_t n = 0; n < globbuf.gl_pathc; ++n) {
        record(plugins, "Plugin %d/%d is %s", n+1, globbuf.gl_pathc, globbuf.gl_pathv[n]);
        LoadPlugin(globbuf.gl_pathv[n]);
    }
    globfree(&globbuf);
}

void ConcreteAgent::LoadPlugin(const std::string &plugin_filename)
{
    record(plugins, "Loading %s", plugin_filename.c_str());
    void *dl = dlopen(plugin_filename.c_str(), RTLD_LOCAL|RTLD_NOW);
    if (!dl) {
        record(plugins, "Error loading plugin %s: %s",
               plugin_filename.c_str(), dlerror());
        syslog(LOG_ERR, "error loading plugin %s: %s",
               plugin_filename.c_str(), dlerror());
        return;
    }

    unsigned *version =
        (unsigned *) dlsym(dl, "spice_streaming_agent_plugin_interface_version");
    if (!version) {
        record(plugins, "Error loading plugin %s: no version information",
               plugin_filename.c_str());
        syslog(LOG_ERR, "error loading plugin %s: no version information",
               plugin_filename.c_str());
        return;
    }
    if (!PluginVersionIsCompatible(*version)) {
        record(plugins, "Error loading plugin %s: plugin version %u.%u not accepted",
               plugin_filename.c_str(),
               MajorVersion(*version), MinorVersion(*version));
        syslog(LOG_ERR,
               "error loading plugin %s: plugin interface version %u.%u not accepted",
               plugin_filename.c_str(),
               MajorVersion(*version), MinorVersion(*version));
        return;
    }

    try {
        PluginInitFunc* init_func =
            (PluginInitFunc *) dlsym(dl, "spice_streaming_agent_plugin_init");
        record(plugins, "Plugin init function is %p", init_func);
        if (!init_func || !init_func(this)) {
            dlclose(dl);
        }
    }
    catch (std::runtime_error &err) {
        syslog(LOG_ERR, "%s", err.what());
        dlclose(dl);
    }
}

FrameCapture *ConcreteAgent::GetBestFrameCapture(const std::set<SpiceVideoCodecType>& codecs)
{
    record(capture, "Selecting capture from %d codecs", codecs.size());
    std::vector<std::pair<unsigned, std::shared_ptr<Plugin>>> sorted_plugins;

    // sort plugins base on ranking, reverse order
    for (const auto& plugin: plugins) {
        unsigned rank = plugin->Rank();
        record(plugins, "Plugin %p has rank %d", plugin.get(), rank);
        sorted_plugins.push_back(make_pair(rank, plugin));
    }
    sort(sorted_plugins.rbegin(), sorted_plugins.rend());

    record(codecs, "There are %u codec types", codecs.size());
    for (auto codecType : codecs) {
        record(codecs, "Available codec type: %u", codecType);
    }

    // return first not null
    for (const auto& plugin: sorted_plugins) {
        if (plugin.first == DontUse) {
            record(plugins, "Plugin %p has rank 'DontUse', exiting", plugin.second.get());
            break;
        }
        // check client supports the codec
        SpiceVideoCodecType codecType = plugin.second->VideoCodecType();
        record(plugins, "Plugin %p offers codec type %u", plugin.second.get(), codecType);
        if (codecs.find(codecType) == codecs.end()) {
            record(plugins, "Codec type not found");
            continue;
        }

        FrameCapture *capture;
        try {
            capture = plugin.second->CreateCapture();
            record(capture, "Created capture %p from plugin %p codec %u rank %u",
                   capture, plugin.second.get(), codecType, plugin.first);
        } catch (const std::exception &err) {
            syslog(LOG_ERR, "Error creating capture engine: %s", err.what());
            continue;
        }
        if (capture) {
            return capture;
        }
    }
    return nullptr;
}
