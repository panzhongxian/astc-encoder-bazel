#include "astc_pdr.h"

#include <cassert>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "astcenc.h"
#include "astcenccli_internal.h"

/* ============================================================================
        Data structure definitions
============================================================================ */

typedef unsigned int astcenc_operation;

struct mode_entry {
  const char* opt;
  astcenc_profile decode_mode;
};

/* ============================================================================
        Constants and literals
============================================================================ */

/** @brief Stage bit indicating we need to load a compressed image. */
static const unsigned int ASTCENC_STAGE_LD_COMP = 1 << 0;

/** @brief Stage bit indicating we need to store a compressed image. */
static const unsigned int ASTCENC_STAGE_ST_COMP = 1 << 1;

/** @brief Stage bit indicating we need to load an uncompressed image. */
static const unsigned int ASTCENC_STAGE_LD_NCOMP = 1 << 2;

/** @brief Stage bit indicating we need to store an uncompressed image. */
static const unsigned int ASTCENC_STAGE_ST_NCOMP = 1 << 3;

/** @brief Stage bit indicating we need compress an image. */
static const unsigned int ASTCENC_STAGE_COMPRESS = 1 << 4;

/** @brief Stage bit indicating we need to decompress an image. */
static const unsigned int ASTCENC_STAGE_DECOMPRESS = 1 << 5;

/** @brief Stage bit indicating we need to compare an image with the original
 * input. */
static const unsigned int ASTCENC_STAGE_COMPARE = 1 << 6;

/** @brief Operation indicating an unknown request (should never happen). */
static const astcenc_operation ASTCENC_OP_UNKNOWN = 0;

/** @brief Operation indicating the user wants to print long-form help text and
 * version info. */
static const astcenc_operation ASTCENC_OP_HELP = 1 << 7;

/** @brief Operation indicating the user wants to print short-form help text and
 * version info. */
static const astcenc_operation ASTCENC_OP_VERSION = 1 << 8;

/** @brief Operation indicating the user wants to compress and store an image.
 */
static const astcenc_operation ASTCENC_OP_COMPRESS =
    ASTCENC_STAGE_LD_NCOMP | ASTCENC_STAGE_COMPRESS | ASTCENC_STAGE_ST_COMP;

/** @brief Operation indicating the user wants to decompress and store an image.
 */
static const astcenc_operation ASTCENC_OP_DECOMPRESS =
    ASTCENC_STAGE_LD_COMP | ASTCENC_STAGE_DECOMPRESS | ASTCENC_STAGE_ST_NCOMP;

/** @brief Operation indicating the user wants to test a compression setting on
 * an image. */
static const astcenc_operation ASTCENC_OP_TEST =
    ASTCENC_STAGE_LD_NCOMP | ASTCENC_STAGE_COMPRESS | ASTCENC_STAGE_DECOMPRESS |
    ASTCENC_STAGE_COMPARE | ASTCENC_STAGE_ST_NCOMP;

/** @brief Decode table for command line operation modes. */
static const mode_entry modes[]{{"l", ASTCENC_PRF_LDR},
                                {"s", ASTCENC_PRF_LDR_SRGB},
                                {"h", ASTCENC_PRF_HDR_RGB_LDR_A},
                                {"H", ASTCENC_PRF_HDR}};

/**
 * @brief Compression workload definition for worker threads.
 */
struct compression_workload {
  astcenc_context* context;
  astcenc_image* image;
  astcenc_swizzle swizzle;
  uint8_t* data_out;
  size_t data_len;
  astcenc_error error;
};

/**
 * @brief Decompression workload definition for worker threads.
 */
struct decompression_workload {
  astcenc_context* context;
  uint8_t* data;
  size_t data_len;
  astcenc_image* image_out;
  astcenc_swizzle swizzle;
  astcenc_error error;
};

/**
 * @brief Test if a string argument is a well formed float.
 */
static bool is_float(std::string target) {
  float test;
  std::istringstream stream(target);

  // Leading whitespace is an error
  stream >> std::noskipws >> test;

  // Ensure entire no remaining string in addition to parse failure
  return stream.eof() && !stream.fail();
}

/**
 * @brief Test if a string ends with a given suffix.
 */
static bool ends_with(const std::string& str, const std::string& suffix) {
  return (str.size() >= suffix.size()) &&
         (0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix));
}

/**
 * @brief Runner callback function for a compression worker thread.
 *
 * @param thread_count   The number of threads in the worker pool.
 * @param thread_id      The index of this thread in the worker pool.
 * @param payload        The parameters for this thread.
 */
