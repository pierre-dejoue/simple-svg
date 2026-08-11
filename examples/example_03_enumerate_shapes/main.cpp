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

struct ShapesCounters {
	struct PolyNum {
		uint32_t numPoly;
		uint32_t numPoints;
	};

	uint32_t numGroups;
	uint32_t numRects;
	uint32_t numCircles;
	uint32_t numEllipses;
	uint32_t numLines;
	uint32_t numTexts;
	PolyNum polygons;
	PolyNum polylines;
	ssvg::PathNum paths;
};

std::ostream& operator<<(std::ostream& out, const ShapesCounters& counters);

void enumerateShapesRecursive(const ssvg::ShapeList* shapeList, ShapesCounters* counters)
{
	assert(shapeList);
	assert(counters);

	const uint32_t numShapes = shapeList->m_NumShapes;
	for (uint32_t shapeIndex = 0; shapeIndex < numShapes; ++shapeIndex) {
		const ssvg::ShapeType::Enum shapeType = ssvg::shapeListGetShapeType(shapeList, shapeIndex);
		switch (shapeType) {
		case ssvg::ShapeType::Group:
			{
				counters->numGroups++;
				const ssvg::ShapeList* childShapeList = ssvg::shapeListGetGroupShapeList(shapeList, shapeIndex);
				assert(childShapeList);
				enumerateShapesRecursive(childShapeList, counters);
			}
			break;
		case ssvg::ShapeType::Rect:
			counters->numRects++;
			break;
		case ssvg::ShapeType::Circle:
			counters->numCircles++;
			break;
		case ssvg::ShapeType::Ellipse:
			counters->numEllipses++;
			break;
		case ssvg::ShapeType::Line:
			counters->numLines++;
			break;
		case ssvg::ShapeType::Polyline:
		case ssvg::ShapeType::Polygon:
			{
				ShapesCounters::PolyNum& polyNum = shapeType == ssvg::ShapeType::Polyline ? counters->polylines : counters->polygons;
				const ssvg::PointList* ptList = ssvg::shapeListGetPointList(shapeList, shapeIndex);
				assert((ptList));
				polyNum.numPoly++;
				polyNum.numPoints += ssvg::pointListGetNumPoints(ptList);
			}
			break;
		case ssvg::ShapeType::Path:
			{
				const ssvg::Path* path = ssvg::shapeListGetPath(shapeList, shapeIndex);
				assert(path);
				const ssvg::PathNum pathNum = ssvg::pathGetSubpathCounters(path);
				counters->paths.open.numSubpaths   += pathNum.open.numSubpaths;
				counters->paths.open.numNodes      += pathNum.open.numNodes;
				counters->paths.closed.numSubpaths += pathNum.closed.numSubpaths;
				counters->paths.closed.numNodes    += pathNum.closed.numNodes;
			}
			break;
		case ssvg::ShapeType::Text:
			counters->numTexts++;
			break;
		default:
			printf(" - (x) Unknown shape type\n");
		}
	}
}

ShapesCounters enumerateShapes(const ssvg::ShapeList* shapeList)
{
	ShapesCounters counters;
	stdutils::memset<ShapesCounters>(&counters, 0);

	enumerateShapesRecursive(shapeList, &counters);

	return counters;
}

std::ostream& operator<<(std::ostream& out, const ShapesCounters& counters)
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

	const auto shapesCounters = enumerateShapes(ssvg::imageGetShapeList(img));

	std::cout << std::endl;
	std::cout << shapesCounters;

	const auto allocatedShapeAttrsCounters = ssvg::internals::enumerateAllocatedShapeAttrs();

	std::cout << std::endl;
	std::cout << allocatedShapeAttrsCounters;

	closeSVGImage(img);

	ssvg::shutdownLib();

	return 0;
}
