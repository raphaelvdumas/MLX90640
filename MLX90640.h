/* MIT License
 *
 * Copyright (c) 2026 Raphael Dumas
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef MLX90640_H
#define MLX90640_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#ifndef MLX90640_USE_DOUBLE_PRECISION
#define MLX90640_USE_DOUBLE_PRECISION 0
#endif

#if (MLX90640_USE_DOUBLE_PRECISION != 0) && (MLX90640_USE_DOUBLE_PRECISION != 1)
#error                                                                         \
    "MLX90640: precision is invalid. Set MLX90640_USE_DOUBLE_PRECISION to 0 (default) or 1."
#endif

#ifndef MLX90640_USE_MATH_H
#define MLX90640_USE_MATH_H 1
#endif

#if (MLX90640_USE_MATH_H != 0) && (MLX90640_USE_MATH_H != 1)
#error                                                                         \
    "MLX90640: math include is invalid. Set MLX90640_USE_MATH_H to 0 (default) or 1."
#endif

#ifndef MLX90640_TGC_AVAILABLE
#define MLX90640_TGC_AVAILABLE 1
#endif

#if (MLX90640_TGC_AVAILABLE != 0) && (MLX90640_TGC_AVAILABLE != 1)
#error                                                                         \
    "MLX90640: thermal gain compensation (TGC) is invalid. Set MLX90640_TGC_AVAILABLE to 0 or 1 (default)."
#endif

#if (MLX90640_USE_DOUBLE_PRECISION) && (MLX90640_USE_MATH_H == 0)
#error                                                                         \
    "MLX90640: 'math.h' is required for double precision. Set MLX90640_USE_MATH_H to 1."
#endif

#if MLX90640_USE_DOUBLE_PRECISION
#include <math.h>
#define MLX90640_REAL_BYTES 8
typedef double mlx90640_real_t;
typedef uint64_t mlx90640_uint_t;
#define MLX90640_REAL_EXP_OFFSET UINT64_C(52)
#define MLX90640_UINT_MAGIC UINT64_C(0)
#define MLX90640_UINT_NAN UINT64_C(0x7ff8000000000000)
#define MLX90640_SQRT4(x) sqrt(sqrt((x)))
#define MLX90640_SQRT4_FAST(x) sqrt(sqrt((x)))
#else
#define MLX90640_REAL_BYTES 4
typedef float mlx90640_real_t;
typedef uint32_t mlx90640_uint_t;
#define MLX90640_REAL_EXP_OFFSET UINT32_C(23)
#define MLX90640_UINT_MAGIC UINT32_C(0x2f9a8354)
// Since multiple values for NaNs exist, Numpy
// (np.nan) is chosen here:
// https://github.com/numpy/numpy/blob/b805fee901ad298089ddde31bfc1f7530d673cf1/numpy/_core/include/numpy/npy_math.h#L36
#define MLX90640_UINT_NAN UINT32_C(0x7fc00000)
#define MLX90640_SQRT4_FAST(x) __mlx90640_real_sqrt4_magic((x))
#if MLX90640_USE_MATH_H
#include <math.h>
#define MLX90640_SQRT4(x) sqrtf(sqrtf((x)))
#else
#define MLX90640_SQRT4(x) __mlx90640_real_sqrt4_nr((x))
#endif
#endif

#define MLX90640_NAN __mlx90640_real_nan()
#define MLX90640_ISNAN(x) __mlx90640_real_isnan((x))
#define MLX90640_WORD_BYTES UINT16_C(2)

/**
 * @defgroup BitCore Core
 * @brief My brief.
 *
 * Description.
 *
 * @{
 */
typedef union {
  mlx90640_real_t real;
  mlx90640_uint_t uint;
} mlx90640_real_uint_t;

/**
 * @defgroup BitReal Core for bit operations on real numbers.
 * @ingroup BitCore
 * @brief My brief.
 *
 * Description.
 *
 */
static inline mlx90640_real_t __mlx90640_real_nan() {
  mlx90640_real_uint_t u = {.uint = MLX90640_UINT_NAN};
  return u.real;
}

static inline mlx90640_real_t __mlx90640_real_isnan(mlx90640_real_t x) {
  return x != x;
}

static inline mlx90640_real_t __mlx90640_real_sqrt4_magic(mlx90640_real_t x) {

  mlx90640_real_uint_t u = {.real = x};
  u.uint = (u.uint >> 2u) + MLX90640_UINT_MAGIC;
  return u.real;
}

static inline mlx90640_real_t __mlx90640_real_sqrt4_nr(mlx90640_real_t x) {

  mlx90640_real_t y = __mlx90640_real_sqrt4_magic(x);

  return 0.25f * (3.0f * y + x / (y * y * y));
}

//@todo: clean this
static inline mlx90640_real_uint_t
__mlx90640_real_exp_add(mlx90640_real_uint_t x, const size_t scale) {
  size_t sign = scale >> 7u;
  // uint8_t abs_scale = (scale ^ sign) - sign;
  mlx90640_uint_t abs_scale = ((mlx90640_uint_t)((scale ^ sign) - sign))
                              << MLX90640_REAL_EXP_OFFSET;
  if (sign == 0) {
    x.uint += abs_scale;
  } else {
    x.uint -= abs_scale;
  }
  return x;
}

static inline mlx90640_real_t __mlx90640_real_scale(const mlx90640_real_t val,
                                                    const int8_t scale) {

  if ((val == 0) || (scale == 0))
    return val;

  mlx90640_real_uint_t x = {.real = val};

  x = __mlx90640_real_exp_add(x, scale);

  return x.real;
}
/** @} */

/**
 * @defgroup BitInt Core for bit operations on integers.
 * @ingroup BitCore
 * @brief My brief.
 *
 * Description.
 *
 */
static inline uint16_t __mlx90640_u16_nibble(const uint8_t i) {
  return (uint16_t)(0xFu << (i << 2u));
}

static inline uint16_t __mlx90640_u16_byte(const uint8_t i) {
  return (uint16_t)(0xFFu << (i << 3u));
}

// Wrappers around GCC builtin functions: needs updating if doesn't work for
// other compilers
static inline uint8_t __mlx90640_u16_ctz(const uint16_t mask) {
  return (uint8_t)__builtin_ctz(mask);
}

static inline uint8_t __mlx90640_u16_clz(const uint16_t mask) {
  return (uint8_t)(__builtin_clz((uint32_t)mask) - 16u);
}

static inline uint8_t __mlx90640_u16_popcount(uint16_t mask) {
  return (uint8_t)__builtin_popcount(mask);
}

static inline uint16_t __mlx90640_u16_set_field(uint16_t data,
                                                const uint16_t mask,
                                                const uint16_t value) {
  return (data & ~mask) | ((value << __mlx90640_u16_ctz(mask)) & mask);
}

static inline uint16_t __mlx90640_u16_get_field(const uint16_t data,
                                                const uint16_t mask) {
  return (data & mask) >> __mlx90640_u16_ctz(mask);
}

static inline int16_t __mlx90640_i16_get_field(uint16_t data, uint16_t mask) {
  if (data == 0) {
    return 0;
  }
  uint16_t value = __mlx90640_u16_get_field(data, mask);
  uint16_t m = 1u << (__mlx90640_u16_popcount(mask) - 1u);
  return (int16_t)((value ^ m) - m);
}

static inline uint16_t __mlx90640_bytes_to_u16(const uint8_t *buffer,
                                               const size_t idx) {
  return ((uint16_t)buffer[MLX90640_WORD_BYTES * idx] << 8u) |
         (uint16_t)buffer[MLX90640_WORD_BYTES * idx + 1u];
}

static inline void __mlx90640_u16_to_bytes(uint8_t *buffer, const uint16_t data,
                                           const size_t idx) {
  buffer[MLX90640_WORD_BYTES * idx] = (uint8_t)(data >> 8u);
  buffer[MLX90640_WORD_BYTES * idx + 1] = (uint8_t)data;
}

static inline void __mlx90640_real_to_bytes(uint8_t *buffer,
                                            const mlx90640_real_t data,
                                            const size_t idx) {
  mlx90640_real_uint_t p = {.real = data};

  for (size_t ib = 0; ib < MLX90640_REAL_BYTES; ib++) {
    buffer[MLX90640_REAL_BYTES * idx + ib] =
        (uint8_t)(p.uint >> ((MLX90640_REAL_BYTES - ib - 1) * 8u));
  }
}

/** @} */
#define MLX90640_REG_CONFIG_ADDRESS UINT16_C(0x800D)
#define MLX90640_REG_CONFIG_MASK UINT16_C(0x9FFF)

#define MLX90640_REG_CONFIG_SUBPAGE_MASK UINT16_C(0x0078u)
#define MLX90640_REG_CONFIG_FPS_MASK UINT16_C(0x0380u)
#define MLX90640_REG_CONFIG_BITDEPTH_MASK UINT16_C(0x0C00u)
#define MLX90640_REG_CONFIG_PATTERN_MASK UINT16_C(0x1000u)

#define MLX90640_REG_STATUS_ADDRESS UINT16_C(0x8000)
#define MLX90640_REG_STATUS_MASK UINT16_C(0x0010)

#define MLX90640_REG_STATUS_SUBPAGE UINT16_C(0x0007)
#define MLX90640_REG_STATUS_NEW_FRAME UINT16_C(0x0008)

#define MLX90640_REG_I2C_ADDRESS UINT16_C(0x800F)
#define MLX90640_REG_I2C_MASK UINT16_C(0x0007)

#define MLX90640_EEPROM_ADDRESS UINT16_C(0x2400)
#define MLX90640_EEPROM_BUFFER_SIZE UINT16_C(0x0340)

