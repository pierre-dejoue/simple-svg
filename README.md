simpler-svg
===========

A lightweight SVG parser and writer library, in C++.

## Project history

This project is a fork of [jdryg/simple-svg](https://github.com/jdryg/simple-svg), it was initially created with the goal of removing the dependency on [bx](https://github.com/bkaradzic/bx).

Finally the following changes were made:

* The bx dependency was removed.
* The design of the style attributes was changed and it now works.
* The parser was made more robust. Most valid SVG files are now parsed without errors, even though unknown attributes and elements are skipped.
* Added support for more attributes and elements.
* Additions to the API.

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
	- `examples/example_02_arcs_to_bezier/main.cpp`

## Examples

Read the examples to see the library in action!

### [Example_01_round_trip](examples/example_01_round_trip/main.cpp)

Perform a round trip of parsing and writing a SVG file. Provide the initial SVG file as input:

```
$  example_01_round_trip.exe ./examples/svg_files/Ghostscript_Tiger.svg
Loading "./examples/svg_files/Ghostscript_Tiger.svg"...
    Nb of groups: 241
    Basic shapes:
        rect: 0; circle: 0; ellipse: 0; text: 0
        line: 0; polyline: 0 (0 points); polygon: 0 (0 points);
    Paths:
        closed: 227 (2246 nodes)
        open:   13 (37 nodes)
    The root element contains 1 shapes
    ShapesAttributes: nodes: 2; allocated: 241; free: 15
Closing ssvg::Image ./examples/svg_files/Ghostscript_Tiger.svg...
Converting "./examples/svg_files/Ghostscript_Tiger.svg" to "./examples/svg_files\round_trip_Ghostscript_Tiger.svg"...
Loading "./examples/svg_files/Ghostscript_Tiger.svg"...
Saved ssvg::Image to "./examples/svg_files\round_trip_Ghostscript_Tiger.svg"...
Closing ssvg::Image ./examples/svg_files/Ghostscript_Tiger.svg...
Loading "./examples/svg_files\round_trip_Ghostscript_Tiger.svg"...
    Nb of groups: 241
    Basic shapes:
        rect: 0; circle: 0; ellipse: 0; text: 0
        line: 0; polyline: 0 (0 points); polygon: 0 (0 points);
    Paths:
        closed: 227 (2246 nodes)
        open:   13 (37 nodes)
    The root element contains 1 shapes
    ShapesAttributes: nodes: 2; allocated: 241; free: 15
Closing ssvg::Image ./examples/svg_files\round_trip_Ghostscript_Tiger.svg...
```

### [Example_02_build_svg](examples/example_02_build_svg/main.cpp)

Build a SVG image programmatically and save it to file `test_output.svg`

```
$ example_02_build_svg.exe
Building "./test_output.svg"
```

## License

[![License](http://img.shields.io/:license-BSD_2_Clause-blue.svg?style=flat-square)](./LICENSE)

## Contributions

This project does not accept pull requests at the moment. This repo is open source but closed to contributions.

If you identify SVG files that do not parse correctly please submit an issue.
