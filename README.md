# Canvas Util

## Description

Linux tool to download all course files into their respective directory trees.

## Dependencies

- [libcurl](https://unix.stackexchange.com/questions/452515/how-to-install-libcurl)
- [jsoncpp](https://github.com/open-source-parsers/jsoncpp)
- g++

## Usage

1. Rename `sample.defines.h` to `defines.h`

2. Set `#define` directives in `defines.h`
```cpp
#define DEFAULTROOTPATH "course files"
#define TERMID -1 // semester ID; leave as -1 if unsure
#define BASEURL "http://canvas.nus.edu.sg/api/v1" // API URI
#define TARGETDIR "/home/minrei/nus/y1s2/"
#define ACCESS_TOKEN // obtained from canvas > settings > generate_token
```

2. Compile & execute.
```
> make run
```