#define MLX90640_FRAME_PIXELS_ADDRESS UINT16_C(0x0400)
#define MLX90640_FRAME_PIXELS_BUFFER_SIZE UINT16_C(0x0300)

#define MLX90640_FRAME_PARAM_ADDRESS UINT16_C(0x0700)
#define MLX90640_FRAME_PARAM_BUFFER_SIZE UINT16_C(0x002B)

enum {
  MLX90640_CHECK = 0,
  MLX90640_ERR_DEVICE_MISSING = 90640,
  MLX90640_ERR_CONFIG_MISSING,
  MLX90640_ERR_BUFFER_MISSING,
  MLX90640_ERR_I2C_SCL_TOO_SLOW,
  MLX90640_ERR_I2C_BUS_MISSING,
  MLX90640_ERR_I2C_BUS_WRITE_READ_MISSING,
  MLX90640_ERR_I2C_BUS_WRITE_MISSING,
  MLX90640_ERR_I2C_BUS_DELAY_MISSING,
  MLX90640_ERR_I2C_SET_FIELD_FAILED,
  MLX90640_ERR_INVALID_INPUT,
};

#define MLX90640_I2C_BUFFER_REG_SIZE 2
#define MLX90640_I2C_BUFFER_DATA_SIZE UINT16_C(0x0340)

#define MLX90640_I2C_DEFAULT_TARGET_ADDRESS UINT8_C(0x33)

#define MLX90640_I2C_REG_BUFFER_MSB 0
#define MLX90640_I2C_REG_BUFFER_LSB 1

#define MLX90640_TRY(expr)                                                     \
  do {                                                                         \
    int err = (expr);                                                          \
    if (err) {                                                                 \
      return err;                                                              \
    }                                                                          \
  } while (0)

//@todo: allow modification of target address in eeprom
// #define MLX90640_I2C_REG_TARGET_ADDRESS UINT16_C(0x8010)
// #define MLX90640_I2C_EEPROM_TARGET_ADDRESS UINT16_C(0x240F)

static const mlx90640_real_t MLX90640_TA_REF = 25.0f;
static const mlx90640_real_t MLX90640_KELVIN_OFFSET = 273.15f;

#define MLX90640_PIXEL_Y_SIZE UINT16_C(24)
#define MLX90640_PIXEL_X_SIZE UINT16_C(32)
#define MLX90640_PIXEL_INDEX(y, x) ((x) + (y) * (MLX90640_PIXEL_X_SIZE))
#define MLX90640_PIXEL_INDEX_Y(index) ((index) / MLX90640_PIXEL_X_SIZE)
#define MLX90640_PIXEL_INDEX_X(index) ((index) % MLX90640_PIXEL_X_SIZE)

#define MLX90640_TK_CORNER_SIZE UINT8_C(4)

#define MLX90640_PATTERN_SIZE UINT8_C(2)
#define MLX90640_PATTERN_VALUE_INTERLEAVED(y, x) ((y) & 1u)
#define MLX90640_PATTERN_VALUE_CHESS(y, x) (((y) ^ (x)) & 1u)

/**
 * @defgroup MLX90640 MLX90640 Configuration
 * @brief Configuration of MLX90640
 * @{
 */

/**
 * @brief Subpage used for all frame acquisitions.
 *
 * - @c MLX90640_SUBPAGE_BOTH : Automatically alternate between SP_0 and SP_1
 * - @c MLX90640_SUBPAGE_0    : Always use subpage 0 (SP_0)
 * - @c MLX90640_SUBPAGE_1    : Always use subpage 1 (SP_1)
 *
 * When @c MLX90640_SUBPAGE_0 (or @c MLX90640_SUBPAGE_1 ) is selected, the
 * subpage remains identical for each frame acquired:\n
 *   Frame 1        → Frame 2          → Frame 3          → ...\n
 *   SP_0 (or SP_1) → SP_0 (or SP_1)   → SP_0 (or SP_1)   → ...
 *
 * When @c MLX90640_SUBPAGE_BOTH are selected, the subpage used alternates for
 * each frame acquired:
 *  Frame 1 → Frame 2 → Frame 3 → ...
 *  SP_0    → SP_1    → SP_0    → ...
 *
 * @note SP_0 and SP_1 are complementary. Use @c MLX90640_SUBPAGE_BOTH to
 * reconstruct a full frame.
 *
 */
typedef enum {
  MLX90640_SUBPAGE_BOTH = 0,
  MLX90640_SUBPAGE_0 = 1,
  MLX90640_SUBPAGE_1 = 3,
} mlx90640_subpage_t;

/**
 * @brief Subpage pattern selection.
 *
 * - @c MLX90640_PATTERN_INTERLEAVED : Set interleaved pattern
 * - @c MLX90640_PATTERN_CHESS       : Set chess pattern
 *
 */
typedef enum {
  MLX90640_PATTERN_INTERLEAVED = 0,
  MLX90640_PATTERN_CHESS
} mlx90640_pattern_t;

/**
 * @brief Subpage frame rate (Hz).
 *
 * When both subpages are used (see @c mlx90640_subpage_t ),
 * the effective full-frame rate is half the configured subpage rate.
 *
 */
typedef enum {
  MLX90640_FPS_0_5 = 0,
  MLX90640_FPS_1,
  MLX90640_FPS_2,
  MLX90640_FPS_4,
  MLX90640_FPS_8,
  MLX90640_FPS_16,
  MLX90640_FPS_32,
  MLX90640_FPS_64,
} mlx90640_fps_t;

/**
 * @brief Pixel bit depth.
 *
 * Selects the effective number of bits used for pixel conversion.
 *
 */
typedef enum {
  MLX90640_BITDEPTH_16 = 0,
  MLX90640_BITDEPTH_17,
  MLX90640_BITDEPTH_18,
  MLX90640_BITDEPTH_19,
} mlx90640_bitdepth_t;

typedef struct {
  mlx90640_subpage_t subpage;
  mlx90640_pattern_t pattern;
  mlx90640_bitdepth_t bitdepth;
  mlx90640_fps_t fps;
} mlx90640_config_t;

static inline uint16_t
_mlx90640_config_to_reg(const mlx90640_config_t *config) {
  uint16_t reg = 1u;
  reg = __mlx90640_u16_set_field(reg, MLX90640_REG_CONFIG_SUBPAGE_MASK,
                                 config->subpage);
  reg =
      __mlx90640_u16_set_field(reg, MLX90640_REG_CONFIG_FPS_MASK, config->fps);
  reg = __mlx90640_u16_set_field(reg, MLX90640_REG_CONFIG_BITDEPTH_MASK,
                                 config->bitdepth);
  reg = __mlx90640_u16_set_field(reg, MLX90640_REG_CONFIG_PATTERN_MASK,
                                 config->pattern);
  return reg;
}

static inline mlx90640_config_t _mlx90640_reg_to_config(const uint16_t reg) {
  mlx90640_config_t config;
  config.subpage = (mlx90640_subpage_t)__mlx90640_u16_get_field(
      reg, MLX90640_REG_CONFIG_SUBPAGE_MASK);
  config.fps = (mlx90640_fps_t)__mlx90640_u16_get_field(
      reg, MLX90640_REG_CONFIG_FPS_MASK);
  config.bitdepth = (mlx90640_bitdepth_t)__mlx90640_u16_get_field(
      reg, MLX90640_REG_CONFIG_BITDEPTH_MASK);
  config.pattern = (mlx90640_pattern_t)__mlx90640_u16_get_field(
      reg, MLX90640_REG_CONFIG_PATTERN_MASK);
  return config;
}

typedef struct mlx90640_i2c_backend_ctx_t mlx90640_i2c_backend_ctx_t;

typedef struct {
  mlx90640_i2c_backend_ctx_t *backend_ctx;
  int (*write_read)(mlx90640_i2c_backend_ctx_t *backend_ctx,
                    const uint8_t *write_buffer, size_t write_size,
                    uint8_t *read_buffer, size_t read_size);
  int (*write)(mlx90640_i2c_backend_ctx_t *backend_ctx,
               const uint8_t *write_buffer, size_t write_size);
  int (*delay_ms)(mlx90640_i2c_backend_ctx_t *backend_ctx, uint32_t ms);
} mlx90640_i2c_bus_t;

typedef struct {
  mlx90640_real_t offset;
  mlx90640_real_t kta;
  mlx90640_real_t kv;
  mlx90640_real_t interleaved_offset;
  mlx90640_real_t alpha;
  uint8_t subpage[MLX90640_PATTERN_SIZE];
  uint8_t accuracy_zone;
  uint8_t outlier;
#if MLX90640_USE_DOUBLE_PRECISION
  uint8_t __padding[4];
#endif
} mlx90640_calib_pixel_t;

typedef struct {
  mlx90640_real_t tk;
  mlx90640_real_t ak;
  mlx90640_real_t bk;
} mlx90640_tk_corner_t;

#if MLX90640_TGC_AVAILABLE
typedef struct {
  mlx90640_real_t offset[MLX90640_PATTERN_SIZE];
  mlx90640_real_t alpha[MLX90640_PATTERN_SIZE];
  mlx90640_real_t kv;
  mlx90640_real_t kta;
  mlx90640_real_t k1;
} mlx90640_pixel_tgc_t;
#endif

typedef struct {
#if MLX90640_TGC_AVAILABLE
  mlx90640_pixel_tgc_t pixel_tgc;
  mlx90640_real_t tgc;
#endif
  mlx90640_real_t gain;
  mlx90640_real_t vdd25;
  mlx90640_real_t kvdd;
  mlx90640_real_t kvptat;
  mlx90640_real_t vptat25;
  mlx90640_real_t ktptat;
  mlx90640_real_t ksta;
  uint8_t bitdepth;
  uint8_t alphaptat;
} mlx90640_calib_param_t;

