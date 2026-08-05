#pragma once

#include <ssvg/ssvg.h>

#include <filesystem>
#include <vector>

std::vector<char> loadFile(const std::filesystem::path& filepath);

bool saveImage(const std::filesystem::path& filepath, const ssvg::Image* img);
