#pragma once

#include <string>

namespace FFConvert {

bool convert_video_file(const std::string& in_path, const std::string& out_path, const std::string& format_name, int scale_width = 0, int scale_height = 0);

} // namespace FFConvert