typedef struct {
  uint8_t reg[MLX90640_I2C_BUFFER_REG_SIZE * MLX90640_WORD_BYTES];
  uint8_t data[MLX90640_I2C_BUFFER_DATA_SIZE * MLX90640_WORD_BYTES];
} mlx90640_i2c_buffer_t;

typedef struct {
#if MLX90640_TGC_AVAILABLE
  mlx90640_real_t pixel_tgc[MLX90640_PATTERN_SIZE];
#endif
  mlx90640_real_t gain;
  mlx90640_real_t delta_vdd;
  mlx90640_real_t delta_ta;
  mlx90640_real_t tk_a_r;
  uint8_t subpage;
  uint8_t pattern;
  uint8_t bitdepth;
} mlx90640_frame_param_t;

typedef struct {
  const mlx90640_config_t *config;
  const mlx90640_i2c_bus_t *i2c_bus;

  mlx90640_i2c_buffer_t i2c_buffer;

  mlx90640_calib_pixel_t pixel[MLX90640_PIXEL_Y_SIZE][MLX90640_PIXEL_X_SIZE];
  mlx90640_calib_param_t calib;
  mlx90640_frame_param_t frame_param;

  mlx90640_tk_corner_t tk_corner[MLX90640_TK_CORNER_SIZE];

  mlx90640_real_t emissivity;
  mlx90640_real_t tc_r;
  mlx90640_real_t ema[2];

  uint8_t zone;
  uint8_t ymin;
  uint8_t ymax;
} mlx90640_device_t;

static inline int16_t _mlx90640_eeprom_offset_average(const uint8_t *eeprom) {

  uint16_t val_index = (uint16_t)0x2411u - MLX90640_EEPROM_ADDRESS;
  return (int16_t)__mlx90640_bytes_to_u16(eeprom, val_index);
}

static inline int16_t _mlx90640_eeprom_offset_y(const uint8_t *eeprom,
                                                const size_t y) {
  uint16_t scale_index = (uint16_t)0x2410u - MLX90640_EEPROM_ADDRESS;
  uint8_t scale = (uint8_t)__mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, scale_index), __mlx90640_u16_nibble(2));

  uint16_t val_index =
      (uint16_t)y / 4u + (uint16_t)0x2412u - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), __mlx90640_u16_nibble(y % 4));

  return val << scale;
}

static inline int16_t _mlx90640_eeprom_offset_x(const uint8_t *eeprom,
                                                const size_t x) {
  uint16_t scale_index = (uint16_t)0x2410u - MLX90640_EEPROM_ADDRESS;
  uint8_t scale = (uint8_t)__mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, scale_index), __mlx90640_u16_nibble(1));

  uint16_t val_index = x / 4u + (uint16_t)0x2418u - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), __mlx90640_u16_nibble(x % 4));
  return val << scale;
}

static inline int16_t _mlx90640_eeprom_offset_pixel(const uint8_t *eeprom,
                                                    const size_t y,
                                                    const size_t x) {
  uint16_t scale_index = (uint16_t)0x2410 - MLX90640_EEPROM_ADDRESS;
  uint8_t scale = (uint8_t)__mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, scale_index), __mlx90640_u16_nibble(0));

  uint16_t val_index =
      MLX90640_PIXEL_INDEX(y, x) + (uint16_t)0x2440u - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), (uint16_t)0xFC00u);
  return val << scale;
}

static inline mlx90640_real_t
_mlx90640_eeprom_offset(const uint8_t *eeprom, const size_t y, const size_t x) {
  return (mlx90640_real_t)(_mlx90640_eeprom_offset_average(eeprom) +
                           _mlx90640_eeprom_offset_x(eeprom, x) +
                           _mlx90640_eeprom_offset_y(eeprom, y) +
                           _mlx90640_eeprom_offset_pixel(eeprom, y, x));
}

static inline mlx90640_real_t
_mlx90640_eeprom_kv(const uint8_t *eeprom, const size_t y, const size_t x) {

  uint16_t scale_index = (uint16_t)0x2438u - MLX90640_EEPROM_ADDRESS;
  int8_t scale = (int8_t)__mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, scale_index), __mlx90640_u16_nibble(2));

  uint16_t shift = 3u - ((y % 2u) + (x % 2u) * 2u);

  uint16_t val_index = (uint16_t)0x2434u - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), __mlx90640_u16_nibble(shift));

  return __mlx90640_real_scale((mlx90640_real_t)val, -scale);
}

static inline uint8_t _mlx90640_eeprom_kta_scale(const uint8_t *eeprom) {
  uint16_t val_index = (uint16_t)0x2438u - MLX90640_EEPROM_ADDRESS;
  uint8_t val = (uint8_t)__mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), __mlx90640_u16_nibble(1));
  return val + 8;
}

static inline mlx90640_real_t _mlx90640_eeprom_kta_pixel(const uint8_t *eeprom,
                                                         const size_t y,
                                                         const size_t x) {
  uint16_t scale_index = (uint16_t)0x2438u - MLX90640_EEPROM_ADDRESS;
  int8_t scale = (int8_t)__mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, scale_index), __mlx90640_u16_nibble(0));

  uint16_t val_index =
      MLX90640_PIXEL_INDEX(y, x) + (uint16_t)0x2440u - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), (uint16_t)0x000Eu);

  return __mlx90640_real_scale((mlx90640_real_t)val, scale);
}

static inline uint8_t _mlx90640_eeprom_outlier(const uint8_t *eeprom,
                                               const size_t y, const size_t x) {
  uint16_t val_index =
      MLX90640_PIXEL_INDEX(y, x) + (uint16_t)0x2440u - MLX90640_EEPROM_ADDRESS;
  uint8_t val = (uint8_t)__mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), (uint16_t)0x0001u);

  return val;
}

static inline mlx90640_real_t
_mlx90640_eeprom_kta_average(const uint8_t *eeprom, const size_t y,
                             const size_t x) {

  uint16_t shift = (y % 2u) + (x % 2u) * 2u;
  uint16_t bytes_index = (shift + 1u) % 2u;

  uint16_t val_index = shift / 2u + (uint16_t)0x2436u - MLX90640_EEPROM_ADDRESS;
  int16_t val =
      __mlx90640_i16_get_field(__mlx90640_bytes_to_u16(eeprom, val_index),
                               __mlx90640_u16_byte(bytes_index));

  return (mlx90640_real_t)val;
}

static inline mlx90640_real_t
_mlx90640_eeprom_kta(const uint8_t *eeprom, const size_t y, const size_t x) {

  int8_t scale = (int8_t)_mlx90640_eeprom_kta_scale(eeprom);
  return __mlx90640_real_scale(_mlx90640_eeprom_kta_average(eeprom, y, x) +
                                   _mlx90640_eeprom_kta_pixel(eeprom, y, x),
                               -scale);
}

static inline mlx90640_real_t _mlx90640_eeprom_ksta(const uint8_t *eeprom) {
  int8_t scale = 13;

  uint16_t val_index = (uint16_t)0x243Cu - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), __mlx90640_u16_byte(1));

  return __mlx90640_real_scale((mlx90640_real_t)val, -scale);
}

static inline mlx90640_real_t _mlx90640_eeprom_ct_ksto(const uint8_t *eeprom,
                                                       uint8_t range) {

  uint16_t scale_index = (uint16_t)0x243Fu - MLX90640_EEPROM_ADDRESS;
  int8_t scale = (int8_t)__mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, scale_index), __mlx90640_u16_nibble(0));
  scale += 8;

  uint16_t val_index =
      (uint16_t)range / 2u + (uint16_t)0x243Du - MLX90640_EEPROM_ADDRESS;
  int16_t val =
      __mlx90640_i16_get_field(__mlx90640_bytes_to_u16(eeprom, val_index),
                               __mlx90640_u16_byte(range % 2));

  return __mlx90640_real_scale((mlx90640_real_t)val, -scale);
}

static inline mlx90640_real_t _mlx90640_eeprom_tgc(const uint8_t *eeprom) {
  int8_t scale = 5;

  uint16_t val_index = (uint16_t)0x243Cu - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), __mlx90640_u16_byte(0));

  return __mlx90640_real_scale((mlx90640_real_t)val, -scale);
}

static inline uint8_t _mlx90640_eeprom_ct_step(const uint8_t *eeprom) {

  uint16_t val_index = (uint16_t)0x243Fu - MLX90640_EEPROM_ADDRESS;
  uint8_t val = (uint8_t)__mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), (uint16_t)0x3000u);

  return val * 10u;
}

static inline mlx90640_real_t _mlx90640_eeprom_ct_value(const uint8_t *eeprom,
                                                        const uint8_t range) {
  if (range == 0) {
    return -40.0f;
  } else if (range == 1) {
    return 0.0f;
  }

  uint16_t r2_index = (uint16_t)0x243Fu - MLX90640_EEPROM_ADDRESS;
  uint16_t r2 = __mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, r2_index), __mlx90640_u16_nibble(1));

  if (range == 2) {
    return (mlx90640_real_t)(r2 * _mlx90640_eeprom_ct_step(eeprom));
  } else if (range == 3) {
    uint16_t r3_index = (uint16_t)0x243Fu - MLX90640_EEPROM_ADDRESS;
    uint16_t r3 = __mlx90640_u16_get_field(
        __mlx90640_bytes_to_u16(eeprom, r3_index), __mlx90640_u16_nibble(2));
    return (mlx90640_real_t)((r2 + r3) * _mlx90640_eeprom_ct_step(eeprom));
  } else {
    return -1.0f;
  }
}