static void compression_workload_runner(int thread_count, int thread_id,
                                        void* payload) {
  (void)thread_count;

  compression_workload* work = static_cast<compression_workload*>(payload);
  astcenc_error error =
      astcenc_compress_image(work->context, work->image, &work->swizzle,
                             work->data_out, work->data_len, thread_id);

  // This is a racy update, so which error gets returned is a random, but it
  // will reliably report an error if an error occurs
  if (error != ASTCENC_SUCCESS) {
    work->error = error;
  }
}

/**
 * @brief Runner callback function for a decompression worker thread.
 *
 * @param thread_count   The number of threads in the worker pool.
 * @param thread_id      The index of this thread in the worker pool.
 * @param payload        The parameters for this thread.
 */
static void decompression_workload_runner(int thread_count, int thread_id,
                                          void* payload) {
  (void)thread_count;

  decompression_workload* work = static_cast<decompression_workload*>(payload);
  astcenc_error error =
      astcenc_decompress_image(work->context, work->data, work->data_len,
                               work->image_out, &work->swizzle, thread_id);

  // This is a racy update, so which error gets returned is a random, but it
  // will reliably report an error if an error occurs
  if (error != ASTCENC_SUCCESS) {
    work->error = error;
  }
}

/**
 * @brief Utility to generate a slice file name from a pattern.
 *
 * Convert "foo/bar.png" in to "foo/bar_<slice>.png"
 *
 * @param basename The base pattern; must contain a file extension.
 * @param index    The slice index.
 * @param error    Set to true on success, false on error (no extension found).
 *
 * @return The slice file name.
 */
static std::string get_slice_filename(const std::string& basename,
                                      unsigned int index, bool& error) {
  size_t sep = basename.find_last_of(".");
  if (sep == std::string::npos) {
    error = true;
    return "";
  }

  std::string base = basename.substr(0, sep);
  std::string ext = basename.substr(sep);
  std::string name = base + "_" + std::to_string(index) + ext;
  error = false;
  return name;
}

/**
 * @brief Load a non-astc image file from memory.
 *
 * @param filename            The file to load, or a pattern for array loads.
 * @param dim_z               The number of slices to load.
 * @param y_flip              Should this image be Y flipped?
 * @param[out] is_hdr         Is the loaded image HDR?
 * @param[out] component_count The number of components in the loaded image.
 *
 * @return The astc image file, or nullptr on error.
 */
static astcenc_image* load_uncomp_file(const char* filename, unsigned int dim_z,
                                       bool y_flip, bool& is_hdr,
                                       unsigned int& component_count) {
  astcenc_image* image = nullptr;

  // For a 2D image just load the image directly
  if (dim_z == 1) {
    image = load_ncimage(filename, y_flip, is_hdr, component_count);
  } else {
    bool slice_is_hdr;
    unsigned int slice_component_count;
    astcenc_image* slice = nullptr;
    std::vector<astcenc_image*> slices;

    // For a 3D image load an array of slices
    for (unsigned int image_index = 0; image_index < dim_z; image_index++) {
      bool error;
      std::string slice_name = get_slice_filename(filename, image_index, error);
      if (error) {
        printf("ERROR: Image pattern does not contain file extension: %s\n",
               filename);
        break;
      }

      slice = load_ncimage(slice_name.c_str(), y_flip, slice_is_hdr,
                           slice_component_count);
      if (!slice) {
        break;
      }

      slices.push_back(slice);

      // Check it is not a 3D image
      if (slice->dim_z != 1) {
        printf("ERROR: Image arrays do not support 3D sources: %s\n",
               slice_name.c_str());
        break;
      }

      // Check slices are consistent with each other
      if (image_index != 0) {
        if ((is_hdr != slice_is_hdr) ||
            (component_count != slice_component_count)) {
          printf("ERROR: Image array[0] and [%d] are different formats\n",
                 image_index);
          break;
        }

        if ((slices[0]->dim_x != slice->dim_x) ||
            (slices[0]->dim_y != slice->dim_y) ||
            (slices[0]->dim_z != slice->dim_z)) {
          printf("ERROR: Image array[0] and [%d] are different dimensions\n",
                 image_index);
          break;
        }
      } else {
        is_hdr = slice_is_hdr;
        component_count = slice_component_count;
      }
    }

    // If all slices loaded correctly then repack them into a single image
    if (slices.size() == dim_z) {
      unsigned int dim_x = slices[0]->dim_x;
      unsigned int dim_y = slices[0]->dim_y;
      int bitness = is_hdr ? 16 : 8;
      int slice_size = dim_x * dim_y;

      image = alloc_image(bitness, dim_x, dim_y, dim_z);

      // Combine 2D source images into one 3D image
      for (unsigned int z = 0; z < dim_z; z++) {
        if (image->data_type == ASTCENC_TYPE_U8) {
          uint8_t* data8 = static_cast<uint8_t*>(image->data[z]);
          uint8_t* data8src = static_cast<uint8_t*>(slices[z]->data[0]);
          size_t copy_size = slice_size * 4 * sizeof(uint8_t);
          memcpy(data8, data8src, copy_size);
        } else if (image->data_type == ASTCENC_TYPE_F16) {
          uint16_t* data16 = static_cast<uint16_t*>(image->data[z]);
          uint16_t* data16src = static_cast<uint16_t*>(slices[z]->data[0]);
          size_t copy_size = slice_size * 4 * sizeof(uint16_t);
          memcpy(data16, data16src, copy_size);
        } else  // if (image->data_type == ASTCENC_TYPE_F32)
        {
          assert(image->data_type == ASTCENC_TYPE_F32);
          float* data32 = static_cast<float*>(image->data[z]);
          float* data32src = static_cast<float*>(slices[z]->data[0]);
          size_t copy_size = slice_size * 4 * sizeof(float);
          memcpy(data32, data32src, copy_size);
        }
      }
    }

    for (auto& i : slices) {
      free_image(i);
    }
  }

  return image;
}

