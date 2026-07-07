# Using UART debug

The UART debugging requires Python 3. 

## Setting up

Create a virtual environment:
```bash
python -m venv .venv
.venv/Scripts/activate
python -m ensurepip
python -m pip install -U pip
```
then, you can install the dependencies
```bash
pip install -r ./debug/requirements.txt
```
and launch `mlx90640_uart_debug.py`.

## Python configuration

- Make sure that both MCU UART configuration and host configuration are *identical*.
- Make sure that the variables  
```python
MLX90640_USE_DOUBLE_PRECISION = False
MLX90640_TGC_AVAILABLE = True
```
are *identical* to your `MLX90640.h`. 
- Ensure endianness (little-endian by default):
```python
ENDIANNESS = '<'
```

## C debugging functions

The following functions provide access to internal driver data for debugging:
- `mlx90640_debug_calib_pixel`: access each pixel's calibration parameters. You only need to call this function once.
- `mlx90640_debug_calib_param`: access global calibration parameters. You only need to call this function once.
- `mlx90640_debug_i2c_buffer_data`: access __last__ raw I2C buffer data. You need to call this function _after_ getting a frame with I2C.
- `mlx90640_debug_frame_param`: access __last__ frame parameters required to reconstruct an image ($\Delta T_a$, $\Delta V_{dd}$, etc.). 
- `mlx90640_debug_image`: access __last__ computed object temperature image.
