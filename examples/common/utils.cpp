#include <ssvg/ssvg.h>

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

std::vector<char> loadFile(const fs::path& filepath)
{
	std::error_code err_code;
	const auto sz = fs::file_size(filepath, err_code);
	if (err_code) {
		std::stringstream oss;
		oss << "std::filesystem::file_size(" << filepath.filename() << "): error_code=" << err_code;
		printf("%s\n", oss.str().c_str());
		return std::vector<char>();
	}
	try {
		std::basic_ifstream<char> istream(filepath, std::ios_base::in);
		if (istream.is_open()) {
			std::vector<char> buf(sz + 1, 0u);
			istream.read(buf.data(), static_cast<std::streamsize>(sz));
			return buf;
		} else {
			std::stringstream oss;
			oss << "Cannot open file: " << filepath;
			printf("%s\n", oss.str().c_str());
		}
	} catch(const std::exception& e) {
		std::stringstream oss;
		oss << "Exception in open_and_parse_file(" << filepath << "): " << e.what();
		printf("%s\n", oss.str().c_str());
	}
	return std::vector<char>();
}

// Image loaded with loadSVGImage *must* be freed with closeSVGImage
ssvg::Image* loadSVGImage(const fs::path& filepath)
{
	printf("Loading \"%s\"...\n", filepath.string().c_str());

	const auto svgFileBuffer = loadFile(filepath);
	if (svgFileBuffer.empty()) {
		printf("(x) Failed to load svg file.\n");
		return nullptr;
	}

	ssvg::Image* img = ssvg::imageLoad(svgFileBuffer.data(), ssvg::ImageLoadFlags::None);
	if (!img) {
		printf("(x) Failed to parse the svg file.\n");
		return nullptr;
	}

	return img;
}

void closeSVGImage(ssvg::Image* img, const fs::path& filepath)
{
	std::string source;
	if (!filepath.empty()) {
		source = " ";
		source.append(filepath.string());
	}
	printf("Closing ssvg::Image%s...\n", source.c_str());
	ssvg::imageFree(img);
}

bool saveImage(const fs::path& filepath, const ssvg::Image* img)
{
	assert(img);
	bool success = false;
	try {
		std::basic_ofstream<char> ostream(filepath, std::ios_base::out);
		if (ostream.is_open()) {
			success = ssvg::imageSave(img, ostream);
			if (success) {
				printf("Saved ssvg::Image to \"%s\"...\n", filepath.string().c_str());
			} else {
				printf("(x) Failed to save ssvg::Image to \"%s\"...\n", filepath.string().c_str());
			}
		} else {
			std::stringstream oss;
			oss << "Cannot open file: " << filepath;
			printf("%s\n", oss.str().c_str());
		}
	} catch(const std::exception& e) {
		std::stringstream oss;
		oss << "Exception in save_file(" << filepath << "): " << e.what();
		printf("%s\n", oss.str().c_str());
	}
	return success;
}

std::ostream& operator<<(std::ostream& out, const ssvg::ShapesCounters& counters)
{
	constexpr const char* INDENT = "    ";
	out << INDENT << "Nb of groups: " << counters.numGroups << std::endl;
	out << INDENT << "Basic shapes:" << std::endl;
	out << INDENT << "    rect: " << counters.numRects << "; circle: " << counters.numCircles << "; ellipse: " << counters.numEllipses << "; text: " << counters.numTexts << std::endl;
	out << INDENT << "    line: " << counters.numLines << "; "
            << "polyline: " << counters.polylines.numPoly << " (" << counters.polylines.numPoints << " points); "
	        << "polygon: "  << counters.polygons.numPoly  << " (" << counters.polygons.numPoints  << " points);" << std::endl;
	out << INDENT << "Paths: " << std::endl;
	out << INDENT << "    closed: " << counters.paths.closed.numSubpaths << " (" << counters.paths.closed.numNodes << " nodes)" << std::endl;
	out << INDENT << "    open:   " << counters.paths.open.numSubpaths   << " (" << counters.paths.open.numNodes   << " nodes)" << std::endl;
	return out;
}

std::ostream& operator<<(std::ostream& out, const ssvg::internals::AllocatedShapeAttrsCounters& counters)
{
	out << "ShapesAttributes:";
	out <<  " nodes: " << counters.numNodes;
	out << "; allocated: " << counters.numAllocAttrs;
	out << "; free: " << counters.numFreeAttrs << std::endl;
	return out;
}
