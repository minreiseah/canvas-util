# Canvas Util

## Description

Linux tool to download all course files and videos into their respective directory trees.

## Dependencies

- [libcurl](https://unix.stackexchange.com/questions/452515/how-to-install-libcurl)
- [jsoncpp](https://github.com/open-source-parsers/jsoncpp)
- g++

## Usage

1. Git clone.
```bash
> git clone https://github.com/minreiseah/canvas-util.git
```

2. Rename `sample.defines.h` to `defines.h`

3. Set `#define` directives in `defines.h`
```cpp
#define DEFAULTROOTPATH "course files"
#define TERMID -1 // semester ID; leave as -1 if unsure
#define BASEURL "http://canvas.nus.edu.sg/api/v1" // API URI
#define TARGETDIR "/home/minrei/nus/y1s2/"
#define ACCESS_TOKEN // obtained from canvas > settings > generate_token
#define ASPXAUTH // obtained from dev tools > application > cookies
```

4. Set courses map under `downloadVideos.cpp`
```cpp
    {"cs2030s", "5a3de09c-6d53-4434-80a1-af8200ddb1da"},
    {"course", ""},
    {"course", ""}
```
To get the courseID, go to canvas > Videos/Panopto. Open chrome dev tools > Network > Filter 'GetFolderInfo'.

![](getFolderInfo.png)


5. Compile & execute.
```bash
> make run
```

If there are issues, re-update `defines.h`. If it still doesn't work, try using postman.

## Features to add

- On download, save file info (name, id, lastUpdated) to log.
- On next download, if no change to file info, do not download.
    - should be toggleable
