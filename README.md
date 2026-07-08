# MLX90640
A header-only, configurable-precision, backend-agnostic I2C driver for the [MLX90640 thermal sensor](https://www.melexis.com/en/product/mlx90640/far-infrared-thermal-sensor-array).

Include the header, wrap your I2C driver, and compile with GCC.

## Features

- Portable
- Lightweight with standard C headers (`stdint.h`, `stddef.h`, `math.h` optional)
- Native single-precision floating-point processing
- Supports double-precision floating-point processing
- No dynamic memory allocation
- Backend-agnostic I2C interface
- Live debugging with UART streaming and Python visualization

![MLX90640live](docs/images/coffee_break.gif)

## Current Status

### Testing

Currently, the driver, I2C backend, and debugging tools have only been tested with ESP-IDF ([see ESP-IDF example](examples/mlx90640_esp_idf_example.c)). Further testing is needed.

### Documentation

The [official MLX90640 datasheet](https://media.melexis.com/-/media/files/documents/datasheets/mlx90640-datasheet-melexis.pdf?rev=637118278440000000) is included in [`docs/`](docs/MLX90640-Datasheet-Melexis.pdf) for convenience.

However, the documentation of `MLX90640.h` is not written yet. Don't try to build the docs with Doxygen, it will almost surely fail.
