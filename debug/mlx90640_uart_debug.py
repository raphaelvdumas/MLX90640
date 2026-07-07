import serial
import numpy as np
import cv2
import matplotlib.pyplot as plt 

#-- Parameters to receive debug data with pyserial. Ensure these match
# your UART definition in your main.
PORT, BAUDRATE = "COM3", 921600
ser = serial.Serial(PORT, BAUDRATE, timeout=1)

#-- These are the macros used before including the header "MLX90640.h". 
# If no macros were defined, these are the default values, don't modify.
MLX90640_USE_DOUBLE_PRECISION = False
MLX90640_TGC_AVAILABLE = True
ENDIANNESS = '<' # Except the endianness if needed (little by default)

#-- These are the definitions inside "MLX90640.h"
# don't modify.
PATTERN_SIZE = 2
PIXEL_Y_SIZE, PIXEL_X_SIZE = 24, 32
PIXEL_SIZE = PIXEL_Y_SIZE*PIXEL_X_SIZE
BUFFER_DATA_Y_SIZE, BUFFER_DATA_X_SIZE = 26, 32
BUFFER_DATA_SIZE = BUFFER_DATA_Y_SIZE*BUFFER_DATA_X_SIZE

dtype_real_t = np.dtype(f'{ENDIANNESS}f8' if MLX90640_USE_DOUBLE_PRECISION else f'{ENDIANNESS}f4')
dtype_word_t = np.dtype('>u2')
fields_calib_pixel = [
    ("offset", dtype_real_t),
    ("kta", dtype_real_t),
    ("kv", dtype_real_t),
    ("interleaved offset", dtype_real_t),
    ("alpha", dtype_real_t),
    ("subpage0", np.uint8),
    ("subpage1", np.uint8),
    ("accuracy zone", np.uint8),
    ("outlier", np.uint8),
]
if MLX90640_USE_DOUBLE_PRECISION:
    fields_calib_pixel.append(("__pad", np.uint8, 4))
dtype_calib_pixel = np.dtype(fields_calib_pixel)


fields_pixel_tgc = [
    ("offset", dtype_real_t, PATTERN_SIZE),
    ("alpha", dtype_real_t, PATTERN_SIZE),
    ("kv", dtype_real_t),
    ("kta", dtype_real_t),
    ("k1", dtype_real_t)
]
dtype_pixel_tgc = np.dtype(fields_pixel_tgc)

tgc_fields = [("pixel_tgc", dtype_pixel_tgc), 
              ("tgc", dtype_real_t)] if MLX90640_TGC_AVAILABLE else []
fields_calib_param = [
    *tgc_fields,
    ("gain", dtype_real_t),
    ("vdd25", dtype_real_t),
    ("kvdd", dtype_real_t),
    ("kvptat", dtype_real_t),
    ("vptat25", dtype_real_t),
    ("ktptat", dtype_real_t),
    ("ksta", dtype_real_t),
    ("bitdepth", np.uint8),
    ("alphaptat", np.uint8)
]
dtype_calib_param = np.dtype(fields_calib_param)

frame_tgc_fields = [("pixel_tgc0", dtype_real_t), ("pixel_tgc1", dtype_real_t)] if MLX90640_TGC_AVAILABLE else []
fields_frame_param = [
    *frame_tgc_fields,
    ("gain", dtype_real_t),
    ("delta_vdd", dtype_real_t),
    ("delta_ta", dtype_real_t),
    ("tk_a_r", dtype_real_t),
    ("subpage", np.uint8),
    ("pattern", np.uint8),
    ("bitdepth", np.uint8),
]
dtype_frame_param = np.dtype(fields_frame_param)


