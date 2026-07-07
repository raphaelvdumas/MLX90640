// This example script has been successfully built and flashed onto a
// ESP32-WROVER-E using ESP-IDF v6.1-dev-6126-ge9da155a726.
#include "freertos/FreeRTOS.h"

#include "driver/i2c_master.h"
#include "driver/uart.h"

#include <stddef.h>
#include <stdint.h>

//---- Configure the driver before including MLX90640.h.
//
// By default, the driver uses:
//   - single precision (float)
//   - math.h for computations
//   - thermal gain compensation (TGC), even if not supported by your device.
//   See example inside app_main below to ensure if TGC should be enabled or
//   not before modification.
//
//-- Uncomment to use double precision
//
// #define MLX90640_USE_DOUBLE_PRECISION 1

//-- Uncomment to use math.h for computations
// (set to 1 by default)
//
// #define MLX90640_USE_MATH_H 1

//-- If no thermal gain compensation (TGC) is available for your device,
// uncomment to remove TGC from computations.
//
// #define MLX90640_TGC_AVAILABLE 0
#include "MLX90640.h"

//---- This section is for debugging live data using UART. Skip and comment if
// not needed.
//-- If you need debugging, a Python script to receive and parse the data is
// provided in the "uart_debug" folder. Each data begins with a header (here a
// word) to identify the debug function called and parse its data.

