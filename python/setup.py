from distutils.core import setup, Extension

module1 = Extension('astc',
                    define_macros=[('NDEBUG', '1'), ('MINOR_VERSION', '0')],
                    include_dirs=['..'],
                    libraries=['astc-encoder', 'astc_wrapper'],
                    library_dirs=[
                        '../bazel-bin/src/',
                        '../bazel-bin/external/astc-encoder/',
                    ],
                    sources=['astcmodule.c'])

setup(name='PackageName',
      version='1.0',
      description='This is a demo package',
      ext_modules=[module1])
