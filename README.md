# darctool

A tool for extracting/creating darc file.

## History

- v1.1.0 @ 2018.01.03 - A new beginning

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
~~~
mkdir project
cd project
cmake ..
make
~~~

- make 32-bit version
~~~
mkdir project
cd project
cmake -DBUILD64=OFF ..
make
~~~

### Installing

~~~
make install
~~~

## Usage

~~~
darctool [option...] [option]...
~~~

## Options

See `darctool --help` messages.