static inline mlx90640_real_t _mlx90640_eeprom_ct_alpha(const uint8_t *eeprom,
                                                        const uint8_t range) {

  if (range == 0) {
    return 1.0f + _mlx90640_eeprom_ct_ksto(eeprom, 0) *
                      (_mlx90640_eeprom_ct_value(eeprom, 1) -
                       _mlx90640_eeprom_ct_value(eeprom, 0));
  } else if (range == 1) {
    return 1.0f;
  }

  mlx90640_real_t alpha_2 = 1.0f + _mlx90640_eeprom_ct_ksto(eeprom, 1) *
                                       (_mlx90640_eeprom_ct_value(eeprom, 2) -
                                        _mlx90640_eeprom_ct_value(eeprom, 1));

  if (range == 2) {
    return alpha_2;
  } else if (range == 3) {
    mlx90640_real_t alpha_3 = 1.0f + _mlx90640_eeprom_ct_ksto(eeprom, 2) *
                                         (_mlx90640_eeprom_ct_value(eeprom, 3) -
                                          _mlx90640_eeprom_ct_value(eeprom, 2));
    return alpha_2 * alpha_3;
  }

  return 0.0f;
}

#if MLX90640_TGC_AVAILABLE
static inline mlx90640_real_t
_mlx90640_eeprom_pixel_tgc_alpha_sp0(const uint8_t *eeprom) {

  uint16_t scale_index = (uint16_t)0x2420u - MLX90640_EEPROM_ADDRESS;
  int8_t scale = (int8_t)__mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, scale_index), __mlx90640_u16_nibble(3));
  scale += 27;

  uint16_t val_index = (uint16_t)0x2439u - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), (uint16_t)0x03ffu);

  return __mlx90640_real_scale((mlx90640_real_t)val, -scale);
}

static inline mlx90640_real_t
_mlx90640_eeprom_pixel_tgc_sp1_sp0(const uint8_t *eeprom) {
  int8_t scale = 7;

  uint16_t val_index = (uint16_t)0x2439u - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), (uint16_t)0xFC00u);

  return __mlx90640_real_scale((mlx90640_real_t)val, -scale);
}

static inline mlx90640_real_t
_mlx90640_eeprom_pixel_tgc_alpha_sp(const uint8_t *eeprom, uint8_t sp_num) {
  return _mlx90640_eeprom_pixel_tgc_alpha_sp0(eeprom) *
         (1.0f +
          (mlx90640_real_t)sp_num * _mlx90640_eeprom_pixel_tgc_sp1_sp0(eeprom));
}

static inline int16_t
_mlx90640_eeprom_pixel_tgc_offset_sp0(const uint8_t *eeprom) {
  uint16_t val_index = (uint16_t)0x243Au - MLX90640_EEPROM_ADDRESS;
  return __mlx90640_i16_get_field(__mlx90640_bytes_to_u16(eeprom, val_index),
                                  (uint16_t)0x03FFu);
}

static inline int16_t
_mlx90640_eeprom_pixel_tgc_offset_sp1_delta(const uint8_t *eeprom) {
  uint16_t val_index = (uint16_t)0x243Au - MLX90640_EEPROM_ADDRESS;
  return __mlx90640_i16_get_field(__mlx90640_bytes_to_u16(eeprom, val_index),
                                  (uint16_t)0xFC00u);
}

static inline mlx90640_real_t
_mlx90640_eeprom_pixel_tgc_kv(const uint8_t *eeprom) {

  uint16_t scale_index = (uint16_t)0x2438u - MLX90640_EEPROM_ADDRESS;
  int8_t scale = (int8_t)__mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, scale_index), __mlx90640_u16_nibble(2));

  uint16_t val_index = (uint16_t)0x243Bu - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), __mlx90640_u16_byte(1));

  return __mlx90640_real_scale((mlx90640_real_t)val, -scale);
}

static inline mlx90640_real_t
_mlx90640_eeprom_pixel_tgc_kta(const uint8_t *eeprom) {

  uint16_t scale_index = (uint16_t)0x2438u - MLX90640_EEPROM_ADDRESS;
  int8_t scale = (int8_t)__mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, scale_index), __mlx90640_u16_nibble(1));
  scale += 8;

  uint16_t val_index = (uint16_t)0x243Bu - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), __mlx90640_u16_byte(0));

  return __mlx90640_real_scale((mlx90640_real_t)val, -scale);
}

static inline mlx90640_real_t
_mlx90640_eeprom_il_chess_k1(const uint8_t *eeprom) {
  int8_t scale = 4;

  uint16_t val_index = (uint16_t)0x2435u - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), (uint16_t)0x003Fu);

  return __mlx90640_real_scale((mlx90640_real_t)val, -scale);
}

static inline mlx90640_real_t
_mlx90640_eeprom_pixel_tgc_offset_sp(const uint8_t *eeprom, uint8_t sp_num) {

  int16_t val =
      _mlx90640_eeprom_pixel_tgc_offset_sp0(eeprom) +
      (int16_t)sp_num * _mlx90640_eeprom_pixel_tgc_offset_sp1_delta(eeprom);

  return (mlx90640_real_t)val;
}

#endif

static inline mlx90640_real_t
_mlx90640_eeprom_il_chess_k2(const uint8_t *eeprom) {
  int8_t scale = 1;

  uint16_t val_index = (uint16_t)0x2435u - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), (uint16_t)0x07C0u);

  return __mlx90640_real_scale((mlx90640_real_t)val, -scale);
}

static inline mlx90640_real_t
_mlx90640_eeprom_il_chess_k3(const uint8_t *eeprom) {
  int8_t scale = 3;

  uint16_t val_index = (uint16_t)0x2435u - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), (uint16_t)0xF800u);

  return __mlx90640_real_scale((mlx90640_real_t)val, -scale);
}

static inline mlx90640_real_t
_mlx90640_eeprom_il_pattern_offset(const uint8_t *eeprom, const size_t y,
                                   const size_t x) {
  int16_t num = (int16_t)MLX90640_PIXEL_INDEX(y, x);

  int16_t conversion =
      (((num - 2) >> 2) - ((num - 1) >> 2) + ((num + 1) >> 2) - (num >> 2)) *
      (1 - 2 * (int16_t)MLX90640_PATTERN_VALUE_INTERLEAVED(y, x));
  return _mlx90640_eeprom_il_chess_k3(eeprom) *
             (2.0f * (mlx90640_real_t)MLX90640_PATTERN_VALUE_INTERLEAVED(y, x) -
              1.0f) -
         _mlx90640_eeprom_il_chess_k2(eeprom) * (mlx90640_real_t)conversion;
}

static inline mlx90640_real_t _mlx90640_eeprom_kvdd(const uint8_t *eeprom) {
  int8_t scale = 5;

  uint16_t val_index = (uint16_t)0x2433u - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), __mlx90640_u16_byte(1));

  return __mlx90640_real_scale((mlx90640_real_t)val, scale);
}

static inline mlx90640_real_t _mlx90640_eeprom_vdd25(const uint8_t *eeprom) {

  uint16_t val_index = (uint16_t)0x2433u - MLX90640_EEPROM_ADDRESS;
  uint16_t val = __mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), __mlx90640_u16_byte(0));
  int16_t vali = (int16_t)((val - 256) * (1u << 5u) - (1u << 13u));
  return (mlx90640_real_t)vali;
}

static inline mlx90640_real_t _mlx90640_eeprom_kvptat(const uint8_t *eeprom) {
  int8_t scale = 12;

  uint16_t val_index = (uint16_t)0x2432u - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), (uint16_t)0xFC00u);

  return __mlx90640_real_scale((mlx90640_real_t)val, -scale);
}

static inline mlx90640_real_t _mlx90640_eeprom_ktptat(const uint8_t *eeprom) {
  int8_t scale = 3;

  uint16_t val_index = (uint16_t)0x2432u - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), (uint16_t)0x03FFu);

  return __mlx90640_real_scale((mlx90640_real_t)val, -scale);
}

static inline uint8_t _mlx90640_eeprom_alphaptat(const uint8_t *eeprom) {

  uint16_t val_index = (uint16_t)0x2410u - MLX90640_EEPROM_ADDRESS;
  uint8_t val = (uint8_t)__mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), __mlx90640_u16_nibble(3));

  return (val >> 2u) + 8u;
}

static inline mlx90640_real_t _mlx90640_eeprom_vptat25(const uint8_t *eeprom) {

  uint16_t val_index = (uint16_t)0x2431u - MLX90640_EEPROM_ADDRESS;
  int16_t val = (int16_t)__mlx90640_bytes_to_u16(eeprom, val_index);
  return (mlx90640_real_t)val;
}

static inline uint8_t _mlx90640_eeprom_alpha_scale(const uint8_t *eeprom) {
  uint16_t val_index = (uint16_t)0x2420u - MLX90640_EEPROM_ADDRESS;
  uint8_t val = __mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), __mlx90640_u16_nibble(3));
  return val + 30u;
}

static inline int16_t _mlx90640_eeprom_alpha_average(const uint8_t *eeprom) {
  uint16_t val_index = (uint16_t)0x2421u - MLX90640_EEPROM_ADDRESS;
  return (int16_t)__mlx90640_bytes_to_u16(eeprom, val_index);
}