#-- List of available debug functions with each item containing:
#   - "header": header used
#   - "sent_by": the actual implemented debug function inside "MLX90640.h"
#   - "size": number of bytes to actually read 
#   - "dtype": numpy dtype to infer all the struct fields from raw byte buffer
#   - "parse": lambda function to automatically reshape and interpret data as a float type
debug_symbols = [
    {"header": b'\x90\x64', "sent_by": "mlx90640_debug_i2c_buffer_data", "size": dtype_word_t.itemsize*BUFFER_DATA_SIZE, "dtype": dtype_word_t,
      "parse": lambda x: x.reshape(BUFFER_DATA_Y_SIZE, BUFFER_DATA_X_SIZE).astype(np.float32)}, 
    {"header": b'\x90\x65', "sent_by": "mlx90640_debug_calib_pixel", "size": dtype_calib_pixel.itemsize*PIXEL_SIZE, "dtype": dtype_calib_pixel,
      "parse": lambda x: x.reshape(PIXEL_Y_SIZE,PIXEL_X_SIZE)}, 
    {"header": b'\x90\x66', "sent_by": "mlx90640_debug_calib_param", "size": dtype_calib_param.itemsize, "dtype": dtype_calib_param,
      "parse": lambda x: x}, 
    {"header": b'\x90\x67', "sent_by": "mlx90640_debug_frame_param", "size": dtype_frame_param.itemsize, "dtype": dtype_frame_param,
      "parse": lambda x: x}, 
    {"header": b'\x90\x68', "sent_by": "mlx90640_debug_image", "size": dtype_real_t.itemsize*PIXEL_SIZE, "dtype": dtype_real_t,
      "parse": lambda x: x.reshape(PIXEL_Y_SIZE,PIXEL_X_SIZE).astype(np.float32)}
]


#-- Always read into buf
buf = bytearray()
while True:
    buf += ser.read(128)

    #-- Test all headers and retrieve the first appearing inside the buffer
    ibuffer, header, which_symbol = 1e16, b'', 0
    for isymbol in range(len(debug_symbols)):
        
        cheader = debug_symbols[isymbol]["header"]
        cidx = buf.find(cheader) 
        if (cidx > - 1) and (cidx < ibuffer):
            ibuffer = cidx
            which_symbol = isymbol
            header = cheader

    #-- If a header was found
    if len(header):
        print("received from: ", debug_symbols[which_symbol]["sent_by"])

        #-- Retrieve all necessary bytes depending on the required size
        nb = debug_symbols[which_symbol]["size"]
        buf = buf[ibuffer+2:]
        lb = len(buf)
        if lb >= nb:
            data = buf[:nb]
            buf = buf[nb:]
        else:
            data = buf[:lb] + ser.read(nb - lb)
            buf = bytearray()

        #-- Interpret buffer as a numpy dtype and parse with the lambda
        data = debug_symbols[which_symbol]["parse"](np.frombuffer(data, dtype=debug_symbols[which_symbol]["dtype"]))  


        #-- Then do whatever you want with the data, three examples are given, the rest is up to your needs (you can store frames, raw buffers etc)
        if "debug_image" in debug_symbols[which_symbol]["sent_by"]:
            cv2.namedWindow("MLX90640 image", cv2.WINDOW_NORMAL)
            img = cv2.normalize(data, None, 0, 255, cv2.NORM_MINMAX)
            img = img.astype(np.uint8)
            img = cv2.applyColorMap(img, cv2.COLORMAP_INFERNO)
            cv2.imshow("MLX90640 image", img)
            cv2.waitKey(1)
        elif "i2c_buffer_data" in debug_symbols[which_symbol]["sent_by"]:
            # Show buffer data when getting a frame
            data = data[:PIXEL_Y_SIZE, :] # Crop image to show raw pixel values 
            cv2.namedWindow("MLX90640 buffer", cv2.WINDOW_NORMAL)
            img = cv2.normalize(data, None, 0, 255, cv2.NORM_MINMAX)
            img = img.astype(np.uint8)
            img = cv2.applyColorMap(img, cv2.COLORMAP_INFERNO)
            cv2.imshow("MLX90640 buffer", img)
            cv2.waitKey(1)
        elif "calib_pixel" in debug_symbols[which_symbol]["sent_by"]:
            # Show each field in calibration pixels as an image
            plt.figure()
            for i, field in enumerate(dtype_calib_pixel.names, 1):
                if (i>9):
                    break
                plt.subplot(3, 3, i)
                plt.imshow(data[field])
                plt.title(field)
                plt.colorbar()
            plt.tight_layout()
            plt.show()
        else:
            print("not implemented for ", debug_symbols[which_symbol]["sent_by"])