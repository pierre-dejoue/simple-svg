simple-svg
==========

A lightweight SVG parser and writer library, in C++.

## About this fork of the library

This fork of [jdryg/simple-svg](https://github.com/jdryg/simple-svg) removes the dependency on [bx](https://github.com/bkaradzic/bx). It also provides a few bug fixes and patches.

This repo is open source and closed to contributions.

## Dependencies

* C++17
* Standard Template Library (STL)

## Code

* Library:
	- `ssvg.h`: Public header of the library. Structs, enums and function declarations
	- `ssvg.cpp`: Generic library functions for dealing with images, shape lists, point lists and paths
	- `ssvg_parser.cpp`: SVG parser
	- `ssvg_writer.cpp`: SVG writer
	- `ssvg_builder.cpp`: Helper functions for building images programmatically
* Demo/Testing:
	- `examples/example_01_round_trip/main.cpp`
	- `examples/example_02_build_svg/main.cpp`

## Examples

Read the examples to see the library in action!

### [Example_01_round_trip](examples/example_01_round_trip/main.cpp)

Do a round trip of parsing and writing a SVG file. Provide the initial SVG file as input:

```
$ ./example_01_round_trip.exe ./examples/svg_files/Ghostscript_Tiger.svg
Loading "./examples/svg_files/Ghostscript_Tiger.svg"...
- Root element contains 1 shapes
Converting "./examples/svg_files/Ghostscript_Tiger.svg" to "./examples/svg_files/round_trip_Ghostscript_Tiger.svg"...
Loading "./examples/svg_files/round_trip_Ghostscript_Tiger.svg"...
- Root element contains 1 shapes
```

### [Example_02_build_svg](examples/example_02_build_svg/main.cpp)

Build a SVG image programmatically and save it to file `test_output.svg`

```
$ ./example_02_build_svg.exe
Building "./test_output.svg"
```

### [Example_03_enumerate_shapes](examples/example_03_enumerate_shapes/main.cpp)

Parse a SVG file, count the shapes and total number of points/nodes in the image.

```
$ ./example_03_enumerate_shapes.exe ./examples/svg_files/Ghostscript_Tiger.svg
Loading "./examples/svg_files/Ghostscript_Tiger.svg"...
Nb of groups: 241
Basic shapes:
    rect: 0; circle: 0; ellipse: 0
    line: 0; polyline: 0 (0 points); polygon: 0 (0 points);
Paths:
    closed: 227 (2246 nodes)
    open:   13 (37 nodes)
```
