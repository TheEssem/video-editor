#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string>

extern "C" {
    #include <libavformat/avformat.h>
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

    AVFormatContext* context = NULL;
    AVPacket pkt;

    av_register_all();

    if (avformat_open_input(&context, file.c_str(), NULL, NULL) != 0) { die("Failed to open file"); }
    if (avformat_find_stream_info(context, NULL) < 0) { die("Failed to find steam info"); }
    av_dump_format(context, 0, file.c_str(), 0);

    int video_stream_index = -1;
    for (int i = 0; i < context->nb_streams; i++) {
        if (context->streams[i]->codec->coder_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    uint32_t video_frames = 0;
    uint32_t audio_frames = 0;
    while(1) {
        int ret = av_read_frame(context, &pkt);
        if(ret < 0) {
            break;
        }
        if(pkt.stream_index == video_stream_index) {
            printf("Video frame\n");
            video_frames++;
        } else {
            printf("Audio frame\n");
            audio_frames++;
        }
    }

    printf("Finished processing video\n");
    printf("%i Video frames\n", video_frames);
    printf("%i Audio frames\n", audio_frames);
    return 0;
}