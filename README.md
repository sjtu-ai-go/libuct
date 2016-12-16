# libuct
[![Build Status](https://travis-ci.org/sjtu-ai-go/libuct.svg)](https://travis-ci.org/sjtu-ai-go/libuct)
[![GNU3 License](https://img.shields.io/github/license/sjtu-ai-go/libuct.svg)](https://github.com/sjtu-ai-go/libuct/blob/master/LICENSE)

An extensible MCTS algorithm library (UCT included) (WIP).


## Usage
```
git submodule add {{repo_url}} vendor/libuct
git submodule update --recursive --init
```
Then, in `CMakeLists.txt`:
```
add_subdirectory(vendor/libuct)
include_directories(${libuct_INCLUDE_DIR})

# After add_executable(your_prog)
target_link_libraries(your_prog fastrollout)
```

Enable test with `libuct_build_tests`, default `OFF`.

NEW: uctTest will dump a `uct_test.dot` now. Convert it to image by `dot -Ttiff -O uct_test.dot`!
