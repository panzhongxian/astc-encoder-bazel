#include <string>

int astc_compress_and_compare(const std::string& profile_str,
                              const std::string& input_filename,
                              const std::string& compressed_output_filename,
                              const std::string& decompressed_output_filename,
                              const std::string dimensions_str,
                              const std::string quality_str);
