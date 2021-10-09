#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdio.h>

#include "src/astc_wrapper.h"

static PyObject *astc_compress_and_compare(PyObject *self, PyObject *args) {
  const char *color_profile;
  const char *uncompressed_filename;
  const char *compressed_filename;
  const char *decompressed_filename;
  const char *block;
  const char *quality;
  int ok;
  ok = PyArg_ParseTuple(args, "ssssss", &color_profile, &uncompressed_filename,
                        &compressed_filename, &decompressed_filename, &block,
                        &quality);

  if (!ok) {
    printf("!ok\n");
    return NULL;
  }
  // c_astc_compress_and_compare("H", "example.png", "example.astc",
  // "example.tga", "6x6", "medium");
  c_astc_compress_and_compare(color_profile, uncompressed_filename,
                              compressed_filename, decompressed_filename, block,
                              quality);
  Py_INCREF(Py_None);
  return Py_None;
}

static PyMethodDef SpamMethods[] = {
    {"astc_compress_and_compare", astc_compress_and_compare, METH_VARARGS,
     "Execute a shell command."},
    {NULL, NULL, 0, NULL} /* Sentinel */
};

static struct PyModuleDef astcmodule = {
    PyModuleDef_HEAD_INIT, "astc", /* name of module */
    NULL,                          /* module documentation, may be NULL */
    -1, /* size of per-interpreter state of the module,
                                            or -1 if the module keeps state in
           global variables. */
    SpamMethods};

PyMODINIT_FUNC PyInit_astc(void) { return PyModule_Create(&astcmodule); }

int main(int argc, char *argv[]) {
  wchar_t *program = Py_DecodeLocale(argv[0], NULL);
  if (program == NULL) {
    fprintf(stderr, "Fatal error: cannot decode argv[0]\n");
    exit(1);
  }

  /* Add a built-in module, before Py_Initialize */
  if (PyImport_AppendInittab("astc", PyInit_astc) == -1) {
    fprintf(stderr, "Error: could not extend in-built modules table\n");
    exit(1);
  }

  /* Pass argv[0] to the Python interpreter */
  Py_SetProgramName(program);

  /* Initialize the Python interpreter.  Required.
   *        If this step fails, it will be a fatal error. */
  Py_Initialize();

  /* Optionally import the module; alternatively,
   *        import can be deferred until the embedded script
   *               imports it. */
  PyObject *pmodule = PyImport_ImportModule("astc");
  if (!pmodule) {
    PyErr_Print();
    fprintf(stderr, "Error: could not import module 'astc'\n");
  }

  PyMem_RawFree(program);
  return 0;
}
