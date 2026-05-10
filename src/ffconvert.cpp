#include "ffconvert.h"

#include <iostream>
#include <stdexcept>
#include <vector>
#include <memory>

// FFmpeg headers - must be extern "C" in C++
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/channel_layout.h>
#include <libavutil/audio_fifo.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

namespace { // Anonymous namespace for internal implementation details

// Custom deleters for FFmpeg unique_ptr management, to handle the double pointer free functions
struct AVFrameDeleter {
    void operator()(AVFrame* frame) const { av_frame_free(&frame); }
};
struct AVPacketDeleter {
    void operator()(AVPacket* packet) const { av_packet_free(&packet); }
};

// Type aliases for convenience
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;

// Helper to log FFmpeg errors
void log_error(const std::string& msg, int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_make_error_string(errbuf, sizeof(errbuf), errnum);
    std::cerr << msg << ": " << errbuf << " (" << errnum << ")" << std::endl;
}

// A struct to hold all the state for a single transcoding operation.
// This simplifies resource management and cleanup.
struct StreamContext {
    AVCodecContext* dec_ctx = nullptr;
    AVCodecContext* enc_ctx = nullptr;
    AVStream* in_stream = nullptr;
    AVStream* out_stream = nullptr;
    int64_t next_pts = 0; // For tracking audio PTS after FIFO
};

// Main context for the entire conversion process
struct ConversionContext {
    AVFormatContext* ifmt_ctx = nullptr;
    AVFormatContext* ofmt_ctx = nullptr;
    
    std::unique_ptr<StreamContext> video_stream_ctx;
    std::unique_ptr<StreamContext> audio_stream_ctx;
    
    SwsContext* sws_ctx = nullptr;
    SwrContext* swr_ctx = nullptr;
    AVAudioFifo* audio_fifo = nullptr;

    // Destructor to handle cleanup automatically (RAII)
    ~ConversionContext() {
        if (video_stream_ctx) {
            avcodec_free_context(&video_stream_ctx->dec_ctx);
            avcodec_free_context(&video_stream_ctx->enc_ctx);
        }
        if (audio_stream_ctx) {
            avcodec_free_context(&audio_stream_ctx->dec_ctx);
            avcodec_free_context(&audio_stream_ctx->enc_ctx);
        }

        if (ifmt_ctx) {
            avformat_close_input(&ifmt_ctx);
        }

        if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&ofmt_ctx->pb);
        }
        avformat_free_context(ofmt_ctx);

        sws_freeContext(sws_ctx);
        swr_free(&swr_ctx);
        av_audio_fifo_free(audio_fifo);
    }
};

bool open_input_file(ConversionContext& ctx, const std::string& in_path) {
    int ret = avformat_open_input(&ctx.ifmt_ctx, in_path.c_str(), nullptr, nullptr);
    if (ret < 0) {
        log_error("Could not open input file " + in_path, ret);
        return false;
    }

    ret = avformat_find_stream_info(ctx.ifmt_ctx, nullptr);
    if (ret < 0) {
        log_error("Failed to find stream information", ret);
        return false;
    }

    for (unsigned int i = 0; i < ctx.ifmt_ctx->nb_streams; ++i) {
        AVStream* in_stream = ctx.ifmt_ctx->streams[i];
        const AVCodec* decoder = avcodec_find_decoder(in_stream->codecpar->codec_id);
        if (!decoder) {
            std::cerr << "Failed to find decoder for stream #" << i << std::endl;
            continue;
        }

        AVCodecContext* dec_ctx = avcodec_alloc_context3(decoder);
        if (!dec_ctx) {
            std::cerr << "Failed to allocate decoder context" << std::endl;
            return false;
        }

        ret = avcodec_parameters_to_context(dec_ctx, in_stream->codecpar);
        if (ret < 0) {
            log_error("Failed to copy decoder parameters to context", ret);
            avcodec_free_context(&dec_ctx);
            return false;
        }
        
        // Let the decoder set its time base
        dec_ctx->time_base = in_stream->time_base;
        if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
            dec_ctx->framerate = av_guess_frame_rate(ctx.ifmt_ctx, in_stream, NULL);
        }

        ret = avcodec_open2(dec_ctx, decoder, nullptr);
        if (ret < 0) {
            log_error("Failed to open decoder", ret);
            avcodec_free_context(&dec_ctx);
            return false;
        }

        if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && !ctx.video_stream_ctx) {
            ctx.video_stream_ctx = std::make_unique<StreamContext>();
            ctx.video_stream_ctx->dec_ctx = dec_ctx;
            ctx.video_stream_ctx->in_stream = in_stream;
            std::cout << "DEBUG: Found video stream." << std::endl;
        } else if (in_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && !ctx.audio_stream_ctx) {
            ctx.audio_stream_ctx = std::make_unique<StreamContext>();
            ctx.audio_stream_ctx->dec_ctx = dec_ctx;
            ctx.audio_stream_ctx->in_stream = in_stream;
            std::cout << "DEBUG: Found audio stream." << std::endl;
        } else {
            avcodec_free_context(&dec_ctx); // Unused stream
        }
    }

    if (!ctx.video_stream_ctx && !ctx.audio_stream_ctx) {
        std::cerr << "No video or audio streams found in input file." << std::endl;
        return false;
    }
    return true;
}

