# Computation Tricks

This section explains the computation optimizations implemented by the driver to ensure that:

- no unintended single-precision to double-precision promotion happens
- a fast evaluation of the fourth root can be achieved.

## Evalution of large powers of two

When directly evaluating large powers of $2^i$ with $i > 31$, a 32-bit integer representation cannot be used, as shifting above the available bit width results in undefined behavior. Using wider integer types or standard power functions introduces unnecessary conversions or promotions to double-precision, as done in the [official driver](https://github.com/melexis/mlx90640-library/blob/f6be7ca1d4a55146b705f3d347f84b773b29cc86/headers/MLX90640_API.h#L76).

To keep all computations within single-precision, the driver uses IEEE-754 binary32 representation and performs the operation by directly modifying the exponent field.

Following the IEEE-754 binary32 standard, the representation of $x$:
$$
x = \left(-1\right)^s\left(1.0 + m\right)2^{E - 127}
$$
with $m$ the mantissa and $E$ the exponent, which means that multiplication by $2^{\pm i}$ is:
$$
x 2^{\pm i} = \left(-1\right)^s\left(1.0 + m\right)2^{E\pm i - 127}~, 
$$
which is valid only when $1 \leq E \pm i \leq 254$.

Therefore, evaluation of large power of $2^i$ with $i > 32$ can be made without any promotion using unions and shifting the exponent. 

## A fast fourth root approximation

To obtain the object temperature \(T_{obj}\), the MLX90640 algorithm requires the computation of a fourth root:

$$
\sqrt[4]{x} = x^{1/4} = \left(x^{-\frac{1}{2}}\right)^{-\frac{1}{2}}
$$

which can be expressed using the inverse square root:

$$
\sqrt[4]{x} = \frac{1}{\sqrt{\frac{1}{\sqrt{x}}}}
$$

We can therefore reuse the concept of a fast inverse square root approximation algorithm from the source code of [Quake III Arena](https://github.com/id-Software/Quake-III-Arena/blob/master/code/game/q_math.c). It uses a bit-level approximation followed by a Newton-Raphson refinement step. It is fast and avoids a direct call to the standard library square root implementation.

This algorithm was adapted to compute the fourth root __for single-precision floating-point values only__. It removes the need to include `math.h` in this case, as a dedicated magic constant (`0x2f9a8354`) and a modified Newton-Raphson iteration to approximate \(\sqrt[4]{x}\) can be used, with:
$$
y_{n+1} = \frac{1}{4} \left(3 y_{n} + \frac{x}{{y_n}^3} \right)~.
$$

This method is faster and computationally cheaper than the standard approach, __but it is not mandatory__. The standard square-root implementation should be preferred when maximum numerical accuracy is required. This approximation is mainly intended for applications requiring reduced computational cost.

This algorithm is not available for double-precision floating-point values.