/* \copyright
 * Copyright 2018 Red Hat Inc. All rights reserved.
 */

#include "utils.hpp"

#include <spice-streaming-agent/error.hpp>

#include <glob.h>
#include <string.h>
#include <stdexcept>


namespace spice {
namespace streaming_agent {
namespace utils {

std::vector<std::string> glob(const std::string& pattern)
{
    glob_t glob_result{};

    std::vector<std::string> filenames;

    int ret = glob(pattern.c_str(), GLOB_ERR, NULL, &glob_result);

    if(ret != 0) {
        globfree(&glob_result);

        if (ret == GLOB_NOMATCH) {
            return filenames;
        }

        throw Error("glob(" + pattern + ") failed with return value " + std::to_string(ret) +
                    ": " + strerror(errno));
    }

    for(size_t i = 0; i < glob_result.gl_pathc; ++i) {
        filenames.push_back(glob_result.gl_pathv[i]);
    }

    globfree(&glob_result);

    return filenames;
}

}}} // namespace spice::streaming_agent::utils
