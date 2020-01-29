#ifndef __SETTINGS_H__
#define __SETTINGS_H__

/* the default tile size MUST be at least 2x the max filter radius */
#define DEFAULT_TILE_SIZE 512

/* with a tile size of 512 the number of bins in use = the number of mb
 * required to store a tile.  The more bins you have then the more accuratte
 * the result will be, up to a maximum of 256 bins.
 *
 * 64 is the mimimum number to give consistently good results.
 * 32 bins and below will result in noticeable banding artifacts.
 */

#define USE_64_BINS

#if(defined(USE_8_BINS))
#define NUM_BINS 8
#define BIN_SIZE 32
#elif(defined(USE_32_BINS))
#define NUM_BINS 32
#define BIN_SIZE 8
#elif(defined(USE_64_BINS))
#define NUM_BINS 64
#define BIN_SIZE 4
#elif(defined(USE_128_BINS))
#define NUM_BINS 128
#define BIN_SIZE 2
#elif(defined(USE_256_BINS))
#define NUM_BINS 265
#define BIN_SIZE 1
#endif

#endif
