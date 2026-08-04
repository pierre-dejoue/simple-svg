simple-svg
==========

A simple SVG parser and writer in C++17.

## About this fork of the library

This fork of [jdryg/simple-svg](https://github.com/jdryg/simple-svg) removes the dependency on [bx](https://github.com/bkaradzic/bx). It also provides a few bug fixes and patches.

This repo is open source and closed to contributions.

## Code

* Library:
	- `ssvg.h`: Structs, enums and function declarations
	- `ssvg.cpp`: Generic library functions for dealing with images, shape lists, point lists and paths
	- `ssvg_parser.cpp`: SVG parser
	- `ssvg_writer.cpp`: SVG writer
	- `ssvg_builder.cpp`: Helper functions for building images
* Demo/Test:
	- `examples/main.cpp`

## Dependencies

* C++17
* Standard Template Library (STL)

## Examples

Read the examples to see the library in action.

### Example_01_round_trip

Round trip of parsing and writing a SVG file. Pass an input SVG file as input:

```
$ ./build/examples/Debug/example_01_round_trip.exe ./examples/svg/Ghostscript_Tiger.svg
```

### Example_02_build_svg

Build a SVG image programmatically and save it to file `test_output.svg`

```
$ ./build/examples/Debug/example_02_build_svg.exe
Building "./test_output.svg"
```
