/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
  * Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
  * Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in the
  documentation and/or other materials provided with the distribution.
  * Neither the name of the copyright holder nor the
  names of its contributors may be used to endorse or promote products
  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Video decode on Raspberry Pi using MMAL
// Based upon example code from the Raspberry Pi

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <bcm_host.h>
#include <interface/mmal/mmal.h>
#include <interface/mmal/util/mmal_default_components.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/vc/mmal_vc_api.h>

#include <ihslib.h>

#include <SDL.h>

#include "decoders.h"
#include "sps_parser.h"

#define MAX_DECODE_UNIT_SIZE 262144

#define ALIGN(x, a) (((x)+(a)-1)&~((a)-1))


static bool started = false;
static VCOS_SEMAPHORE_T semaphore;
static MMAL_COMPONENT_T *decoder = NULL, *renderer = NULL;
static MMAL_POOL_T *pool_in = NULL, *pool_out = NULL;


static bool SizeChanged(const MMAL_VIDEO_FORMAT_T *video, const sps_dimension_t *dimension);

static void ChangedSize(IHS_Session *session, const sps_dimension_t *dimension);

static int setup_decoder(uint32_t width, uint32_t height);

static void input_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buf) {
    mmal_buffer_header_release(buf);

    if (port == decoder->input[0])
        vcos_semaphore_post(&semaphore);
}

static void control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buf) {
    if (buf->cmd == MMAL_EVENT_ERROR) {
        MMAL_STATUS_T status = *(uint32_t *) buf->data;
        fprintf(stderr, "Video decode error MMAL_EVENT_ERROR:%d\n", status);
    }

    mmal_buffer_header_release(buf);
}

static void output_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buf) {
    if (mmal_port_send_buffer(renderer->input[0], buf) != MMAL_SUCCESS) {
        fprintf(stderr, "Can't display decoded frame\n");
        mmal_buffer_header_release(buf);
    }
}

static int Start(IHS_Session *session, const IHS_StreamVideoConfig *config, void *context) {
    MMAL_STATUS_T status;

    if (config->codec != IHS_StreamVideoCodecH264) {
        fprintf(stderr, "Video format not supported\n");
        return ERROR_UNKNOWN_CODEC;
    }

    bcm_host_init();
    mmal_vc_init();

    uint32_t width = config->width;
    uint32_t height = config->height;

    return setup_decoder(width, height);

}

