#include <ssvg/ssvg.h>

#include "ssvg_debug.h"
#include "ssvg_math.h"
#include "ssvg_private.h"

#include <stdutils/macros.h>
#include <stdutils/memory.h>
#include <stdutils/minmax.h>
#include <stdutils/string.h>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <string_view>
#include <string>

namespace ssvg
{

namespace {

int strlenint(std::string_view str)
{
	return static_cast<int>(str.length());
}

struct ShapeAttributeFreeListNode
{
	ShapeAttributeFreeListNode* m_Next;
	ShapeAttributeFreeListNode* m_Prev;
	ShapeAttributes* m_Attrs;
	uint32_t m_NumAttrs;
	uint32_t m_FirstFreeID;
	uint32_t m_NumFree;
};

ShapeAttributeFreeListNode* s_ShapeAttrFreeListHead = nullptr;

ShapeAttributes* shapeAttrsAlloc();
void shapeAttrsFree(ShapeAttributes* attrs);

const OwnedString& emptyOwnedString()
{
	static const OwnedString empty = []() {
		OwnedString str;
		str.m_Buf = nullptr;
		str.m_Len = 0;
		return str;
	}();

	return empty;
}

OwnedString ownedStringAlloc(const char* str)
{
	OwnedString ownedStr;
	ownedStr.m_Buf = nullptr;
	ownedStr.m_Len = 0;
	if (str == nullptr) {
		return ownedStr;
	}
	const std::size_t len = stdutils::strnlen(str);
	if (len == 0) {
		return ownedStr;
	}
	ownedStr.m_Buf = (char*)malloc((len + 1) * sizeof(char));
	IGNORE_RETURN stdutils::memcpy<char>(ownedStr.m_Buf, len, str, len);
	ownedStr.m_Buf[len] = '\0';
	ownedStr.m_Len = len;
	return ownedStr;
}

OwnedString ownedStringAlloc(uint32_t len)
{
	OwnedString ownedStr;
	ownedStr.m_Buf = nullptr;
	ownedStr.m_Len = 0;
	if (len == 0) {
		return ownedStr;
	}
	ownedStr.m_Buf = (char*)malloc((len + 1) * sizeof(char));
	IGNORE_RETURN stdutils::memset<char>(ownedStr.m_Buf, len, '*', len * sizeof(char));
	ownedStr.m_Buf[len] = '\0';
	ownedStr.m_Len = len;
	return ownedStr;
}

void ownedStringClear(OwnedString& ownedStr)
{
	if (ownedStr.m_Buf) {
		std::free(ownedStr.m_Buf);
	}
	ownedStr.m_Buf = nullptr;
	ownedStr.m_Len = 0;
}

void ownedStringSet(OwnedString& ownedStr, const char* str)
{
	ownedStringClear(ownedStr);
	if (str == nullptr) {
		return;
	}
	ownedStr = ownedStringAlloc(str);
}

} // namespace

std::string_view lengthUnitToString(LengthUnit::Enum lengthUnit)
{
	switch(lengthUnit) {
		case LengthUnit::User:          return "";
		case LengthUnit::PX:            return "px";
		case LengthUnit::Percent:       return "%";
		case LengthUnit::EM:            return "em";
		case LengthUnit::EX:            return "ex";
		case LengthUnit::IN:            return "in";
		case LengthUnit::CM:            return "cm";
		case LengthUnit::MM:            return "mm";
		case LengthUnit::PT:            return "pt";
		case LengthUnit::PC:            return "pc";
		default:
			assert(0);
			break;
	}
	return "";
}

float convertLengthToPixel(const Length& length, LengthAxis::Enum axis, const LengthContext* lengthContext)
{
	constexpr float DPI = (float)(SSVG_CONFIG_DEFAULT_DPI);
	switch(length.m_Unit) {
		case LengthUnit::User:
		case LengthUnit::PX:
			return length.m_Length;
		case LengthUnit::Percent:
		{
			assert(lengthContext);
			if (!lengthContext) { break; }
			const float referenceLength = (axis == LengthAxis::Radial
				? lengthContext->m_ViewportDiag
				: (axis == LengthAxis::Y ? lengthContext->m_ViewportHeight : lengthContext->m_ViewportWidth));
			return referenceLength * (length.m_Length / 100.f);
		}
		case LengthUnit::EM:
			assert(lengthContext);
			if (!lengthContext) { break; }
			return lengthContext->m_FontSize * length.m_Length;
		case LengthUnit::EX:
			assert(lengthContext);
			if (!lengthContext) { break; }
			return 0.5f * lengthContext->m_FontSize * length.m_Length;
		case LengthUnit::IN:
			return DPI * length.m_Length;
		case LengthUnit::CM:
			return (DPI / 2.54f) * length.m_Length;
		case LengthUnit::MM:
			return (DPI / 25.4f) * length.m_Length;
		case LengthUnit::PT:
			return (DPI / 72.f) * length.m_Length;
		case LengthUnit::PC:
			return (DPI / 6.f) * length.m_Length;
		default:
			assert(0);
			break;
	}
	// By default, return the raw length value
	return length.m_Length;
}

void transformIdentity(float* transform)
{
	assert(transform);
	transform[0] = 1.0f;
	transform[1] = 0.0f;
	transform[2] = 0.0f;
	transform[3] = 1.0f;
	transform[4] = 0.0f;
	transform[5] = 0.0f;
}

void transformTranslation(float* transform, float x, float y)
{
	assert(transform);
	transform[0] = 1.0f;
	transform[1] = 0.0f;
	transform[2] = 0.0f;
	transform[3] = 1.0f;
	transform[4] = x;
	transform[5] = y;
}

// a = a * b;
void transformMultiply(float* transform_a, const float* transform_b)
{
	assert(transform_a);
	assert(transform_b);
	float* a = transform_a;
	const float* b = transform_b;
	float res[6];
	res[0] = a[0] * b[0] + a[2] * b[1];
	res[1] = a[1] * b[0] + a[3] * b[1];
	res[2] = a[0] * b[2] + a[2] * b[3];
	res[3] = a[1] * b[2] + a[3] * b[3];
	res[4] = a[0] * b[4] + a[2] * b[5] + a[4];
	res[5] = a[1] * b[4] + a[3] * b[5] + a[5];
	stdutils::memcpy<float>(a, TRANSFORM_ARRAY_SZ, res, sizeof(float) * TRANSFORM_ARRAY_SZ);
}

void transformTranslate(float* transform, float x, float y)
{
	assert(transform);
	float tmp[6];
	transformTranslation(&tmp[0], x, y);
	transformMultiply(transform, tmp);
}

void transformPoint(const float* transform, const float* localPos, float* globalPos)
{
	assert(transform);
	const float x = localPos[0];
	const float y = localPos[1];
	globalPos[0] = transform[0] * x + transform[2] * y + transform[4];
	globalPos[1] = transform[1] * x + transform[3] * y + transform[5];
}

void transformBoundingRect(const float* transform, const float* localRect, float* globalRect)
{
	assert(transform);
	float transformedRect[4];
	transformPoint(transform, &localRect[0], &transformedRect[0]);
	transformPoint(transform, &localRect[2], &transformedRect[2]);

	globalRect[0] = stdutils::min<float>(transformedRect[0], transformedRect[2]);
	globalRect[1] = stdutils::min<float>(transformedRect[1], transformedRect[3]);
	globalRect[2] = stdutils::max<float>(transformedRect[0], transformedRect[2]);
	globalRect[3] = stdutils::max<float>(transformedRect[1], transformedRect[3]);
}

void shapeGetTransform(const Shape* shape, float* out_transform)
{
	assert(shape);
	assert(out_transform);
	if (shape->m_Attrs) {
		stdutils::memcpy<float>(out_transform, TRANSFORM_ARRAY_SZ, &shape->m_Attrs->m_Transform[0], sizeof(float) * TRANSFORM_ARRAY_SZ);
	} else {
		transformIdentity(out_transform);
	}
}

void shapeSetTransform(Shape* shape, const float* transform)
{
	assert(shape);
	assert(transform);
	if (!shape->m_Attrs) {
		shapeAllocAttributes(shape);
	}
	stdutils::memcpy<float>(&shape->m_Attrs->m_Transform[0], TRANSFORM_ARRAY_SZ, transform, sizeof(float) * TRANSFORM_ARRAY_SZ);
	shape->m_Attrs->m_Flags &= AttribFlags::Transformation;
}

void shapeSetIdentityTransform(Shape* shape)
{
	assert(shape);
	if (shape->m_Attrs) {
	    transformIdentity(&shape->m_Attrs->m_Transform[0]);
	}
	// Else, do nothing: The transformation of a shape without private attributes is implicitly the Identity.
}

void shapeApplyTransform(Shape* shape, const float* transform)
{
	assert(shape);
	if (!shape->m_Attrs) {
		shapeAllocAttributes(shape);
	}
	transformMultiply(&shape->m_Attrs->m_Transform[0], transform);
	shape->m_Attrs->m_Flags &= AttribFlags::Transformation;
}

namespace {

void resetShapeAttributes(ShapeAttributes* attrs)
{
	SSVG_CHECK(attrs, "Nullptr to ShapeAttributes");
	if (!attrs) { return; }

	stdutils::memset<ShapeAttributes>(attrs, 0);
	transformIdentity(&attrs->m_Transform[0]);

	assert(attrs->m_Flags == AttribFlags::None);
	assert(stdutils::strnlen(&attrs->m_FontFamily[0], SSVG_CONFIG_FONT_FAMILY_MAX_LEN) == 0);
	assert(stdutils::strnlen(&attrs->m_ID[0], SSVG_CONFIG_ID_MAX_LEN) == 0);
	#if SSVG_CONFIG_CLASS_MAX_LEN
		assert(stdutils::strnlen(&attrs->m_Class[0], SSVG_CONFIG_CLASS_MAX_LEN) == 0);
	#endif
}

bool shapeListIsReadOnly(ShapeList* shapeList)
{
	assert(shapeList);
	// We may use a ShapeList as a temporary reference to another, in which case the ShapeList does not own the memory buffer
	return shapeList->m_Shapes != nullptr && shapeList->m_Capacity == 0;
}

} // namespace

ShapeList* groupGetShapeList(Group* group)
{
	return const_cast<ShapeList*>(groupGetShapeList(const_cast<const Group*>(group)));
}

const ShapeList* groupGetShapeList(const Group* group)
{
	SSVG_CHECK(group, "Nullptr to Group");
	if (!group) { return nullptr; }

	return &group->m_ShapeList;
}

const OwnedString& groupGetTitle(const Group* group)
{
	SSVG_CHECK(group, "Nullptr to Group");
	if (!group) { return emptyOwnedString(); }

	return group->m_Title;
}

void groupSetTitle(Group* group, const char* str)
{
	SSVG_CHECK(group, "Nullptr to Group");
	if (!group) { return; }

	ownedStringSet(group->m_Title, str);
}

void groupClear(Group* group)
{
	SSVG_CHECK(group, "Nullptr to Group");
	if (!group) { return; }

	ownedStringClear(group->m_Title);
	shapeListClear(&group->m_ShapeList);
}

// This is the state of a newly created shape. An empty group with no resources allocated.
bool shapeIsEmptyGroup(Shape* shape)
{
	SSVG_CHECK(shape, "Nullptr to Shape");
	return shape
	    && shape->m_Type == ShapeType::Group
	    && shape->m_Group.m_ShapeList.m_Shapes == nullptr
	    && shape->m_Group.m_ShapeList.m_NumShapes == 0;
}

Shape* shapeListAllocShape(ShapeList* shapeList, ShapeType::Enum type)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return nullptr; }
	SSVG_CHECK(!shapeListIsReadOnly(shapeList), "Trying to expand a read-only shape list?");
	SSVG_CHECK(shapeList->m_NumShapes <= shapeList->m_Capacity, "Invalid capacity of a shape list (expand)");
	if (shapeListIsReadOnly(shapeList)) {
		return nullptr;
	}

	// Ensure capacity to allocate at least one new Shape
	if (shapeList->m_NumShapes + 1 > shapeList->m_Capacity) {
		const uint32_t oldCapacity = shapeList->m_Capacity;

		// TODO: Since shapes are fairly large objects, check if allocating a constant amount each time somehow helps
		const uint32_t newCapacity = stdutils::max<uint32_t>(oldCapacity ? (oldCapacity * 3) / 2 : 4, shapeList->m_NumShapes + 1);
		if (newCapacity > oldCapacity) {
			shapeList->m_Capacity = newCapacity;
			shapeList->m_Shapes = (Shape*)std::realloc(shapeList->m_Shapes, sizeof(Shape) * newCapacity);
		}
	}

	// Allocate the new shape
	Shape* newShape = [shapeList]() {
		assert(shapeList->m_NumShapes + 1 <= shapeList->m_Capacity);
		Shape* shape = &shapeList->m_Shapes[shapeList->m_NumShapes++];
		stdutils::memset<Shape>(shape, 0);
		assert(shapeIsEmptyGroup(shape));
		return shape;
	}();

	// Set its type
	newShape->m_Type = type;

	return newShape;
}

