/* Plugin interface for all streaming / capture cards
 * used by SPICE streaming-agent.
 *
 * \copyright
 * Copyright 2017 Red Hat Inc. All rights reserved.
 */


#include <spice-streaming-agent/plugin.hpp>
#include <sstream>


namespace spice {
namespace streaming_agent {

int Plugin::OptionValueAsInt(const string &name,
                             const string &value,
                             string &error,
                             int min, int max)
// ----------------------------------------------------------------------------
//   Check that an input option parses validly as an int in the given range
// ----------------------------------------------------------------------------
{
    std::ostringstream err;
    std::istringstream input(value);
    int result = 0;
    input >> result;

    if (!input.fail() && !input.eof()) {
        string suffix;
        input >> suffix;
        bool ok = false;
        if (!input.fail() && suffix.length() == 1) {
            switch (suffix[0]) {
            case 'b': case 'B': ok = true; break;
            case 'k': case 'K': ok = true; result *= 1000; break;
            case 'm': case 'M': ok = true; result *= 1000000; break;
            default: ok = false;
            }
        }
        if (!ok) {
            err << "Unknown number suffix " << suffix
                << " for " << name << "\n";
            error = err.str();
        }
    }

    if (input.fail()) {
        err << "The value " << value << " for " << name << " is not a number\n";
        error = err.str();
    }
    if (!input.eof()) {
        err << "Some junk after value " << value << " for " << name << "\n";
        error = err.str();
    }
    if (result < min || result > max) {
        err << "The value " << value << " for " << name
            << " must be between " << min << " and " << max << "\n";
        error = err.str();        // May actually combine an earlier error
        result = (min + max) / 2; // Give a value acceptable by caller
    }

    return result;
}


bool Plugin::BaseOptions(const string &name, const string &value, string &error)
// ----------------------------------------------------------------------------
//   Apply the common options, return true if one was recognized
// ----------------------------------------------------------------------------
{
    if (name == "framerate" || name == "fps") {
        framerate = OptionValueAsInt(name, value, error, 1, 240);
        return true;
    }
    if (name == "quality") {
        quality = OptionValueAsInt(name, value, error, 0, 100);
        return true;
    }
    if (name == "avg_bitrate" || name == "bitrate") {
        avg_bitrate = OptionValueAsInt(name, value, error, 10000, 32000000);
        return true;
    }
    if (name == "max_bitrate") {
        max_bitrate = OptionValueAsInt(name, value, error, 10000, 32000000);
        return true;
    }

    return false;               // Unrecognized option
}

} } // namespace spice::streaming_agent