//-- esp-idf internal uart initialization (verify parameters for your use case)
static const uart_port_t uart_num = UART_NUM_0;
void mlx90640_uart_init() {
  static const uart_config_t uart_config = {
      .baud_rate = 921600,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
  };

  ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
  ESP_ERROR_CHECK(uart_set_pin(uart_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                               UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_driver_install(uart_num, 4096, 0, 0, NULL, 0));
}

//-- To debug, it requires a wrapper to the 'uart_write_bytes' of esp-idf to
// internally call it and send data itself.
int mlx90640_esp_uart_debug(mlx90640_uart_backend_ctx_t *backend_ctx,
                            const uint8_t *write_buffer,
                            const size_t write_size);
int mlx90640_esp_uart_debug(mlx90640_uart_backend_ctx_t *backend_ctx,
                            const uint8_t *write_buffer,
                            const size_t write_size) {
  return uart_write_bytes(uart_num, write_buffer, write_size);
}

//---- This section is to create a wrapper to esp-idf I2C functions. Don't skip.
//-- To communicate with the device, the driver needs wrappers to functions of
// an I2C backend:
//   - to read into the device RAM / EEPROM (here, i2c_master_transmit_receive)
//   - to write values into device registers (here, i2c_master_transmit to
//   write)
//   - a delay of your choice in ms for power on (here, vTaskDelay as an
//   example)
// Everything else is handled internally by the driver (buffers, transfer sizes,
// ...)
//
// You can define a context struct containing all the input variables needed.
// Here, we only need a 'i2c_master_dev_handle_t' variable. This struct can be
// optional. It is not verified or modified at any time by the driver itself.
struct mlx90640_i2c_backend_ctx_t {
  i2c_master_dev_handle_t dev_handle;
};

// Function wrappers definitions to avoid name collision. The driver checks at
// each call and throws an error if invalid.
int mlx90640_esp_write_read(mlx90640_i2c_backend_ctx_t *ctx,
                            const uint8_t *write_buffer, size_t write_size,
                            uint8_t *read_buffer, size_t read_size);
int mlx90640_esp_delay_ms(mlx90640_i2c_backend_ctx_t *ctx, uint32_t ms);
int mlx90640_esp_write(mlx90640_i2c_backend_ctx_t *ctx,
                       const uint8_t *write_buffer, size_t write_size);

// Implementation of the write/read wrapper
int mlx90640_esp_write_read(mlx90640_i2c_backend_ctx_t *ctx,
                            const uint8_t *write_buffer, size_t write_size,
                            uint8_t *read_buffer, size_t read_size) {
  i2c_master_dev_handle_t dev_handle =
      ctx->dev_handle; // Retrieve the dev_handle from the context
  return i2c_master_transmit_receive(dev_handle, write_buffer, write_size,
                                     read_buffer, read_size,
                                     pdMS_TO_TICKS(1000));
}

// Implementation of the delay wrapper (vTaskDelay here)
int mlx90640_esp_delay_ms(mlx90640_i2c_backend_ctx_t *ctx, uint32_t ms) {
  vTaskDelay(pdMS_TO_TICKS((uint32_t)ms));
  return 0;
};

// Implementation of the write wrapper
int mlx90640_esp_write(mlx90640_i2c_backend_ctx_t *ctx,
                       const uint8_t *write_buffer, size_t write_size) {
  i2c_master_dev_handle_t dev_handle =
      ctx->dev_handle; // Retrieve the dev_handle from the context
  return i2c_master_transmit(dev_handle, write_buffer, write_size,
                             pdMS_TO_TICKS(1000));
}

//-- The driver does not store the image buffer internally to remain flexible
// depending on use case.
// A simple image buffer is given here as an example.
static mlx90640_real_t image[MLX90640_PIXEL_Y_SIZE][MLX90640_PIXEL_X_SIZE];

void app_main(void) {

  //-- esp-idf internal I2C initialization (verify parameters for your use case)
  i2c_master_bus_config_t i2c_mst_config = {
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .i2c_port = -1,
      .scl_io_num = GPIO_NUM_13,
      .sda_io_num = GPIO_NUM_14,
      .glitch_ignore_cnt = 7,
  };
  i2c_master_bus_handle_t bus_handle;

  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

  i2c_device_config_t dev_cfg = {
      .dev_addr_length = I2C_ADDR_BIT_LEN_7,
      .device_address = 0x33,
      .scl_speed_hz = 400000,
  };

  i2c_master_dev_handle_t dev_handle;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

  //---- This section is for debugging live data using UART. Skip and comment if
  // not needed.
  // Initialize UART and create a debug instance. No backend needed here.
  mlx90640_uart_init();
  mlx90640_uart_debug_t mlx90640_debug = {.backend_ctx = NULL,
                                          .write = &mlx90640_esp_uart_debug};

  //---- This section is to create an instance of the device and initialize.
  //-- Create an instance of mlx90640 device
  static mlx90640_device_t mlx90640;

  //-- Create a context and a I2C bus to the defined wrappers
  mlx90640_i2c_backend_ctx_t mlx_i2c_context = {.dev_handle = dev_handle};
  mlx90640_i2c_bus_t mlx_i2c_handles = {.backend_ctx = &mlx_i2c_context,
                                        .write_read = &mlx90640_esp_write_read,
                                        .write = &mlx90640_esp_write,
                                        .delay_ms = &mlx90640_esp_delay_ms};

  //-- Define an actual configuration for the mlx90640
  mlx90640_config_t mlx90640_config = {
      .subpage = MLX90640_SUBPAGE_BOTH,
      .pattern = MLX90640_PATTERN_INTERLEAVED,
      .bitdepth = MLX90640_BITDEPTH_19,
      .fps = MLX90640_FPS_4,
  };

  //-- Initialize device with config and i2c_bus.
  //
  // Internally, on initialization, the device directly uses I2C to set
  // variables needed for computation. If an error happens at any point, it
  // skips everything and forwards it directly. Therefore, you can handle what
  // happens next depending on the error value. To avoid collisions, error
  // values 90640+ are internal driver errors. A 0 means no error.
  int err = mlx90640_init(&mlx90640, &mlx90640_config, &mlx_i2c_handles);
  if (err != MLX90640_CHECK) {
    // Handle the error
  }

  //-- Verify if TGC is available to set MLX90640_TGC_AVAILABLE.
  // This function won't be available once MLX90640_TGC_AVAILABLE is 0.
  //
  // uint8_t tgc_available = mlx90640_is_tgc_available(&mlx90640);

  //---- Examples to set optional input variables
  //-- The emissivity (1.0f by default).
  mlx90640_set_emissivity(&mlx90640, 0.89f);

  //-- The reflected temperature in °C (NAN by default).
  mlx90640_set_tc_r(&mlx90640, 24.0f);

  //-- An exponential moving average (EMA, 1.0f by default).
  mlx90640_set_ema_value(&mlx90640, 0.7f);

  //-- Debug over UART the calibration pixels value to reconstruct an image.
  // Uncomment if needed.
  //
  // mlx90640_debug_calib_pixel(&mlx90640_debug, &mlx90640);

  //-- Main loop
  while (1) {
    //-- Get the latest frame available on the device (blocked wait
    // loop)
    err = mlx90640_i2c_get_frame(&mlx90640);
    if (err != MLX90640_CHECK) {
      // Handle the error
    }

    //-- Compute object temperature in Celsius (tc) with an approximate but fast
    // method.
    mlx90640_frame_tc_object_approx(image, &mlx90640);

    //-- Or compute object temperature in Celsius (tc) without approximation but
    // slower method. Calling 'mlx90640_frame_tc_object_approx' then
    // 'mlx90640_frame_tc_object' compute the image twice. Choose either one
    // but not both.
    mlx90640_frame_tc_object(image, &mlx90640);

    //-- You can also update the device configuration in real time by modifying
    // any of the field, here the pattern as an example,
    mlx90640_config.pattern = MLX90640_PATTERN_INTERLEAVED;
    // and update it via I2C:
    err = mlx90640_i2c_set_config(&mlx90640);
    if (err != MLX90640_CHECK) {
      // Handle the error
    }

    //-- Debug over UART the data buffer containing the latest raw pixels
    // values.
    // Uncomment if needed.
    //
    // mlx90640_debug_i2c_buffer_data(&mlx90640_debug, &mlx90640);

    //-- Debug over UART the image and computed object temperature.
    // Uncomment if needed.
    //
    // mlx90640_debug_image(&mlx90640_debug, image);
  }
}