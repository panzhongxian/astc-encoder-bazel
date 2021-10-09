#include "src/astc_wrapper.h"

#include <unistd.h>

#include <iostream>

int main() {
  char tmp[256];
  getcwd(tmp, 256);
  std::cout << "Current working directory: " << tmp << std::endl;

  std::string input_filename = "images/example.png";
  std::string compressed_output_filename = "example.astc";
  std::string decompressed_output_filename = "example.tga";
  std::string dimensions_str = "6x6";
  std::string quality_str = "medium";
  // astc_compress_and_compare("H", input_filename, compressed_output_filename,
  //                           decompressed_output_filename, dimensions_str,
  //                           quality_str);
  c_astc_compress_and_compare(
      "H", input_filename.c_str(), compressed_output_filename.c_str(),
      decompressed_output_filename.c_str(), "8x8", quality_str.c_str());
}
