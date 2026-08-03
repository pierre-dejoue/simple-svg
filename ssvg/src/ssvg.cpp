#include <ssvg/ssvg.h>

#include "ssvg_debug.h"
#include "ssvg_math.h"

#include <stdutils/macros.h>
#include <stdutils/memory.h>
#include <stdutils/minmax.h>
#include <stdutils/string.h>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <string_view>

namespace ssvg
{

namespace {

int strlenint(std::string_view str)
{
	return static_cast<int>(str.length());
}

} // namespace

struct ShapeAttributeFreeListNode
{
	ShapeAttributeFreeListNode* m_Next;
	ShapeAttributeFreeListNode* m_Prev;
	ShapeAttributes* m_Attrs;
	uint32_t m_NumAttrs;
	uint32_t m_FirstFreeID;
	uint32_t m_NumFree;
};

namespace {
ShapeAttributeFreeListNode* s_ShapeAttrFreeListHead = nullptr;

ShapeAttributes* shapeAttrsAlloc();
void shapeAttrsFree(ShapeAttributes* attrs);
} // namespace

void transformIdentity(float* transform)
{
	stdutils::memset<float>(transform, TRANSFORM_ARRAY_SZ, 0, sizeof(float) * TRANSFORM_ARRAY_SZ);
	transform[0] = 1.0f;
	transform[3] = 1.0f;
}

void transformTranslation(float* transform, float x, float y)
{
	transform[0] = 1.0f;
	transform[1] = 0.0f;
	transform[2] = 0.0f;
	transform[3] = 1.0f;
	transform[4] = x;
	transform[5] = y;
}

// a = a * b;
void transformMultiply(float* a, const float* b)
{
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
	float tmp[6];
	transformTranslation(&tmp[0], x, y);
	transformMultiply(transform, tmp);
}

void transformPoint(const float* transform, const float* localPos, float* globalPos)
{
	const float x = localPos[0];
	const float y = localPos[1];
	globalPos[0] = transform[0] * x + transform[2] * y + transform[4];
	globalPos[1] = transform[1] * x + transform[3] * y + transform[5];
}

void transformBoundingRect(const float* transform, const float* localRect, float* globalRect)
{
	float transformedRect[4];
	transformPoint(transform, &localRect[0], &transformedRect[0]);
	transformPoint(transform, &localRect[2], &transformedRect[2]);

	globalRect[0] = stdutils::min<float>(transformedRect[0], transformedRect[2]);
	globalRect[1] = stdutils::min<float>(transformedRect[1], transformedRect[3]);
	globalRect[2] = stdutils::max<float>(transformedRect[0], transformedRect[2]);
	globalRect[3] = stdutils::max<float>(transformedRect[1], transformedRect[3]);
}

bool shapeListIsReadOnly(ShapeList* shapeList)
{
	// We may use a ShapeList as a temporary reference to another, in which case the ShapeList does not own the memory buffer
	return shapeList->m_Shapes != nullptr && shapeList->m_Capacity == 0;
}

Shape* shapeListAllocShape(ShapeList* shapeList, ShapeType::Enum type, const ShapeAttributes* parentAttrs)
{
	SSVG_CHECK(!shapeListIsReadOnly(shapeList), "Trying to expand a read-only shape list?");
	SSVG_CHECK(shapeList->m_NumShapes <= shapeList->m_Capacity, "Invalid capacity of a shape list (expand)");

	if (shapeList->m_NumShapes + 1 > shapeList->m_Capacity) {
		const uint32_t oldCapacity = shapeList->m_Capacity;

		// TODO: Since shapes are fairly large objects, check if allocating a constant amount each time somehow helps
		const uint32_t newCapacity = stdutils::max<uint32_t>(oldCapacity ? (oldCapacity * 3) / 2 : 4, shapeList->m_NumShapes + 1);
		if (newCapacity > oldCapacity) {
			shapeList->m_Capacity = newCapacity;
			shapeList->m_Shapes = (Shape*)std::realloc(shapeList->m_Shapes, sizeof(Shape) * newCapacity);
			const uint32_t capacityIncr = newCapacity - oldCapacity;
			stdutils::memset<Shape>(&shapeList->m_Shapes[oldCapacity], capacityIncr, 0, sizeof(Shape) * capacityIncr);
		}
	}
	assert(shapeList->m_NumShapes + 1 <= shapeList->m_Capacity);

	Shape* shape = &shapeList->m_Shapes[shapeList->m_NumShapes++];
	shape->m_Type = type;
	shape->m_Attrs = shapeAttrsAlloc();
	stdutils::memset<ShapeAttributes>(shape->m_Attrs, 0);
	shape->m_Attrs->m_Parent = parentAttrs;
	shape->m_Attrs->m_Flags = AttribFlags::InheritAll;
	shape->m_Attrs->m_Opacity = 1.0f;
	shape->m_Attrs->m_ID[0] = '\0';
#if SSVG_CONFIG_CLASS_MAX_LEN
	shape->m_Attrs->m_Class[0] = '\0';
#endif
	transformIdentity(&shape->m_Attrs->m_Transform[0]);

	return shape;
}

void shapeListShrinkToFit(ShapeList* shapeList)
{
	SSVG_CHECK(!shapeListIsReadOnly(shapeList), "Trying to shrink a read-only shape list?");
	SSVG_CHECK(shapeList->m_NumShapes <= shapeList->m_Capacity, "Invalid capacity of a shape list (shrink)");

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

void shapeListFree(ShapeList* shapeList)
{
	SSVG_CHECK(!shapeListIsReadOnly(shapeList), "Trying to free a read-only shape list?");
	SSVG_CHECK(shapeList->m_NumShapes <= shapeList->m_Capacity, "Invalid capacity of a shape list (free)");

	const uint32_t n = shapeList->m_NumShapes;
	for (uint32_t i = 0; i < n; ++i) {
		Shape* shape = &shapeList->m_Shapes[i];
		shapeFree(shape);
	}

	std::free(shapeList->m_Shapes);
	shapeList->m_Shapes = nullptr;
	shapeList->m_Capacity = 0;
	shapeList->m_NumShapes = 0;
}

void shapeListReserve(ShapeList* shapeList, uint32_t capacity)
{
	SSVG_CHECK(!shapeListIsReadOnly(shapeList), "Trying to reserve memory for a read-only shape list?");
	SSVG_CHECK(shapeList->m_NumShapes <= shapeList->m_Capacity, "Invalid capacity of a shape list (reserve)");

	const uint32_t oldCapacity = shapeList->m_Capacity;
	if (capacity <= oldCapacity) {
		return;
	}

	shapeList->m_Shapes = (Shape*)std::realloc(shapeList->m_Shapes, sizeof(Shape) * capacity);
	shapeList->m_Capacity = capacity;
	assert(oldCapacity <= capacity);
	stdutils::memset<Shape>(&shapeList->m_Shapes[oldCapacity], capacity - oldCapacity, 0, sizeof(Shape) * (capacity - oldCapacity));
	assert(shapeList->m_NumShapes <= shapeList->m_Capacity);
}

uint32_t shapeListMoveShapeToBack(ShapeList* shapeList, uint32_t shapeID)
{
	SSVG_CHECK(shapeID < shapeList->m_NumShapes, "Invalid shape ID");
	if (shapeID == 0 || shapeList->m_NumShapes <= 1) {
		return shapeID;
	}

	Shape tmp;
	stdutils::memcpy<Shape>(&tmp, &shapeList->m_Shapes[shapeID - 1]);
	stdutils::memcpy<Shape>(&shapeList->m_Shapes[shapeID - 1], &shapeList->m_Shapes[shapeID]);
	stdutils::memcpy<Shape>(&shapeList->m_Shapes[shapeID], &tmp);

	return shapeID - 1;
}

uint32_t shapeListMoveShapeToFront(ShapeList* shapeList, uint32_t shapeID)
{
	SSVG_CHECK(shapeID < shapeList->m_NumShapes, "Invalid shape ID");
	if (shapeID == shapeList->m_NumShapes - 1) {
		return shapeID;
	}

	Shape tmp;
	stdutils::memcpy<Shape>(&tmp, &shapeList->m_Shapes[shapeID + 1]);
	stdutils::memcpy<Shape>(&shapeList->m_Shapes[shapeID + 1], &shapeList->m_Shapes[shapeID]);
	stdutils::memcpy<Shape>(&shapeList->m_Shapes[shapeID], &tmp);

	return shapeID + 1;
}

void shapeListDeleteShape(ShapeList* shapeList, uint32_t shapeID)
{
	SSVG_CHECK(shapeID < shapeList->m_NumShapes, "Invalid shape ID");

	shapeFree(&shapeList->m_Shapes[shapeID]);

	const uint32_t numShapesToMove = shapeList->m_NumShapes > (1 + shapeID) ? (shapeList->m_NumShapes - 1 - shapeID) : 0;
	if (numShapesToMove > 0) {
		stdutils::memmove<Shape>(&shapeList->m_Shapes[shapeID], numShapesToMove, &shapeList->m_Shapes[shapeID + 1], sizeof(Shape) * numShapesToMove);
	}

	shapeList->m_NumShapes--;
}

void shapeListCalcBounds(ShapeList* shapeList, float* bounds)
{
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
		float childTransformedRect[4];
		transformBoundingRect(&shape->m_Attrs->m_Transform[0], &shape->m_BoundingRect[0], &childTransformedRect[0]);

		bounds[0] = stdutils::min<float>(bounds[0], childTransformedRect[0]);
		bounds[1] = stdutils::min<float>(bounds[1], childTransformedRect[1]);
		bounds[2] = stdutils::max<float>(bounds[2], childTransformedRect[2]);
		bounds[3] = stdutils::max<float>(bounds[3], childTransformedRect[3]);
	}
}

PathCmd* pathAllocCommands(Path* path, uint32_t n)
{
	if (path->m_NumCommands + n > path->m_Capacity) {
		const uint32_t oldCapacity = path->m_Capacity;
		const uint32_t newCapacity = stdutils::max<uint32_t>(oldCapacity ? (oldCapacity * 3) / 2 : 4, oldCapacity + n);
		assert(newCapacity >= oldCapacity);
		const uint32_t capacityIncr = newCapacity - oldCapacity;
		path->m_Capacity = newCapacity;
		path->m_Commands = (PathCmd*)std::realloc(path->m_Commands, sizeof(PathCmd) * newCapacity);
		stdutils::memset<PathCmd>(&path->m_Commands[oldCapacity], capacityIncr, 0, sizeof(PathCmd) * capacityIncr);
	}
	assert(path->m_NumCommands + n <= path->m_Capacity);

	PathCmd* firstCmd = &path->m_Commands[path->m_NumCommands];
	path->m_NumCommands += n;

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

	const uint32_t numOldCommands = path->m_NumCommands;
	assert(at <= numOldCommands);
	const uint32_t commandsIncr = numOldCommands - at;
	pathAllocCommands(path, n);
	stdutils::memmove<PathCmd>(&path->m_Commands[at + n], commandsIncr, &path->m_Commands[at], sizeof(PathCmd) * commandsIncr);
	return &path->m_Commands[at];
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

void pathFree(Path* path)
{
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
		case PathCmdType::ClosePath:
			// Noop
			break;
		default:
			SSVG_CHECK(false, "Unknown path command");
			break;
		}
	}
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

void pointListFree(PointList* ptList)
{
	std::free(ptList->m_Coords);
	ptList->m_Coords = nullptr;
	ptList->m_NumPoints = 0;
	ptList->m_Capacity = 0;
}

void pointListCalcBounds(const PointList* ptList, float* bounds)
{
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

std::string_view shapeAttrsGetID(const ShapeAttributes* attrs)
{
	uint32_t len = 0;
	while (len < SSVG_CONFIG_ID_MAX_LEN && attrs->m_ID[len] != '\0') { len++; }
	return std::string_view(&attrs->m_ID[0], len);
}

void shapeAttrsSetID(ShapeAttributes* attrs, const std::string_view& value)
{
	uint32_t maxLen = stdutils::min<uint32_t>(SSVG_CONFIG_ID_MAX_LEN - 1, static_cast<uint32_t>(value.length()));
	SSVG_WARN((int32_t)maxLen >= strlenint(value), "id \"%.*s\" truncated to %d characters", strlenint(value), value.data(), maxLen);
	stdutils::memcpy<char>(&attrs->m_ID[0], SSVG_CONFIG_ID_MAX_LEN, value.data(), maxLen);
	attrs->m_ID[maxLen] = '\0';
}

void shapeAttrsSetFontFamily(ShapeAttributes* attrs, const std::string_view& value)
{
	uint32_t maxLen = stdutils::min<uint32_t>(SSVG_CONFIG_FONT_FAMILY_MAX_LEN - 1, static_cast<uint32_t>(value.length()));
	SSVG_WARN((int32_t)maxLen >= strlenint(value), "font-family \"%.*s\" truncated to %d characters", strlenint(value), value.data(), maxLen);
	stdutils::memcpy<char>(&attrs->m_FontFamily[0], SSVG_CONFIG_FONT_FAMILY_MAX_LEN, value.data(), maxLen);
	attrs->m_FontFamily[maxLen] = '\0';
}

void shapeAttrsSetClass(ShapeAttributes* attrs, const std::string_view& value)
{
#if SSVG_CONFIG_CLASS_MAX_LEN
	uint32_t maxLen = stdutils::min<uint32_t>(SSVG_CONFIG_CLASS_MAX_LEN - 1, static_cast<uint32_t>(value.length()));
	SSVG_WARN((int32_t)maxLen >= strlenint(value), "class \"%.*s\" truncated to %d characters", strlenint(value), value.data(), maxLen);
	stdutils::memcpy<char>(&attrs->m_Class[0], SSVG_CONFIG_CLASS_MAX_LEN, value.data(), maxLen);
	attrs->m_Class[maxLen] = '\0';
#else
	UNUSED(attrs);
	UNUSED(value);
#endif
}

Image* imageCreate(const ShapeAttributes* baseAttrs)
{
	assert(baseAttrs);
	Image* img = (Image*)std::malloc(sizeof(Image));
	stdutils::memset<Image>(img, 0);
	stdutils::memcpy<ShapeAttributes>(&img->m_BaseAttrs, baseAttrs);

	return img;
}

void imageDestroy(Image* img)
{
	shapeListFree(&img->m_ShapeList);
	std::free(img);
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

bool shapeCopy(Shape* dst, const Shape* src, bool copyAttrs)
{
	const ShapeType::Enum type = src->m_Type;

	dst->m_Type = type;
	stdutils::memcpy<float>(&dst->m_BoundingRect[0], BOUNDING_RECT_ARRAY_SZ, &src->m_BoundingRect[0], sizeof(float) * BOUNDING_RECT_ARRAY_SZ);
	if (copyAttrs) {
		stdutils::memcpy<ShapeAttributes>(dst->m_Attrs, src->m_Attrs);
	}

	switch (type) {
	case ShapeType::Group:
	{
		const ShapeList* srcShapeList = &src->m_ShapeList;
		const uint32_t numShapes = srcShapeList->m_NumShapes;

		ShapeList* dstShapeList = &dst->m_ShapeList;
		shapeListReserve(dstShapeList, numShapes);

		for (uint32_t i = 0; i < numShapes; ++i) {
			const Shape* srcShape = &srcShapeList->m_Shapes[i];
			Shape* dstShape = shapeListAllocShape(dstShapeList, srcShape->m_Type, nullptr);
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
		stdutils::memcpy<float>(
			pointListAllocPoints(&dst->m_PointList, src->m_PointList.m_NumPoints),
			2 * dst->m_PointList.m_Capacity,
			src->m_PointList.m_Coords,
			sizeof(float) * 2 * src->m_PointList.m_NumPoints);
		break;
	case ShapeType::Path:
	{
		const Path* srcPath = &src->m_Path;
		const uint32_t numCommands = srcPath->m_NumCommands;

		Path* dstPath = &dst->m_Path;
		PathCmd* dstCommands = pathAllocCommands(dstPath, numCommands);
		stdutils::memcpy<PathCmd>(dstCommands, dstPath->m_Capacity, srcPath->m_Commands, sizeof(PathCmd) * numCommands);
	}
	break;
	case ShapeType::Text:
	{
		const Text* srcText = &src->m_Text;
		Text* dstText = &dst->m_Text;

		dstText->x = srcText->x;
		dstText->y = srcText->y;
		dstText->m_Anchor = srcText->m_Anchor;

		const uint32_t len = stdutils::strnlen(srcText->m_String);
		dstText->m_String = (char*)std::malloc(sizeof(char) * (len + 1));
		stdutils::memcpy<char>(dstText->m_String, len, srcText->m_String, len);
		dstText->m_String[len] = '\0';
	}
	break;
	default:
		SSVG_CHECK(false, "Unknown shape type");
		return false;
	}

	return true;
}

void shapeFree(Shape* shape)
{
	switch (shape->m_Type) {
	case ShapeType::Group:
		shapeListFree(&shape->m_ShapeList);
		break;
	case ShapeType::Path:
		pathFree(&shape->m_Path);
		break;
	case ShapeType::Polygon:
	case ShapeType::Polyline:
		pointListFree(&shape->m_PointList);
		break;
	case ShapeType::Text:
		std::free(shape->m_Text.m_String);
		shape->m_Text.m_String = nullptr;
		break;
	default:
		break;
	}

	shapeAttrsFree(shape->m_Attrs);
}

void shapeUpdateBounds(Shape* shape)
{
	float bounds[4] = { math::kFloatMax, math::kFloatMax, -math::kFloatMax, -math::kFloatMax };

	const ShapeType::Enum type = shape->m_Type;
	switch (type) {
	case ShapeType::Group:
		shapeListCalcBounds(&shape->m_ShapeList, &bounds[0]);
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
	SSVG_CHECK(id < node->m_NumAttrs, "Index out of bounds");

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

} // namespace svg