static inline int16_t _mlx90640_eeprom_alpha_y(const uint8_t *eeprom,
                                               const size_t y) {
  uint16_t scale_index = (uint16_t)0x2420u - MLX90640_EEPROM_ADDRESS;
  uint8_t scale = (uint8_t)__mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, scale_index), __mlx90640_u16_nibble(2));

  uint16_t val_index =
      (uint16_t)y / 4u + (uint16_t)0x2422u - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), __mlx90640_u16_nibble(y % 4));

  return val << scale;
}

static inline int16_t _mlx90640_eeprom_alpha_x(const uint8_t *eeprom,
                                               const size_t x) {

  uint16_t scale_index = (uint16_t)0x2420u - MLX90640_EEPROM_ADDRESS;
  uint8_t scale = (uint8_t)__mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, scale_index), __mlx90640_u16_nibble(1));

  uint16_t val_index =
      (uint16_t)x / (uint16_t)4u + (uint16_t)0x2428u - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), __mlx90640_u16_nibble(x % 4));

  return val << scale;
}

static inline int16_t _mlx90640_eeprom_alpha_pixel(const uint8_t *eeprom,
                                                   const size_t y,
                                                   const size_t x) {

  uint16_t scale_index = (uint16_t)0x2420u - MLX90640_EEPROM_ADDRESS;
  uint8_t scale = (uint8_t)__mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, scale_index), __mlx90640_u16_nibble(0));

  uint16_t val_index =
      MLX90640_PIXEL_INDEX(y, x) + (uint16_t)0x2440u - MLX90640_EEPROM_ADDRESS;
  int16_t val = __mlx90640_i16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), (uint16_t)0x03F0u);

  return val << scale;
}

static inline mlx90640_real_t
_mlx90640_eeprom_alpha(const uint8_t *eeprom, const size_t y, const size_t x) {

  int8_t scale = (int8_t)_mlx90640_eeprom_alpha_scale(eeprom);
  int16_t val = _mlx90640_eeprom_alpha_average(eeprom) +
                _mlx90640_eeprom_alpha_y(eeprom, y) +
                _mlx90640_eeprom_alpha_x(eeprom, x) +
                _mlx90640_eeprom_alpha_pixel(eeprom, y, x);
  return __mlx90640_real_scale((mlx90640_real_t)val, -scale);
}

static inline uint8_t _mlx90640_eeprom_bitdepth(const uint8_t *eeprom) {

  uint16_t val_index = (uint16_t)0x2438u - MLX90640_EEPROM_ADDRESS;
  uint8_t val = (uint8_t)__mlx90640_u16_get_field(
      __mlx90640_bytes_to_u16(eeprom, val_index), (uint16_t)0x3000u);

  return val;
}

static inline mlx90640_real_t _mlx90640_eeprom_gain(const uint8_t *eeprom) {
  uint16_t val_index = (uint16_t)0x2430u - MLX90640_EEPROM_ADDRESS;
  int16_t val = (int16_t)__mlx90640_bytes_to_u16(eeprom, val_index);
  return (mlx90640_real_t)val;
}

static inline mlx90640_real_t
_mlx90640_frame_gain(const uint8_t *frame,
                     const mlx90640_calib_param_t *calib) {

  uint16_t val_index = (uint16_t)0x070Au - MLX90640_FRAME_PIXELS_ADDRESS;
  int16_t gain = (int16_t)__mlx90640_bytes_to_u16(frame, val_index);

  return calib->gain / (mlx90640_real_t)gain;
}

static inline mlx90640_real_t
_mlx90640_frame_delta_vdd(const uint8_t *frame,
                          const mlx90640_calib_param_t *calib,
                          const uint8_t bitdepth) {

  uint16_t val_index = (uint16_t)0x072Au - MLX90640_FRAME_PIXELS_ADDRESS;
  int16_t vdd = (int16_t)__mlx90640_bytes_to_u16(frame, val_index);

  return (__mlx90640_real_scale((mlx90640_real_t)vdd,
                                (int8_t)calib->bitdepth - (int8_t)bitdepth) -
          calib->vdd25) /
         calib->kvdd;
}

static inline mlx90640_real_t
_mlx90640_frame_vptatart(const uint8_t *frame,
                         const mlx90640_calib_param_t *calib) {

  uint16_t vbe_index = (uint16_t)0x0700u - MLX90640_FRAME_PIXELS_ADDRESS;
  int16_t vbe = (int16_t)__mlx90640_bytes_to_u16(frame, vbe_index);

  uint16_t vptat_index = (uint16_t)0x0720u - MLX90640_FRAME_PIXELS_ADDRESS;
  int16_t vptat = (int16_t)__mlx90640_bytes_to_u16(frame, vptat_index);

  int8_t scale = 18;
  mlx90640_real_t vptatart =
      1.0f / ((mlx90640_real_t)calib->alphaptat +
              (mlx90640_real_t)vbe / (mlx90640_real_t)vptat);

  return __mlx90640_real_scale(vptatart, scale);
}

static inline mlx90640_real_t
_mlx90640_frame_delta_ta(const uint8_t *frame,
                         const mlx90640_calib_param_t *calib,
                         const mlx90640_frame_param_t *frame_param) {
  return (_mlx90640_frame_vptatart(frame, calib) /
              (1.0f + calib->kvptat * frame_param->delta_vdd) -
          calib->vptat25) /
         calib->ktptat;
}

static inline int16_t _mlx90640_frame_pixel_tgc_value(const uint8_t *frame,
                                                      const size_t num) {
  uint16_t val_index = (uint16_t)0x0708u - MLX90640_FRAME_PIXELS_ADDRESS;
  val_index += (uint16_t)0x0020u * num;

  return (int16_t)__mlx90640_bytes_to_u16(frame, val_index);
}

static inline mlx90640_real_t
_mlx90640_pixel_ir(const mlx90640_real_t *raw_pixel,
                   const mlx90640_calib_pixel_t *calib_pixel,
                   const mlx90640_calib_param_t *calib,
                   const mlx90640_frame_param_t *frame_param,
                   const mlx90640_real_t emissivity) {

  if (calib_pixel->outlier) {
    return MLX90640_NAN;
  }
  mlx90640_real_t vir_comp =
      (*raw_pixel * frame_param->gain -
       calib_pixel->offset * (1.0f + calib_pixel->kta * frame_param->delta_ta) *
           (1.0f + calib_pixel->kv * frame_param->delta_vdd)) /
      emissivity;

#if MLX90640_TGC_AVAILABLE
  vir_comp -=
      calib->tgc *
      frame_param->pixel_tgc[calib_pixel->subpage[frame_param->pattern]];
#endif

  if (frame_param->pattern == MLX90640_PATTERN_INTERLEAVED) {
    vir_comp += calib_pixel->interleaved_offset;
  }
  mlx90640_real_t alpha_comp = calib_pixel->alpha;

#if MLX90640_TGC_AVAILABLE
  alpha_comp -=
      calib->tgc *
      calib->pixel_tgc.alpha[calib_pixel->subpage[frame_param->pattern]];
#endif

  alpha_comp = alpha_comp * (1.0f + calib->ksta * frame_param->delta_ta);

  return vir_comp / alpha_comp;
}

static inline mlx90640_real_t
_mlx90640_tk_a_r(const mlx90640_real_t tk_a, const mlx90640_real_t tk_r,
                 const mlx90640_real_t emissivity) {

  if (emissivity == 1.0f) {
    return tk_a * tk_a * tk_a * tk_a;
  } else if ((emissivity > 0.0f) && (emissivity < 1.0f)) {
    return (tk_a * tk_a * tk_a * tk_a +
            (emissivity - 1.0f) * tk_r * tk_r * tk_r * tk_r) /
           emissivity;
  }
  return MLX90640_NAN;
}

static inline mlx90640_real_t
_mlx90640_pixel_tk_object(const mlx90640_real_t *ir_value,
                          const mlx90640_real_t scale,
                          const mlx90640_real_t tk_a_r) {
  return MLX90640_SQRT4(scale * (*ir_value) + tk_a_r);
}

static inline mlx90640_real_t
_mlx90640_pixel_tk_object_fast(const mlx90640_real_t *ir_value,
                               const mlx90640_real_t scale,
                               const mlx90640_real_t tk_a_r) {
  return MLX90640_SQRT4_FAST(scale * (*ir_value) + tk_a_r);
}

static inline mlx90640_real_t _mlx90640_pixel_tk_object_scale(
    const mlx90640_real_t *tk,
    const mlx90640_tk_corner_t tkcorner[MLX90640_TK_CORNER_SIZE]) {
  uint8_t index =
      (uint8_t)(*tk > tkcorner[0].tk) + (uint8_t)(*tk > tkcorner[1].tk) +
      (uint8_t)(*tk > tkcorner[2].tk) + (uint8_t)(*tk > tkcorner[3].tk);
  if (index > 0) {
    index -= 1;
  }
  return 1.0f / (tkcorner[index].ak * (*tk) + tkcorner[index].bk);
}

static inline mlx90640_real_t _mlx90640_pixel_tk_object_extended(
    const mlx90640_real_t *ir_pixel, const mlx90640_frame_param_t *frame_param,
    const size_t y, const size_t x,
    const mlx90640_tk_corner_t tkcorner[MLX90640_TK_CORNER_SIZE]) {

  if (MLX90640_ISNAN(*ir_pixel)) {
    return *ir_pixel;
  }

  mlx90640_real_t scale = 1.0f;
  mlx90640_real_t tk_object =
      _mlx90640_pixel_tk_object(ir_pixel, scale, frame_param->tk_a_r);

  scale = _mlx90640_pixel_tk_object_scale(&tk_object, tkcorner);
  return _mlx90640_pixel_tk_object(ir_pixel, scale, frame_param->tk_a_r);
}