static int setup_decoder(uint32_t width, uint32_t height) {
    vcos_semaphore_create(&semaphore, "video_decoder", 1);
    if (mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_DECODER, &decoder) != MMAL_SUCCESS) {
        fprintf(stderr, "Can't create decoder\n");
        return ERROR_DECODER_OPEN_FAILED;
    }
    printf("create decoder %d x %d\n", width, height);

    MMAL_ES_FORMAT_T *format_in = decoder->input[0]->format;
    format_in->type = MMAL_ES_TYPE_VIDEO;
    format_in->encoding = MMAL_ENCODING_H264;
    format_in->es->video.width = ALIGN(width, 32);
    format_in->es->video.height = ALIGN(height, 16);
    format_in->es->video.crop.width = width;
    format_in->es->video.crop.height = height;
    format_in->es->video.frame_rate.num = 0;
    format_in->es->video.frame_rate.den = 1;
    format_in->es->video.par.num = 1;
    format_in->es->video.par.den = 1;
    format_in->flags = MMAL_ES_FORMAT_FLAG_FRAMED;

    if (mmal_port_format_commit(decoder->input[0]) != MMAL_SUCCESS) {
        fprintf(stderr, "Can't commit input format to decoder\n");
        return ERROR_DECODER_OPEN_FAILED;
    }

    decoder->input[0]->buffer_num = 5;
    decoder->input[0]->buffer_size = MAX_DECODE_UNIT_SIZE;
    pool_in = mmal_port_pool_create(decoder->input[0], decoder->input[0]->buffer_num, decoder->output[0]->buffer_size);

    MMAL_ES_FORMAT_T *format_out = decoder->output[0]->format;
    format_out->encoding = MMAL_ENCODING_OPAQUE;
    if (mmal_port_format_commit(decoder->output[0]) != MMAL_SUCCESS) {
        fprintf(stderr, "Can't commit output format to decoder\n");
        return ERROR_DECODER_OPEN_FAILED;
    }

    decoder->output[0]->buffer_num = 3;
    decoder->output[0]->buffer_size = decoder->output[0]->buffer_size_recommended;
    pool_out = mmal_port_pool_create(decoder->output[0], decoder->output[0]->buffer_num,
                                     decoder->output[0]->buffer_size);

    if (mmal_port_enable(decoder->control, control_callback) != MMAL_SUCCESS) {
        fprintf(stderr, "Can't enable control port\n");
        return ERROR_DECODER_OPEN_FAILED;
    }

    if (mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &renderer) != MMAL_SUCCESS) {
        fprintf(stderr, "Can't create renderer\n");
        return ERROR_DECODER_OPEN_FAILED;
    }

    format_in = renderer->input[0]->format;
    format_in->encoding = MMAL_ENCODING_OPAQUE;
    format_in->es->video.width = width;
    format_in->es->video.height = height;
    format_in->es->video.crop.x = 0;
    format_in->es->video.crop.y = 0;
    format_in->es->video.crop.width = width;
    format_in->es->video.crop.height = height;
    if (mmal_port_format_commit(renderer->input[0]) != MMAL_SUCCESS) {
        fprintf(stderr, "Can't set output format\n");
        return ERROR_DECODER_OPEN_FAILED;
    }

    MMAL_DISPLAYREGION_T param;
    param.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
    param.hdr.size = sizeof(MMAL_DISPLAYREGION_T);
    param.set = MMAL_DISPLAY_SET_LAYER | MMAL_DISPLAY_SET_NUM | MMAL_DISPLAY_SET_FULLSCREEN |
                MMAL_DISPLAY_SET_TRANSFORM;
    const char *vdrv = SDL_GetCurrentVideoDriver();
    if (strcmp(vdrv, "KMSDRM") == 0 || strcmp(vdrv, "RPI") == 0) {
        param.layer = 10000;
    } else {
        param.layer = 0;
    }
    param.display_num = 0;
    param.fullscreen = true;
    param.transform = MMAL_DISPLAY_ROT0;
//    int displayRotation = drFlags & DISPLAY_ROTATE_MASK;
//    switch (displayRotation) {
//        case DISPLAY_ROTATE_90:
//            param.transform = MMAL_DISPLAY_ROT90;
//            break;
//        case DISPLAY_ROTATE_180:
//            param.transform = MMAL_DISPLAY_ROT180;
//            break;
//        case DISPLAY_ROTATE_270:
//            param.transform = MMAL_DISPLAY_ROT270;
//            break;
//        default:
//            param.transform = MMAL_DISPLAY_ROT0;
//            break;
//    }

    if (mmal_port_parameter_set(renderer->input[0], &param.hdr) != MMAL_SUCCESS) {
        fprintf(stderr, "Can't set parameters\n");
        return ERROR_DECODER_OPEN_FAILED;
    }

    if (mmal_port_enable(renderer->control, control_callback) != MMAL_SUCCESS) {
        fprintf(stderr, "Can't enable control port\n");
        return ERROR_DECODER_OPEN_FAILED;
    }

    if (mmal_component_enable(renderer) != MMAL_SUCCESS) {
        fprintf(stderr, "Can't enable renderer\n");
        return ERROR_DECODER_OPEN_FAILED;
    }

    if (mmal_port_enable(renderer->input[0], input_callback) != MMAL_SUCCESS) {
        fprintf(stderr, "Can't enable renderer input port\n");
        return ERROR_DECODER_OPEN_FAILED;
    }

    if (mmal_port_enable(decoder->input[0], input_callback) != MMAL_SUCCESS) {
        fprintf(stderr, "Can't enable decoder input port\n");
        return ERROR_DECODER_OPEN_FAILED;
    }

    if (mmal_port_enable(decoder->output[0], output_callback) != MMAL_SUCCESS) {
        fprintf(stderr, "Can't enable decoder output port\n");
        return ERROR_DECODER_OPEN_FAILED;
    }

    if (mmal_component_enable(decoder) != MMAL_SUCCESS) {
        fprintf(stderr, "Can't enable decoder\n");
        return ERROR_DECODER_OPEN_FAILED;
    }

    printf("mmal decoder initialized\n");
    started = true;
    return 0;
}

