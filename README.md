# darctool

A tool for extracting/creating darc file.

## History

- v1.2.0 @ 2022.09.03 - Add option

### v1.1

- v1.1.0 @ 2018.01.03 - A new beginning
- v1.1.1 @ 2018.07.27 - Update cmake

### v1.0

- v1.0.0 @ 2015.03.02 - First release
- v1.0.1 @ 2017.06.16 - Refactoring

## Platforms

- Windows
- Linux
- macOS

## Building

### Dependencies

- cmake
- libiconv

### Compiling

- make 64-bit version

~~~Shell
mkdir build
cd build
cmake ..
make
~~~

- make 32-bit version

~~~Shell
mkdir build
cd build
cmake -DBUILD64=OFF ..
make
~~~

### Installing

~~~Shell
make install
~~~

## Usage

~~~Shell
darctool [option...] [option]...
~~~

## Options

See `darctool --help` messages.
