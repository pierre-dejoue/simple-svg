#include <ssvg/ssvg.h>
#include <stdutils/memory.h>

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

constexpr const char* DEFAULT_INPUT_SVG_FILE         = "./Ghostscript_Tiger.svg";
constexpr const char* DEFAULT_OUTPUT_ROUND_TRIP_FILE = "./round_trip_tiger.svg";
constexpr const char* OUTPUT_TEST_FILE               = "./test_output.svg";

std::string build_output_round_trip_filename(const std::string& input_svg_file)
{
	const fs::path input_path(input_svg_file);
	std::string output_filename = "round_trip_";
	output_filename.append(input_path.filename().string());
	return input_path.parent_path().append(output_filename).string();
}

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

bool saveImage(const fs::path& filepath, ssvg::Image* img)
{
	assert(img);
	bool success = false;
	try {
		std::basic_ofstream<char> ostream(filepath, std::ios_base::out);
		if (ostream.is_open()) {
			success = ssvg::imageSave(img, ostream);
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

bool testParser(const char* filename, const ssvg::ShapeAttributes* baseAttrs)
{
	printf("Loading \"%s\"...\n", filename);

	const auto svgFileBuffer = loadFile(filename);
	if (svgFileBuffer.empty()) {
		printf("(x) Failed to load svg file.\n");
		return false;
	}

	ssvg::Image* img = nullptr;
	{
		constexpr uint32_t svg_parser_flags = 0;
		img = ssvg::imageLoad(svgFileBuffer.data(), svg_parser_flags, baseAttrs);
	}

	if (!img) {
		printf("(x) Failed to parse svg file.\n");
		return false;
	}

	printf("- Root element contains %d shapes\n", img->m_ShapeList.m_NumShapes);

	ssvg::imageDestroy(img);

	return true;
}

bool testBuilder(const char* filepath)
{
	ssvg::ShapeAttributes defaultAttrs = ssvg::defaultShapeAttributes();
	defaultAttrs.m_StrokeWidth = 1.0f;
	defaultAttrs.m_StrokeMiterLimit = 4.0f;
	defaultAttrs.m_StrokeOpacity = 1.0f;
	defaultAttrs.m_StrokePaint.m_Type = ssvg::PaintType::Color;
	defaultAttrs.m_StrokePaint.m_ColorABGR = 0xFF000000; // Black
	defaultAttrs.m_StrokeLineCap = ssvg::LineCap::Butt;
	defaultAttrs.m_StrokeLineJoin = ssvg::LineJoin::Miter;
	defaultAttrs.m_FillOpacity = 1.0f;
	defaultAttrs.m_FillPaint.m_Type = ssvg::PaintType::None;
	defaultAttrs.m_FillPaint.m_ColorABGR = 0x00000000;

	ssvg::ShapeAttributes textAttrs = defaultAttrs;
	ssvg::shapeAttrsSetFontFamily(&textAttrs, "sans-serif");
	textAttrs.m_FontSize = 20.0f;
	textAttrs.m_FillPaint.m_Type = ssvg::PaintType::Color;
	textAttrs.m_FillPaint.m_ColorABGR = 0xFF000000;
	textAttrs.m_FillRule = ssvg::FillRule::EvenOdd;
	textAttrs.m_StrokePaint.m_Type = ssvg::PaintType::None;

	ssvg::Image* img = ssvg::imageCreate(&textAttrs);

	ssvg::ShapeList* imgShapeList = &img->m_ShapeList;

	// Add shapes to the image shape list
	{
		uint32_t rectID = ssvg::shapeListAddRect(imgShapeList, &defaultAttrs, 100.0f, 100.0f, 200.0f, 200.0f, 0.0f, 0.0f);
		uint32_t circleID = ssvg::shapeListAddCircle(imgShapeList, &defaultAttrs, 200.0f, 200.0f, 80.0f);

		// Path
		uint32_t pathID = ssvg::shapeListAddPath(imgShapeList, &defaultAttrs, nullptr, 0);
		ssvg::Path* path = &imgShapeList->m_Shapes[pathID].m_Path;
		ssvg::pathMoveTo(path, 0.0f, 0.0f);
		ssvg::pathLineTo(path, 10.0f, 10.0f);
		ssvg::pathCubicTo(path, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 50.0f);
		ssvg::pathClose(path);

		// Text
		ssvg::shapeListAddText(imgShapeList, &textAttrs, 200.0f, 50.0f, ssvg::TextAnchor::Start, "This is a test string");
	}

	// Add shapes to a group
	{
		uint32_t groupID = ssvg::shapeListAddGroup(imgShapeList, &defaultAttrs, nullptr, 0);

		float groupTransform[6] = { 1.0f, 0.0f, 0.0f, 1.0f, 400.0f, 0.0f };
		ssvg::shapeSetTransform(&imgShapeList->m_Shapes[groupID], &groupTransform[0]);

		ssvg::ShapeList* groupShapeList = &imgShapeList->m_Shapes[groupID].m_ShapeList;
		uint32_t rectID = ssvg::shapeListAddRect(groupShapeList, &defaultAttrs, 100.0f, 100.0f, 200.0f, 200.0f, 0.0f, 0.0f);
		uint32_t circleID = ssvg::shapeListAddCircle(groupShapeList, &defaultAttrs, 200.0f, 200.0f, 80.0f);
	}

	// Add shapes to a group (alt version)
	{
		// Create a temporary shape list
		ssvg::ShapeList tempShapeList;
		stdutils::memset<ssvg::ShapeList>(&tempShapeList, 0);
		uint32_t rectID = ssvg::shapeListAddRect(&tempShapeList, &defaultAttrs, 100.0f, 100.0f, 200.0f, 200.0f, 0.0f, 0.0f);
		uint32_t circleID = ssvg::shapeListAddCircle(&tempShapeList, &defaultAttrs, 200.0f, 200.0f, 80.0f);

		// Add a new group using the shapes from the temp shape list
		uint32_t groupID = ssvg::shapeListAddGroup(imgShapeList, &defaultAttrs, tempShapeList.m_Shapes, tempShapeList.m_NumShapes);

		// Free the temp shape list
		ssvg::shapeListFree(&tempShapeList);

		// Transform the group
		float groupTransform[6] = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 400.0f };
		ssvg::shapeSetTransform(&imgShapeList->m_Shapes[groupID], &groupTransform[0]);
	}

	const bool save_success = saveImage(filepath, img);

	ssvg::imageDestroy(img);

	return save_success;
}

bool testRoundTrip(const char* input_filepath, const char* output_filepath, const ssvg::ShapeAttributes* baseAttrs)
{
	printf("Converting \"%s\" to \"%s\"...\n", input_filepath, output_filepath);

	const auto svgFileBuffer = loadFile(input_filepath);
	if (svgFileBuffer.empty()) {
		printf("(x) Failed to load svg file.\n");
		return false;
	}

	constexpr uint32_t svg_parser_flags = 0;
	ssvg::Image* img = ssvg::imageLoad(svgFileBuffer.data(), svg_parser_flags, baseAttrs);
	if (!img) {
		printf("(x) Failed to parse svg file.\n");
		return false;
	}

	const bool save_success = saveImage(output_filepath, img);

	ssvg::imageDestroy(img);

	return save_success;
}

int main(int argc, char* argv[])
{
	// Either call with:
	// - No argument, this will open the default test file
	// - One argument, which should be the path to an input SVG file
	if (argc > 2)
	{
		printf("(x) Wrong number of arguments.\n");
		return 1;
	}

	std::string input_svg_file = DEFAULT_INPUT_SVG_FILE;
	std::string output_round_trip_file = DEFAULT_OUTPUT_ROUND_TRIP_FILE;
	std::string output_test_file = OUTPUT_TEST_FILE;

	// Call with one argument, the path to an input SVG file
	if (argc == 2)
	{
		input_svg_file = argv[1];
		output_round_trip_file = build_output_round_trip_filename(input_svg_file);
		output_test_file.clear();
	}

	ssvg::ShapeAttributes defaultAttrs = ssvg::defaultShapeAttributes();
	defaultAttrs.m_StrokeWidth = 1.0f;
	defaultAttrs.m_StrokeMiterLimit = 4.0f;
	defaultAttrs.m_StrokeOpacity = 1.0f;
	defaultAttrs.m_StrokePaint.m_Type = ssvg::PaintType::None;
	defaultAttrs.m_StrokePaint.m_ColorABGR = 0x00000000;
	defaultAttrs.m_FillOpacity = 1.0f;
	defaultAttrs.m_FillPaint.m_Type = ssvg::PaintType::None;
	defaultAttrs.m_FillPaint.m_ColorABGR = 0x00000000;
	shapeAttrsSetFontFamily(&defaultAttrs, "sans-serif");

	ssvg::initLib();

	if (!output_test_file.empty()) { testBuilder(output_test_file.c_str()); }
	testParser(input_svg_file.c_str(), &defaultAttrs);
	testRoundTrip(input_svg_file.c_str(), output_round_trip_file.c_str(), &defaultAttrs);
	testParser(output_round_trip_file.c_str(), &defaultAttrs);

	ssvg::shutdownLib();

	return 0;
}
