#include "astc_pdr.h"

int main() {
  std::string input_filename = "example.png";
  std::string compressed_output_filename = "example.astc";
  std::string decompressed_output_filename = "example.tga";
  std::string dimensions_str = "6x6";
  std::string quality_str = "medium";
  astc_compress_and_compare("l", input_filename, compressed_output_filename,
                            decompressed_output_filename, dimensions_str,
                            quality_str);
}
