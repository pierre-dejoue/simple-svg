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

std::ostream& operator<<(std::ostream& out, const ssvg::ShapesCounters& counters)
{
	out << "Nb of groups: " << counters.numGroups << std::endl;
	out << "Basic shapes:" << std::endl;
	out << "    rect: " << counters.numRects << "; circle: " << counters.numCircles << "; ellipse: " << counters.numEllipses << std::endl;
	out << "    line: " << counters.numLines << "; "
            << "polyline: " << counters.polylines.numPoly << " (" << counters.polylines.numPoints << " points); "
	        << "polygon: "  << counters.polygons.numPoly  << " (" << counters.polygons.numPoints  << " points);" << std::endl;
	out << "Paths: " << std::endl;
	out << "    closed: " << counters.paths.closed.numSubpaths << " (" << counters.paths.closed.numNodes << " nodes)" << std::endl;
	out << "    open:   " << counters.paths.open.numSubpaths   << " (" << counters.paths.open.numNodes   << " nodes)" << std::endl;
	return out;
}

std::ostream& operator<<(std::ostream& out, const ssvg::internals::AllocatedShapeAttrsCounters& counters)
{
	out << "ShapesAttributes:" << std::endl;
	out << "    nodes:     " << counters.numNodes << std::endl;
	out << "    allocated: " << counters.numAllocAttrs << std::endl;
	out << "    free:      " << counters.numFreeAttrs << std::endl;
	return out;
}

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

	const auto shapesCounters = ssvg::shapeListEnumerate(ssvg::imageGetShapeList(img));

	std::cout << std::endl;
	std::cout << shapesCounters;

	const auto allocatedShapeAttrsCounters = ssvg::internals::enumerateAllocatedShapeAttrs();

	std::cout << std::endl;
	std::cout << allocatedShapeAttrsCounters;

	closeSVGImage(img);

	ssvg::shutdownLib();

	return 0;
}
