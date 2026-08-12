#pragma once

#include <ssvg/ssvg.h>

#include <filesystem>
#include <vector>

std::vector<char> loadFile(const std::filesystem::path& filepath);

// Image loaded with loadSVGImage *must* be freed with closeSVGImage
ssvg::Image* loadSVGImage(const std::filesystem::path& filepath);
void closeSVGImage(ssvg::Image* img, const std::filesystem::path& filepath = std::filesystem::path());

bool saveImage(const std::filesystem::path& filepath, const ssvg::Image* img);

std::ostream& operator<<(std::ostream& out, const ssvg::ShapesCounters& counters);
std::ostream& operator<<(std::ostream& out, const ssvg::internals::AllocatedShapeAttrsCounters& counters);