void shapeListShrinkToFit(ShapeList* shapeList)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return; }
	SSVG_CHECK(!shapeListIsReadOnly(shapeList), "Trying to shrink a read-only shape list?");
	SSVG_CHECK(shapeList->m_NumShapes <= shapeList->m_Capacity, "Invalid capacity of a shape list (shrink)");
	if (shapeListIsReadOnly(shapeList)) {
		return;
	}

	if (shapeList->m_NumShapes == 0 && shapeList->m_Capacity > 0) {
		std::free(shapeList->m_Shapes);
		shapeList->m_Shapes = nullptr;
		shapeList->m_Capacity = 0;
	} else if (shapeList->m_NumShapes != shapeList->m_Capacity) {
		shapeList->m_Shapes = (Shape*)std::realloc(shapeList->m_Shapes, sizeof(Shape) * shapeList->m_NumShapes);
		shapeList->m_Capacity = shapeList->m_NumShapes;
	}
	assert(shapeList->m_NumShapes == shapeList->m_Capacity);
}

void shapeListClear(ShapeList* shapeList)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return; }
	SSVG_CHECK(!shapeListIsReadOnly(shapeList), "Trying to free a read-only shape list?");
	SSVG_CHECK(shapeList->m_NumShapes <= shapeList->m_Capacity, "Invalid capacity of a shape list (free)");
	if (shapeListIsReadOnly(shapeList)) {
		shapeList->m_Shapes = nullptr;
		return;
	}

	const uint32_t n = shapeList->m_NumShapes;
	for (uint32_t i = 0; i < n; ++i) {
		Shape* shape = &shapeList->m_Shapes[i];
		shapeClear(shape);
	}

	std::free(shapeList->m_Shapes);
	shapeList->m_Shapes = nullptr;
	shapeList->m_Capacity = 0;
	shapeList->m_NumShapes = 0;
}

void shapeListReserve(ShapeList* shapeList, uint32_t capacity)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return; }
	SSVG_CHECK(!shapeListIsReadOnly(shapeList), "Trying to reserve memory for a read-only shape list?");
	SSVG_CHECK(shapeList->m_NumShapes <= shapeList->m_Capacity, "Invalid capacity of a shape list (reserve)");
	if (shapeListIsReadOnly(shapeList)) { return; }

	if (capacity > shapeList->m_Capacity) {
		shapeList->m_Capacity = capacity;
		shapeList->m_Shapes = (Shape*)std::realloc(shapeList->m_Shapes, sizeof(Shape) * capacity);
	}
}

uint32_t shapeListMoveShapeToBack(ShapeList* shapeList, uint32_t shapeIndex)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return shapeIndex; }
	SSVG_CHECK(shapeIndex < shapeList->m_NumShapes, "Out of bounds shape index");
	if (shapeIndex >= shapeList->m_NumShapes) { return shapeIndex; }

	// If already in the back, return the same index
	if (shapeIndex == 0 || shapeList->m_NumShapes <= 1) {
		return shapeIndex;
	}

	Shape tmp;
	stdutils::memcpy<Shape>(&tmp, &shapeList->m_Shapes[shapeIndex - 1]);
	stdutils::memcpy<Shape>(&shapeList->m_Shapes[shapeIndex - 1], &shapeList->m_Shapes[shapeIndex]);
	stdutils::memcpy<Shape>(&shapeList->m_Shapes[shapeIndex], &tmp);

	return shapeIndex - 1;
}

uint32_t shapeListMoveShapeToFront(ShapeList* shapeList, uint32_t shapeIndex)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return shapeIndex; }
	SSVG_CHECK(shapeIndex < shapeList->m_NumShapes, "Out of bounds shape index");
	if (shapeIndex >= shapeList->m_NumShapes) { return shapeIndex; }

	// If already in the front, return the same index
	if (shapeIndex + 1 == shapeList->m_NumShapes) {
		return shapeIndex;
	}

	Shape tmp;
	stdutils::memcpy<Shape>(&tmp, &shapeList->m_Shapes[shapeIndex + 1]);
	stdutils::memcpy<Shape>(&shapeList->m_Shapes[shapeIndex + 1], &shapeList->m_Shapes[shapeIndex]);
	stdutils::memcpy<Shape>(&shapeList->m_Shapes[shapeIndex], &tmp);

	return shapeIndex + 1;
}

