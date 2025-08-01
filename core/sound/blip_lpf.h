/* https://fiiir.com/ */


/*
note: use higher sampling rates for better de-aliasing  [384K, 768K]
      frontend can do nearest neighbor integer downsampling  [48K target w/ 24K cutoff]
*/


/* 1.16.15 */
enum { lpf_frac = 15 };
enum { lpf_scale = 32768 };


static int blip_lpf_cutoff = 0;  /* set to nyquist (1/2) of final output sampling rate - 0 = none */


#define LPF_TAPS(x) (buf_t) ((double) (x) * (double) (1UL << lpf_frac) * (double) lpf_scale)


#include "blip_lpf_48K.h"
#include "blip_lpf_96K.h"
#include "blip_lpf_192K.h"
#include "blip_lpf_384K.h"
#include "blip_lpf_768K.h"


#ifdef __cplusplus
extern "C" {
#endif

void set_blip_lowpass(int rate)
{
	blip_lpf_cutoff = rate;
}

#ifdef __cplusplus
}
#endif


static int blip_lpf_taps(int sample_rate)
{
	switch( sample_rate ) {
	case 48000: return blip_lpf_48K_taps;
	case 96000: return blip_lpf_96K_taps;
	case 192000: return blip_lpf_192K_taps;
	case 384000: return blip_lpf_384K_taps;
	case 768000: return blip_lpf_768K_taps;
	};

	return 0;
}