static inline mlx90640_real_t _mlx90640_pixel_tk_object_extended_fast(
    const mlx90640_real_t *ir_pixel, const mlx90640_frame_param_t *frame_param,
    const size_t y, const size_t x,
    const mlx90640_tk_corner_t tkcorner[MLX90640_TK_CORNER_SIZE]) {

  if (MLX90640_ISNAN(*ir_pixel)) {
    return *ir_pixel;
  }

  mlx90640_real_t scale = 1.0f;
  mlx90640_real_t tk_object =
      _mlx90640_pixel_tk_object_fast(ir_pixel, scale, frame_param->tk_a_r);

  scale = _mlx90640_pixel_tk_object_scale(&tk_object, tkcorner);
  return _mlx90640_pixel_tk_object_fast(ir_pixel, scale, frame_param->tk_a_r);
}

static inline int mlx90640_i2c_read_reg(mlx90640_device_t *mlx90640,
                                        const uint16_t reg_address,
                                        uint16_t *value) {

  if (!mlx90640) {
    return MLX90640_ERR_DEVICE_MISSING;
  }
  if (!mlx90640->i2c_bus) {
    return MLX90640_ERR_I2C_BUS_MISSING;
  }
  if (!mlx90640->i2c_bus->write_read) {
    return MLX90640_ERR_I2C_BUS_WRITE_READ_MISSING;
  }

  __mlx90640_u16_to_bytes(mlx90640->i2c_buffer.reg, reg_address,
                          MLX90640_I2C_REG_BUFFER_MSB);
  MLX90640_TRY(mlx90640->i2c_bus->write_read(
      mlx90640->i2c_bus->backend_ctx, mlx90640->i2c_buffer.reg,
      MLX90640_WORD_BYTES, mlx90640->i2c_buffer.reg, MLX90640_WORD_BYTES));
  *value = __mlx90640_bytes_to_u16(mlx90640->i2c_buffer.reg,
                                   MLX90640_I2C_REG_BUFFER_MSB);
  return MLX90640_CHECK;
}

static inline int mlx90640_i2c_write_reg(mlx90640_device_t *mlx90640,
                                         const uint16_t reg_address,
                                         const uint16_t *value) {

  if (!mlx90640) {
    return MLX90640_ERR_DEVICE_MISSING;
  }
  if (!mlx90640->i2c_bus) {
    return MLX90640_ERR_I2C_BUS_MISSING;
  }
  if (!mlx90640->i2c_bus->write) {
    return MLX90640_ERR_I2C_BUS_WRITE_MISSING;
  }

  __mlx90640_u16_to_bytes(mlx90640->i2c_buffer.reg, reg_address,
                          MLX90640_I2C_REG_BUFFER_MSB);
  __mlx90640_u16_to_bytes(mlx90640->i2c_buffer.reg, *value,
                          MLX90640_I2C_REG_BUFFER_LSB);
  return mlx90640->i2c_bus->write(mlx90640->i2c_bus->backend_ctx,
                                  mlx90640->i2c_buffer.reg,
                                  2u * MLX90640_WORD_BYTES);
}

static inline int mlx90640_i2c_set_reg(mlx90640_device_t *mlx90640,
                                       const uint16_t reg_address,
                                       const uint16_t field_value,
                                       const uint16_t mask) {

  if (!mlx90640) {
    return MLX90640_ERR_DEVICE_MISSING;
  }
  uint16_t reg, reg_updated;

  // Read
  MLX90640_TRY(mlx90640_i2c_read_reg(mlx90640, reg_address, &reg));

  // Modify
  reg = __mlx90640_u16_set_field(reg, mask, field_value);

  // Write
  MLX90640_TRY(mlx90640_i2c_write_reg(mlx90640, reg_address, &reg));

  // Read updated value
  MLX90640_TRY(mlx90640_i2c_read_reg(mlx90640, reg_address, &reg_updated));

  // Verify field has been set properly
  if (__mlx90640_u16_get_field(reg, mask) !=
      __mlx90640_u16_get_field(reg_updated, mask)) {
    return MLX90640_ERR_I2C_SET_FIELD_FAILED;
  }
  return MLX90640_CHECK;
}

static inline int _mlx90640_i2c_delay(mlx90640_device_t *mlx90640,
                                      const uint32_t delay_ms) {

  if (!mlx90640) {
    return MLX90640_ERR_DEVICE_MISSING;
  }
  if (!mlx90640->i2c_bus) {
    return MLX90640_ERR_I2C_BUS_MISSING;
  }
  if (!mlx90640->i2c_bus->delay_ms) {
    return MLX90640_ERR_I2C_BUS_DELAY_MISSING;
  }
  return mlx90640->i2c_bus->delay_ms(mlx90640->i2c_bus->backend_ctx, delay_ms);
}

static inline int mlx90640_i2c_get_eeprom(mlx90640_device_t *mlx90640) {
  if (!mlx90640) {
    return MLX90640_ERR_DEVICE_MISSING;
  }
  if (!mlx90640->i2c_bus) {
    return MLX90640_ERR_I2C_BUS_MISSING;
  }
  if (!mlx90640->i2c_bus->write_read) {
    return MLX90640_ERR_I2C_BUS_WRITE_READ_MISSING;
  }

  __mlx90640_u16_to_bytes(mlx90640->i2c_buffer.reg, MLX90640_EEPROM_ADDRESS,
                          MLX90640_I2C_REG_BUFFER_MSB);
  return mlx90640->i2c_bus->write_read(
      mlx90640->i2c_bus->backend_ctx, mlx90640->i2c_buffer.reg,
      MLX90640_WORD_BYTES, mlx90640->i2c_buffer.data,
      MLX90640_EEPROM_BUFFER_SIZE * MLX90640_WORD_BYTES);
}

static inline int mlx90640_i2c_set_config(mlx90640_device_t *mlx90640) {

  if (!mlx90640) {
    return MLX90640_ERR_DEVICE_MISSING;
  }
  if (!mlx90640->config) {
    return MLX90640_ERR_CONFIG_MISSING;
  }

  uint16_t config = _mlx90640_config_to_reg(mlx90640->config);
  MLX90640_TRY(mlx90640_i2c_set_reg(mlx90640, MLX90640_REG_CONFIG_ADDRESS,
                                    config, MLX90640_REG_CONFIG_MASK));

  mlx90640->frame_param.bitdepth = mlx90640->config->bitdepth;
  mlx90640->frame_param.pattern = mlx90640->config->pattern;

  return MLX90640_CHECK;
}

static inline int mlx90640_set_eeprom(mlx90640_device_t *mlx90640,
                                      const uint8_t *buffer) {

  if (!mlx90640) {
    return MLX90640_ERR_DEVICE_MISSING;
  }
  if (!buffer) {
    return MLX90640_ERR_BUFFER_MISSING;
  }
  mlx90640_calib_param_t *calib = &mlx90640->calib;

#if MLX90640_TGC_AVAILABLE
  for (size_t ipc = 0; ipc < MLX90640_PATTERN_SIZE; ipc++) {
    calib->pixel_tgc.offset[ipc] =
        _mlx90640_eeprom_pixel_tgc_offset_sp(buffer, ipc);
    calib->pixel_tgc.alpha[ipc] =
        _mlx90640_eeprom_pixel_tgc_alpha_sp(buffer, ipc);
  }
  calib->pixel_tgc.kta = _mlx90640_eeprom_pixel_tgc_kta(buffer);
  calib->pixel_tgc.kv = _mlx90640_eeprom_pixel_tgc_kv(buffer);
  calib->pixel_tgc.k1 = _mlx90640_eeprom_il_chess_k1(buffer);

  calib->tgc = _mlx90640_eeprom_tgc(buffer);
#endif

  for (size_t y = 0; y < MLX90640_PIXEL_Y_SIZE; y++) {
    for (size_t x = 0; x < MLX90640_PIXEL_X_SIZE; x++) {

      mlx90640_calib_pixel_t *pix = &mlx90640->pixel[y][x];

      pix->outlier = _mlx90640_eeprom_outlier(buffer, y, x);
      pix->subpage[MLX90640_PATTERN_CHESS] = MLX90640_PATTERN_VALUE_CHESS(y, x);
      pix->subpage[MLX90640_PATTERN_INTERLEAVED] =
          MLX90640_PATTERN_VALUE_INTERLEAVED(y, x);
      pix->accuracy_zone =
          (uint8_t)((x > 7) && (x < (MLX90640_PIXEL_X_SIZE - 7)) && (y > 3) &&
                    (y < (MLX90640_PIXEL_Y_SIZE - 3))) |
          (1u << 2u);
      pix->interleaved_offset =
          _mlx90640_eeprom_il_pattern_offset(buffer, y, x);
      pix->offset = _mlx90640_eeprom_offset(buffer, y, x);
      pix->kta = _mlx90640_eeprom_kta(buffer, y, x);
      pix->kv = _mlx90640_eeprom_kv(buffer, y, x);
      pix->alpha = _mlx90640_eeprom_alpha(buffer, y, x);
    }
  }

  calib->gain = _mlx90640_eeprom_gain(buffer);

  // to reconstruct current vdd
  calib->bitdepth = _mlx90640_eeprom_bitdepth(buffer);
  calib->vdd25 = _mlx90640_eeprom_vdd25(buffer);
  calib->kvdd = _mlx90640_eeprom_kvdd(buffer);

  // to reconstruct current ta
  calib->alphaptat = _mlx90640_eeprom_alphaptat(buffer);
  calib->kvptat = _mlx90640_eeprom_kvptat(buffer);
  calib->vptat25 = _mlx90640_eeprom_vptat25(buffer);
  calib->ktptat = _mlx90640_eeprom_ktptat(buffer);

  // to reconstruct current sensitivity
  calib->ksta = _mlx90640_eeprom_ksta(buffer);

  // to extend tobject
  for (size_t itc = 0; itc < MLX90640_TK_CORNER_SIZE; itc++) {
    mlx90640->tk_corner[itc].tk =
        _mlx90640_eeprom_ct_value(buffer, itc) + MLX90640_KELVIN_OFFSET;
    mlx90640->tk_corner[itc].ak = _mlx90640_eeprom_ct_alpha(buffer, itc) *
                                  _mlx90640_eeprom_ct_ksto(buffer, itc);
    mlx90640->tk_corner[itc].bk =
        _mlx90640_eeprom_ct_alpha(buffer, itc) *
        (1 -
         _mlx90640_eeprom_ct_ksto(buffer, itc) *
             (_mlx90640_eeprom_ct_value(buffer, itc) + MLX90640_KELVIN_OFFSET));
  }
  return MLX90640_CHECK;
}