Shape* shapeListGetShape(ShapeList* shapeList, uint32_t shapeIndex)
{
	return const_cast<Shape*>(shapeListGetShape(const_cast<const ShapeList*>(shapeList), shapeIndex));
}

const Shape* shapeListGetShape(const ShapeList* shapeList, uint32_t shapeIndex)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return nullptr; }
	SSVG_CHECK(shapeIndex < shapeList->m_NumShapes, "Out of bounds shape index");
	if (shapeIndex >= shapeList->m_NumShapes) { return nullptr; }

	return &shapeList->m_Shapes[shapeIndex];
}

ShapeType::Enum shapeListGetShapeType(const ShapeList* shapeList, uint32_t shapeIndex)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return (ShapeType::Enum)0; }
	SSVG_CHECK(shapeIndex < shapeList->m_NumShapes, "Out of bounds shape index");
	if (shapeIndex >= shapeList->m_NumShapes) { return (ShapeType::Enum)0; }

	return shapeList->m_Shapes[shapeIndex].m_Type;
}

Group* shapeListGetGroup(ShapeList* shapeList, uint32_t shapeIndex)
{
	return const_cast<Group*>(shapeListGetGroup(const_cast<const ShapeList*>(shapeList), shapeIndex));
}

const Group* shapeListGetGroup(const ShapeList* shapeList, uint32_t shapeIndex)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return nullptr; }
	SSVG_CHECK(shapeIndex < shapeList->m_NumShapes, "Out of bounds shape index");
	if (shapeIndex >= shapeList->m_NumShapes) { return nullptr; }

	Shape& shape = shapeList->m_Shapes[shapeIndex];
	SSVG_WARN(shape.m_Type == ShapeType::Group, "The shape is not a Group");

	return shape.m_Type == ShapeType::Group ? &shape.m_Group : nullptr;
}

ShapeList* shapeListGetGroupShapeList(ShapeList* shapeList, uint32_t shapeIndex)
{
	return const_cast<ShapeList*>(shapeListGetGroupShapeList(const_cast<const ShapeList*>(shapeList), shapeIndex));
}

const ShapeList* shapeListGetGroupShapeList(const ShapeList* shapeList, uint32_t shapeIndex)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return nullptr; }
	SSVG_CHECK(shapeIndex < shapeList->m_NumShapes, "Out of bounds shape index");
	if (shapeIndex >= shapeList->m_NumShapes) { return nullptr; }

	Shape& shape = shapeList->m_Shapes[shapeIndex];
	SSVG_WARN(shape.m_Type == ShapeType::Group, "The shape is not a Group");

	return shape.m_Type == ShapeType::Group ? &shape.m_Group.m_ShapeList : nullptr;
}

PointList* shapeListGetPointList(ShapeList* shapeList, uint32_t shapeIndex)
{
	return const_cast<PointList*>(shapeListGetPointList(const_cast<const ShapeList*>(shapeList), shapeIndex));
}

const PointList* shapeListGetPointList(const ShapeList* shapeList, uint32_t shapeIndex)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return nullptr; }
	SSVG_CHECK(shapeIndex < shapeList->m_NumShapes, "Out of bounds shape index");
	if (shapeIndex >= shapeList->m_NumShapes) { return nullptr; }

	Shape& shape = shapeList->m_Shapes[shapeIndex];
	SSVG_WARN(shape.m_Type == ShapeType::Polygon || shape.m_Type == ShapeType::Polyline, "The shape is not a Polygon or Polyline");

	return (shape.m_Type == ShapeType::Polygon || shape.m_Type == ShapeType::Polyline) ? &shape.m_PointList : nullptr;
}

Path* shapeListGetPath(ShapeList* shapeList, uint32_t shapeIndex)
{
	return const_cast<Path*>(shapeListGetPath(const_cast<const ShapeList*>(shapeList), shapeIndex));
}

const Path* shapeListGetPath(const ShapeList* shapeList, uint32_t shapeIndex)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return nullptr; }
	SSVG_CHECK(shapeIndex < shapeList->m_NumShapes, "Out of bounds shape index");
	if (shapeIndex >= shapeList->m_NumShapes) { return nullptr; }

	Shape& shape = shapeList->m_Shapes[shapeIndex];
	SSVG_WARN(shape.m_Type == ShapeType::Path, "The shape is not a Path");

	return shape.m_Type == ShapeType::Path ? &shape.m_Path : nullptr;
}

const Rect* shapeListGetRect(const ShapeList* shapeList, uint32_t shapeIndex)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return nullptr; }
	SSVG_CHECK(shapeIndex < shapeList->m_NumShapes, "Out of bounds shape index");
	if (shapeIndex >= shapeList->m_NumShapes) { return nullptr; }

	Shape& shape = shapeList->m_Shapes[shapeIndex];
	SSVG_WARN(shape.m_Type == ShapeType::Rect, "The shape is not a Rect");

	return shape.m_Type == ShapeType::Rect ? &shape.m_Rect : nullptr;
}

const Circle* shapeListGetCircle(const ShapeList* shapeList, uint32_t shapeIndex)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return nullptr; }
	SSVG_CHECK(shapeIndex < shapeList->m_NumShapes, "Out of bounds shape index");
	if (shapeIndex >= shapeList->m_NumShapes) { return nullptr; }

	Shape& shape = shapeList->m_Shapes[shapeIndex];
	SSVG_WARN(shape.m_Type == ShapeType::Circle, "The shape is not a Circle");

	return shape.m_Type == ShapeType::Circle ? &shape.m_Circle : nullptr;
}

const Ellipse* shapeListGetEllipse(const ShapeList* shapeList, uint32_t shapeIndex)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return nullptr; }
	SSVG_CHECK(shapeIndex < shapeList->m_NumShapes, "Out of bounds shape index");
	if (shapeIndex >= shapeList->m_NumShapes) { return nullptr; }

	Shape& shape = shapeList->m_Shapes[shapeIndex];
	SSVG_WARN(shape.m_Type == ShapeType::Ellipse, "The shape is not a Ellipse");

	return shape.m_Type == ShapeType::Ellipse ? &shape.m_Ellipse : nullptr;
}

const Line* shapeListGetLine(const ShapeList* shapeList, uint32_t shapeIndex)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return nullptr; }
	SSVG_CHECK(shapeIndex < shapeList->m_NumShapes, "Out of bounds shape index");
	if (shapeIndex >= shapeList->m_NumShapes) { return nullptr; }

	Shape& shape = shapeList->m_Shapes[shapeIndex];
	SSVG_WARN(shape.m_Type == ShapeType::Line, "The shape is not a Line");

	return shape.m_Type == ShapeType::Line ? &shape.m_Line : nullptr;
}

const Text* shapeListGetText(const ShapeList* shapeList, uint32_t shapeIndex)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return nullptr; }
	SSVG_CHECK(shapeIndex < shapeList->m_NumShapes, "Out of bounds shape index");
	if (shapeIndex >= shapeList->m_NumShapes) { return nullptr; }

	Shape& shape = shapeList->m_Shapes[shapeIndex];
	SSVG_WARN(shape.m_Type == ShapeType::Text, "The shape is not a Text");

	return shape.m_Type == ShapeType::Text ? &shape.m_Text : nullptr;
}

uint32_t shapeListGetNumShapes(const ShapeList* shapeList)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return 0; }

	return shapeList->m_NumShapes;
}

