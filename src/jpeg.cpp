/* Jpeg functions
 *
 * \copyright
 * Copyright 2017 Red Hat Inc. All rights reserved.
 */
#include <config.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <jpeglib.h>
#include <setjmp.h>

#include "jpeg.hpp"

struct JpegBuffer: public jpeg_destination_mgr
{
    JpegBuffer(std::vector<uint8_t>& buffer);
    ~JpegBuffer();

    std::vector<uint8_t>& buffer;
};

static boolean buf_empty_output_buffer(j_compress_ptr cinfo)
{
    JpegBuffer *buf = (JpegBuffer *) cinfo->dest;
    size_t size = buf->buffer.size();
    buf->buffer.resize(buf->buffer.capacity() * 2);
    buf->next_output_byte = &buf->buffer[0] + size;
    buf->free_in_buffer = buf->buffer.size() - size;
    return TRUE;
}

static void dummy_destination(j_compress_ptr cinfo)
{
}

JpegBuffer::JpegBuffer(std::vector<uint8_t>& buffer):
    buffer(buffer)
{
    if (buffer.capacity() < 32 * 1024) {
        buffer.resize(32 * 1024);
    } else {
        buffer.resize(buffer.capacity());
    }
    next_output_byte = &buffer[0];
    free_in_buffer = buffer.size();
    init_destination = dummy_destination;
    empty_output_buffer = buf_empty_output_buffer;
    term_destination = dummy_destination;
}

JpegBuffer::~JpegBuffer()
{
    buffer.resize(next_output_byte - &buffer[0]);
}

/* from https://github.com/LuaDist/libjpeg/blob/master/example.c */
void write_JPEG_file(std::vector<uint8_t>& buffer, int quality, uint8_t *data, unsigned width, unsigned height)
{
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    JSAMPROW row_pointer[1];
    int row_stride;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    JpegBuffer buf(buffer);
    cinfo.dest = &buf;

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 4;
    cinfo.in_color_space = JCS_EXT_BGRX;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    row_stride = width * 4;

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &data[cinfo.next_scanline * row_stride];
        // TODO check error
        (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);

    jpeg_destroy_compress(&cinfo);
}
