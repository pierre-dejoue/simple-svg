#include <ssvg/ssvg.h>

#include <examples/utils.h>

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

bool testParser(const char* filepath)
{
	ssvg::Image* img = loadSVGImage(fs::path(filepath));

	const bool load_success = (img != nullptr);
	if (!load_success) {
		return false;
	}

	printf("- Root element contains %d shapes\n",  ssvg::imageGetNumShapes(img));

	closeSVGImage(img);

	return load_success;
}

bool testRoundTrip(const char* input_filepath, const char* output_filepath)
{
	printf("Converting \"%s\" to \"%s\"...\n", input_filepath, output_filepath);

	ssvg::Image* img = loadSVGImage(fs::path(input_filepath));
	if (!img) {
		return false;
	}

	const bool save_success = saveImage(output_filepath, img);

	closeSVGImage(img);

	return save_success;
}

std::string build_output_round_trip_filename(const std::string& input_svg_file)
{
	const fs::path input_path(input_svg_file);
	std::string output_filename = "round_trip_";
	output_filename.append(input_path.filename().string());
	return input_path.parent_path().append(output_filename).string();
}

int main(int argc, char* argv[])
{
	if (argc != 2)
	{
		printf("(x) Wrong number of arguments.\n");
		printf("Usage: example_01_round_trip input_file.svg\n");
		return 1;
	}

	// Call with one argument: the path to an input SVG file
	const std::string input_svg_file = argv[1];
	const std::string output_round_trip_file = build_output_round_trip_filename(input_svg_file);

	ssvg::initLib();

	testParser(input_svg_file.c_str());
	testRoundTrip(input_svg_file.c_str(), output_round_trip_file.c_str());
	testParser(output_round_trip_file.c_str());

	ssvg::shutdownLib();

	return 0;
}