namespace {

void shapeListEnumerateRecursive(const ShapeList* shapeList, ShapesCounters* counters)
{
	assert(shapeList);
	assert(counters);

	const uint32_t numShapes = shapeList->m_NumShapes;
	for (uint32_t shapeIndex = 0; shapeIndex < numShapes; ++shapeIndex) {
		const ShapeType::Enum shapeType = shapeListGetShapeType(shapeList, shapeIndex);
		switch (shapeType) {
		case ShapeType::Group:
			{
				counters->numGroups++;
				const ShapeList* childShapeList = shapeListGetGroupShapeList(shapeList, shapeIndex);
				assert(childShapeList);
				shapeListEnumerateRecursive(childShapeList, counters);
			}
			break;
		case ShapeType::Rect:
			counters->numRects++;
			break;
		case ShapeType::Circle:
			counters->numCircles++;
			break;
		case ShapeType::Ellipse:
			counters->numEllipses++;
			break;
		case ShapeType::Line:
			counters->numLines++;
			break;
		case ShapeType::Polyline:
		case ShapeType::Polygon:
			{
				ShapesCounters::PolyNum& polyNum = shapeType == ShapeType::Polyline ? counters->polylines : counters->polygons;
				const PointList* ptList = shapeListGetPointList(shapeList, shapeIndex);
				assert((ptList));
				polyNum.numPoly++;
				polyNum.numPoints += pointListGetNumPoints(ptList);
			}
			break;
		case ShapeType::Path:
			{
				const Path* path = shapeListGetPath(shapeList, shapeIndex);
				assert(path);
				const PathNum pathNum = pathGetSubpathCounters(path);
				counters->paths.open.numSubpaths   += pathNum.open.numSubpaths;
				counters->paths.open.numNodes      += pathNum.open.numNodes;
				counters->paths.closed.numSubpaths += pathNum.closed.numSubpaths;
				counters->paths.closed.numNodes    += pathNum.closed.numNodes;
			}
			break;
		case ShapeType::Text:
			counters->numTexts++;
			break;
		default:
			printf(" - (x) Unknown shape type\n");
		}
	}
}

} // namespace

ShapesCounters shapeListEnumerate(const ShapeList* shapeList)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");

	ShapesCounters counters;
	stdutils::memset<ShapesCounters>(&counters, 0);

	if (shapeList) {
		shapeListEnumerateRecursive(shapeList, &counters);
	}

	return counters;
}

void shapeListDeleteShape(ShapeList* shapeList, uint32_t shapeIndex)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return; }
	SSVG_CHECK(shapeIndex < shapeList->m_NumShapes, "Out of bounds shape index");
	if (shapeIndex >= shapeList->m_NumShapes) { return; }

	shapeClear(&shapeList->m_Shapes[shapeIndex]);
	const uint32_t numShapesToMove = shapeList->m_NumShapes > (1 + shapeIndex) ? (shapeList->m_NumShapes - 1 - shapeIndex) : 0;
	if (numShapesToMove > 0) {
		stdutils::memmove<Shape>(&shapeList->m_Shapes[shapeIndex], numShapesToMove, &shapeList->m_Shapes[shapeIndex + 1], sizeof(Shape) * numShapesToMove);
	}

	shapeList->m_NumShapes--;
}

void shapeListDeleteLastShape(ShapeList* shapeList)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return; }

	if (shapeList->m_NumShapes > 0) {
		return shapeListDeleteShape(shapeList, shapeList->m_NumShapes - 1);
	}
}

void shapeListCalcBounds(ShapeList* shapeList, float* bounds)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return; }
	SSVG_CHECK(bounds, "Nullptr to bounds array");
	if (!bounds) { return; ;}

	const uint32_t numShapes = shapeList->m_NumShapes;
	if (numShapes == 0) {
		bounds[0] = bounds[1] = bounds[2] = bounds[3] = 0.0f;
		return;
	}

	bounds[0] = math::kFloatMax;
	bounds[1] = math::kFloatMax;
	bounds[2] = -math::kFloatMax;
	bounds[3] = -math::kFloatMax;
	for (uint32_t i = 0; i < numShapes; ++i) {
		Shape* shape = &shapeList->m_Shapes[i];
		shapeUpdateBounds(shape);

		// Since this is a group, the child's bounding rect should be transformed using its
		// transformation matrix before calculating the group's local bounding rect.
		if (shape->m_Attrs) {
			float childTransformedRect[BOUNDING_RECT_ARRAY_SZ];
			transformBoundingRect(&shape->m_Attrs->m_Transform[0], &shape->m_BoundingRect[0], &childTransformedRect[0]);
			bounds[0] = stdutils::min<float>(bounds[0], childTransformedRect[0]);
			bounds[1] = stdutils::min<float>(bounds[1], childTransformedRect[1]);
			bounds[2] = stdutils::max<float>(bounds[2], childTransformedRect[2]);
			bounds[3] = stdutils::max<float>(bounds[3], childTransformedRect[3]);
		} else {
			bounds[0] = stdutils::min<float>(bounds[0], shape->m_BoundingRect[0]);
			bounds[1] = stdutils::min<float>(bounds[1], shape->m_BoundingRect[1]);
			bounds[2] = stdutils::max<float>(bounds[2], shape->m_BoundingRect[2]);
			bounds[3] = stdutils::max<float>(bounds[3], shape->m_BoundingRect[3]);
		}
	}
}

namespace {

// Allocate uninitialized commands. The caller is responsible for proper initialization
// Return the index of the first new command in path->m_Commands[]
uint32_t pathAllocRawUninitializedCommands(Path* path, uint32_t n)
{
	if (path->m_NumCommands + n > path->m_Capacity) {
		const uint32_t oldCapacity = path->m_Capacity;
		const uint32_t newCapacity = stdutils::max<uint32_t>(oldCapacity ? (oldCapacity * 3) / 2 : 4, oldCapacity + n);
		assert(newCapacity >= oldCapacity);
		path->m_Capacity = newCapacity;
		path->m_Commands = (PathCmd*)std::realloc(path->m_Commands, sizeof(PathCmd) * newCapacity);
	}
	uint32_t firstCmdAt = path->m_NumCommands;
	path->m_NumCommands += n;
	assert(path->m_NumCommands <= path->m_Capacity);

	return firstCmdAt;
}

} // namespace

PathCmd* pathAllocCommands(Path* path, uint32_t n)
{
	const uint32_t firstCmdAt = pathAllocRawUninitializedCommands(path, n);
	PathCmd* firstCmd = &path->m_Commands[firstCmdAt];
	const uint32_t remainingCapacity = firstCmdAt < path->m_Capacity ? path->m_Capacity - firstCmdAt : 0;
	stdutils::memset<PathCmd>(firstCmd, remainingCapacity, 0, sizeof(PathCmd) * n);

	assert(firstCmd->m_Type == PathCmdType::Nop);
	return firstCmd;
}

PathCmd* pathAllocCommand(Path* path, PathCmdType::Enum type)
{
	PathCmd* cmd = pathAllocCommands(path, 1);
	cmd->m_Type = type;

	return cmd;
}

PathCmd* pathInsertCommands(Path* path, uint32_t at, uint32_t n)
{
	SSVG_CHECK(at <= path->m_NumCommands, "Out of bound index on buffer m_commands");
	at = at > path->m_NumCommands ? path->m_NumCommands : at;

	if (at == path->m_NumCommands) {
		// Insert at the end == alloc
		return pathAllocCommands(path, n);
	}

	assert(at <= path->m_NumCommands);
	const uint32_t nbOfMovedCommands = path->m_NumCommands - at;
	IGNORE_RETURN pathAllocRawUninitializedCommands(path, n);
	stdutils::memmove<PathCmd>(&path->m_Commands[at + n], nbOfMovedCommands, &path->m_Commands[at], sizeof(PathCmd) * nbOfMovedCommands);
	stdutils::memset<PathCmd>(&path->m_Commands[at], n, 0, sizeof(PathCmd) * n);
	PathCmd* firstCmd = &path->m_Commands[at];

	assert(firstCmd->m_Type == PathCmdType::Nop);
	return firstCmd;
}

void pathReserveCommands(Path* path, uint32_t capacity)
{
	if (capacity > path->m_Capacity) {
		path->m_Capacity = capacity;
		path->m_Commands = (PathCmd*)std::realloc(path->m_Commands, sizeof(PathCmd) * capacity);
	}
}

void pathShrinkToFit(Path* path)
{
	if (path->m_NumCommands == 0 && path->m_Capacity > 0) {
		std::free(path->m_Commands);
		path->m_Commands = nullptr;
		path->m_Capacity = 0;
	} else if (path->m_NumCommands != path->m_Capacity) {
		assert(path->m_NumCommands < path->m_Capacity);
		path->m_Commands = (PathCmd*)std::realloc(path->m_Commands, sizeof(PathCmd) * path->m_NumCommands);
		path->m_Capacity = path->m_NumCommands;
	}
	assert(path->m_NumCommands == path->m_Capacity);
}

void pathClear(Path* path)
{
	SSVG_CHECK(path, "Nullptr to Path");
	if (!path) { return; }

	std::free(path->m_Commands);
	path->m_Commands = nullptr;
	path->m_NumCommands = 0;
	path->m_Capacity = 0;
}

inline uint32_t solveQuad(float a, float b, float c, float* t)
{
	if (std::abs(a) < 1e-5f) {
		if (std::abs(b) > 1e-5f) {
			t[0] = -c / b;
			return 1;
		}
	} else {
		const float desc = b * b - 4.0f * a * c;
		if (std::abs(desc) > 1e-5f) {
			const float desc_sqrt = std::sqrt(desc);
			t[0] = (-b + desc_sqrt) / (2.0f * a);
			t[1] = (-b - desc_sqrt) / (2.0f * a);

			return 2;
		}
	}

	return 0;
}

