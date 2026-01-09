
//
// @@ Copyright (c) 2014, ramenhut. All rights reserved @@
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#ifndef __HALF_H__
#define __HALF_H__

typedef unsigned short                  UINT16;
typedef unsigned int                    UINT32;
typedef short                           INT16;
typedef int                             INT32;
typedef float                           FLOAT32;
typedef double                          FLOAT64;

// MODIFIED DUE TO CONFLICT WITH WINDOWS TYPEDEF
// typedef bool                            BOOL;
#define BOOL bool
// MODIFIED DUE TO CONFLICT WITH WINDOWS TYPEDEF

#ifndef CONST
#define CONST                           const
#endif

//
// FLOAT16 Helpers
//

#define HALF_SIGN_SHIFT                 (15)
#define HALF_EXP_SHIFT                  (10)
#define HALF_MANT_SHIFT                 (0)

#define HALF_SIGN_MASK                  (0x8000)
#define HALF_EXP_MASK                   (0x7C00)
#define HALF_MANT_MASK                  (0x03FF)

#define HALF_POS_INFINITY               (0x7C00)
#define HALF_NEG_INFINITY               (0xFC00)

#define GET_HALF_SIGN_BIT(x)            ((x) >> HALF_SIGN_SHIFT)
#define GET_HALF_EXP_BITS(x)            (((x) >> HALF_EXP_SHIFT) & 0x1F)
#define GET_HALF_MANT_BITS(x)           ((x) & HALF_MANT_MASK)

#define SET_HALF_SIGN_BIT(x,dest)       ((dest) = ((((x) << HALF_SIGN_SHIFT) & HALF_SIGN_MASK) | ( (dest) & ( HALF_EXP_MASK  | HALF_MANT_MASK ))))
#define SET_HALF_EXP_BITS(x,dest)       ((dest) = ((((x) << HALF_EXP_SHIFT)  & HALF_EXP_MASK)  | ( (dest) & ( HALF_SIGN_MASK | HALF_MANT_MASK ))))
#define SET_HALF_MANT_BITS(x,dest)      ((dest) = ((((x) << HALF_MANT_SHIFT) & HALF_MANT_MASK) | ( (dest) & ( HALF_SIGN_MASK | HALF_EXP_MASK ))))

//
// FLOAT32 Helpers
//

#define SINGLE_SIGN_SHIFT               (31)
#define SINGLE_EXP_SHIFT                (23)
#define SINGLE_MANT_SHIFT               (0)

#define SINGLE_SIGN_MASK                (0x80000000)
#define SINGLE_EXP_MASK                 (0x7F800000)
#define SINGLE_MANT_MASK                (0x007FFFFF)

#define SINGLE_POS_INFINITY             (0x7F800000)
#define SINGLE_NEG_INFINITY             (0xFF800000)

#define GET_SINGLE_SIGN_BIT(x)          ((x) >> SINGLE_SIGN_SHIFT)
#define GET_SINGLE_EXP_BITS(x)          (((x) >> SINGLE_EXP_SHIFT) & 0xFF)
#define GET_SINGLE_MANT_BITS(x)         ((x) & SINGLE_MANT_MASK)

#define SET_SINGLE_SIGN_BIT(x,dest)     ((dest) = ((((x) << SINGLE_SIGN_SHIFT) & SINGLE_SIGN_MASK) | ( (dest) & ( SINGLE_EXP_MASK  | SINGLE_MANT_MASK ))))
#define SET_SINGLE_EXP_BITS(x,dest)     ((dest) = ((((x) << SINGLE_EXP_SHIFT)  & SINGLE_EXP_MASK)  | ( (dest) & ( SINGLE_SIGN_MASK | SINGLE_MANT_MASK ))))
#define SET_SINGLE_MANT_BITS(x,dest)    ((dest) = ((((x) << SINGLE_MANT_SHIFT) & SINGLE_MANT_MASK) | ( (dest) & ( SINGLE_SIGN_MASK | SINGLE_EXP_MASK ))))

class FLOAT16
{
    UINT16 m_uiFormat;

public:

    FLOAT16();
    FLOAT16( CONST FLOAT16 & rhs );
    FLOAT16( CONST FLOAT32 & rhs );
    ~FLOAT16();

    //
    // Member operations
    //
    // (!) Note: the float16 (i.e. half) format is provided for storage purposes 
    //           only, and should not be used for computation. As a result, we do 
    //           not provide any arithmetic operators.
    //

    BOOL      operator == ( CONST FLOAT16 & rhs ) CONST;
    BOOL      operator != ( CONST FLOAT16 & rhs ) CONST;
    FLOAT16 & operator = ( CONST FLOAT16 & rhs );
    FLOAT16 & operator = ( CONST FLOAT32 & rhs );
              operator FLOAT32();

    //
    // Conversion control
    //

    static FLOAT32 ToFloat32( FLOAT16 rhs );
    static FLOAT16 ToFloat16( FLOAT32 rhs );    

    //
    // The faster variants handle only the most common normalized conversion case.
    // If a conversion requires QNaN, SNaN, Inf, or denormalized handling, do not
    // use these.
    //

    static FLOAT32 ToFloat32Fast( FLOAT16 rhs );
    static FLOAT16 ToFloat16Fast( FLOAT32 rhs );    
};

#undef BOOL

#endif // __HALF_H__
