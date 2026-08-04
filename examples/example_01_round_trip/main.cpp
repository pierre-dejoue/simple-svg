#include <ssvg/ssvg.h>

#include <examples/utils.h>

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

std::string build_output_round_trip_filename(const std::string& input_svg_file)
{
	const fs::path input_path(input_svg_file);
	std::string output_filename = "round_trip_";
	output_filename.append(input_path.filename().string());
	return input_path.parent_path().append(output_filename).string();
}

bool testParser(const char* filepath, const ssvg::ShapeAttributes& baseAttrs)
{
	printf("Loading \"%s\"...\n", filepath);

	const auto svgFileBuffer = loadFile(filepath);
	if (svgFileBuffer.empty()) {
		printf("(x) Failed to load svg file.\n");
		return false;
	}

	ssvg::Image* img = nullptr;
	{
		constexpr uint32_t svg_parser_flags = 0;
		img = ssvg::imageLoad(svgFileBuffer.data(), svg_parser_flags, &baseAttrs);
	}

	if (!img) {
		printf("(x) Failed to parse svg file.\n");
		return false;
	}

	printf("- Root element contains %d shapes\n", img->m_ShapeList.m_NumShapes);

	ssvg::imageFree(img);

	return true;
}

bool testRoundTrip(const char* input_filepath, const char* output_filepath, const ssvg::ShapeAttributes& baseAttrs)
{
	printf("Converting \"%s\" to \"%s\"...\n", input_filepath, output_filepath);

	const auto svgFileBuffer = loadFile(input_filepath);
	if (svgFileBuffer.empty()) {
		printf("(x) Failed to load svg file.\n");
		return false;
	}

	constexpr uint32_t svg_parser_flags = 0;
	ssvg::Image* img = ssvg::imageLoad(svgFileBuffer.data(), svg_parser_flags, &baseAttrs);
	if (!img) {
		printf("(x) Failed to parse svg file.\n");
		return false;
	}

	const bool save_success = saveImage(output_filepath, img);

	ssvg::imageFree(img);

	return save_success;
}

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		printf("(x) Wrong number of arguments.\n");
		printf("Usage: example_01_round_trip input_file.svg\n");
		return 1;
	}

	// Call with one argument, the path to an input SVG file
	const std::string input_svg_file = argv[1];
	const std::string output_round_trip_file = build_output_round_trip_filename(input_svg_file);

	const ssvg::ShapeAttributes defaultAttrs = []() {
	    auto attrs = ssvg::defaultShapeAttributes();
		attrs.m_StrokeWidth = 1.0f;
		attrs.m_StrokeMiterLimit = 4.0f;
		attrs.m_StrokeOpacity = 1.0f;
		attrs.m_StrokePaint.m_Type = ssvg::PaintType::None;
		attrs.m_StrokePaint.m_ColorABGR = 0x00000000;
		attrs.m_FillOpacity = 1.0f;
		attrs.m_FillPaint.m_Type = ssvg::PaintType::None;
		attrs.m_FillPaint.m_ColorABGR = 0x00000000;
		shapeAttrsSetFontFamily(&attrs, "sans-serif");
		return attrs;
	}();

	ssvg::initLib();

	testParser(input_svg_file.c_str(), defaultAttrs);
	testRoundTrip(input_svg_file.c_str(), output_round_trip_file.c_str(), defaultAttrs);
	testParser(output_round_trip_file.c_str(), defaultAttrs);

	ssvg::shutdownLib();

	return 0;
}