bool setup_output_file(ConversionContext& ctx, const std::string& out_path, const std::string& format_name, int scale_width = 0, int scale_height = 0) {
    int ret = avformat_alloc_output_context2(&ctx.ofmt_ctx, nullptr, nullptr, out_path.c_str());
    if (ret < 0) {
        log_error("Could not create output context", ret);
        return false;
    }

    // Setup Video Encoder
    if (ctx.video_stream_ctx) {
        const char* codec_name = nullptr;
        AVPixelFormat pix_fmt;

        if (format_name.find("H.264") != std::string::npos) {
            codec_name = "libx264";
            pix_fmt = AV_PIX_FMT_YUV420P;
        } else if (format_name.find("H.265") != std::string::npos) {
            codec_name = "libx265";
            pix_fmt = AV_PIX_FMT_YUV420P;
        } else if (format_name.find("Avid DNxHR") != std::string::npos) {
            codec_name = "dnxhd";
            pix_fmt = AV_PIX_FMT_YUV422P10LE; // 10-bit 4:2:2
        } else {
            std::cerr << "Unsupported video format: " << format_name << std::endl;
            return false;
        }

        const AVCodec* encoder = avcodec_find_encoder_by_name(codec_name);
        if (!encoder) {
            std::cerr << "Could not find encoder: " << codec_name << std::endl;
            return false;
        }

        ctx.video_stream_ctx->enc_ctx = avcodec_alloc_context3(encoder);
        if (!ctx.video_stream_ctx->enc_ctx) {
            std::cerr << "Failed to allocate video encoder context" << std::endl;
            return false;
        }
        
        AVCodecContext* enc_ctx = ctx.video_stream_ctx->enc_ctx;
        AVCodecContext* dec_ctx = ctx.video_stream_ctx->dec_ctx;

        // Use scale dimensions if provided, otherwise use original dimensions
        // Make sure dimensions are even (required by many codecs like H.264/H.265)
        if (scale_width > 0 && scale_height > 0) {
            enc_ctx->width = (scale_width / 2) * 2;  // Ensure even
            enc_ctx->height = (scale_height / 2) * 2; // Ensure even
            std::cout << "DEBUG: Scaling to " << enc_ctx->width << "x" << enc_ctx->height << std::endl;
        } else {
            enc_ctx->height = dec_ctx->height;
            enc_ctx->width = dec_ctx->width;
        }
        enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
        enc_ctx->pix_fmt = pix_fmt;
        // Set the time base for the encoder from the decoder's framerate.
        // This is crucial for correct video speed.
        enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
        enc_ctx->framerate = dec_ctx->framerate;

        if (format_name.find("Avid DNxHR") != std::string::npos) {
             av_opt_set(enc_ctx->priv_data, "profile", "dnxhr_hqx", 0);
        }

        if (ctx.ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
            enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        ret = avcodec_open2(enc_ctx, encoder, nullptr);
        if (ret < 0) {
            log_error("Cannot open video encoder", ret);
            return false;
        }

        ctx.video_stream_ctx->out_stream = avformat_new_stream(ctx.ofmt_ctx, nullptr);
        if (!ctx.video_stream_ctx->out_stream) {
            std::cerr << "Failed allocating output video stream" << std::endl;
            return false;
        }
        avcodec_parameters_from_context(ctx.video_stream_ctx->out_stream->codecpar, enc_ctx);
        ctx.video_stream_ctx->out_stream->time_base = enc_ctx->time_base;
    }

    // Setup Audio Encoder
    if (ctx.audio_stream_ctx) {
        const AVCodec* encoder = avcodec_find_encoder_by_name("aac");
        if (!encoder) {
            std::cerr << "Could not find AAC encoder" << std::endl;
            return false;
        }

        ctx.audio_stream_ctx->enc_ctx = avcodec_alloc_context3(encoder);
        if (!ctx.audio_stream_ctx->enc_ctx) {
            std::cerr << "Failed to allocate audio encoder context" << std::endl;
            return false;
        }
        
        AVCodecContext* enc_ctx = ctx.audio_stream_ctx->enc_ctx;
        AVCodecContext* dec_ctx = ctx.audio_stream_ctx->dec_ctx;

        enc_ctx->sample_rate = dec_ctx->sample_rate;
        // Use default/stereo channel layout for AAC encoder
        // Some AVI files (especially from v4l2 capture) have unusual layouts that AAC doesn't support
        av_channel_layout_default(&enc_ctx->ch_layout, dec_ctx->ch_layout.nb_channels);
        // Use a known good sample format for AAC instead of the deprecated encoder->sample_fmts
        enc_ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;
        // The time base for audio is 1 / sample_rate
        enc_ctx->time_base = {1, enc_ctx->sample_rate};
        
        if (ctx.ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
            enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        }

        ret = avcodec_open2(enc_ctx, encoder, nullptr);
        if (ret < 0) {
            log_error("Cannot open audio encoder", ret);
            return false;
        }

        ctx.audio_stream_ctx->out_stream = avformat_new_stream(ctx.ofmt_ctx, nullptr);
        if (!ctx.audio_stream_ctx->out_stream) {
            std::cerr << "Failed allocating output audio stream" << std::endl;
            return false;
        }
        avcodec_parameters_from_context(ctx.audio_stream_ctx->out_stream->codecpar, enc_ctx);
        ctx.audio_stream_ctx->out_stream->time_base = enc_ctx->time_base;
    }

    if (!(ctx.ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ctx.ofmt_ctx->pb, out_path.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            log_error("Could not open output file " + out_path, ret);
            return false;
        }
    }

    ret = avformat_write_header(ctx.ofmt_ctx, nullptr);
    if (ret < 0) {
        log_error("Error occurred when opening output file", ret);
        return false;
    }

    return true;
}

bool encode_and_write(ConversionContext& ctx, AVFrame* frame, StreamContext* s_ctx) {
    int ret = avcodec_send_frame(s_ctx->enc_ctx, frame);
    if (ret < 0) {
        log_error("Error sending a frame for encoding", ret);
        return false;
    }

    while (ret >= 0) {
        AVPacketPtr pkt(av_packet_alloc());
        ret = avcodec_receive_packet(s_ctx->enc_ctx, pkt.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return true;
        } else if (ret < 0) {
            log_error("Error during encoding", ret);
            return false;
        }

        pkt->stream_index = s_ctx->out_stream->index;
        // Rescale packet timestamp from codec to stream time base
        av_packet_rescale_ts(pkt.get(), s_ctx->enc_ctx->time_base, s_ctx->out_stream->time_base);

        ret = av_interleaved_write_frame(ctx.ofmt_ctx, pkt.get());
        if (ret < 0) {
            log_error("Error while writing output packet", ret);
            return false;
        }
    }
    return true;
}

bool transcode_frame(ConversionContext& ctx, AVFrame* frame, StreamContext* s_ctx) {
    if (s_ctx->enc_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
        // Video frame processing
        if (!ctx.sws_ctx) {
            // Use SWS_LANCZOS for high-quality scaling (better than bilinear, not too soft)
            ctx.sws_ctx = sws_getContext(
                s_ctx->dec_ctx->width, s_ctx->dec_ctx->height, s_ctx->dec_ctx->pix_fmt,
                s_ctx->enc_ctx->width, s_ctx->enc_ctx->height, s_ctx->enc_ctx->pix_fmt,
                SWS_LANCZOS, nullptr, nullptr, nullptr);
            if (!ctx.sws_ctx) {
                std::cerr << "Failed to create SwsContext" << std::endl;
                return false;
            }
        }
        
        AVFramePtr scaled_frame(av_frame_alloc());
        scaled_frame->format = s_ctx->enc_ctx->pix_fmt;
        scaled_frame->width = s_ctx->enc_ctx->width;
        scaled_frame->height = s_ctx->enc_ctx->height;
        if (av_frame_get_buffer(scaled_frame.get(), 0) < 0) {
            std::cerr << "Could not allocate scaled frame buffer" << std::endl;
            return false;
        }

        sws_scale(ctx.sws_ctx, (const uint8_t* const*)frame->data, frame->linesize, 0,
                  s_ctx->dec_ctx->height, scaled_frame->data, scaled_frame->linesize);

        // **FIX**: Rescale the PTS from the input stream's time base to the encoder's time base.
        // This is the key to fixing the video frame rate.
        scaled_frame->pts = av_rescale_q(frame->pts, s_ctx->in_stream->time_base, s_ctx->enc_ctx->time_base);
        return encode_and_write(ctx, scaled_frame.get(), s_ctx);

    } else if (s_ctx->enc_ctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        // Audio frame processing
        if (!ctx.swr_ctx) {
            if (swr_alloc_set_opts2(&ctx.swr_ctx,
                &s_ctx->enc_ctx->ch_layout, s_ctx->enc_ctx->sample_fmt, s_ctx->enc_ctx->sample_rate,
                &s_ctx->dec_ctx->ch_layout, s_ctx->dec_ctx->sample_fmt, s_ctx->dec_ctx->sample_rate,
                0, nullptr) < 0) {
                std::cerr << "Failed to create SwrContext" << std::endl;
                return false;
            }
            if (swr_init(ctx.swr_ctx) < 0) {
                std::cerr << "Failed to initialize SwrContext" << std::endl;
                return false;
            }
        }
        if (!ctx.audio_fifo) {
            ctx.audio_fifo = av_audio_fifo_alloc(s_ctx->enc_ctx->sample_fmt, s_ctx->enc_ctx->ch_layout.nb_channels, 1);
            if (!ctx.audio_fifo) {
                std::cerr << "Failed to create audio FIFO" << std::endl;
                return false;
            }
        }
        
        AVFramePtr resampled_frame(av_frame_alloc());
        resampled_frame->sample_rate = s_ctx->enc_ctx->sample_rate;
        resampled_frame->ch_layout = s_ctx->enc_ctx->ch_layout;
        resampled_frame->format = s_ctx->enc_ctx->sample_fmt;
        resampled_frame->nb_samples = av_rescale_rnd(swr_get_delay(ctx.swr_ctx, s_ctx->dec_ctx->sample_rate) + frame->nb_samples,
                                                    s_ctx->enc_ctx->sample_rate, s_ctx->dec_ctx->sample_rate, AV_ROUND_UP);
        if (av_frame_get_buffer(resampled_frame.get(), 0) < 0) {
            std::cerr << "Could not allocate resampled frame buffer" << std::endl;
            return false;
        }

        int samples_converted = swr_convert(ctx.swr_ctx, resampled_frame->data, resampled_frame->nb_samples, (const uint8_t**)frame->data, frame->nb_samples);
        if (samples_converted < 0) {
            std::cerr << "Error during audio resampling" << std::endl;
            return false;
        }

        // **FIX**: Initialize the audio PTS tracker on the first valid frame.
        if (s_ctx->next_pts == 0 && frame->pts != AV_NOPTS_VALUE) {
            s_ctx->next_pts = av_rescale_q(frame->pts, s_ctx->in_stream->time_base, s_ctx->enc_ctx->time_base);
        }

        if (av_audio_fifo_write(ctx.audio_fifo, (void**)resampled_frame->data, samples_converted) < samples_converted) {
            std::cerr << "Could not write to audio FIFO" << std::endl;
            return false;
        }

        while (av_audio_fifo_size(ctx.audio_fifo) >= s_ctx->enc_ctx->frame_size) {
            AVFramePtr fifo_frame(av_frame_alloc());
            fifo_frame->nb_samples = s_ctx->enc_ctx->frame_size;
            fifo_frame->ch_layout = s_ctx->enc_ctx->ch_layout;
            fifo_frame->format = s_ctx->enc_ctx->sample_fmt;
            fifo_frame->sample_rate = s_ctx->enc_ctx->sample_rate;
            if (av_frame_get_buffer(fifo_frame.get(), 0) < 0) {
                std::cerr << "Could not allocate FIFO frame buffer" << std::endl;
                return false;
            }

            if (av_audio_fifo_read(ctx.audio_fifo, (void**)fifo_frame->data, s_ctx->enc_ctx->frame_size) < s_ctx->enc_ctx->frame_size) {
                std::cerr << "Could not read from audio FIFO" << std::endl;
                return false;
            }
            
            // **FIX**: Set the PTS for the outgoing audio frame and increment for the next one.
            // This fixes the "Timestamps are unset" warning.
            fifo_frame->pts = s_ctx->next_pts;
            s_ctx->next_pts += fifo_frame->nb_samples;

            if (!encode_and_write(ctx, fifo_frame.get(), s_ctx)) return false;
        }
    }
    return true;
}

bool run_conversion_loop(ConversionContext& ctx) {
    AVFramePtr frame(av_frame_alloc());
    AVPacketPtr pkt(av_packet_alloc());

    while (av_read_frame(ctx.ifmt_ctx, pkt.get()) >= 0) {
        StreamContext* s_ctx = nullptr;
        if (ctx.video_stream_ctx && pkt->stream_index == ctx.video_stream_ctx->in_stream->index) {
            s_ctx = ctx.video_stream_ctx.get();
        } else if (ctx.audio_stream_ctx && pkt->stream_index == ctx.audio_stream_ctx->in_stream->index) {
            s_ctx = ctx.audio_stream_ctx.get();
        } else {
            av_packet_unref(pkt.get());
            continue;
        }
        
        int ret = avcodec_send_packet(s_ctx->dec_ctx, pkt.get());
        av_packet_unref(pkt.get());
        if (ret < 0) {
            log_error("Error submitting a packet for decoding", ret);
            return false;
        }

        while (ret >= 0) {
            ret = avcodec_receive_frame(s_ctx->dec_ctx, frame.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                log_error("Error during decoding", ret);
                return false;
            }
            
            // Use best_effort_timestamp as a fallback if PTS is missing
            if (frame->pts == AV_NOPTS_VALUE) {
                frame->pts = frame->best_effort_timestamp;
            }

            if (!transcode_frame(ctx, frame.get(), s_ctx)) return false;
            av_frame_unref(frame.get());
        }
    }
    return true;
}

bool flush_encoders(ConversionContext& ctx) {
    if (ctx.video_stream_ctx) {
        if (!encode_and_write(ctx, nullptr, ctx.video_stream_ctx.get())) return false;
    }
    if (ctx.audio_stream_ctx) {
        if (!encode_and_write(ctx, nullptr, ctx.audio_stream_ctx.get())) return false;
    }
    return true;
}

} // end anonymous namespace

