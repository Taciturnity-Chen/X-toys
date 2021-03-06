//
// mbrast.h
//
// Copyright (c) 2013 Matt Pharr
// Stanford cs348b, Assignment 1
//

#ifndef MBRAST_H
#define MBRAST_H 1

#include <vector>
#include <assert.h>
#include <math.h>
#include <stdint.h>

#define MAX_SAMPLES_PER_PIXEL 256

//  MBRAST_MODE:
//      NAIVE_RAST_2D: the original reference 2d rasterizer
//      SMART_RAST_2D: the smarter 2d rasterizer for step 4
//      BASIC_RAST_3D: the basic 3d rasterizer for step 5
//      INTER_RAST_3D: the interval 3d rasterizer for step 6
#define NAIVE_RAST_2D   0
#define SMART_RAST_2D   1
#define BASIC_RAST_3D   2
#define INTER_RAST_3D   3
#define BASIC_RAST_5D   4
#define MBRAST_MODE     INTER_RAST_3D


typedef struct _BBox2D {
    int Xmin;
    int Ymin;
    int Xmax;
    int Ymax;
}BBox2D;

//  define data structures to store points
struct Point
{
    float x, y, z, w;
    Point()
    {}
    void set(float xx, float yy, float zz, float ww)
    {
        x = xx; y = yy; z = zz; w = ww;
    }

    Point operator+(const Point& p) const
    {
        Point p2;
        p2.set(x + p.x, y + p.y, z + p.z, w + p.w);
        return p2;
    }

    Point operator*(const float f) const 
    {
        Point p;
        p.set(x * f, y * f, z * f, w * f);
        return p;
    }
    float x2d() {return x / w;}
    float y2d() {return y / w;}
    float z2d() {return z / w;}
};


// A shaded grid of micropolygons
struct ShadedGrid {
    // These give the number of micropolygon *vertices* in the u and v
    // directions.  Thus, the total number of micropolygons is
    // (nu-1)*(nv-1).
    int nu, nv;

    // The vertex positions and colors are laid out in a single array of
    // floats.  First come nu*nv values giving the vertex x coordinates at
    // shutter open time, then nu*nv y coordinates, nu*nv z coordinates,
    // and nu*nv w coordinates.  After this are the xyzw at shutter close,
    // finally followed by nu*nv red values, nu*nv green, and nu*nv blue
    // for the vertex colors.
    float *buf;

    //BBox2D box;
    // For convenience, the following methods do the initial indexing into
    // the buf array to give a pointer to the start of the corresponding
    // per-vertex values.
    const float *x0() const { return buf; }
    const float *y0() const { return buf + nu*nv; }
    const float *z0() const { return buf + 2 * nu*nv; }
    const float *w0() const { return buf + 3 * nu*nv; }

    const float *x1() const { return buf + 4 * nu*nv; }
    const float *y1() const { return buf + 5 * nu*nv; }
    const float *z1() const { return buf + 6 * nu*nv; }
    const float *w1() const { return buf + 7 * nu*nv; }

    const float *r() const { return buf + 8 * nu*nv; }
    const float *g() const { return buf + 9 * nu*nv; }
    const float *b() const { return buf + 10 * nu*nv; }
};


// A bucket representing a tile of the screen that we're rasterizing.  A
// separate Bucket is allocated for each processing thread, so that if
// we're doing multi-threaded rasterization, rasterizers can update their
// buckets directly without needing to worry about coordinating framebuffer
// updates with other threads.
class Bucket {
public:
    Bucket(int extent, int spp);
    ~Bucket();

    // Start() prepares the bucket for rasterizing the subset of the screen
    // from (xx0,yy0) up to but not including (xx1,yy1).
    void Start(int xx0, int yy0, int xx1, int yy1);

    // After processing for the bucket is done, Resolve() updates the given
    // output image with the down-sampled results for this bucket.
    void Resolve(uint8_t *image, int xRes, int yRes);

