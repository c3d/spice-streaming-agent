/* Jpeg functions
 *
 * \copyright
 * Copyright 2017 Red Hat Inc. All rights reserved.
 */
#ifndef RH_STREAMING_AGENT_JPEG_HPP_
#define RH_STREAMING_AGENT_JPEG_HPP_

#include <stdio.h>
#include <vector>

void write_JPEG_file(std::vector<uint8_t>& buffer, int quality, uint8_t *data, unsigned width, unsigned height);

#endif
