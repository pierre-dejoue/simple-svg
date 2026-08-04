#pragma once

#include <ssvg/ssvg.h>

#include <vector>

std::vector<char> loadFile(const char* filepath);

bool saveImage(const char* filepath, ssvg::Image* img);