/**
 * @brief Initialize the astcenc_config
 *
 * @param      operation    Codec operation mode.
 * @param[out] profile      Codec color profile.
 * @param      comp_image   Compressed image if a decompress operation.
 * @param[out] config       Codec configuration.
 *
 * @return 0 if everything is okay, 1 if there is some error
 */
static int init_astcenc_config(std::string dimensions_str,
                               std::string quality_str, astcenc_profile profile,
                               astcenc_operation operation,
                               astc_compressed_image& comp_image,
                               astcenc_config& config) {
  unsigned int block_x = 0;
  unsigned int block_y = 0;
  unsigned int block_z = 1;

  // For decode the block size is set by the incoming image.
  if (operation == ASTCENC_OP_DECOMPRESS) {
    block_x = comp_image.block_x;
    block_y = comp_image.block_y;
    block_z = comp_image.block_z;
  }

  float quality = 0.0f;

  // parse the command line's encoding options.
  if (operation & ASTCENC_STAGE_COMPRESS) {
    int cnt2D, cnt3D;
    int dimensions = sscanf(dimensions_str.c_str(), "%ux%u%nx%u%n", &block_x,
                            &block_y, &cnt2D, &block_z, &cnt3D);
    // Character after the last match should be a NUL
    if (!(((dimensions == 2) && !dimensions_str[cnt2D]) ||
          ((dimensions == 3) && !dimensions_str[cnt3D]))) {
      printf("ERROR: Block size '%s' is invalid, cnt2D: %d\n",
             dimensions_str.c_str(), cnt2D);
      return 1;
    }

    // Read and decode search quality
    if (!strcmp(quality_str.c_str(), "fastest")) {
      quality = ASTCENC_PRE_FASTEST;
    } else if (!strcmp(quality_str.c_str(), "fast")) {
      quality = ASTCENC_PRE_FAST;
    } else if (!strcmp(quality_str.c_str(), "medium")) {
      quality = ASTCENC_PRE_MEDIUM;
    } else if (!strcmp(quality_str.c_str(), "thorough")) {
      quality = ASTCENC_PRE_THOROUGH;
    } else if (!strcmp(quality_str.c_str(), "exhaustive")) {
      quality = ASTCENC_PRE_EXHAUSTIVE;
    } else if (is_float(quality_str.c_str())) {
      quality = static_cast<float>(atof(quality_str.c_str()));
    } else {
      printf("ERROR: Search quality/preset '%s' is invalid\n",
             quality_str.c_str());
      return 1;
    }
  }

  unsigned int flags = 0;

#if defined(ASTCENC_DECOMPRESS_ONLY)
  flags |= ASTCENC_FLG_DECOMPRESS_ONLY;
#else
  // Decompression can skip some memory allocation, but need full tables
  if (operation == ASTCENC_OP_DECOMPRESS) {
    flags |= ASTCENC_FLG_DECOMPRESS_ONLY;
  }
  // Compression and test passes can skip some decimation initialization
  // as we know we are decompressing images that were compressed using the
  // same settings and heuristics ...
  else {
    flags |= ASTCENC_FLG_SELF_DECOMPRESS_ONLY;
  }
#endif

  astcenc_error status = astcenc_config_init(profile, block_x, block_y, block_z,
                                             quality, flags, &config);
  if (status == ASTCENC_ERR_BAD_BLOCK_SIZE) {
    printf("ERROR: Block size '%s' is invalid\n", dimensions_str.c_str());
    return 1;
  } else if (status == ASTCENC_ERR_BAD_CPU_ISA) {
    printf("ERROR: Required SIMD ISA support missing on this CPU\n");
    return 1;
  } else if (status == ASTCENC_ERR_BAD_CPU_FLOAT) {
    printf("ERROR: astcenc must not be compiled with -ffast-math\n");
    return 1;
  } else if (status != ASTCENC_SUCCESS) {
    printf("ERROR: Init config failed with %s\n",
           astcenc_get_error_string(status));
    return 1;
  }

  return 0;
}

