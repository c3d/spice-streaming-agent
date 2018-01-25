/* Plugin interface for all streaming / capture cards
 * used by SPICE streaming-agent.
 *
 * \copyright
 * Copyright 2017 Red Hat Inc. All rights reserved.
 */
#ifndef SPICE_STREAMING_AGENT_PLUGIN_HPP
#define SPICE_STREAMING_AGENT_PLUGIN_HPP

#include <spice/enums.h>

/*!
 * \file
 * \brief Plugin interface
 *
 * Each module loaded by the agent should implement one or more
 * Plugins and register them.
 */

namespace SpiceStreamingAgent {

class FrameCapture;

/*!
 * Plugin version, only using few bits, schema is 0xMMmm
 * where MM is major and mm is the minor, can be easily expanded
 * using more bits in the future
 */
enum Constants : unsigned { PluginVersion = 0x100u };

enum Ranks : unsigned {
    /// this plugin should not be used
    DontUse = 0,
    /// use plugin only as a fallback
    FallBackMin = 1,
    /// plugin supports encoding in software
    SoftwareMin = 0x40000000,
    /// plugin supports encoding in hardware
    HardwareMin = 0x80000000,
    /// plugin provides access to specific card hardware not only for compression
    SpecificHardwareMin = 0xC0000000
};

/*!
 * Configuration option.
 * An array of these will be passed to the plugin.
 * Simply a couple name and value passed as string.
 * For instance "framerate" and "20".
 */
struct ConfigureOption
{
    const char *name;
    const char *value;
};

/*!
 * Interface a plugin should implement and register to the Agent.
 *
 * A plugin module can register multiple Plugin interfaces to handle
 * multiple codecs. In this case each Plugin will report data for a
 * specific codec.
 */
class Plugin
{
public:
    /*!
     * Allows to free the plugin when not needed
     */
    virtual ~Plugin() {};

    /*!
     * Request an object for getting frames.
     * Plugin should return proper object or nullptr if not possible
     * to initialize.
     * Plugin can also raise std::runtime_error which will be logged.
     */
    virtual FrameCapture *CreateCapture() = 0;

    /*!
     * Request to rank the plugin.
     * See Ranks enumeration for details on ranges.
     * \return Ranks::DontUse if not possible to use the plugin, this
     * is necessary as the condition for capturing frames can change
     * from the time the plugin decided to register and now.
     */
    virtual unsigned Rank() = 0;

    /*!
     * Get video codec used to encode last frame
     */
    virtual SpiceVideoCodecType VideoCodecType() const = 0;
};

/*!
 * Interface the plugin should use to interact with the agent.
 * The agent will pass it to the entry point.
 * Exporting functions from an executable in Windows OS is not easy
 * and a standard way to do it so better to implement the interface
 * that way for compatibility.
 */
class Agent
{
public:
    /*!
     * Get agent version.
     * Plugin should check the version for compatibility before doing
     * everything.
     * \return version specified like PluginVersion
     */
    virtual unsigned Version() const = 0;

    /*!
     * Check if a given plugin version is compatible with this agent
     * \return true is compatible
     */
    virtual bool PluginVersionIsCompatible(unsigned pluginVersion) const = 0;

    /*!
     * Register a plugin in the system.
     */
    virtual void Register(Plugin& plugin) = 0;

    /*!
     * Get options array.
     * Array is terminated with {nullptr, nullptr}.
     * Never nullptr.
     * \todo passing options to entry point instead?
     */
    virtual const ConfigureOption* Options() const = 0;
};

typedef bool PluginInitFunc(SpiceStreamingAgent::Agent* agent);

}

#ifndef SPICE_STREAMING_AGENT_PROGRAM
/*!
 * Plugin main entry point.
 * Plugins should check if the version of the agent is compatible.
 * If is compatible should register itself to the agent and return
 * true.
 * If is not compatible can decide to stay in memory or not returning
 * true (do not unload) or false (safe to unload). This is necessary
 * if the plugin uses some library which are not safe to be unloaded.
 * This public interface is also designed to avoid exporting data from
 * the plugin which could be a problem in some systems.
 * \return true if plugin should stay loaded, false otherwise
 */
extern "C" SpiceStreamingAgent::PluginInitFunc spice_streaming_agent_plugin_init;
#endif

#endif // SPICE_STREAMING_AGENT_PLUGIN_HPP