static void blip_lpf_stereo(int sample_rate, buf_t* out_l, buf_t* out_r, int delta_l, int delta_r)
{
	/* 31-bit * 15-bit = 46-bit >> 15 = 31-bit */

	if( blip_lpf_cutoff == 24000 ) {
		switch( sample_rate ) {
		case 48000:
			for( int lcv = 0; lcv < blip_lpf_48K_taps; lcv++ ) {
				out_l [lcv] += ((signed long long)blip_lpf_48K_24K[lcv] * delta_l) / lpf_scale;
				out_r [lcv] += ((signed long long)blip_lpf_48K_24K[lcv] * delta_r) / lpf_scale;
			}
			return;

		case 96000:
			for( int lcv = 0; lcv < blip_lpf_96K_taps; lcv++ ) {
				out_l [lcv] += ((signed long long)blip_lpf_96K_24K[lcv] * delta_l) / lpf_scale;
				out_r [lcv] += ((signed long long)blip_lpf_96K_24K[lcv] * delta_r) / lpf_scale;
			}
			return;

		case 192000:
			for( int lcv = 0; lcv < blip_lpf_192K_taps; lcv++ ) {
				out_l [lcv] += ((signed long long)blip_lpf_192K_24K[lcv] * delta_l) / lpf_scale;
				out_r [lcv] += ((signed long long)blip_lpf_192K_24K[lcv] * delta_r) / lpf_scale;
			}
			return;

		case 384000:
			for( int lcv = 0; lcv < blip_lpf_384K_taps; lcv++ ) {
				out_l [lcv] += ((signed long long)blip_lpf_384K_24K[lcv] * delta_l) / lpf_scale;
				out_r [lcv] += ((signed long long)blip_lpf_384K_24K[lcv] * delta_r) / lpf_scale;
			}
			return;

		case 768000:
			for( int lcv = 0; lcv < blip_lpf_768K_taps; lcv++ ) {
				out_l [lcv] += ((signed long long)blip_lpf_768K_24K[lcv] * delta_l) / lpf_scale;
				out_r [lcv] += ((signed long long)blip_lpf_768K_24K[lcv] * delta_r) / lpf_scale;
			}
			return;
		}
	}


	if( blip_lpf_cutoff == 48000 ) {
		switch( sample_rate ) {
		case 96000:
			for( int lcv = 0; lcv < blip_lpf_96K_taps; lcv++ ) {
				out_l [lcv] += ((signed long long)blip_lpf_96K_48K[lcv] * delta_l) / lpf_scale;
				out_r [lcv] += ((signed long long)blip_lpf_96K_48K[lcv] * delta_r) / lpf_scale;
			}
			return;

		case 192000:
			for( int lcv = 0; lcv < blip_lpf_192K_taps; lcv++ ) {
				out_l [lcv] += ((signed long long)blip_lpf_192K_48K[lcv] * delta_l) / lpf_scale;
				out_r [lcv] += ((signed long long)blip_lpf_192K_48K[lcv] * delta_r) / lpf_scale;
			}
			return;

		case 384000:
			for( int lcv = 0; lcv < blip_lpf_384K_taps; lcv++ ) {
				out_l [lcv] += ((signed long long)blip_lpf_384K_48K[lcv] * delta_l) / lpf_scale;
				out_r [lcv] += ((signed long long)blip_lpf_384K_48K[lcv] * delta_r) / lpf_scale;
			}
			return;

		case 768000:
			for( int lcv = 0; lcv < blip_lpf_768K_taps; lcv++ ) {
				out_l [lcv] += ((signed long long)blip_lpf_768K_48K[lcv] * delta_l) / lpf_scale;
				out_r [lcv] += ((signed long long)blip_lpf_768K_48K[lcv] * delta_r) / lpf_scale;
			}
			return;
		}
	}


	if( blip_lpf_cutoff == 96000 ) {
		switch( sample_rate ) {
		case 192000:
			for( int lcv = 0; lcv < blip_lpf_192K_taps; lcv++ ) {
				out_l [lcv] += ((signed long long)blip_lpf_192K_96K[lcv] * delta_l) / lpf_scale;
				out_r [lcv] += ((signed long long)blip_lpf_192K_96K[lcv] * delta_r) / lpf_scale;
			}
			return;

		case 384000:
			for( int lcv = 0; lcv < blip_lpf_384K_taps; lcv++ ) {
				out_l [lcv] += ((signed long long)blip_lpf_384K_96K[lcv] * delta_l) / lpf_scale;
				out_r [lcv] += ((signed long long)blip_lpf_384K_96K[lcv] * delta_r) / lpf_scale;
			}
			return;

		case 768000:
			for( int lcv = 0; lcv < blip_lpf_768K_taps; lcv++ ) {
				out_l [lcv] += ((signed long long)blip_lpf_768K_96K[lcv] * delta_l) / lpf_scale;
				out_r [lcv] += ((signed long long)blip_lpf_768K_96K[lcv] * delta_r) / lpf_scale;
			}
			return;
		}
	}


	if( blip_lpf_cutoff == 192000 ) {
		switch( sample_rate ) {
		case 384000:
			for( int lcv = 0; lcv < blip_lpf_384K_taps; lcv++ ) {
				out_l [lcv] += ((signed long long)blip_lpf_384K_192K[lcv] * delta_l) / lpf_scale;
				out_r [lcv] += ((signed long long)blip_lpf_384K_192K[lcv] * delta_r) / lpf_scale;
			}
			return;

		case 768000:
			for( int lcv = 0; lcv < blip_lpf_768K_taps; lcv++ ) {
				out_l [lcv] += ((signed long long)blip_lpf_768K_192K[lcv] * delta_l) / lpf_scale;
				out_r [lcv] += ((signed long long)blip_lpf_768K_192K[lcv] * delta_r) / lpf_scale;
			}
			return;
		}
	}


	if( blip_lpf_cutoff == 384000 ) {
		switch( sample_rate ) {
		case 768000:
			for( int lcv = 0; lcv < blip_lpf_768K_taps; lcv++ ) {
				out_l [lcv] += ((signed long long)blip_lpf_768K_384K[lcv] * delta_l) / lpf_scale;
				out_r [lcv] += ((signed long long)blip_lpf_768K_384K[lcv] * delta_r) / lpf_scale;
			}
			return;
		}
	}


	out_l [0] += (buf_t) delta_l * (1UL << lpf_frac);
	out_r [0] += (buf_t) delta_r * (1UL << lpf_frac);
}


