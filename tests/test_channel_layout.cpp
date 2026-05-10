#include "ffconvert.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <string>

TEST_CASE("av_channel_layout_default sets correct channel count") {
    // Test that av_channel_layout_default properly initializes layouts
    // This tests the v4l2 audio fix - ensuring channel layouts are properly set
    
    AVChannelLayout layout = {};
    
    // Test mono (1 channel)
    av_channel_layout_default(&layout, 1);
    CHECK(layout.nb_channels == 1);
    av_channel_layout_uninit(&layout);
    
    // Test stereo (2 channels)
    av_channel_layout_default(&layout, 2);
    CHECK(layout.nb_channels == 2);
    av_channel_layout_uninit(&layout);
    
    // Test 5.1 (6 channels)
    av_channel_layout_default(&layout, 6);
    CHECK(layout.nb_channels == 6);
    av_channel_layout_uninit(&layout);
}

TEST_CASE("AVChannelLayout comparison and copying") {
    AVChannelLayout layout1 = {};
    AVChannelLayout layout2 = {};
    
    // Both should default to 0 channels (empty/uninitialized)
    CHECK(layout1.nb_channels == 0);
    CHECK(layout2.nb_channels == 0);
    
    // Set both to stereo
    av_channel_layout_default(&layout1, 2);
    av_channel_layout_default(&layout2, 2);
    
    CHECK(layout1.nb_channels == layout2.nb_channels);
    CHECK(layout1.nb_channels == 2);
    
    av_channel_layout_uninit(&layout1);
    av_channel_layout_uninit(&layout2);
}

TEST_CASE("Codec finding") {
    // Test that AAC encoder can be found (used in conversion)
    const AVCodec* aac_encoder = avcodec_find_encoder_by_name("aac");
    REQUIRE(aac_encoder != nullptr);
    CHECK(std::string(aac_encoder->name) == "aac");
    
    // Test video codec finding
    const AVCodec* h264_encoder = avcodec_find_encoder_by_name("libx264");
    REQUIRE(h264_encoder != nullptr);
    CHECK(std::string(h264_encoder->name) == "libx264");
    
    const AVCodec* h265_encoder = avcodec_find_encoder_by_name("libx265");
    REQUIRE(h265_encoder != nullptr);
    CHECK(std::string(h265_encoder->name) == "libx265");
    
    const AVCodec* dnxhd_encoder = avcodec_find_encoder_by_name("dnxhd");
    REQUIRE(dnxhd_encoder != nullptr);
    CHECK(std::string(dnxhd_encoder->name) == "dnxhd");
}

TEST_CASE("Unsupported codec returns null") {
    // Test that nonexistent codec returns nullptr
    const AVCodec* invalid = avcodec_find_encoder_by_name("nonexistent_codec_xyz");
    CHECK(invalid == nullptr);
}

TEST_CASE("Format parsing logic") {
    // Test the format name detection logic used in setup_output_file
    auto matches_format = [](const std::string& name, const std::string& format) {
        return name.find(format) != std::string::npos;
    };
    
    SUBCASE("H.264 detection") {
        CHECK(matches_format("H.264 - MP4", "H.264"));
        CHECK(matches_format("H.264 - MKV", "H.264"));
        CHECK(!matches_format("H.265 - MP4", "H.264"));
    }
    
    SUBCASE("H.265 detection") {
        CHECK(matches_format("H.265 - MP4", "H.265"));
        CHECK(matches_format("H.265 - MKV", "H.265"));
        CHECK(!matches_format("H.264 - MP4", "H.265"));
    }
    
    SUBCASE("Avid DNxHR detection") {
        CHECK(matches_format("Avid DNxHR - MXF", "Avid DNxHR"));
        CHECK(!matches_format("H.264 - MP4", "Avid DNxHR"));
    }
}

TEST_CASE("Sample format constants") {
    // Verify AAC expected format is available
    // AAC encoder in FFmpeg typically outputs FLTP (float planar)
    CHECK(AV_SAMPLE_FMT_FLTP == 8);  // Float planar, stereo default for AAC
    CHECK(AV_SAMPLE_FMT_S16P == 6);  // Signed 16-bit planar
}

TEST_CASE("Even dimension calculation for codecs") {
    // H.264/H.265 require even dimensions
    auto make_even = [](int dim) { return (dim / 2) * 2; };
    
    CHECK(make_even(1920) == 1920);
    CHECK(make_even(1921) == 1920);
    CHECK(make_even(1080) == 1080);
    CHECK(make_even(1081) == 1080);
    CHECK(make_even(720) == 720);
    CHECK(make_even(721) == 720);
    
    // Edge cases
    CHECK(make_even(0) == 0);
    CHECK(make_even(1) == 0);
}