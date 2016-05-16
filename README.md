# testing
[![Build Status](https://travis-ci.org/TyRoXx/warpcoil.svg?branch=master)](https://travis-ci.org/TyRoXx/warpcoil)
[![Build status](https://ci.appveyor.com/api/projects/status/tmygcx40pj2cupkg/branch/master?svg=true)](https://ci.appveyor.com/project/TyRoXx/warpcoil/branch/master)
[![codecov](https://codecov.io/gh/TyRoXx/warpcoil/branch/master/graph/badge.svg)](https://codecov.io/gh/TyRoXx/warpcoil)

# dependencies

## C++
Install the required C++ libraries with the Conan package manager (https://conan.io).

## clang-format
The C++ code is formatted automatically with clang-format if CMake can find the `clang-format` executable on your system.
For consistent formatting use version 3.7 (http://llvm.org/releases/download.html#3.7.1).
You can disable formatting with the CMake option `WARPCOIL_USE_CLANG_FORMAT`.

# wire protocol

## basic types
| name | parameters | encoding | remarks |
| --- | --- | --- | --- |
| integer | minimum, maximum  | big endian, as many bytes as necessary for maximum | i. e. 1 byte for 255, 2 bytes for 256 |
| UTF-8 | length (integer) | actual length followed by the usual UTF-8 code units | length is the number of code units. Has to be valid and complete UTF-8 |
| tuple | element types | elements in order | |
| variant | element types | actual element index encoded as an integer followed by the actual element | |
| vector | length (integer) | length followed by the elements in order | |

## client to server
| part of the message | type |
| --- | --- |
| request ID | 8 byte unsigned |
| method | UTF-8, length 0-255 |
| argument |  the parameters of the method being called |

## server to client
| part of the message | type |
| --- | --- |
| request ID | 8 byte unsigned |
| result |  the result of the method being called |