static void blip_lpf_mono(int sample_rate, buf_t* out, int delta)
{
	if( blip_lpf_cutoff == 24000 ) {
		switch( sample_rate ) {
		case 48000:
			for( int lcv = 0; lcv < blip_lpf_48K_taps; lcv++ ) {
				out [lcv] += ((signed long long)blip_lpf_48K_24K[lcv] * delta) / lpf_scale;
			}
			return;

		case 96000:
			for( int lcv = 0; lcv < blip_lpf_96K_taps; lcv++ ) {
				out [lcv] += ((signed long long)blip_lpf_96K_24K[lcv] * delta) / lpf_scale;
			}
			return;

		case 192000:
			for( int lcv = 0; lcv < blip_lpf_192K_taps; lcv++ ) {
				out [lcv] += ((signed long long)blip_lpf_192K_24K[lcv] * delta) / lpf_scale;
			}
			return;

		case 384000:
			for( int lcv = 0; lcv < blip_lpf_384K_taps; lcv++ ) {
				out [lcv] += ((signed long long)blip_lpf_384K_24K[lcv] * delta) / lpf_scale;
			}
			return;

		case 768000:
			for( int lcv = 0; lcv < blip_lpf_768K_taps; lcv++ ) {
				out [lcv] += ((signed long long)blip_lpf_768K_24K[lcv] * delta) / lpf_scale;
			}
			return;
		}
	}


	if( blip_lpf_cutoff == 48000 ) {
		switch( sample_rate ) {
		case 96000:
			for( int lcv = 0; lcv < blip_lpf_96K_taps; lcv++ ) {
				out [lcv] += ((signed long long)blip_lpf_96K_48K[lcv] * delta) / lpf_scale;
			}
			return;

		case 192000:
			for( int lcv = 0; lcv < blip_lpf_192K_taps; lcv++ ) {
				out [lcv] += ((signed long long)blip_lpf_192K_48K[lcv] * delta) / lpf_scale;
			}
			return;

		case 384000:
			for( int lcv = 0; lcv < blip_lpf_384K_taps; lcv++ ) {
				out [lcv] += ((signed long long)blip_lpf_384K_48K[lcv] * delta) / lpf_scale;
			}
			return;

		case 768000:
			for( int lcv = 0; lcv < blip_lpf_768K_taps; lcv++ ) {
				out [lcv] += ((signed long long)blip_lpf_768K_48K[lcv] * delta) / lpf_scale;
			}
			return;
		}
	}


	if( blip_lpf_cutoff == 96000 ) {
		switch( sample_rate ) {
		case 192000:
			for( int lcv = 0; lcv < blip_lpf_192K_taps; lcv++ ) {
				out [lcv] += ((signed long long)blip_lpf_192K_96K[lcv] * delta) / lpf_scale;
			}
			return;

		case 384000:
			for( int lcv = 0; lcv < blip_lpf_384K_taps; lcv++ ) {
				out [lcv] += ((signed long long)blip_lpf_384K_96K[lcv] * delta) / lpf_scale;
			}
			return;

		case 768000:
			for( int lcv = 0; lcv < blip_lpf_768K_taps; lcv++ ) {
				out [lcv] += ((signed long long)blip_lpf_768K_96K[lcv] * delta) / lpf_scale;
			}
			return;
		}
	}


	if( blip_lpf_cutoff == 192000 ) {
		switch( sample_rate ) {
		case 384000:
			for( int lcv = 0; lcv < blip_lpf_384K_taps; lcv++ ) {
				out [lcv] += ((signed long long)blip_lpf_384K_192K[lcv] * delta) / lpf_scale;
			}
			return;

		case 768000:
			for( int lcv = 0; lcv < blip_lpf_768K_taps; lcv++ ) {
				out [lcv] += ((signed long long)blip_lpf_768K_192K[lcv] * delta) / lpf_scale;
			}
			return;
		}
	}


	if( blip_lpf_cutoff == 384000 ) {
		switch( sample_rate ) {
		case 768000:
			for( int lcv = 0; lcv < blip_lpf_768K_taps; lcv++ ) {
				out [lcv] += ((signed long long)blip_lpf_768K_384K[lcv] * delta) / lpf_scale;
			}
			return;
		}
	}


	out [0] += (buf_t) delta * (1UL << lpf_frac);
}
