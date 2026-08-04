#include <ssvg/ssvg.h>

#include <examples/utils.h>

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

constexpr const char* OUTPUT_TEST_FILE = "./test_output.svg";

bool testBuilder(const char* filepath)
{
	printf("Building \"%s\"\n", filepath);

	ssvg::ShapeAttributes defaultAttrs = ssvg::defaultShapeAttributes();
	ssvg::ShapeAttributes textAttrs = []() {
		auto attrs = ssvg::defaultShapeAttributes();
		ssvg::shapeAttrsSetFontFamily(&attrs, "sans-serif");
		attrs.m_FontSize = 20.0f;
		return attrs;
	}();

	ssvg::Image* img = ssvg::imageCreate();

	ssvg::ShapeList* imgShapeList = &img->m_ShapeList;

	// Add shapes to the image shape list
	{
		uint32_t rectID = ssvg::shapeListAddRect(imgShapeList, &defaultAttrs, 100.0f, 100.0f, 200.0f, 200.0f, 0.0f, 0.0f);
		uint32_t circleID = ssvg::shapeListAddCircle(imgShapeList, &defaultAttrs, 200.0f, 200.0f, 80.0f);

		// Path
		uint32_t pathID = ssvg::shapeListAddPath(imgShapeList, &defaultAttrs);
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
		uint32_t groupID = ssvg::shapeListAddGroup(imgShapeList, &defaultAttrs);

		float groupTransform[6] = { 1.0f, 0.0f, 0.0f, 1.0f, 400.0f, 0.0f };
		ssvg::shapeSetTransform(&imgShapeList->m_Shapes[groupID], &groupTransform[0]);

		ssvg::ShapeList* groupShapeList = &imgShapeList->m_Shapes[groupID].m_ShapeList;
		uint32_t rectID = ssvg::shapeListAddRect(groupShapeList, &defaultAttrs, 100.0f, 100.0f, 200.0f, 200.0f, 0.0f, 0.0f);
		uint32_t circleID = ssvg::shapeListAddCircle(groupShapeList, &defaultAttrs, 200.0f, 200.0f, 80.0f);
	}

	// Add shapes to a group (alt version)
	{
		// Create a temporary shape list
		ssvg::ShapeList* tempShapeList = ssvg::shapeListCreate();
		uint32_t rectID = ssvg::shapeListAddRect(tempShapeList, &defaultAttrs, 100.0f, 100.0f, 200.0f, 200.0f, 0.0f, 0.0f);
		uint32_t circleID = ssvg::shapeListAddCircle(tempShapeList, &defaultAttrs, 200.0f, 200.0f, 80.0f);

		// Add a new group using the shapes from the temp shape list (the shapes are copied)
		uint32_t groupID = ssvg::shapeListAddGroup(imgShapeList, &defaultAttrs, tempShapeList);

		// Free the temp shape list
		ssvg::shapeListFree(tempShapeList);

		// Transform the group
		float groupTransform[6] = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 400.0f };
		ssvg::shapeSetTransform(&imgShapeList->m_Shapes[groupID], &groupTransform[0]);
	}

	const bool save_success = saveImage(filepath, img);

	ssvg::imageFree(img);

	return save_success;
}


int main()
{
	const char* output_test_file = OUTPUT_TEST_FILE;

	ssvg::initLib();

	testBuilder(output_test_file);

	ssvg::shutdownLib();

	return 0;
}
