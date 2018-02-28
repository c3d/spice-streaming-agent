/* Implementation of the agent
 *
 * \copyright
 * Copyright 2017 Red Hat Inc. All rights reserved.
 */

#include "concrete-agent.hpp"
#include "message.hpp"
#include "frame-log.hpp"

#include <spice-streaming-agent/frame-capture.hpp>
#include <spice-streaming-agent/plugin.hpp>
#include <spice-streaming-agent/errors.hpp>

#include <config.h>
#include <algorithm>
#include <syslog.h>
#include <glob.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <string>

using namespace spice::streaming_agent;

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

void ConcreteAgent::LoadPlugin(const std::string &plugin_filename)
{
    void *dl = dlopen(plugin_filename.c_str(), RTLD_LOCAL|RTLD_NOW);
    if (!dl) {
        syslog(LOG_ERR, "error loading plugin %s: %s",
               plugin_filename.c_str(), dlerror());
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

FrameCapture *ConcreteAgent::GetBestFrameCapture(const std::set<SpiceVideoCodecType>& codecs)
{
    std::vector<std::pair<unsigned, std::shared_ptr<Plugin>>> sorted_plugins;

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
        // check client supports the codec
        if (codecs.find(plugin.second->VideoCodecType()) == codecs.end())
            continue;
        FrameCapture *capture = plugin.second->CreateCapture();
        if (capture) {
            return capture;
        }
    }
    return nullptr;
}

class FormatMessage : public Message<StreamMsgFormat, FormatMessage, STREAM_TYPE_FORMAT>
{
public:
    FormatMessage(unsigned w, unsigned h, uint8_t c) : Message(w, h, c) {}
    static size_t size(unsigned width, unsigned height, uint8_t codec)
    {
        return sizeof(MessagePayload);
    }
    void write_message_body(Stream &stream, unsigned w, unsigned h, uint8_t c)
    {
        StreamMsgFormat msg = { .width = w, .height = h, .codec = c, .padding1 = {} };
        stream.write_all("format", &msg, sizeof(msg));
    }
};

class FrameMessage : public Message<StreamMsgData, FrameMessage, STREAM_TYPE_DATA>
{
public:
    FrameMessage(const void *frame, size_t length) : Message(frame, length) {}
    static size_t size(const void *frame, size_t length)
    {
        return sizeof(MessagePayload) + length;
    }
    void write_message_body(Stream &stream, const void *frame, size_t length)
    {
        stream.write_all("frame", frame, length);
    }
};


void ConcreteAgent::CaptureLoop(Stream &stream, FrameLog &frame_log)
{
    unsigned int frame_count = 0;
    while (!quit_requested) {
        while (!quit_requested && !stream.streaming_requested()) {
            if (stream.read_command(true) < 0) {
                syslog(LOG_ERR, "FAILED to read command\n");
                return;
            }
        }

        if (quit_requested) {
            return;
        }

        syslog(LOG_INFO, "streaming starts now\n");
        uint64_t time_last = 0;

        std::unique_ptr<FrameCapture> capture(GetBestFrameCapture(stream.client_codecs()));
        if (!capture) {
            throw Error("cannot find a suitable capture system");
        }

        while (!quit_requested && stream.streaming_requested()) {
            if (++frame_count % 100 == 0) {
                syslog(LOG_DEBUG, "SENT %d frames\n", frame_count);
            }
            uint64_t time_before = get_time();

            FrameInfo frame = capture->CaptureFrame();

            uint64_t time_after = get_time();
            syslog(LOG_DEBUG,
                   "got a frame -- size is %zu (%" PRIu64 " ms) "
                   "(%" PRIu64 " ms from last frame)(%" PRIu64 " us)\n",
                   frame.buffer_size, (time_after - time_before)/1000,
                   (time_after - time_last)/1000,
                   (time_before - time_last));
            time_last = time_after;

            if (frame.stream_start) {
                unsigned width, height;
                uint8_t codec;

                width = frame.size.width;
                height = frame.size.height;
                codec = capture->VideoCodecType();

                syslog(LOG_DEBUG, "wXh %uX%u  codec=%u\n", width, height, codec);

                stream.send<FormatMessage>(width, height, codec);
            }
            frame_log.dump(frame.buffer, frame.buffer_size);
            stream.send<FrameMessage>(frame.buffer, frame.buffer_size);
            //usleep(1);
            if (stream.read_command(false) < 0) {
                syslog(LOG_ERR, "FAILED to read command\n");
                return;
            }
        }
    }
}
