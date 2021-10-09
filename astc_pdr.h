#include <string>

int astc_compress_and_compare(const std::string& profile_str,
                              const std::string& input_filename,
                              const std::string& compressed_output_filename,
                              const std::string& decompressed_output_filename,
                              const std::string dimensions_str,
                              const std::string quality_str);

extern "C" {
int c_astc_compress_and_compare(const char* profile_str,
                                const char* input_filename,
                                const char* compressed_output_filename,
                                const char* decompressed_output_filename,
                                const char* dimensions_str,
                                const char* quality_str);
}