inline void evalCubicBezierAt(float t, const float* p0, const float* p1, const float* p2, const float* p3, float* p)
{
	const float t2 = t * t;
	const float t3 = t2 * t;
	const float one_t = 1.0f - t;
	const float one_t2 = one_t * one_t;
	const float one_t3 = one_t2 * one_t;

	const float a = one_t3;
	const float b = 3.0f * t * one_t2;
	const float c = 3.0f * t2 * one_t;
	const float d = t3;

	p[0] = a * p0[0] + b * p1[0] + c * p2[0] + d * p3[0];
	p[1] = a * p0[1] + b * p1[1] + c * p2[1] + d * p3[1];
}

inline void evalQuadraticBezierAt(float t, const float* p0, const float* p1, const float* p2, float* p)
{
	const float t2 = t * t;
	const float one_t = 1.0f - t;
	const float one_t2 = one_t * one_t;

	const float a = one_t2;
	const float b = 2.0f * one_t * t;
	const float c = t2;

	p[0] = a * p0[0] + b * p1[0] + c * p2[0];
	p[1] = a * p0[1] + b * p1[1] + c * p2[1];
}

void pathCalcBounds(const Path* path, float* bounds)
{
	const uint32_t numCommands = path->m_NumCommands;
	if (!numCommands) {
		// No commands -> invalid bounding rect
		bounds[0] = bounds[1] = math::kFloatMax;
		bounds[2] = bounds[3] = -math::kFloatMax;
		return;
	}

	const PathCmd* cmd = path->m_Commands;
	SSVG_CHECK(cmd->m_Type == PathCmdType::MoveTo, "First path command must be MoveTo");
	bounds[0] = bounds[2] = cmd->m_Data[0];
	bounds[1] = bounds[3] = cmd->m_Data[1];

	float last[2] = { cmd->m_Data[0], cmd->m_Data[1] };
	for (uint32_t iCmd = 1; iCmd < numCommands; ++iCmd) {
		cmd = &path->m_Commands[iCmd];

		switch (cmd->m_Type) {
		case PathCmdType::MoveTo:
		case PathCmdType::LineTo:
			bounds[0] = stdutils::min<float>(bounds[0], cmd->m_Data[0]);
			bounds[1] = stdutils::min<float>(bounds[1], cmd->m_Data[1]);
			bounds[2] = stdutils::max<float>(bounds[2], cmd->m_Data[0]);
			bounds[3] = stdutils::max<float>(bounds[3], cmd->m_Data[1]);

			last[0] = cmd->m_Data[0];
			last[1] = cmd->m_Data[1];
			break;
		case PathCmdType::CubicTo:
		{
			// Bezier end point
			bounds[0] = stdutils::min<float>(bounds[0], cmd->m_Data[4]);
			bounds[1] = stdutils::min<float>(bounds[1], cmd->m_Data[5]);
			bounds[2] = stdutils::max<float>(bounds[2], cmd->m_Data[4]);
			bounds[3] = stdutils::max<float>(bounds[3], cmd->m_Data[5]);

			// Extremities
			for (uint32_t dim = 0; dim < 2; ++dim) {
				const float c0 = last[dim];
				const float c1 = cmd->m_Data[dim + 0];
				const float c2 = cmd->m_Data[dim + 2];
				const float c3 = cmd->m_Data[dim + 4];

				const float a = 3.0f * (-c0 + 3.0f * (c1 - c2) + c3);
				const float b = 6.0f * (c0 - 2.0f * c1 + c2);
				const float c = 3.0f * (c1 - c0);

				float root[2] = { -1.0f, -1.0f }; // Max 2 roots
				uint32_t numRoots = solveQuad(a, b, c, &root[0]);

				for (uint32_t iRoot = 0; iRoot < numRoots; ++iRoot) {
					const float t = root[iRoot];
					if (t > 1e-5f && t < (1.0f - 1e-5f)) {
						float pos[2];
						evalCubicBezierAt(t, &last[0], &cmd->m_Data[0], &cmd->m_Data[2], &cmd->m_Data[4], &pos[0]);

						bounds[0] = stdutils::min<float>(bounds[0], pos[0]);
						bounds[1] = stdutils::min<float>(bounds[1], pos[1]);
						bounds[2] = stdutils::max<float>(bounds[2], pos[0]);
						bounds[3] = stdutils::max<float>(bounds[3], pos[1]);
					}
				}
			}

			last[0] = cmd->m_Data[4];
			last[1] = cmd->m_Data[5];
		}
		break;
		case PathCmdType::QuadraticTo:
			// Bezier end point
			bounds[0] = stdutils::min<float>(bounds[0], cmd->m_Data[2]);
			bounds[1] = stdutils::min<float>(bounds[1], cmd->m_Data[3]);
			bounds[2] = stdutils::max<float>(bounds[2], cmd->m_Data[2]);
			bounds[3] = stdutils::max<float>(bounds[3], cmd->m_Data[3]);

			// Extremities
			for (uint32_t dim = 0; dim < 2; ++dim) {
				const float c0 = last[dim];
				const float c1 = cmd->m_Data[dim + 0];
				const float c2 = cmd->m_Data[dim + 2];

				// dBezier(2,t)/dt = 2 * (a * t + b)
				const float a = (c2 - c1);
				const float b = (c1 - c0);

				if (std::abs(a) > 1e-5f) {
					const float t = -b / a;

					if (t > 1e-5f && t < (1.0f - 1e-5f)) {
						float pos[2];
						evalQuadraticBezierAt(t, &last[0], &cmd->m_Data[0], &cmd->m_Data[2], &pos[0]);

						bounds[0] = stdutils::min<float>(bounds[0], pos[0]);
						bounds[1] = stdutils::min<float>(bounds[1], pos[1]);
						bounds[2] = stdutils::max<float>(bounds[2], pos[0]);
						bounds[3] = stdutils::max<float>(bounds[3], pos[1]);
					}
				}
			}

			last[0] = cmd->m_Data[2];
			last[1] = cmd->m_Data[3];
			break;
		case PathCmdType::ArcTo:
			// TODO: Find the true bounds of the arc.

			// End point
			bounds[0] = stdutils::min<float>(bounds[0], cmd->m_Data[5]);
			bounds[1] = stdutils::min<float>(bounds[1], cmd->m_Data[6]);
			bounds[2] = stdutils::max<float>(bounds[2], cmd->m_Data[5]);
			bounds[3] = stdutils::max<float>(bounds[3], cmd->m_Data[6]);

			last[0] = cmd->m_Data[5];
			last[1] = cmd->m_Data[6];
			break;
		case PathCmdType::Nop:
		case PathCmdType::ClosePath:
			// Noop
			break;
		default:
			SSVG_CHECK(false, "Unknown path command");
			break;
		}
	}
}

PathNum pathGetSubpathCounters(const Path* path)
{
	PathNum counters;
	stdutils::memset<PathNum>(&counters, 0);
	SSVG_CHECK(path, "Nullptr to Path");
	if (!path) { return counters; }

	// SVG2 spec:
	// If a "closepath" is followed immediately by a "moveto", then the "moveto" identifies the
	// start point of the next subpath. If a "closepath" is followed immediately by any other
	// command, then the next subpath starts at the same initial point as the current subpath.
	uint32_t subpathNumNodes = 0;
	for (uint32_t cmdIndex = 0; cmdIndex < path->m_NumCommands; cmdIndex++) {
		const PathCmdType::Enum cmdType = path->m_Commands[cmdIndex].m_Type;
		switch (cmdType) {
		case PathCmdType::Nop:
			// Do nothing
			break;
		case PathCmdType::MoveTo:
			if (subpathNumNodes > 0) {
				counters.open.numSubpaths++;
				counters.open.numNodes += subpathNumNodes;
				subpathNumNodes = 0;
			}
			subpathNumNodes++;
			break;
		case PathCmdType::ClosePath:
			if (subpathNumNodes > 0) {
				counters.closed.numSubpaths++;
				counters.closed.numNodes += subpathNumNodes;
				subpathNumNodes = 0;
			}
			break;
		case PathCmdType::LineTo:
		case PathCmdType::CubicTo:
		case PathCmdType::QuadraticTo:
		case PathCmdType::ArcTo:
			subpathNumNodes++;
			break;
		default:
			SSVG_CHECK(false, "Unknown command type");
			break;
		}
	}
	// Implicit end of an open subpath
	if (subpathNumNodes > 0) {
		counters.open.numSubpaths++;
		counters.open.numNodes += subpathNumNodes;
	}

	return counters;
}