static void Stop(IHS_Session *session, void *context) {
    started = false;
    if (decoder)
        mmal_component_destroy(decoder);

    if (renderer)
        mmal_component_destroy(renderer);

    if (pool_in)
        mmal_pool_destroy(pool_in);

    if (pool_out)
        mmal_pool_destroy(pool_out);

    vcos_semaphore_delete(&semaphore);
}

static int Submit(IHS_Session *session, IHS_Buffer *data, IHS_StreamVideoFrameFlag flags,
                  void *context) {
    if (!started) {
        return DR_NEED_IDR;
    }
    if (flags == IHS_StreamVideoFrameKeyFrame) {
        sps_dimension_t dimension;
        if (sps_parse_dimension_h264(IHS_BufferPointerAt(data, 4), &dimension) &&
            SizeChanged(&decoder->input[0]->format->es->video, &dimension)) {
            ChangedSize(session, &dimension);
        }
    }
    MMAL_STATUS_T status;
    MMAL_BUFFER_HEADER_T *buf;

    vcos_semaphore_wait(&semaphore);
    if ((buf = mmal_queue_get(pool_in->queue)) != NULL) {
        buf->flags = 0;
        buf->offset = 0;
        buf->pts = buf->dts = MMAL_TIME_UNKNOWN;
    } else {
        fprintf(stderr, "Video buffer full\n");
        return DR_NEED_IDR;
    }

//    if (type != IHS_StreamVideoFramePIC)
//        buf->flags |= MMAL_BUFFER_HEADER_FLAG_CONFIG;
//    else if (!first_entry) {
//        buf->flags |= MMAL_BUFFER_HEADER_FLAG_FRAME_START;
//        first_entry = true;
//    }
    if (flags & IHS_StreamVideoFrameKeyFrame) {
        buf->flags |= MMAL_BUFFER_HEADER_FLAG_KEYFRAME;
    }

    if (data->size + buf->length > buf->alloc_size) {
        fprintf(stderr, "Video decoder buffer too small\n");
        mmal_buffer_header_release(buf);
        return DR_NEED_IDR;
    }
    IHS_BufferReadMem(data, 0, buf->data + buf->length, data->size);
    buf->length += data->size;

    if ((status = mmal_port_send_buffer(decoder->input[0], buf)) != MMAL_SUCCESS) {
        mmal_buffer_header_release(buf);
        return DR_NEED_IDR;
    }

    //Send available output buffers to decoder
    while ((buf = mmal_queue_get(pool_out->queue))) {
        if ((status = mmal_port_send_buffer(decoder->output[0], buf)) != MMAL_SUCCESS)
            mmal_buffer_header_release(buf);
    }
    (void) status;

    return DR_OK;
}

static void ChangedSize(IHS_Session *session, const sps_dimension_t *dimension) {
    Stop(session, NULL);
    setup_decoder(dimension->width, dimension->height);
}

static bool SizeChanged(const MMAL_VIDEO_FORMAT_T *video, const sps_dimension_t *dimension) {
    return ALIGN(dimension->width, 32) != video->width || ALIGN(dimension->height, 16) != video->height;
}

bool mmalvid_set_region(bool fullscreen, int x, int y, int w, int h) {
    MMAL_DISPLAYREGION_T param;
    param.hdr.id = MMAL_PARAMETER_DISPLAYREGION;
    param.hdr.size = sizeof(MMAL_DISPLAYREGION_T);
    param.set = MMAL_DISPLAY_SET_FULLSCREEN | MMAL_DISPLAY_SET_DEST_RECT;
//    const char *vdrv = SDL_GetCurrentVideoDriver();
    param.display_num = 0;
    if (fullscreen) {
        param.fullscreen = true;
    } else {
        param.dest_rect.width = w;
        param.dest_rect.height = h;
        param.dest_rect.x = x;
        param.dest_rect.y = y;
    }

    return mmal_port_parameter_set(renderer->input[0], &param.hdr) == MMAL_SUCCESS;
}

const IHS_StreamVideoCallbacks VideoCallbacks = {
        .start = Start,
        .submit = Submit,
        .stop = Stop,
};

const IHS_StreamVideoCallbacks *module_video_callbacks() {
    return &VideoCallbacks;
}
