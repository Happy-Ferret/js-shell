# JS shell

A standalone executable for developing and testing embedded [SpiderMonkey](https://developer.mozilla.org/en-US/docs/Mozilla/Projects/SpiderMonkey).

## Example usage

```bash
$ git clone https://github.com/bkircher/js-shell.git
$ cd js-shell
$ cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON -DENABLE_JS_DEBUG=OFF .
$ make
$ ./run test.js
```
