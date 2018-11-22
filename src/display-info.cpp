/* \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#include <spice-streaming-agent/display-info.hpp>

#include "utils.hpp"
#include <spice-streaming-agent/error.hpp>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits.h>


namespace spice {
namespace streaming_agent {

namespace {

std::string get_card_real_path(const std::string &card_path)
{
    char real_path_buf[PATH_MAX];
    if (realpath(card_path.c_str(), real_path_buf) == NULL) {
        throw Error("Failed to realpath \"" + card_path + "\": " + strerror(errno));
    }

    return real_path_buf;
}

} // namespace

std::vector<std::string> list_cards()
{
    std::string glob_path = "/sys/class/drm/card*";
    auto globs = utils::glob(glob_path);

    globs.erase(std::remove_if(globs.begin(), globs.end(), [](const std::string &glob) {
            // if the path contains "-", it is an output, not a card, filter it out
            return glob.find("-") != glob.npos;
        }),
        globs.end()
    );

    return globs;
}

uint32_t read_hex_number_from_file(const std::string &path)
{
    uint32_t res;
    std::ifstream vf(path);

    if (vf.fail()) {
        throw Error("Failed to open " + path + ": " + std::strerror(errno));
    }

    if (!(vf >> std::hex >> res)) {
        throw Error("Failed to read from " + path + ": " + std::strerror(errno));
    }

    return res;
}

std::string get_device_address(const std::string &card_path)
{
    std::string real_path = get_card_real_path(card_path);

    // real_path is e.g. /sys/devices/pci0000:00/0000:00:02.0/drm/card0
    std::string device_address = "pci/";

    const std::string path_prefix = "/sys/devices/pci";

    // if real_path doesn't start with path_prefix
    if (real_path.compare(0, path_prefix.length(), path_prefix)) {
        throw Error("Invalid device path \"" + real_path + "\"");
    }

    // /sys/devices/pci0000:00/0000:00:02.0/drm/card0
    //                 ^ ptr
    const char *ptr = real_path.c_str() + path_prefix.length();

    // copy the domain
    device_address += std::string(ptr, 4);

    // /sys/devices/pci0000:00/0000:00:02.0/drm/card0
    //                        ^ ptr
    ptr += 7;

    uint32_t domain, bus, slot, function, n;
    while (sscanf(ptr, "/%x:%x:%x.%x%n", &domain, &bus, &slot, &function, &n) == 4) {
        char pci_node[6];
        snprintf(pci_node, 6, "/%02x.%01x", slot, function);
        device_address += pci_node;
        ptr += n;
    }

    return device_address;
}

}} // namespace spice::streaming_agent