/**
 * @brief The main entry point.
 *
 *
 * @return 0 on success, non-zero otherwise.
 */

struct error_ret {
  int errno;
  std::string msg;
};

int astc_compress_and_compare(const std::string& profile_str,
                              const std::string& input_filename,
                              const std::string& compressed_output_filename,
                              const std::string& decompressed_output_filename,
                              const std::string dimensions_str,
                              const std::string quality_str) {
  astcenc_operation operation =
      ASTCENC_STAGE_LD_NCOMP | ASTCENC_STAGE_ST_COMP | ASTCENC_STAGE_ST_NCOMP |
      ASTCENC_STAGE_COMPRESS | ASTCENC_STAGE_DECOMPRESS;

  astcenc_profile profile = ASTCENC_PRF_LDR_SRGB;
  int modes_count = sizeof(modes) / sizeof(modes[0]);
  for (int i = 0; i < modes_count; i++) {
    if (!strcmp(modes[i].opt, profile_str.c_str())) {
      profile = modes[i].decode_mode;
      break;
    }
  }

  int error;

  if (input_filename.empty()) {
    printf("ERROR: Input file not specified\n");
    return 1;
  }

  if (compressed_output_filename.empty()) {
    printf("ERROR: Compressed file not specified\n");
    return 1;
  }

  if (decompressed_output_filename.empty()) {
    printf("ERROR: Decompressed file not specified\n");
    return 1;
  }

  // This has to come first, as the block size is in the file header
  astc_compressed_image image_comp{};

  astcenc_config config{};
  error = init_astcenc_config(dimensions_str, quality_str, profile, operation,
                              image_comp, config);
  if (error) {
    return 1;
  }

  // Initialize cli_config_options with default values
  cli_config_options cli_config{
      0,
      1,
      false,
      false,
      -10,
      10,
      {ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A},
      {ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A}};

  cli_config.silentmode = 1;
  cli_config.thread_count = 1;
  cli_config.thread_count = get_cpu_count();

  astcenc_image* image_uncomp_in = nullptr;
  unsigned int image_uncomp_in_component_count = 0;
  bool image_uncomp_in_is_hdr = false;
  astcenc_image* image_decomp_out = nullptr;

  // TODO: Handle RAII resources so they get freed when out of scope
  astcenc_error codec_status;
  astcenc_context* codec_context;

  // 1. 加载未压缩的图片文件
  image_uncomp_in = load_uncomp_file(
      input_filename.c_str(), cli_config.array_size, cli_config.y_flip,
      image_uncomp_in_is_hdr, image_uncomp_in_component_count);
  if (!image_uncomp_in) {
    printf("ERROR: Failed to load uncompressed image file\n");
    return 1;
  }

  codec_status =
      astcenc_context_alloc(&config, cli_config.thread_count, &codec_context);
  if (codec_status != ASTCENC_SUCCESS) {
    printf("ERROR: Codec context alloc failed: %s\n",
           astcenc_get_error_string(codec_status));
    return 1;
  }

  double image_size = 0.0;
  if (image_uncomp_in) {
    image_size = (double)image_uncomp_in->dim_x *
                 (double)image_uncomp_in->dim_y *
                 (double)image_uncomp_in->dim_z;
  } else {
    image_size = (double)image_comp.dim_x * (double)image_comp.dim_y *
                 (double)image_comp.dim_z;
  }

  // 2. 压缩文件 Compress an image
  {
    unsigned int blocks_x =
        (image_uncomp_in->dim_x + config.block_x - 1) / config.block_x;
    unsigned int blocks_y =
        (image_uncomp_in->dim_y + config.block_y - 1) / config.block_y;
    unsigned int blocks_z =
        (image_uncomp_in->dim_z + config.block_z - 1) / config.block_z;
    size_t buffer_size = blocks_x * blocks_y * blocks_z * 16;
    uint8_t* buffer = new uint8_t[buffer_size];

    compression_workload work;
    work.context = codec_context;
    work.image = image_uncomp_in;
    work.swizzle = cli_config.swz_encode;
    work.data_out = buffer;
    work.data_len = buffer_size;
    work.error = ASTCENC_SUCCESS;

    // Only launch worker threads for multi-threaded use - it makes basic
    // single-threaded profiling and debugging a little less convoluted
    if (cli_config.thread_count > 1) {
      launch_threads(cli_config.thread_count, compression_workload_runner,
                     &work);
    } else {
      work.error =
          astcenc_compress_image(work.context, work.image, &work.swizzle,
                                 work.data_out, work.data_len, 0);
    }

    if (work.error != ASTCENC_SUCCESS) {
      printf("ERROR: Codec compress failed: %s\n",
             astcenc_get_error_string(work.error));
      return 1;
    }

    image_comp.block_x = config.block_x;
    image_comp.block_y = config.block_y;
    image_comp.block_z = config.block_z;
    image_comp.dim_x = image_uncomp_in->dim_x;
    image_comp.dim_y = image_uncomp_in->dim_y;
    image_comp.dim_z = image_uncomp_in->dim_z;
    image_comp.data = buffer;
    image_comp.data_len = buffer_size;
  }

  // 3. 解压缩图片 Decompress an image
  {
    int out_bitness = get_output_filename_enforced_bitness(
        decompressed_output_filename.c_str());
    if (out_bitness == 0) {
      bool is_hdr = (config.profile == ASTCENC_PRF_HDR) ||
                    (config.profile == ASTCENC_PRF_HDR_RGB_LDR_A);
      out_bitness = is_hdr ? 16 : 8;
    }

    image_decomp_out = alloc_image(out_bitness, image_comp.dim_x,
                                   image_comp.dim_y, image_comp.dim_z);

    decompression_workload work;
    work.context = codec_context;
    work.data = image_comp.data;
    work.data_len = image_comp.data_len;
    work.image_out = image_decomp_out;
    work.swizzle = cli_config.swz_decode;
    work.error = ASTCENC_SUCCESS;

    // Only launch worker threads for multi-threaded use - it makes basic
    // single-threaded profiling and debugging a little less convoluted
    if (cli_config.thread_count > 1) {
      launch_threads(cli_config.thread_count, decompression_workload_runner,
                     &work);
    } else {
      work.error =
          astcenc_decompress_image(work.context, work.data, work.data_len,
                                   work.image_out, &work.swizzle, 0);
    }

    if (work.error != ASTCENC_SUCCESS) {
      printf("ERROR: Codec decompress failed: %s\n",
             astcenc_get_error_string(codec_status));
      return 1;
    }
  }

  // Print metrics in comparison mode
  if (false) {
    compute_error_metrics(image_uncomp_in_is_hdr,
                          image_uncomp_in_component_count, image_uncomp_in,
                          image_decomp_out, cli_config.low_fstop,
                          cli_config.high_fstop);
  }

  // Store compressed image
  {
    if (ends_with(compressed_output_filename, ".astc")) {
      error = store_cimage(image_comp, compressed_output_filename.c_str());
      if (error) {
        printf("ERROR: Failed to store compressed image\n");
        return 1;
      }
    } else if (ends_with(compressed_output_filename, ".ktx")) {
      bool srgb = profile == ASTCENC_PRF_LDR_SRGB;
      error = store_ktx_compressed_image(
          image_comp, compressed_output_filename.c_str(), srgb);
      if (error) {
        printf("ERROR: Failed to store compressed image\n");
        return 1;
      }
    } else {
      printf("ERROR: Unknown compressed output file type\n");
      return 1;
    }
  }

  // Store decompressed image
  {
    bool store_result =
        store_ncimage(image_decomp_out, decompressed_output_filename.c_str(),
                      cli_config.y_flip);
    if (!store_result) {
      printf("ERROR: Failed to write output image %s\n",
             decompressed_output_filename.c_str());
      return 1;
    }
  }

  free_image(image_uncomp_in);
  free_image(image_decomp_out);
  astcenc_context_free(codec_context);

  delete[] image_comp.data;
  return 0;
}

int c_astc_compress_and_compare(const char* profile_str,
                                const char* input_filename,
                                const char* compressed_output_filename,
                                const char* decompressed_output_filename,
                                const char* dimensions_str,
                                const char* quality_str) {
  return astc_compress_and_compare(
      std::string(profile_str), std::string(input_filename),
      std::string(compressed_output_filename),
      std::string(decompressed_output_filename), std::string(dimensions_str),
      std::string(quality_str));
}