#if MLX90640_TGC_AVAILABLE
static inline uint8_t mlx90640_is_tgc_available(mlx90640_device_t *mlx90640) {
  return (mlx90640->calib.tgc != 0.0f);
}
#endif

static inline int mlx90640_init(mlx90640_device_t *mlx90640,
                                mlx90640_config_t *config,
                                mlx90640_i2c_bus_t *i2c_bus) {
  if (!mlx90640) {
    return MLX90640_ERR_DEVICE_MISSING;
  }
  if (!i2c_bus) {
    return MLX90640_ERR_I2C_BUS_MISSING;
  }
  if (!config) {
    return MLX90640_ERR_CONFIG_MISSING;
  }

  mlx90640->i2c_bus = i2c_bus;
  mlx90640->config = config;

  mlx90640->emissivity = 1.0f;
  mlx90640->tc_r = MLX90640_NAN;
  mlx90640->zone = 3;

  mlx90640->ymin = 0u;
  mlx90640->ymax = MLX90640_PIXEL_Y_SIZE;

  mlx90640->ema[0] = 1.0f;
  mlx90640->ema[1] = 0.0f;

  MLX90640_TRY(_mlx90640_i2c_delay(mlx90640, 80u));
  MLX90640_TRY(mlx90640_i2c_set_config(mlx90640));

  MLX90640_TRY(mlx90640_i2c_get_eeprom(mlx90640));
  MLX90640_TRY(mlx90640_set_eeprom(mlx90640, mlx90640->i2c_buffer.data));

  return MLX90640_CHECK;
}

static inline int mlx90640_set_crop_y(mlx90640_device_t *mlx90640,
                                      const uint8_t y_index_min,
                                      const uint8_t y_index_max) {
  if (!mlx90640) {
    return MLX90640_ERR_DEVICE_MISSING;
  }

  if ((y_index_max <= y_index_min) || (y_index_max > MLX90640_PIXEL_Y_SIZE)) {
    return MLX90640_ERR_INVALID_INPUT;
  }

  mlx90640->ymin = y_index_min;
  mlx90640->ymax = y_index_max;

  return MLX90640_CHECK;
}

static inline int mlx90640_set_crop_accuracy(mlx90640_device_t *mlx90640,
                                             const uint8_t accuracy_zone) {
  if (!mlx90640) {
    return MLX90640_ERR_DEVICE_MISSING;
  }

  if (accuracy_zone > 1) {
    return MLX90640_ERR_INVALID_INPUT;
  }

  mlx90640->zone = accuracy_zone;

  return MLX90640_CHECK;
}

static inline int mlx90640_set_ema_value(mlx90640_device_t *mlx90640,
                                         const mlx90640_real_t ema) {
  if (!mlx90640) {
    return MLX90640_ERR_DEVICE_MISSING;
  }

  if ((ema > 1.0f) || (ema < 0.0f)) {
    return MLX90640_ERR_INVALID_INPUT;
  }

  mlx90640->ema[0] = ema;
  mlx90640->ema[1] = 1.0f - ema;

  return MLX90640_CHECK;
}

static inline int mlx90640_set_emissivity(mlx90640_device_t *mlx90640,
                                          const mlx90640_real_t emissivity) {
  if (!mlx90640) {
    return MLX90640_ERR_DEVICE_MISSING;
  }

  if ((emissivity < 0.0f) || (emissivity > 1.0f)) {
    return MLX90640_ERR_INVALID_INPUT;
  }

  mlx90640->emissivity = emissivity;

  return MLX90640_CHECK;
}

static inline int mlx90640_set_tc_r(mlx90640_device_t *mlx90640,
                                    const mlx90640_real_t tc_r) {
  if (!mlx90640) {
    return MLX90640_ERR_DEVICE_MISSING;
  }

  mlx90640->tc_r = tc_r;

  return MLX90640_CHECK;
}

static inline int mlx90640_i2c_read_frame_pixels(mlx90640_device_t *mlx90640,
                                                 const uint16_t y_min,
                                                 const uint16_t y_max) {
  if (!mlx90640) {
    return MLX90640_ERR_DEVICE_MISSING;
  }
  if (!mlx90640->i2c_bus) {
    return MLX90640_ERR_I2C_BUS_MISSING;
  }
  if (!mlx90640->i2c_bus->write_read) {
    return MLX90640_ERR_I2C_BUS_WRITE_READ_MISSING;
  }

  uint16_t pixels_offset_word = MLX90640_PIXEL_INDEX(y_min, 0);
  __mlx90640_u16_to_bytes(mlx90640->i2c_buffer.reg,
                          pixels_offset_word + MLX90640_FRAME_PIXELS_ADDRESS,
                          MLX90640_I2C_REG_BUFFER_MSB);
  return mlx90640->i2c_bus->write_read(
      mlx90640->i2c_bus->backend_ctx, mlx90640->i2c_buffer.reg,
      MLX90640_WORD_BYTES,
      &mlx90640->i2c_buffer.data[pixels_offset_word * MLX90640_WORD_BYTES],
      (y_max - y_min) * MLX90640_PIXEL_X_SIZE * MLX90640_WORD_BYTES);
}

static inline int mlx90640_i2c_read_frame_param(mlx90640_device_t *mlx90640) {
  if (!mlx90640) {
    return MLX90640_ERR_DEVICE_MISSING;
  }
  if (!mlx90640->i2c_bus) {
    return MLX90640_ERR_I2C_BUS_MISSING;
  }
  if (!mlx90640->i2c_bus->write_read) {
    return MLX90640_ERR_I2C_BUS_WRITE_READ_MISSING;
  }

  __mlx90640_u16_to_bytes(mlx90640->i2c_buffer.reg,
                          MLX90640_FRAME_PARAM_ADDRESS,
                          MLX90640_I2C_REG_BUFFER_MSB);
  return mlx90640->i2c_bus->write_read(
      mlx90640->i2c_bus->backend_ctx, mlx90640->i2c_buffer.reg,
      MLX90640_WORD_BYTES,
      &(mlx90640->i2c_buffer.data[(MLX90640_FRAME_PARAM_ADDRESS -
                                   MLX90640_FRAME_PIXELS_ADDRESS) *
                                  MLX90640_WORD_BYTES]),
      MLX90640_FRAME_PARAM_BUFFER_SIZE * MLX90640_WORD_BYTES);
}

static inline void mlx90640_set_frame_param(mlx90640_frame_param_t *frame_param,
                                            const uint8_t *frame,
                                            const mlx90640_calib_param_t *calib,
                                            const mlx90640_real_t emissivity,
                                            const mlx90640_real_t tc_r) {

  frame_param->gain = _mlx90640_frame_gain(frame, calib);

  // Ordering is required here since delta ta depends on delta vdd.
  frame_param->delta_vdd =
      _mlx90640_frame_delta_vdd(frame, calib, frame_param->bitdepth);

  frame_param->delta_ta = _mlx90640_frame_delta_ta(frame, calib, frame_param);

  mlx90640_real_t tk_a =
      frame_param->delta_ta + MLX90640_TA_REF + MLX90640_KELVIN_OFFSET;
  mlx90640_real_t tk_r =
      (MLX90640_ISNAN(tc_r)) ? tk_a - 8.0f : tc_r + MLX90640_KELVIN_OFFSET;

  frame_param->tk_a_r = _mlx90640_tk_a_r(tk_a, tk_r, emissivity);

#if MLX90640_TGC_AVAILABLE
  frame_param->pixel_tgc[0] =
      _mlx90640_frame_pixel_tgc_value(frame, 0) * frame_param->gain -
      calib->pixel_tgc.offset[0] *
          (1.0f + calib->pixel_tgc.kta * frame_param->delta_ta) *
          (1.0f + calib->pixel_tgc.kv * frame_param->delta_vdd);

  mlx90640_real_t pixel_tgc_offset_interleaved = 0;

  if (frame_param->pattern == MLX90640_PATTERN_INTERLEAVED) {
    pixel_tgc_offset_interleaved += calib->pixel_tgc.k1;
  }

  frame_param->pixel_tgc[1] =
      _mlx90640_frame_pixel_tgc_value(frame, 1) * frame_param->gain -
      (calib->pixel_tgc.offset[1] + pixel_tgc_offset_interleaved) *
          (1.0f + calib->pixel_tgc.kta * frame_param->delta_ta) *
          (1.0f + calib->pixel_tgc.kv * frame_param->delta_vdd);
#endif
}