    // Convenience methods: R() returns the pointer to the bucket
    // framebuffer location for sample 's' in pixel (x,y).
    float *R(int x, int y, int s) {
        assert(x >= x0 && x < x1);
        assert(y >= y0 && y < y1);
        assert(s < kSamplesPerPixel);
        x -= x0;
        y -= y0;
        int pixelOffset = y * kExtent + x;
        int pixelSize = 4 * kSamplesPerPixel; // RGBZ -> 4
        int offset = pixelSize * pixelOffset + s;
        return &samples[offset];
    }
    // And G, B, and Z() do the equivalent for the other framebuffer
    // channels.
    float *G(int x, int y, int s) {
        assert(x >= x0 && x < x1);
        assert(y >= y0 && y < y1);
        assert(s < kSamplesPerPixel);
        x -= x0;
        y -= y0;
        int pixelOffset = y * kExtent + x;
        int pixelSize = 4 * kSamplesPerPixel; // RGBZ -> 4
        int offset = pixelSize * pixelOffset + kSamplesPerPixel + s;
        return &samples[offset];
    }
    float *B(int x, int y, int s) {
        assert(x >= x0 && x < x1);
        assert(y >= y0 && y < y1);
        assert(s < kSamplesPerPixel);
        x -= x0;
        y -= y0;
        int pixelOffset = y * kExtent + x;
        int pixelSize = 4 * kSamplesPerPixel; // RGBZ -> 4
        int offset = pixelSize * pixelOffset + 2 * kSamplesPerPixel + s;
        return &samples[offset];
    }
    float *Z(int x, int y, int s) {
        assert(x >= x0 && x < x1);
        assert(y >= y0 && y < y1);
        assert(s < kSamplesPerPixel);
        x -= x0;
        y -= y0;
        int pixelOffset = y * kExtent + x;
        int pixelSize = 4 * kSamplesPerPixel; // RGBZ -> 4
        int offset = pixelSize * pixelOffset + 3 * kSamplesPerPixel + s;
        return &samples[offset];
    }

    // The bucket's sample values are laid out in memory in order to enable
    // potential SIMD vectorization of the rasterizer.  High-performance
    // implementations can index into the pixel sample values directly, if
    // desired.
    // 
    // Pixels are laid out in column-major format.  If each pixel has
    // kSamplesPerPixel samples, each of which needs 4 float values (3 for
    // RGB and one for Z).  These samples are laid out with all
    // kSamplesPerPixel R values consecutive in memory for the pixel, then
    // all kSamplesPerPixel G values, and so forth.  Thus, if
    // samplesPerPixel is 16, we have:
    //
    // RRRRRRRRRRRRRRRR   // 16 R values for pixel (0,0)
    // GGGGGGGGGGGGGGGG   // 16 G values
    // BBBBBBBBBBBBBBBB   // 16 B values
    // ZZZZZZZZZZZZZZZZ   // 16 Z values
    // RRRRRRRRRRRRRRRR   // 16 R values for pixel (1, 0)
    // ...
    // ZZZZZZZZZZZZZZZZ   // 16 Z values for pixel (kXResolution-1, 0)
    // RRRRRRRRRRRRRRRR   // 16 R values for pixel (1, 1)
    // ...
    float *samples;

    int x0, y0, x1, y1;
    const int kExtent;
    const int kSamplesPerPixel;
};


// Basic interface that rasterizer implementations must implement.
class Rasterizer {
public:
    virtual ~Rasterizer() { }

    // Indicates whether it's safe to use multiple processing threads when
    // rasterizing; i.e. is the rasterizer implementation thread safe.
    virtual bool IsThreadSafe() const = 0;

    // Before rasterization starts, this method is called to provide the
    // rasterizer with all of the grids and to allow it to do any desired
    // per-preprocessing (e.g. computing bounding boxes, building data
    // structures over the grids) before rasteriztion begins.
    virtual void PreprocessGrids(const std::vector<ShadedGrid> &grids,
                                 int bucketEdgeLength, int xRes, int yRes) = 0;

    // Rasterize the grids into the provided Bucket of the image.  The
    // given number of intervals, numIntervals, should be used to bound the
    // micropolygons when doing 3D rasterization.
    virtual void Rasterize(const std::vector<ShadedGrid> &grids,
                           int numIntervals, Bucket *bucket) = 0;
};

// Returns a reference 2D rasterizer
Rasterizer *Create2DRasterizer();

// Returns your 3D rasterizer; you will implement this in rast3d.cpp. 
Rasterizer *Create3DRasterizer();

void InitSamples();

// Returns the (x,y,time) sample positions to use for the given pixel on
// the screen.  Note: the time values are guaranteed to be sorted from low
// to high; this fact may be useful for various culling algorithms.
void GetSamples(int xPixel, int yPixel, int samplesPerPixel,
                float xSamples[], float ySamples[],
                float timeSamples[]);

bool OverLapRect(int ax0, int ay0, int ax1, int ay1,
                 int bx0, int by0, int bx1, int by1);
#endif // MBRAST_H
