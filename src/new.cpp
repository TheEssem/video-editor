#define INBUF_SIZE 4096
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavfilter/avfilter.h>
    #include <libavcodec/avcodec.h>
    #include <x264.h>
    #include <libavutil/mathematics.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <ao/ao.h>
}

void die(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

int decode(AVCodecContext *ctx, AVFrame *frame, AVPacket *pkt) {
    char buf[1024];
    int ret = avcodec_send_packet(ctx, pkt);
    if (ret < 0) die("Error sending a packet for decoding");
    while (ret >= 0) {
        ret = avcodec_receive_frame(ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return -1;
        else if (ret < 0) die("Error during decoding");
        
        printf("Parsing frame %3d\n", ctx->frame_number);
        // fflush(stdout);
        /* the picture is allocated by the decoder. no need to
           free it */
        // snprintf(buf, sizeof(buf), filename, dec_ctx->frame_number);
        // pgm_save(frame->data[0], frame->linesize[0],
                //  frame->width, frame->height, buf);
    }
    return 0;
}

int main(int argc, char **argv) {
    if(argc < 2) die("You need to provide input file");
    uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
    uint8_t *data;
    size_t   data_size;

    std::string in_file = argv[1];
    std::string out_file = "out.mp4";
    FILE *f;

    const AVCodec* codec;
    AVCodecParserContext* parser;
    AVCodecContext* ctx = NULL;
    AVFrame* frame;
    AVPacket* pkt;

    avcodec_register_all();
    pkt = av_packet_alloc();
    if(!pkt) die("Failed to allocate packet");

    // This ensures no overreading happens for damaged MPEG streams
    memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);

    /*
        Currently using this for testing
        In the future i will copy the code from main.cpp for finding the correct decoder
    */
    codec = avcodec_find_decoder(AV_CODEC_ID_MPEG1VIDEO);
    if(!codec) die("Codec not found");

    /*
        I guess switching the code to the one from main.cpp for detecting the correct decoder
        will also make this yield the correct parser
    */
    parser = av_parser_init(codec->id);
    if(!parser) die("Parser not found");

    // Allocate context and frame
    ctx = avcodec_alloc_context3(codec);
    frame = av_frame_alloc();

    // Open the codec
    if (avcodec_open2(ctx, codec, NULL) < 0) die("Could not open codec");

    /*
        Read the entire file and then let the parser split it into individual frames
        And then loop through them
    */
    f = fopen(in_file.c_str(), "rb");
    if(!f) die("Could not open input file");
    int n_frames = 0;
    while(!feof(f)) {
        /*
            If data_size is equal to 0 then we either encountered an error while reading
            or reached the end of the file, either way, break the loop
        */
        data_size = fread(inbuf, 1, INBUF_SIZE, f);
        if(!data_size) break;
        data = inbuf;
        while (data_size > 0) {
            int ret = av_parser_parse2(parser, ctx, &pkt->data, &pkt->size,
                                   data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
            if (ret < 0) die("Error while parsing");
            data      += ret;
            data_size -= ret;
            if (pkt->size) {
                //Read a packet :)
                if(decode(ctx, frame, pkt) < 0) {

                } else {
                    n_frames++;
                }

                // printf("Packet: %d\n", pkt->stream_index);
            }
        }
    }

    printf("Number of frames: %i\n", n_frames);
    av_parser_close(parser);
    avcodec_free_context(&ctx);
    av_frame_free(&frame);
    av_packet_free(&pkt);
    return 1;
}