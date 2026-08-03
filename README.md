simple-svg
==========

A simple SVG parser and writer in C++17.

## About this fork of the library

This fork of [jdryg/simple-svg](https://github.com/jdryg/simple-svg) removes the dependency on [bx](https://github.com/bkaradzic/bx). It also provides a few bug fixes and patches.

There is no intent to provide any level of maintainance or support of this repository. It is open source and closed to contributions.

### Code

* Library:
	- `ssvg.h`: Structs, enums and function declarations
	- `ssvg.cpp`: Generic library functions for dealing with images, shape lists, point lists and paths
	- `ssvg_parser.cpp`: SVG parser
	- `ssvg_writer.cpp`: SVG writer
	- `ssvg_builder.cpp`: Helper functions for building images
* Demo/Test:
	- `examples/main.cpp`

### Dependencies

* C++17
* Standard Template Library (STL)