float* pointListAllocPoints(PointList* ptList, uint32_t n)
{
	SSVG_CHECK(n != 0, "Requested invalid number of points");

	if (ptList->m_NumPoints + n > ptList->m_Capacity) {
		const uint32_t oldCapacity = ptList->m_Capacity;
		const uint32_t newCapacity = oldCapacity ? (oldCapacity * 3) / 2 : 8;

		ptList->m_Capacity = stdutils::max<uint32_t>(newCapacity, oldCapacity + n);
		ptList->m_Coords = (float*)std::realloc(ptList->m_Coords, sizeof(float) * 2 * ptList->m_Capacity);
	}
	assert(ptList->m_NumPoints + n <= ptList->m_Capacity);

	float* coords = &ptList->m_Coords[ptList->m_NumPoints << 1];
	ptList->m_NumPoints += n;

	return coords;
}

void pointListShrinkToFit(PointList* ptList)
{
	if (ptList->m_NumPoints == 0 && ptList->m_Capacity > 0) {
		std::free(ptList->m_Coords);
		ptList->m_Coords = nullptr;
		ptList->m_Capacity = 0;
	} else if (ptList->m_NumPoints != ptList->m_Capacity) {
		assert(ptList->m_NumPoints < ptList->m_Capacity);
		ptList->m_Coords = (float*)std::realloc(ptList->m_Coords, sizeof(float) * 2 * ptList->m_NumPoints);
		ptList->m_Capacity = ptList->m_NumPoints;
	}
	assert(ptList->m_NumPoints == ptList->m_Capacity);
}

void pointListClear(PointList* ptList)
{
	SSVG_CHECK(ptList, "Nullptr to PointList");
	if (!ptList) { return; }

	std::free(ptList->m_Coords);
	ptList->m_Coords = nullptr;
	ptList->m_NumPoints = 0;
	ptList->m_Capacity = 0;
}

void pointListCalcBounds(const PointList* ptList, float* bounds)
{
	SSVG_CHECK(ptList, "Nullptr to PointList");
	if (!ptList) { return; }

	const uint32_t numPoints = ptList->m_NumPoints;
	if (!numPoints) {
		// No points -> invalid bounding rect
		bounds[0] = bounds[1] = math::kFloatMax;
		bounds[2] = bounds[3] = -math::kFloatMax;
		return;
	}

	bounds[0] = bounds[2] = ptList->m_Coords[0];
	bounds[1] = bounds[3] = ptList->m_Coords[1];

	for (uint32_t i = 1; i < numPoints; ++i) {
		const float x = ptList->m_Coords[i * 2 + 0];
		const float y = ptList->m_Coords[i * 2 + 1];

		bounds[0] = stdutils::min<float>(bounds[0], x);
		bounds[1] = stdutils::min<float>(bounds[1], y);
		bounds[2] = stdutils::max<float>(bounds[2], x);
		bounds[3] = stdutils::max<float>(bounds[3], y);
	}
}

uint32_t pointListGetNumPoints(const PointList* ptList)
{
	SSVG_CHECK(ptList, "Nullptr to PointList");
	if (!ptList) { return 0; }

	return  ptList->m_NumPoints;
}

void textSetString(Text* text, const char* str)
{
	SSVG_CHECK(text, "Nullptr to Text");
	if (!text) { return; }

	ownedStringSet(text->m_String, str);
}

void textClear(Text* text)
{
	SSVG_CHECK(text, "Nullptr to Text");
	if (!text) { return; }

	ownedStringClear(text->m_String);
	stdutils::memset<Text>(text, 0);
}

std::string_view shapeAttrsGetID(const ShapeAttributes* attrs)
{
	if (!attrs) { return ""; }
	const uint32_t len = stdutils::strnlen(&attrs->m_ID[0], SSVG_CONFIG_ID_MAX_LEN);
	return std::string_view(&attrs->m_ID[0], len);
}

std::string_view shapeAttrsGetFontFamily(const ShapeAttributes* attrs)
{
	if (!attrs) { return ""; }
	const uint32_t len = stdutils::strnlen(&attrs->m_FontFamily[0], SSVG_CONFIG_FONT_FAMILY_MAX_LEN);
	return std::string_view(&attrs->m_FontFamily[0], len);
}

std::string_view shapeAttrsGetClass(const ShapeAttributes* attrs)
{
	if (!attrs) { return ""; }
#if SSVG_CONFIG_CLASS_MAX_LEN
	const uint32_t len = stdutils::strnlen(&attrs->m_Class[0], SSVG_CONFIG_CLASS_MAX_LEN);
	return std::string_view(&attrs->m_Class[0], len);
#else
	return std::string_view();
#endif
}

void shapeAttrsSetID(ShapeAttributes* attrs, const std::string_view& value)
{
	SSVG_CHECK(attrs, "Nullptr to ShapeAttrubutes. Allocate them first with shapeAllocAttributes.");
	if (!attrs) { return; }
	uint32_t maxLen = stdutils::min<uint32_t>(SSVG_CONFIG_ID_MAX_LEN - 1, static_cast<uint32_t>(value.length()));
	SSVG_WARN((std::int32_t)maxLen >= strlenint(value), "id \"%.*s\" truncated to %d characters", strlenint(value), value.data(), maxLen);
	stdutils::memcpy<char>(&attrs->m_ID[0], SSVG_CONFIG_ID_MAX_LEN, value.data(), maxLen);
	attrs->m_ID[maxLen] = '\0';
}

void shapeAttrsSetFontFamily(ShapeAttributes* attrs, const std::string_view& value)
{
	SSVG_CHECK(attrs, "Nullptr to ShapeAttrubutes. Allocate them first with shapeAllocAttributes.");
	if (!attrs) { return; }
	uint32_t maxLen = stdutils::min<uint32_t>(SSVG_CONFIG_FONT_FAMILY_MAX_LEN - 1, static_cast<uint32_t>(value.length()));
	SSVG_WARN((std::int32_t)maxLen >= strlenint(value), "font-family \"%.*s\" truncated to %d characters", strlenint(value), value.data(), maxLen);
	stdutils::memcpy<char>(&attrs->m_FontFamily[0], SSVG_CONFIG_FONT_FAMILY_MAX_LEN, value.data(), maxLen);
	attrs->m_FontFamily[maxLen] = '\0';
}

void shapeAttrsSetClass(ShapeAttributes* attrs, const std::string_view& value)
{
	SSVG_CHECK(attrs, "Nullptr to ShapeAttrubutes. Allocate them first with shapeAllocAttributes.");
	if (!attrs) { return; }
#if SSVG_CONFIG_CLASS_MAX_LEN
	uint32_t maxLen = stdutils::min<uint32_t>(SSVG_CONFIG_CLASS_MAX_LEN - 1, static_cast<uint32_t>(value.length()));
	SSVG_WARN((std::int32_t)maxLen >= strlenint(value), "class \"%.*s\" truncated to %d characters", strlenint(value), value.data(), maxLen);
	stdutils::memcpy<char>(&attrs->m_Class[0], SSVG_CONFIG_CLASS_MAX_LEN, value.data(), maxLen);
	attrs->m_Class[maxLen] = '\0';
#else
	UNUSED(value);
#endif
}

Image* imageCreate(const ShapeAttributes* baseAttrs)
{
	Image* img = (Image*)std::malloc(sizeof(Image));
	if (!img) { return nullptr; }
	stdutils::memset<Image>(img, 0);

	if (baseAttrs) {
		img->m_BaseAttrs = *baseAttrs;
	} else {
		img->m_BaseAttrs = defaultShapeAttributes();
	}

	return img;
}

void imageFree(Image* img)
{
	if (!img) { return; }
	groupClear(&img->m_RootContainer);
	std::free(img);
}

ShapeList* shapeListCreate()
{
	ShapeList* newList = (ShapeList*)std::malloc(sizeof(ShapeList));
	stdutils::memset<ShapeList>(newList, 0);

	return newList;
}

void shapeListFree(ShapeList* shapeList)
{
	if (!shapeList) { return; }
	shapeListClear(shapeList);
	std::free(shapeList);
}

void initLib()
{
	s_ShapeAttrFreeListHead = nullptr;
}

void shutdownLib()
{
	ShapeAttributeFreeListNode* node = s_ShapeAttrFreeListHead;
	while (node) {
		ShapeAttributeFreeListNode* next = node->m_Next;

		std::free(node->m_Attrs);
		std::free(node);

		node = next;
	}
	s_ShapeAttrFreeListHead = nullptr;
}

