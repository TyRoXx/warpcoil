# warpcoil

## goals
* create network APIs with little effort
* be as type safe as C++
* don't waste developer time hunting down unnecessary encoding-related issues
* don't waste bandwidth or CPU time for no reason
* support many languages, not only C++
* no coupling to a specific transport protocol
  * support the most common ones by default though: TCP, TLS, Websocket, TLS Websocket
* UTF-8 everywhere

## non-goals
* "human-readability"
* support for inheritance or similar nonsense
* support for ancient compilers, standards or Boost versions
* reinventing Boost.Asio, TLS, authentication or Websockets

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
| message type | 1 byte unsigned with value 0 |
| request ID | 8 byte unsigned |
| method | UTF-8, length 0-255 |
| argument |  the parameters of the method being called |

## server to client
| part of the message | type |
| --- | --- |
| message type (planned new field) | 1 byte unsigned with value 1 |
| request ID | 8 byte unsigned |
| result |  the result of the method being called |
