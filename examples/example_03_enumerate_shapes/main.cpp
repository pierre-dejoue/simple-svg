#include <ssvg/ssvg.h>

#include <examples/utils.h>
#include <stdutils/memory.h>

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		printf("(x) Wrong number of arguments.\n");
		printf("Usage: example_03_enumerate_shapes input_file.svg\n");
		return 1;
	}

	// Call with one argument: the path to an input SVG file
	const std::string input_svg_file = argv[1];

	ssvg::initLib();

	ssvg::Image* img = loadSVGImage(fs::path(input_svg_file));
	if (!img) {
		return 1;
	}

	const auto shapesCounters = ssvg::shapeListEnumerate(ssvg::imageGetRootShapeList(img));

	std::cout << std::endl;
	std::cout << shapesCounters;

	const auto allocatedShapeAttrsCounters = ssvg::internals::enumerateAllocatedShapeAttrs();

	std::cout << std::endl;
	std::cout << allocatedShapeAttrsCounters;

	closeSVGImage(img);

	ssvg::shutdownLib();

	return 0;
}