const ShapeAttributes& defaultShapeAttributes()
{
	static ShapeAttributes defaultAttrs = []() {
		ShapeAttributes attrs;
		resetShapeAttributes(&attrs);

		// Default according to the SVG standards
		attrs.m_Opacity = 1.0f;
		attrs.m_StrokeWidth = Length{1.0f, LengthUnit::User};
		attrs.m_StrokeMiterLimit = 4.0f;
		attrs.m_StrokeOpacity = 1.0f;
		attrs.m_StrokePaint.m_Type = PaintType::None;
		attrs.m_StrokePaint.m_ColorABGR = 0xFF000000;               // Black
		attrs.m_StrokeLineCap = LineCap::Butt;
		attrs.m_StrokeLineJoin = LineJoin::Miter;
		attrs.m_FillOpacity = 1.0f;
		attrs.m_FillPaint.m_Type = PaintType::Color;
		attrs.m_FillPaint.m_ColorABGR = 0xFF000000;                 // Black
		attrs.m_FillRule = FillRule::NonZero;
		attrs.m_ColorPaint.m_Type = PaintType::None;
		attrs.m_ColorPaint.m_ColorABGR = 0xFF000000;                // Black
		attrs.m_FontSize = Length{SSVG_CONFIG_PARSER_DEFAULT_FONT_SIZE_IN_PX, LengthUnit::PX};  // Usually this is 12pt = 16px

		return attrs;
	}();

	return defaultAttrs;
}

ShapeAttributes* imageGetShapeAttributes(Image* img)
{
	return const_cast<ShapeAttributes*>(imageGetShapeAttributes(const_cast<const Image*>(img)));
}

const ShapeAttributes* imageGetShapeAttributes(const Image* img)
{
	SSVG_CHECK(img, "Nullptr to Image");
	if (!img) { return nullptr; }

	return &img->m_BaseAttrs;
}

Group* imageGetRootGroup(Image* img)
{
	return const_cast<Group*>(imageGetRootGroup(const_cast<const Image*>(img)));
}

const Group* imageGetRootGroup(const Image* img)
{
	SSVG_CHECK(img, "Nullptr to Image");
	if (!img) { return nullptr; }

	return &img->m_RootContainer;
}

ShapeList* imageGetRootShapeList(Image* img)
{
	return const_cast<ShapeList*>(imageGetRootShapeList(const_cast<const Image*>(img)));
}

const ShapeList* imageGetRootShapeList(const Image* img)
{
	SSVG_CHECK(img, "Nullptr to Image");
	if (!img) { return nullptr; }

	return &img->m_RootContainer.m_ShapeList;
}

const OwnedString& imageGetTile(const Image* img)
{
	SSVG_CHECK(img, "Nullptr to Image");
	if (!img) { return emptyOwnedString(); }

	return img->m_RootContainer.m_Title;
}

void imageSetTitle(Image* img, const char* str)
{
	SSVG_CHECK(img, "Nullptr to Image");
	if (!img) { return; }

	ownedStringSet(img->m_RootContainer.m_Title, str);
}

uint32_t imageGetNumShapes(const Image* img)
{
	SSVG_CHECK(img, "Nullptr to Image");
	if (!img) { return 0; }

	return img->m_RootContainer.m_ShapeList.m_NumShapes;
}

ShapeType::Enum shapeGetType(const Shape* shape)
{
	SSVG_CHECK(shape, "Nullptr to Shape");
	if (!shape) { return (ShapeType::Enum)0; }

	return shape->m_Type;
}

ShapeAttributes* shapeAllocAttributes(Shape* shape, const ShapeAttributes* parentAttrs)
{
	SSVG_CHECK(shape, "Nullptr to Shape");
	if (!shape) { return nullptr; }
	SSVG_WARN(!shape->m_Attrs, "The shape already has allocated attributes. Use shapeGetAttributes to access them");
	if (shape->m_Attrs) { return shape->m_Attrs; }

	ShapeAttributes* attrs = shape->m_Attrs = shapeAttrsAlloc();
	if (parentAttrs) {
		// We copy all attributes from the parent, except for:
		// - The transformation, else we would apply it twice on the shape.
		// - The ID and Class, specific to an alement.
		*attrs = *parentAttrs;
		attrs->m_Flags = AttribFlags::None;
		transformIdentity(attrs->m_Transform);
		attrs->m_ID[0] = '\0';
#if SSVG_CONFIG_CLASS_MAX_LEN
		attrs->m_Class[0] = '\0';
#endif
	}

	assert(shape->m_Attrs);
	return shape->m_Attrs;
}

ShapeAttributes* shapeGetAttributes(Shape* shape)
{
	return const_cast<ShapeAttributes*>(shapeGetAttributes(const_cast<const Shape*>(shape)));
}

const ShapeAttributes* shapeGetAttributes(const Shape* shape)
{
	SSVG_CHECK(shape, "Nullptr to Shape");
	if (!shape) { return nullptr; }

	return shape->m_Attrs;
}

ShapeAttributes* shapeListAllocShapeAttributes(ShapeList* shapeList, uint32_t shapeIndex, const ShapeAttributes* parentAttrs)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return nullptr; }
	SSVG_CHECK(shapeIndex < shapeList->m_NumShapes, "Out of bounds shape index");
	if (shapeIndex >= shapeList->m_NumShapes) { return nullptr; }

	return shapeAllocAttributes(&shapeList->m_Shapes[shapeIndex], parentAttrs);
}

ShapeAttributes* shapeListGetShapeAttributes(ShapeList* shapeList, uint32_t shapeIndex)
{
	return const_cast<ShapeAttributes*>(shapeListGetShapeAttributes(const_cast<const ShapeList*>(shapeList), shapeIndex));
}

const ShapeAttributes* shapeListGetShapeAttributes(const ShapeList* shapeList, uint32_t shapeIndex)
{
	SSVG_CHECK(shapeList, "Nullptr to ShapeList");
	if (!shapeList) { return nullptr; }
	SSVG_CHECK(shapeIndex < shapeList->m_NumShapes, "Out of bounds shape index");
	if (shapeIndex >= shapeList->m_NumShapes) { return nullptr; }

	return shapeList->m_Shapes[shapeIndex].m_Attrs;
}

bool shapeCopy(Shape* dst, const Shape* src, bool copyAttrs)
{
	const ShapeType::Enum type = src->m_Type;

	if (type != dst->m_Type) {
		SSVG_CHECK(shapeIsEmptyGroup(dst), "Cannot copy onto a Shape of a different type that is not cleared");
		if (!shapeIsEmptyGroup(dst)) { return false; }
		dst->m_Type = type;
	}

	stdutils::memcpy<float>(&dst->m_BoundingRect[0], BOUNDING_RECT_ARRAY_SZ, &src->m_BoundingRect[0], sizeof(float) * BOUNDING_RECT_ARRAY_SZ);

	if (copyAttrs && src->m_Attrs) {
		if (!dst->m_Attrs) {
			shapeAllocAttributes(dst);
		}
		assert(dst->m_Attrs);
		stdutils::memcpy<ShapeAttributes>(dst->m_Attrs, src->m_Attrs);
	}

	switch (type) {
	case ShapeType::Group:
		{
			ownedStringSet(dst->m_Group.m_Title, src->m_Group.m_Title.c_str());

			const ShapeList* srcShapeList = &src->m_Group.m_ShapeList;
			const uint32_t numShapes = srcShapeList->m_NumShapes;

			ShapeList* dstShapeList = &dst->m_Group.m_ShapeList;
			shapeListReserve(dstShapeList, dstShapeList->m_NumShapes + numShapes);

			for (uint32_t i = 0; i < numShapes; ++i) {
				const Shape* srcShape = &srcShapeList->m_Shapes[i];
				Shape* dstShape = shapeListAllocShape(dstShapeList, srcShape->m_Type);
				shapeCopy(dstShape, srcShape);
			}
		}
		break;
	case ShapeType::Rect:
		stdutils::memcpy<Rect>(&dst->m_Rect, &src->m_Rect);
		break;
	case ShapeType::Circle:
		stdutils::memcpy<Circle>(&dst->m_Circle, &src->m_Circle);
		break;
	case ShapeType::Ellipse:
		stdutils::memcpy<Ellipse>(&dst->m_Ellipse, &src->m_Ellipse);
		break;
	case ShapeType::Line:
		stdutils::memcpy<Line>(&dst->m_Line, &src->m_Line);
		break;
	case ShapeType::Polyline:
	case ShapeType::Polygon:
		{
			const uint32_t nbOfNewCoords = src->m_PointList.m_NumPoints;
			float* newPoints = pointListAllocPoints(&dst->m_PointList, nbOfNewCoords);
			stdutils::memcpy<float>(newPoints, 2 * nbOfNewCoords, src->m_PointList.m_Coords, sizeof(float) * 2 * nbOfNewCoords);
		}
	    break;
	case ShapeType::Path:
		{
			const Path* srcPath = &src->m_Path;
			const uint32_t nbOfNewCommands = srcPath->m_NumCommands;
			PathCmd* newCommands = pathAllocCommands(&dst->m_Path, nbOfNewCommands);
			stdutils::memcpy<PathCmd>(newCommands, nbOfNewCommands, srcPath->m_Commands, sizeof(PathCmd) * nbOfNewCommands);
		}
		break;
	case ShapeType::Text:
		{
			const Text* srcText = &src->m_Text;
			Text* dstText = &dst->m_Text;
			dstText->x = srcText->x;
			dstText->y = srcText->y;
			dstText->m_Anchor = srcText->m_Anchor;
			textSetString(dstText, srcText->m_String.c_str());
		}
		break;
	default:
		SSVG_CHECK(false, "Unknown shape type");
		return false;
	}

	return true;
}