namespace FFConvert {

bool convert_video_file(const std::string& in_path, const std::string& out_path, const std::string& format_name, int scale_width, int scale_height) {
    ConversionContext ctx;

    std::cout << "DEBUG: Starting conversion for " << in_path << std::endl;
    if (scale_width > 0 && scale_height > 0) {
        std::cout << "DEBUG: Scale enabled: " << scale_width << "x" << scale_height << std::endl;
    }

    if (!open_input_file(ctx, in_path)) {
        std::cerr << "Error opening input file." << std::endl;
        return false;
    }
    
    if (!setup_output_file(ctx, out_path, format_name, scale_width, scale_height)) {
        std::cerr << "Error setting up output file." << std::endl;
        return false;
    }

    std::cout << "DEBUG: Starting transcoding loop..." << std::endl;
    if (!run_conversion_loop(ctx)) {
        std::cerr << "Error during transcoding." << std::endl;
        return false;
    }
    std::cout << "DEBUG: Transcoding loop finished." << std::endl;

    std::cout << "DEBUG: Flushing encoders..." << std::endl;
    if (!flush_encoders(ctx)) {
        std::cerr << "Error flushing encoders." << std::endl;
        return false;
    }
    std::cout << "DEBUG: Encoders flushed." << std::endl;

    int ret = av_write_trailer(ctx.ofmt_ctx);
    if (ret < 0) {
        log_error("Error writing trailer", ret);
        return false;
    }

    std::cout << "DEBUG: Conversion finished successfully for " << out_path << std::endl;
    return true;
}

} // namespace FFConvert

