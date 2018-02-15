/* Jpeg functions
 *
 * \copyright
 * Copyright 2017 Red Hat Inc. All rights reserved.
 */
#ifndef SPICE_STREAMING_AGENT_JPEG_HPP
#define SPICE_STREAMING_AGENT_JPEG_HPP

#include <stdio.h>
#include <vector>

void write_JPEG_file(std::vector<uint8_t>& buffer, int quality, uint8_t *data, unsigned width, unsigned height);

#endif