// Clear the Shape, release resources
// The cleared Shape is an empty Group
void shapeClear(Shape* shape)
{
	SSVG_CHECK(shape, "Nullptr to Shape");
	if (!shape) { return; }

	switch (shape->m_Type) {
	case ShapeType::Group:
		groupClear(&shape->m_Group);
		break;
	case ShapeType::Path:
		pathClear(&shape->m_Path);
		break;
	case ShapeType::Polygon:
	case ShapeType::Polyline:
		pointListClear(&shape->m_PointList);
		break;
	case ShapeType::Text:
		textClear(&shape->m_Text);
		break;
	default:
		break;
	}
	shapeAttrsFree(shape->m_Attrs);
	stdutils::memset<Shape>(shape, 0);
	assert(shapeIsEmptyGroup(shape));
}

void shapeUpdateBounds(Shape* shape)
{
	float bounds[4] = { math::kFloatMax, math::kFloatMax, -math::kFloatMax, -math::kFloatMax };

	const ShapeType::Enum type = shape->m_Type;
	switch (type) {
	case ShapeType::Group:
		shapeListCalcBounds(&shape->m_Group.m_ShapeList, &bounds[0]);
		break;
	case ShapeType::Rect:
		bounds[0] = shape->m_Rect.x;
		bounds[1] = shape->m_Rect.y;
		bounds[2] = shape->m_Rect.x + shape->m_Rect.width;
		bounds[3] = shape->m_Rect.y + shape->m_Rect.height;
		break;
	case ShapeType::Circle:
		bounds[0] = shape->m_Circle.cx - shape->m_Circle.r;
		bounds[1] = shape->m_Circle.cy - shape->m_Circle.r;
		bounds[2] = shape->m_Circle.cx + shape->m_Circle.r;
		bounds[3] = shape->m_Circle.cy + shape->m_Circle.r;
		break;
	case ShapeType::Ellipse:
		bounds[0] = shape->m_Ellipse.cx - shape->m_Ellipse.rx;
		bounds[1] = shape->m_Ellipse.cy - shape->m_Ellipse.ry;
		bounds[2] = shape->m_Ellipse.cx + shape->m_Ellipse.rx;
		bounds[3] = shape->m_Ellipse.cy + shape->m_Ellipse.ry;
		break;
	case ShapeType::Line:
		bounds[0] = stdutils::min<float>(shape->m_Line.x1, shape->m_Line.x2);
		bounds[1] = stdutils::min<float>(shape->m_Line.y1, shape->m_Line.y2);
		bounds[2] = stdutils::max<float>(shape->m_Line.x1, shape->m_Line.x2);
		bounds[3] = stdutils::max<float>(shape->m_Line.y1, shape->m_Line.y2);
		break;
	case ShapeType::Polyline:
	case ShapeType::Polygon:
		pointListCalcBounds(&shape->m_PointList, &bounds[0]);
		break;
	case ShapeType::Path:
		pathCalcBounds(&shape->m_Path, &bounds[0]);
		break;
	case ShapeType::Text:
		// TODO: This is complicated!
		bounds[0] = bounds[1] = bounds[2] = bounds[3] = 0.0f;
		break;
	default:
		break;
	}

	stdutils::memcpy<float>(&shape->m_BoundingRect[0], BOUNDING_RECT_ARRAY_SZ, &bounds[0], sizeof(float) * 4);
}

namespace {

ShapeAttributes* shapeAttrsAllocFromNode(ShapeAttributeFreeListNode* node)
{
	SSVG_CHECK(node->m_FirstFreeID != UINT32_MAX, "No free slot in free list node. This function shouldn't have been called");

	ShapeAttributes* attrs = &node->m_Attrs[node->m_FirstFreeID];
	const uint32_t nextFreeID = *(uint32_t*)attrs;
	node->m_FirstFreeID = nextFreeID;
	node->m_NumFree--;
	resetShapeAttributes(attrs);

	return attrs;
}

ShapeAttributes* shapeAttrsAlloc()
{
	constexpr uint32_t kNumShapeAttributesPerBatch = 1024;

	ShapeAttributeFreeListNode* node = s_ShapeAttrFreeListHead;
	while (node) {
		if (node->m_FirstFreeID != UINT32_MAX) {
			return shapeAttrsAllocFromNode(node);
		}

		node = node->m_Next;
	}

	node = (ShapeAttributeFreeListNode*)std::malloc(sizeof(ShapeAttributeFreeListNode));
	SSVG_CHECK(node != nullptr, "Failed to allocate shape attributes");

	node->m_Attrs = (ShapeAttributes*)std::malloc(sizeof(ShapeAttributes) * kNumShapeAttributesPerBatch);
	node->m_NumAttrs = kNumShapeAttributesPerBatch;
	node->m_Next = s_ShapeAttrFreeListHead;
	node->m_Prev = nullptr;
	node->m_NumFree = kNumShapeAttributesPerBatch;

	node->m_FirstFreeID = 0;
	for (uint32_t i = 0; i < kNumShapeAttributesPerBatch - 1; ++i) {
		*(uint32_t*)&node->m_Attrs[i] = i + 1;
	}
	*(uint32_t*)&node->m_Attrs[kNumShapeAttributesPerBatch - 1] = UINT32_MAX;

	if (s_ShapeAttrFreeListHead != nullptr) {
		s_ShapeAttrFreeListHead->m_Prev = node;
	}
	s_ShapeAttrFreeListHead = node;

	return shapeAttrsAllocFromNode(node);
}

void shapeAttrsFree(ShapeAttributes* attrs)
{
	if (!attrs) { return; }

	// Find the free list node attrs belongs to
	ShapeAttributeFreeListNode* node = s_ShapeAttrFreeListHead;
	while (node) {
		if (attrs >= node->m_Attrs && attrs < node->m_Attrs + node->m_NumAttrs) {
			break;
		}

		node = node->m_Next;
	}

	SSVG_CHECK(node != nullptr, "Shape attributes not allocated via the free list (double deallocation?)");

	const uint32_t id = (uint32_t)(attrs - node->m_Attrs);
	SSVG_CHECK(id < node->m_NumAttrs, "ShapeAttributes index is out of bounds");

	*(uint32_t*)attrs = node->m_FirstFreeID;
	node->m_FirstFreeID = id;

	node->m_NumFree++;
	if (node->m_NumFree == node->m_NumAttrs) {
		std::free(node->m_Attrs);

		ShapeAttributeFreeListNode* prev = node->m_Prev;
		ShapeAttributeFreeListNode* next = node->m_Next;
		if (prev) {
			prev->m_Next = next;
		}
		if (next) {
			next->m_Prev = prev;
		}

		if (s_ShapeAttrFreeListHead == node) {
			s_ShapeAttrFreeListHead = next;
		}

		std::free(node);
	}
}

} // namespace

namespace internals {

AllocatedShapeAttrsCounters enumerateAllocatedShapeAttrs()
{
	AllocatedShapeAttrsCounters counters;
	stdutils::memset<AllocatedShapeAttrsCounters>(&counters, 0);

	ShapeAttributeFreeListNode* node = s_ShapeAttrFreeListHead;
	while (node) {
		counters.numNodes++;
		counters.numAllocAttrs += node->m_NumAttrs - node->m_NumFree;
		counters.numFreeAttrs += node->m_NumFree;
		node = node->m_Next;
	}

	return counters;
}

} // namespace internals

} // namespace svg
