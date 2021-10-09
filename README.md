# astc-encoder extension

This is an extensions set for Python, PHP and maybe more other languages to use [astc-encoder](https://github.com/ARM-software/astc-encoder)(the official repository for the Arm® Adaptive Scalable Texture Compression (ASTC) Encoder). I know little about CMake, so I choose Bazel to build the astc-encoder shared library.

## Usage


### Python

Build all libraries:

```bash
bazel build //...
```

Install python module:

```bash
cd python
python3 setup.py install
```

Invoke astc in .py file:

```python
import astc

astc.astc_compress_and_compare(color_profile, \
        uncompressed_file_path, \
        compressed_file_path, \
        decompressed_file_path, \
        block, \
        quality)
```

### PHP

## TODO List

- [ ] Check CPU instruction set extension automatically.
- [ ] Add PHP extension.

