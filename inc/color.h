#ifndef COLOR_H
#define COLOR_H
#include "gpu.h"

typedef enum{
    RGBA8    = 0,
    RGB8     = 1,
	RGB5A1   = 2,
    RGB565   = 3,
    RGBA4    = 4,
    IA8      = 5,
    HILO8    = 6,
    I8       = 7,
    A8       = 8,
    IA4      = 9,
    I4       = 10,
    A4       = 11,
    ETC1     = 12,
    ETC1A4   = 13,
    // TODO: Support for the other formats is not implemented, yet.
    // Seems like they are luminance formats and compressed textures.
} TextureFormat;

//Stored as RGBA8
typedef struct  {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
} color;

//Decode one pixel into RGBA8 color format
void color_decode(unsigned char* input, const TextureFormat format, color *output);

//Encode one pixel from RGBA8 color format
void color_encode(color *input, const TextureFormat format, u8* output);

clov4 DecodeRGBA8(u8* input);

clov4 DecodeRGB8(u8* input);

clov4 DecodeRGB565(u8* input);

clov4 DecodeRGB5A1(u8* input);

clov4 DecodeRGBA4(u8* input);

u32 DecodeD16(const u8* bytes);

u32 DecodeD24(const u8* bytes);

void EncodeRGBA8(const clov4 color, u8* bytes);

void EncodeRGB8(const clov4 color, u8* bytes);

void EncodeRGB565(const clov4 color, u8* bytes);

void EncodeRGB5A1(const clov4 color, u8* bytes);

void EncodeRGBA4(const clov4 color, u8* bytes);

void EncodeD16(u32 value, u8* bytes);

void EncodeD24(u32 value, u8* bytes);

void EncodeD24S8(u32 depth, u8 stencil, u8* bytes);

void EncodeD24X8(u32 depth, u8* bytes);

void EncodeX24S8(u8 stencil, u8* bytes);

/// Convert a 1-bit color component to 8 bit
u8 Convert1To8(u8 value);

/// Convert a 4-bit color component to 8 bit
u8 Convert4To8(u8 value);

/// Convert a 5-bit color component to 8 bit
u8 Convert5To8(u8 value);

/// Convert a 6-bit color component to 8 bit
u8 Convert6To8(u8 value);

/// Convert a 8-bit color component to 1 bit
u8 Convert8To1(u8 value);

/// Convert a 8-bit color component to 4 bit
u8 Convert8To4(u8 value);

/// Convert a 8-bit color component to 5 bit
u8 Convert8To5(u8 value);

/// Convert a 8-bit color component to 6 bit
u8 Convert8To6(u8 value);


// Interleave the lower 3 bits of each coordinate to get the intra-block offsets, which are
// arranged in a Z-order curve. More details on the bit manipulation at:
// https://fgiesen.wordpress.com/2009/12/13/decoding-morton-codes/
//
u32 MortonInterleave(u32 x, u32 y);

// Calculates the offset of the position of the pixel in Morton order 
u32 GetMortonOffset(u32 x, u32 y, u32 bytes_per_pixel);


#endif