static inline int mlx90640_i2c_new_frame_reset(mlx90640_device_t *mlx90640) {
  MLX90640_TRY(mlx90640_i2c_set_reg(mlx90640, MLX90640_REG_STATUS_ADDRESS, 0u,
                                    MLX90640_REG_STATUS_NEW_FRAME));
  return MLX90640_CHECK;
}
static inline int mlx90640_i2c_frame_wait_sync(mlx90640_device_t *mlx90640,
                                               uint16_t *status) {
  *status = 0;
  while (!__mlx90640_u16_get_field(*status, MLX90640_REG_STATUS_NEW_FRAME)) {
    MLX90640_TRY(
        mlx90640_i2c_read_reg(mlx90640, MLX90640_REG_STATUS_ADDRESS, status));
  }
  MLX90640_TRY(mlx90640_i2c_new_frame_reset(mlx90640));
  return MLX90640_CHECK;
}

static inline int mlx90640_i2c_get_frame(mlx90640_device_t *mlx90640) {
  if (!mlx90640) {
    return MLX90640_ERR_DEVICE_MISSING;
  }
  MLX90640_TRY(mlx90640_i2c_new_frame_reset(mlx90640));

  uint16_t status = 0;

  MLX90640_TRY(mlx90640_i2c_frame_wait_sync(mlx90640, &status));

  MLX90640_TRY(mlx90640_i2c_read_frame_param(mlx90640));
  mlx90640->frame_param.subpage =
      (uint8_t)__mlx90640_u16_get_field(status, MLX90640_REG_STATUS_SUBPAGE);
  mlx90640_set_frame_param(&mlx90640->frame_param, mlx90640->i2c_buffer.data,
                           &mlx90640->calib, mlx90640->emissivity,
                           mlx90640->tc_r);

  MLX90640_TRY(
      mlx90640_i2c_read_frame_pixels(mlx90640, mlx90640->ymin, mlx90640->ymax));

  // Check if valid transfer
  MLX90640_TRY(
      mlx90640_i2c_read_reg(mlx90640, MLX90640_REG_STATUS_ADDRESS, &status));

  if (__mlx90640_u16_get_field(status, MLX90640_REG_STATUS_NEW_FRAME)) {

    return MLX90640_ERR_I2C_SCL_TOO_SLOW;
  }

  return MLX90640_CHECK;
}

static inline void mlx90640_frame_tc_object_approx(
    mlx90640_real_t image[MLX90640_PIXEL_Y_SIZE][MLX90640_PIXEL_X_SIZE],
    mlx90640_device_t *mlx90640) {

  mlx90640_real_t pixel;
  mlx90640_calib_pixel_t *calib_pixel;

  const size_t ymin = mlx90640->ymin;
  const size_t ymax = mlx90640->ymax;
  const mlx90640_real_t *ema = mlx90640->ema;

  for (size_t y = ymin; y < ymax; y++) {
    for (size_t x = 0; x < MLX90640_PIXEL_X_SIZE; x++) {

      calib_pixel = &mlx90640->pixel[y][x];

      //@todo: verify properly
      // if (calib_pixel->accuracy_zone != mlx90640->zone) {
      //   // image[y][x] = MLX90640_NAN;
      //   continue;
      // }

      if (calib_pixel->subpage[mlx90640->frame_param.pattern] !=
          mlx90640->frame_param.subpage) {
        continue;
      }

      pixel = (mlx90640_real_t)((int16_t)__mlx90640_bytes_to_u16(
          mlx90640->i2c_buffer.data, MLX90640_PIXEL_INDEX(y, x)));

      pixel =
          _mlx90640_pixel_ir(&pixel, &mlx90640->pixel[y][x], &mlx90640->calib,
                             &mlx90640->frame_param, mlx90640->emissivity);
      pixel = _mlx90640_pixel_tk_object_fast(&pixel, 1.0f,
                                             mlx90640->frame_param.tk_a_r) -
              MLX90640_KELVIN_OFFSET;

      image[y][x] = ema[1] * image[y][x] + ema[0] * pixel;
    }
  }
}

static inline void mlx90640_frame_tc_object(
    mlx90640_real_t image[MLX90640_PIXEL_Y_SIZE][MLX90640_PIXEL_X_SIZE],
    mlx90640_device_t *mlx90640) {
  mlx90640_real_t pixel;
  mlx90640_calib_pixel_t *calib_pixel;

  const size_t ymin = mlx90640->ymin;
  const size_t ymax = mlx90640->ymax;
  const mlx90640_real_t *ema = mlx90640->ema;

  for (size_t y = ymin; y < ymax; y++) {
    for (size_t x = 0; x < MLX90640_PIXEL_X_SIZE; x++) {

      calib_pixel = &mlx90640->pixel[y][x];

      //@todo: verify properly
      // if (calib_pixel->accuracy_zone != mlx90640->zone) {
      //   // image[y][x] = MLX90640_NAN;
      //   continue;
      // }

      if (calib_pixel->subpage[mlx90640->frame_param.pattern] !=
          mlx90640->frame_param.subpage) {
        continue;
      }

      pixel = (mlx90640_real_t)((int16_t)__mlx90640_bytes_to_u16(
          mlx90640->i2c_buffer.data, MLX90640_PIXEL_INDEX(y, x)));

      pixel =
          _mlx90640_pixel_ir(&pixel, &mlx90640->pixel[y][x], &mlx90640->calib,
                             &mlx90640->frame_param, mlx90640->emissivity);
      pixel = _mlx90640_pixel_tk_object_extended(&pixel, &mlx90640->frame_param,
                                                 y, x, mlx90640->tk_corner) -
              MLX90640_KELVIN_OFFSET;

      image[y][x] = ema[1] * image[y][x] + ema[0] * pixel;
    }
  }
}

typedef struct mlx90640_uart_backend_ctx_t mlx90640_uart_backend_ctx_t;

typedef struct {
  mlx90640_uart_backend_ctx_t *backend_ctx;
  int (*write)(mlx90640_uart_backend_ctx_t *backend_ctx,
               const uint8_t *write_buffer, const size_t write_size);
} mlx90640_uart_debug_t;

static inline int
mlx90640_debug_i2c_buffer_data(const mlx90640_uart_debug_t *uart_debug,
                               const mlx90640_device_t *mlx90640) {
  if ((!uart_debug) || (!mlx90640)) {
    return MLX90640_ERR_DEVICE_MISSING;
  }
  const static uint8_t header[2] = {0x90, 0x64u};
  uart_debug->write(uart_debug->backend_ctx, header, MLX90640_WORD_BYTES);
  uart_debug->write(uart_debug->backend_ctx, mlx90640->i2c_buffer.data,
                    MLX90640_WORD_BYTES * MLX90640_I2C_BUFFER_DATA_SIZE);
  return MLX90640_CHECK;
}

static inline int
mlx90640_debug_calib_pixel(const mlx90640_uart_debug_t *uart_debug,
                           const mlx90640_device_t *mlx90640) {
  if ((!uart_debug) || (!mlx90640)) {
    return MLX90640_ERR_DEVICE_MISSING;
  }

  const static uint8_t header[2] = {0x90, 0x65u};

  uart_debug->write(uart_debug->backend_ctx, header, MLX90640_WORD_BYTES);

  // If struct is not packed you won't be able to retrieve all the calibration
  // pixels value, which is a debug objective here along with checking the
  // actual values.
  uart_debug->write(uart_debug->backend_ctx, (uint8_t *)mlx90640->pixel,
                    MLX90640_PIXEL_Y_SIZE * MLX90640_PIXEL_X_SIZE *
                        sizeof(mlx90640_calib_pixel_t));
  return MLX90640_CHECK;
}
static inline int
mlx90640_debug_calib_param(const mlx90640_uart_debug_t *uart_debug,
                           const mlx90640_device_t *mlx90640) {
  if ((!uart_debug) || (!mlx90640)) {
    return MLX90640_ERR_DEVICE_MISSING;
  }
  const static uint8_t header[2] = {0x90, 0x66u};
  uart_debug->write(uart_debug->backend_ctx, header, MLX90640_WORD_BYTES);

  uart_debug->write(uart_debug->backend_ctx, (uint8_t *)&mlx90640->calib,
                    sizeof(mlx90640_calib_param_t));
  return MLX90640_CHECK;
}

static inline int
mlx90640_debug_frame_param(const mlx90640_uart_debug_t *uart_debug,
                           const mlx90640_device_t *mlx90640) {
  if ((!uart_debug) || (!mlx90640)) {
    return MLX90640_ERR_DEVICE_MISSING;
  }
  const static uint8_t header[2] = {0x90, 0x67u};
  uart_debug->write(uart_debug->backend_ctx, header, MLX90640_WORD_BYTES);

  uart_debug->write(uart_debug->backend_ctx, (uint8_t *)&mlx90640->frame_param,
                    sizeof(mlx90640_frame_param_t));
  return MLX90640_CHECK;
}

static inline int mlx90640_debug_image(
    mlx90640_uart_debug_t *uart_debug,
    mlx90640_real_t image[MLX90640_PIXEL_Y_SIZE][MLX90640_PIXEL_X_SIZE]) {
  if ((!uart_debug) || (!image)) {
    return MLX90640_ERR_DEVICE_MISSING;
  }
  const static uint8_t header[2] = {0x90, 0x68u};
  uart_debug->write(uart_debug->backend_ctx, header, MLX90640_WORD_BYTES);

  uart_debug->write(uart_debug->backend_ctx, (uint8_t *)image,
                    MLX90640_PIXEL_Y_SIZE * MLX90640_PIXEL_X_SIZE *
                        sizeof(mlx90640_real_t));
  return MLX90640_CHECK;
}

#ifdef __cplusplus
}
#endif

#endif /* MLX90640_H */