#define OUT_FILE "out.mp4"

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

int main(int argc, char **argv) {
    if(argc < 2) die("You need to provide a file");
    std::string file = argv[1];

    AVOutputFormat* out_fmt = NULL;
    AVFormatContext* in_ctx = NULL;
    AVFormatContext* out_ctx = NULL;

    av_register_all();

    if (avformat_open_input(&in_ctx, file.c_str(), NULL, NULL) != 0) { die("Could not open input file"); }
    if (avformat_find_stream_info(in_ctx, NULL) < 0) { die("Failed to find stream info"); }
    av_dump_format(in_ctx, 0, file.c_str(), 0);

    /*
        Commented out for testing
    */
    avformat_alloc_output_context2(&out_ctx, NULL, NULL, OUT_FILE);
    if(!out_ctx) die("Could not create output context");
    out_fmt = out_ctx->oformat;

    /*
        Here i should allocate the streams for the output
        otherwise avformat_write_header() fails and ffmpeg says
        "No streams to mux were specified"
    */
    for(int i = 0; i < in_ctx->nb_streams; i++) {
        AVStream* in_stream = in_ctx->streams[i];
        AVStream* out_stream = avformat_new_stream(out_ctx, in_stream->codec->codec);
        if(!out_stream) die("Could not allocating output stream");
        if(avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) die("Could not copy context from input to output");
        out_stream->codec->codec_tag = 0;
        if(out_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
            out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }
    }


    /*
        Dump the file format into the context.
        I'm not sure why this is neccesary if the file is empty?

        maybe it just switches the pointer to a different class with the same values
        but with more fields for format data
    */
    av_dump_format(out_ctx, 0, OUT_FILE, 1);


    
    /*
        I think this just verifies if the file exists
    */
    if (!(out_fmt->flags & AVFMT_NOFILE))
        if (avio_open(&out_ctx->pb, OUT_FILE, AVIO_FLAG_WRITE) < 0) die("Could not open output file");

    /*
        Writes the format header to the file
    */
    if(avformat_write_header(out_ctx, NULL) < 0) die("Error occured while opening output file");
    

    /*
        I think this finds the correct codec for reading the input
    */
    int in_video_stream_index = -1;
    AVCodec* in_codec = nullptr;
    AVCodecContext* in_avctx = nullptr;
    for (int i = 0; i < in_ctx->nb_streams; i++) {
        if (in_ctx->streams[i]->codec->coder_type == AVMEDIA_TYPE_VIDEO) {
            in_video_stream_index = i;
            in_avctx = in_ctx->streams[i]->codec;
            in_codec = avcodec_find_decoder(in_avctx->codec_id);
            if (!in_codec) die("Input codec not found");
            break;
        }
    }

    /*
        Find the correct codec for writing to output
    */
    int out_video_stream_index = -1;
    AVCodec* out_codec = nullptr;
    AVCodecContext* out_avctx = nullptr;
    for(int i = 0; i < out_ctx->nb_streams; i++) {
        if(out_ctx->streams[i]->codec->coder_type == AVMEDIA_TYPE_VIDEO) {
            out_video_stream_index = i;
            out_codec = avcodec_find_encoder(out_ctx->streams[i]->codec->codec_id);
            out_avctx = avcodec_alloc_context3(out_codec);

            out_avctx->height = in_avctx->height;
            out_avctx->width = in_avctx->width;
            out_avctx->sample_aspect_ratio = in_avctx->sample_aspect_ratio;
            
            if(out_codec->pix_fmts) 
                out_avctx->pix_fmt = out_codec->pix_fmts[0];
            else
                out_avctx->pix_fmt = in_avctx->pix_fmt;
            
            out_avctx->time_base = in_avctx->time_base;

            avcodec_open2(out_avctx, out_codec, NULL);
        }
    }

    /*
        Initalization for main loop
    */
    uint32_t video_frames = 0;
    uint32_t audio_frames = 0;
    int frame_finished;
    AVPacket pkt;
    AVFrame *frame = NULL;
    frame = av_frame_alloc();
    avcodec_open2(in_avctx, in_codec, NULL);

    // Main Loop
    while(1) {
        int ret = av_read_frame(in_ctx, &pkt);
        /*
            If ret < 0 then we are either at the end of the file
            Or we encountered an error while reading
        */
        if(ret < 0) break;
        if(pkt.stream_index == in_video_stream_index) {
            /*
                Seg Faults if you dont initialize frame with the return value of av_frame_alloc()
            */
            avcodec_decode_video2(in_avctx, frame, &frame_finished, &pkt);
            
            if(frame_finished) {
                // Operate on frame


                /*
                    Reencode packet and write to file!
                */
                int got_packet = 0;
                AVPacket enc_pkt = { 0 };
                av_init_packet(&enc_pkt);

                avcodec_encode_video2(out_avctx, &enc_pkt, frame, &got_packet);
                av_interleaved_write_frame(out_ctx, &enc_pkt);
            } else {
                // ???
                // I'm not sure under what circumstances frame_finished is equal to 0
            }
            printf("Video frame\n");
            video_frames++;
        } else {
            avcodec_decode_audio4(in_avctx, frame, &frame_finished, &pkt);
            printf("Audio frame\n");
            audio_frames++;
        }

        av_packet_unref(&pkt);
    }

    /*
        Free memory
    */
    avformat_close_input(&in_ctx);
    avcodec_close(in_avctx);
    avcodec_close(out_avctx);
    av_free(frame);

    printf("Finished processing video\n");
    printf("%i Video frames\n", video_frames);
    printf("%i Audio frames\n", audio_frames);
    return 0;
}