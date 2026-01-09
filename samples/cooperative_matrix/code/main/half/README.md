half  
====  
  
IEEE 754 16-bit Floating Point Format

This is a simple 16 bit floating point storage interface. It is intended to 
serve as a learning aid for students, and is not in an optimized form.

This was designed for the following scenarios:

  1. Reduced file storage costs for terrain maps consisting of floating 
     point 3D position, normal, texture coordinates, and color data.

  2. In memory support for the half format as supported by NVIDIA and AMD
     GPUs.    


#### Convention:  

     Float32:                     Float16:

     1 bit sign                   1 bit sign
     8 bit exponent               5 bit exponent
     23 bit mantissa              10 bit mantissa
     Bias of 127                  Bias of 15  


#### Special Values:

     +-Zero:          s,  0e, 0m
     +-Denormalized:  s,  0e, (1 -> max)m
     +-Normalized:    s, (1 -> [max-1])e, m
     +-Infinity:      s, (all 1)e, (all 0s)m
     +-SNaN:          s, (all 1)e, (1 -> [max-high_bit])m
     +-QNaN:          s, (all 1)e, (high_bit -> all 1s)m