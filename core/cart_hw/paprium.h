/*
Project Little Man
*/

#ifdef _WIN32
#include <windows.h>
#include <stdio.h>
#endif


#define DEBUG_SPRITE 0
#define DEBUG_MODE 0
#define DEBUG_CHEAT 0


#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"


static mp3dec_t paprium_mp3d;
static mp3dec_file_info_t paprium_mp3d_info;

static mp3dec_t paprium_mp3d_boss1;
static mp3dec_file_info_t paprium_mp3d_info_boss1;

static mp3dec_t paprium_mp3d_boss2;
static mp3dec_file_info_t paprium_mp3d_info_boss2;

static mp3dec_t paprium_mp3d_boss3;
static mp3dec_file_info_t paprium_mp3d_info_boss3;

static mp3dec_t paprium_mp3d_boss4;
static mp3dec_file_info_t paprium_mp3d_info_boss4;

static int paprium_track_last;
extern char g_rom_dir[256];


#define PAPRIUM_BOSS1 0x17
#define PAPRIUM_BOSS2 0x21
#define PAPRIUM_BOSS3 0x22
#define PAPRIUM_BOSS4 0x23


static int skip_boot1 = 1;


extern T_SRAM sram;


extern retro_log_printf_t log_cb;
static char error_str[1024];
static int paprium_cmd_count = 0;

#define m68k_read_immediate_16(address) *(uint16 *)(m68k.memory_map[((address)>>16)&0xff].base + ((address) & 0xffff))
#define m68k_read_immediate_32(address) (m68k_read_immediate_16(address) << 16) | (m68k_read_immediate_16(address+2))


static uint8 paprium_obj_ram[0x80000];
static uint8 paprium_wave_ram[0x180000];

static int paprium_tmss = 1;
int fast_dma_hack = 0;

static int paprium_music_ptr;
static int paprium_wave_ptr;
static int paprium_sfx_ptr;
static int paprium_tile_ptr;
static int paprium_sprite_ptr;


typedef struct paprium_voice_t
{
	int volume;
	int panning;
	int flags;
	int type;

	int size;
	int ptr;
	int tick;

	int loop;
	int echo;
	int program;


	int count;

	int time;

	int start;
	int num;

	int decay;
	int decay2;
	int release;
	int sustain;

	int duration;
	int velocity;
	int keyon;
	int key_type;
	int pitch;
	int cents;
	int modulation;
} paprium_voice_t;


struct paprium_t
{
	uint8 ram[0x10000];
	uint8 decoder_ram[0x10000];
	uint8 scaler_ram[0x1000];
	uint8 music_ram[0x8000];
	uint8 exps_ram[14*8];

	paprium_voice_t sfx[8];
	paprium_voice_t music[26];

	int music_section, audio_tick, music_segment;

	int out_l, out_r;
	int audio_flags, sfx_volume, music_volume;

	int decoder_mode, decoder_ptr, decoder_size;
	int draw_src, draw_dst;
	int obj[0x31];

	int echo_l[48000/4], echo_r[48000/4];
	int echo_ptr, echo_pan;

	int music_track, mp3_ptr, music_tick;
} paprium_s;


uint8 paprium_volume_table[256] =
{
    0x00, 0x03, 0x07, 0x0B, 0x0E, 0x12, 0x15, 0x18, 0x1B, 0x1E, 0x21, 0x24, 0x27, 0x29, 0x2C, 0x2F, 
    0x31, 0x34, 0x36, 0x38, 0x3B, 0x3D, 0x3F, 0x41, 0x44, 0x46, 0x48, 0x4A, 0x4C, 0x4E, 0x50, 0x51, 
    0x53, 0x55, 0x57, 0x59, 0x5A, 0x5C, 0x5E, 0x5F, 0x61, 0x63, 0x64, 0x66, 0x67, 0x69, 0x6A, 0x6C, 
    0x6D, 0x6F, 0x70, 0x72, 0x73, 0x74, 0x76, 0x77, 0x78, 0x7A, 0x7B, 0x7C, 0x7E, 0x7F, 0x80, 0x81, 
    0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 
    0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F, 0xA0, 0xA1, 0xA2, 0xA3, 
    0xA4, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xAF, 0xB0, 
    0xB1, 0xB2, 0xB3, 0xB3, 0xB4, 0xB5, 0xB6, 0xB6, 0xB7, 0xB8, 0xB9, 0xB9, 0xBA, 0xBB, 0xBC, 0xBC, 
    0xBD, 0xBE, 0xBE, 0xBF, 0xC0, 0xC1, 0xC1, 0xC2, 0xC3, 0xC3, 0xC4, 0xC5, 0xC5, 0xC6, 0xC7, 0xC7, 
    0xC8, 0xC9, 0xC9, 0xCA, 0xCA, 0xCB, 0xCC, 0xCC, 0xCD, 0xCE, 0xCE, 0xCF, 0xCF, 0xD0, 0xD1, 0xD1, 
    0xD2, 0xD2, 0xD3, 0xD3, 0xD4, 0xD5, 0xD5, 0xD6, 0xD6, 0xD7, 0xD7, 0xD8, 0xD9, 0xD9, 0xDA, 0xDA, 
    0xDB, 0xDB, 0xDC, 0xDC, 0xDD, 0xDD, 0xDE, 0xDF, 0xDF, 0xE0, 0xE0, 0xE1, 0xE1, 0xE2, 0xE2, 0xE3, 
    0xE3, 0xE4, 0xE4, 0xE5, 0xE5, 0xE6, 0xE6, 0xE7, 0xE7, 0xE8, 0xE8, 0xE9, 0xE9, 0xEA, 0xEA, 0xEA, 
    0xEB, 0xEB, 0xEC, 0xEC, 0xED, 0xED, 0xEE, 0xEE, 0xEF, 0xEF, 0xF0, 0xF0, 0xF0, 0xF1, 0xF1, 0xF2, 
    0xF2, 0xF3, 0xF3, 0xF4, 0xF4, 0xF4, 0xF5, 0xF5, 0xF6, 0xF6, 0xF7, 0xF7, 0xF7, 0xF8, 0xF8, 0xF9, 
    0xF9, 0xF9, 0xFA, 0xFA, 0xFB, 0xFB, 0xFC, 0xFC, 0xFC, 0xFD, 0xFD, 0xFE, 0xFE, 0xFE, 0xFF, 0xFF, 
};


static void paprium_load_mp3(int track, int reload)
{
	static char name[512];

	paprium_s.music_track = track;

#ifdef _WIN32
	sprintf(error_str, "%s\\paprium\\", g_rom_dir);
#else
	sprintf(error_str, "%s/paprium/", g_rom_dir);
#endif

	switch(track) {
	case 0x01: sprintf(name, "%s02 90's Acid Dub Character Select.mp3", error_str); break;
	case 0x02: sprintf(name, "%s08 90's Dance.mp3", error_str); break;
	case 0x03: sprintf(name, "%s42 1988 Commercial.mp3", error_str); break;
	case 0x04: sprintf(name, "%s05 Asian Chill.mp3", error_str); break;
	case 0x05: sprintf(name, "%s31 Bad Dudes vs Paprium.mp3", error_str); break;
	case 0x06: sprintf(name, "%s43 Blade FM.mp3", error_str); break;
	case 0x07: sprintf(name, "%s03 Bone Crusher.mp3", error_str); break;
	case 0x0B: sprintf(name, "%s26 Club Shuffle.mp3", error_str); break;
	case 0x0C: sprintf(name, "%s23 Continue.mp3", error_str); break;  /* Continue Fast */
	case 0x0E: sprintf(name, "%s07 Cool Groove.mp3", error_str); break;
	case 0x0F: sprintf(name, "%s36 Cyberpunk Ninja.mp3", error_str); break;
	case 0x10: sprintf(name, "%s35 Cyberpunk Funk.mp3", error_str); break;
	case 0x11: sprintf(name, "%s30 Cyber Interlude.mp3", error_str); break;
	case 0x12: sprintf(name, "%s21 Cyborg Invasion.mp3", error_str); break;
	case 0x13: sprintf(name, "%s44 Dark Alley.mp3", error_str); break;
	case 0x14: sprintf(name, "%s29 Dark & Power Mad.mp3", error_str); break;
	case 0x15: sprintf(name, "%s24 Intro.mp3", error_str); break;  /* Dark Bounce */
	case 0x16: sprintf(name, "%s27 Dark Rock.mp3", error_str); break;
	case 0x17: sprintf(name, "%s04 Drumbass Boss.mp3", error_str); break;
	case 0x18: sprintf(name, "%s45 Dubstep Groove.mp3", error_str); break;
	case 0x19: sprintf(name, "%s15 Electro Acid Funk.mp3", error_str); break;
	case 0x1B: sprintf(name, "%s28 Evolve.mp3", error_str); break;
	case 0x1C: sprintf(name, "%s33 Funk Enhanced Mix.mp3", error_str); break;
	case 0x1D: sprintf(name, "%s41 Game Over.mp3", error_str); break;
	case 0x1E: sprintf(name, "%s46 Gothic.mp3", error_str); break;
	case 0x20: sprintf(name, "%s13 Hard Rock.mp3", error_str); break;
	case 0x21: sprintf(name, "%s22 Hardcore BP1.mp3", error_str); break;
	case 0x22: sprintf(name, "%s11 Hardcore BP2.mp3", error_str); break;
	case 0x23: sprintf(name, "%s38 Hardcore BP3.mp3", error_str); break;
	case 0x24: sprintf(name, "%s40 Score.mp3", error_str); break;  /* High Score */
	case 0x25: sprintf(name, "%s47 House.mp3", error_str); break;
	case 0x26: sprintf(name, "%s17 Indie Shuffle.mp3", error_str); break;
	case 0x27: sprintf(name, "%s25 Indie Break Beat.mp3", error_str); break;
	case 0x28: sprintf(name, "%s16 Jazzy Shuffle.mp3", error_str); break;
	case 0x2A: sprintf(name, "%s19 Neo Metal.mp3", error_str); break;
	case 0x2B: sprintf(name, "%s14 Neon Rider.mp3", error_str); break;
	case 0x2E: sprintf(name, "%s09 Retro Beat.mp3", error_str); break;
	case 0x2F: sprintf(name, "%s20 Sadness.mp3", error_str); break;
	case 0x31: sprintf(name, "%s18 Slow Asian Beat.mp3", error_str); break;
	case 0x32: sprintf(name, "%s48 Slow Mood.mp3", error_str); break;  /* Slow Mood Ext. ? */
	case 0x33: sprintf(name, "%s49 Smooth Coords.mp3", error_str); break;
	case 0x34: sprintf(name, "%s10 Spiral.mp3", error_str); break;
	case 0x35: sprintf(name, "%s12 Stage Clear.mp3", error_str); break;
	case 0x36: sprintf(name, "%s32 Summer Breeze.mp3", error_str); break;
	case 0x37: sprintf(name, "%s06 Techno Beats.mp3", error_str); break;
	case 0x38: sprintf(name, "%s50 Tension.mp3", error_str); break;
	case 0x39: sprintf(name, "%s01 Theme of Paprium.mp3", error_str); break;
	case 0x3A: sprintf(name, "%s39 Ending.mp3", error_str); break;  /* Tough Guy */
	case 0x3B: sprintf(name, "%s34 Transe.mp3", error_str); break;
	case 0x3C: sprintf(name, "%s37 Urban.mp3", error_str); break;
	case 0x3D: sprintf(name, "%s51 Water.mp3", error_str); break;
	case 0x3E: sprintf(name, "%s52 Waterfront Beat.mp3", error_str); break;
	default: paprium_s.music_track = 0; return;
	}

	if( reload )
		paprium_s.mp3_ptr = 0;

	paprium_s.music_tick = 0;

	if( track != PAPRIUM_BOSS1 && track != PAPRIUM_BOSS2 && track != PAPRIUM_BOSS3 && track != PAPRIUM_BOSS4 ) {
		if( mp3dec_load(&paprium_mp3d, name, &paprium_mp3d_info, NULL, NULL) ) {
			paprium_s.music_track = 0;
			return;
		}
	}
}


static void paprium_load_mp3_boss()
{
	static char name[512];

#ifdef _WIN32
	sprintf(error_str, "%s\\paprium\\", g_rom_dir);
#else
	sprintf(error_str, "%s/paprium/", g_rom_dir);
#endif

	sprintf(name, "%s04 Drumbass Boss.mp3", error_str);
	mp3dec_load(&paprium_mp3d_boss1, name, &paprium_mp3d_info_boss1, NULL, NULL);

	sprintf(name, "%s22 Hardcore BP1.mp3", error_str);
	mp3dec_load(&paprium_mp3d_boss2, name, &paprium_mp3d_info_boss2, NULL, NULL);

	sprintf(name, "%s11 Hardcore BP2.mp3", error_str);
	mp3dec_load(&paprium_mp3d_boss3, name, &paprium_mp3d_info_boss3, NULL, NULL);

	sprintf(name, "%s38 Hardcore BP3.mp3", error_str);
	mp3dec_load(&paprium_mp3d_boss4, name, &paprium_mp3d_info_boss4, NULL, NULL);
}


static void paprium_music_sheet()
{
	int ch;
	int l = 0, r = 0;

	/* 00-04 = WMMM */
	/* 05, 06, 07 */
	int sections = paprium_s.music_ram[0x09];
	/* 09 */
	int bars = (paprium_s.music_ram[0x0B] ? paprium_s.music_ram[0x0B] : 0x100) + 8;
	/* 0B */
	int cmds = paprium_s.music_ram[0x0D];
	/* 10-29 */
	/* 2A-43 */
	/* 44-5D */
	/* 5E-77 */
	/* 78-97 = title ^ 0xA5 */
	/* 98-B7 = author ^ 0xA5 */
	/* B8-D7 = comment ^ 0xA5 */


	if( paprium_s.music_segment == -1 ) {
		paprium_s.music_track = 0;
		return;
	}

	if( paprium_s.audio_tick < 4 )
		return;
	paprium_s.audio_tick = 0;


#if 0
	sprintf(error_str, "Music_Sheet %X %X - %X %X\n", paprium_s.music_segment, paprium_s.music_section, sections, bars);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	for( ch = 0; ch < 26; ch++ ) {
		int ptr = *(uint16*)(paprium_s.music_ram + 0xD8 + (ch*sections*2) + (paprium_s.music_segment*2));
		int index = paprium_s.music_ram[(ptr + paprium_s.music_section)^1];
		int code = 0, arg = 0;
		int keyon = 0, duration = 0, freq = -1, volume = 0;
		paprium_voice_t *voice = paprium_s.music + ch;


#if 0
//if(ch == 0) {
{
	sprintf(error_str, "[%d] Music %X %X %X = %X\n", ch, ptr, ptr + paprium_s.music_section, index, (index == 0) ? 0 : ptr + bars + ((index-1)*8) + paprium_s.music_section*2);
	log_cb(RETRO_LOG_ERROR, error_str);
}
#endif


		if( index == 0 )
			goto next;
		ptr += bars + (index-1) * 8;


		for( int lcv = 0; lcv < cmds; lcv++ ) {
			code = *(uint16*)(paprium_s.music_ram + ptr + lcv*2);
			arg = code & 0xFF;
			code >>= 8;


			if( code == 0x00 )
				{} //break;

			else if( code == 0x01 ) {  /* ?? */
				if( ch >= 11-1 ) keyon = code;
				voice->volume = 255 - paprium_volume_table[arg];  /* z80 table ? */
			}

			else if( code == 0x02 )
				voice->panning = arg;

			else if( code == 0x03 ) {
				voice->volume = 255 - paprium_volume_table[arg];  /* z80 table ? */
			}

			else if( code == 0x05 ) {
				voice->volume = 255 - paprium_volume_table[arg];  /* z80 table ? */
			}

			/* velocity */

			else if( code == 0x08 ) {
#if 0
				sprintf(error_str, "08 - %X %X %X\n", ch, ptr, arg);
				log_cb(RETRO_LOG_ERROR, error_str);
				//MessageBoxA(0, error_str, "---", 0);
#endif

				freq = arg;
			}

			else if( code == 0x0A ) {
#if 0
				sprintf(error_str, "0B - %X %X %X\n", ch, ptr, arg);
				log_cb(RETRO_LOG_ERROR, error_str);
				//MessageBoxA(0, error_str, "---", 0);
#endif			
				freq = 0;  /* faster ? */
			}

			else if( code == 0x0B ) {
#if 0
				sprintf(error_str, "0B - %X %X %X\n", ch, ptr, arg);
				log_cb(RETRO_LOG_ERROR, error_str);
#endif
				//MessageBoxA(0, error_str, "---", 0);
			}

			else if( code == 0x0C ) {
#if 0
				sprintf(error_str, "0C - %X %X %X\n", ch, ptr, arg);
				log_cb(RETRO_LOG_ERROR, error_str);
#endif
				//MessageBoxA(0, error_str, "---", 0);
			}

			else if( code == 0x0E ) {  /* stop ? */
				keyon = 0;
				voice->size = 0;
			}

			else if( code == 0x0F ) {  /* program */
				voice->program = arg;

				if( ch < 6 ) {  /* YM */
				}

				else if( ch < 10 ) {  /* PSG */
				}

				else
				{  /* wave */
				}

#if DEBUG_MODE
				sprintf(error_str, "[%d] Sample %X %X\n", ch, arg, ptr);
				log_cb(RETRO_LOG_ERROR, error_str);
#endif
			}
		}


		if( keyon ) {
#if 0
			uint8* ptr = paprium_wave_ram + voice->program*16;

			voice->ptr = (*(uint16*)(ptr + 0x00)<<16) + *(uint16*)(ptr + 0x02);
			voice->size = (*(uint16*)((paprium_wave_ram + 0x04 + voice->program*16))<<16) + *(uint16*)(paprium_wave_ram + 0x06 + voice->program*16);
			/* *(uint16*)(paprium_wave_ram + 0x08 + voice->program*16)<<16) + *(uint16*)(paprium_wave_ram + 0x0A + voice->program*16); */
			voice->type = *(uint16*)(paprium_wave_ram + 0x0C + voice->program*16) + 1;
			/* *(uint16*)(paprium_wave_ram + 0x0E + voice->program*16); */			

			voice->tick = 0;
			voice->time = 0;

			voice->keyon = keyon;
			voice->duration = 65536;
			voice->release = 256;
#endif
		}

		if( freq >= 0 ) {
			voice->type = freq;
			//if(ch == 16-1) MessageBoxA(0,"hit","-",0);
		}

next:
		*(uint16*)(paprium_s.ram + ch*4 + 0x1B98) = index ? 0xE0 : 0;  /* L */
		*(uint16*)(paprium_s.ram + ch*4 + 0x1B9A) = index ? 0xE0 : 0;  /* R */
	}


end:
	if( (++paprium_s.music_section) >= bars ) {
		paprium_s.music_section = 0;

		if( (++paprium_s.music_segment) >= sections )
			paprium_s.music_segment = 0;
	}
}


static void paprium_music_synth(int *out_l, int *out_r)
{
	int ch;
	int l = 0, r = 0;


#if 1
	if( paprium_s.music_track ) {
		if( paprium_s.music_track == PAPRIUM_BOSS1 ) {
			l = paprium_mp3d_info_boss1.buffer[paprium_s.mp3_ptr];
			r = paprium_mp3d_info_boss1.buffer[paprium_s.mp3_ptr];
		}
		else if( paprium_s.music_track == PAPRIUM_BOSS2 ) {
			l = paprium_mp3d_info_boss2.buffer[paprium_s.mp3_ptr];
			r = paprium_mp3d_info_boss2.buffer[paprium_s.mp3_ptr];
		}
		else if( paprium_s.music_track == PAPRIUM_BOSS3 ) {
			l = paprium_mp3d_info_boss3.buffer[paprium_s.mp3_ptr];
			r = paprium_mp3d_info_boss3.buffer[paprium_s.mp3_ptr];
		}
		else if( paprium_s.music_track == PAPRIUM_BOSS4 ) {
			l = paprium_mp3d_info_boss4.buffer[paprium_s.mp3_ptr];
			r = paprium_mp3d_info_boss4.buffer[paprium_s.mp3_ptr];
		}
		else {
			l = paprium_mp3d_info.buffer[paprium_s.mp3_ptr];
			r = paprium_mp3d_info.buffer[paprium_s.mp3_ptr];
		}


		l = (l * paprium_s.music_volume) / 256;
		r = (r * paprium_s.music_volume) / 256;


		paprium_s.music_tick += 0x10000;
		if( paprium_s.music_tick >= 0x10000 ) {
			paprium_s.music_tick -= 0x10000;

			paprium_s.mp3_ptr += 2;
		}


		if( paprium_s.music_track == PAPRIUM_BOSS1 ) {
			if( paprium_s.mp3_ptr >= paprium_mp3d_info_boss1.samples)
				paprium_s.mp3_ptr -= paprium_mp3d_info_boss1.samples;
		}
		else if( paprium_s.music_track == PAPRIUM_BOSS2 ) {
			if( paprium_s.mp3_ptr >= paprium_mp3d_info_boss2.samples)
				paprium_s.mp3_ptr -= paprium_mp3d_info_boss2.samples;
		}
		else if( paprium_s.music_track == PAPRIUM_BOSS3 ) {
			if( paprium_s.mp3_ptr >= paprium_mp3d_info_boss3.samples)
				paprium_s.mp3_ptr -= paprium_mp3d_info_boss3.samples;
		}
		else if( paprium_s.music_track == PAPRIUM_BOSS4 ) {
			if( paprium_s.mp3_ptr >= paprium_mp3d_info_boss4.samples)
				paprium_s.mp3_ptr -= paprium_mp3d_info_boss4.samples;
		}
		else {
			if( paprium_s.mp3_ptr >= paprium_mp3d_info.samples)
				paprium_s.mp3_ptr -= paprium_mp3d_info.samples;
		}
	}

	*out_l = l;
	*out_r = r;

	return;
#endif


	for( ch = 0; ch < 26; ch++ ) {
		paprium_voice_t *voice = paprium_s.music + ch;
		const int _rates[] = {2,4,5,8,9,10};  /* 24000 ?, 12000, 9600, 6000, 5333-?, 4800-? */

		int rate = _rates[voice->type] << 16;  /* 16.16 */
		int vol = voice->volume;
		int pan = voice->panning;

		int sample, sample_l, sample_r;


		//if( ch < 15-1 || ch > 17-1 ) goto next;

#if 0
		if( ch == 20 ) {
			sprintf(error_str, "[%d] %X %X\n", ch, voice->ptr, voice->size);
			log_cb(RETRO_LOG_ERROR, error_str);
		}
		else
			goto next;
#endif


		if( ((paprium_s.audio_tick % 4) == 0) && voice->duration) {
			//voice->duration--;
			if( voice->duration == 0 ) {
				if( voice->keyon == 1 )  /* hard off */
					voice->size = 0;
			}
		}


		if( voice->size == 0 ) goto next;


		sample = *(uint8 *)(paprium_wave_ram + (voice->ptr^1));

		sample = (((sample & 0xFF) * 65536) / 256) - 32768;
		sample = (sample * vol) / 0x300;

		sample_l = (sample * ((pan <= 0x80) ? 0x80 : 0x100 - pan)) / 0x80;
		sample_r = (sample * ((pan >= 0x80) ? 0x80 : pan)) / 0x80;

		l += sample_l;
		r += sample_r;


		voice->time++;
		voice->tick += 0x10000;


		if( voice->tick >= rate ) {
			voice->tick -= rate;

			voice->size--;
			voice->ptr++;
		}


next:
	}


	*out_l += l;
	*out_r += r;
}


static void paprium_sfx_voice(int *out_l, int *out_r)
{
	int ch;
	int l = 0, r = 0;

	for( ch = 0; ch < 8; ch++ ) {
		paprium_voice_t *voice = paprium_s.sfx + ch;
		const int _rates[] = {1,2,4,5,8,9};  /* 48000, 24000, 12000, 9600, 6000, 5333 */

		int rate = _rates[voice->type >> 4] << 16;  /* 16.16 */
		int depth = voice->type & 0x03;
		/* voice->type & 0xC0; */

		int vol = voice->volume;
		int pan = voice->panning;

		int sample, sample_l, sample_r;


		if( voice->size == 0 ) goto next;


		sample = *(uint8 *)(cart.rom + paprium_sfx_ptr + (voice->ptr^1));

		if( depth == 1 )
			sample = (((sample & 0xFF) * 65536) / 256) - 32768;

		else if( depth == 2 ) {
			if( voice->count == 0 )
				sample >>= 4;

			sample = (((sample & 0x0F) * 65536) / 16) - 32768;
		}


		sample = (sample * vol) / 0x400;

		sample_l = (sample * ((pan <= 0x80) ? 0x80 : 0x100 - pan)) / 0x80;
		sample_r = (sample * ((pan >= 0x80) ? 0x80 : pan)) / 0x80;

		l += sample_l;
		r += sample_r;


		if( voice->flags & 0x4000 ) {  /* echo */
			if( voice->echo & 1 )
				paprium_s.echo_l[paprium_s.echo_ptr % (48000/6)] += (sample_l * 33) / 100;
			else
				paprium_s.echo_r[paprium_s.echo_ptr % (48000/6)] += (sample_r * 33) / 100;
		}


		if( voice->flags & 0x100 ) {  /* amplify */
			l = (l * 125) / 100;
			r = (r * 125) / 100;
		}

		voice->time++;

		voice->tick += 0x10000;
		voice->tick -= (voice->flags & 0x8000) ? 0x800 : 0;  /* tiny pitch */
		voice->tick -= (voice->flags & 0x2000) ? 0x8000 : 0;  /* huge pitch */


		if( voice->tick >= rate ) {
			voice->tick -= rate;

			voice->count++;
			voice->size--;

			if( voice->count >= depth ) {
				voice->ptr++;

				voice->count = 0;
			}
		}
 

next:
		if( voice->size == 0 ) {
			voice->count = 0;

			if( voice->loop ) {
				uint8 *sfx = paprium_sfx_ptr + cart.rom;

				voice->ptr = (*(uint16 *)(sfx + voice->num*8) << 16) | (*(uint16 *)(sfx + voice->num*8 + 2));
				voice->size = (*(uint8 *)(sfx + voice->num*8 + 4) << 16) | (*(uint16 *)(sfx + voice->num*8 + 6));
			}
		}
	}

	*out_l += l;
	*out_r += r;
}


void paprium_audio(int cycles)
{
	int lcv;
	int samples = blip_clocks_needed(snd.blips[3], cycles);


#if DEBUG_MODE
	sprintf(error_str, "Audio frame %X - %X\n", samples, cycles);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	if( paprium_s.music_track != paprium_track_last ) {
		paprium_load_mp3(paprium_s.music_track, 0);
	}


	paprium_s.audio_tick++;


	for( lcv = 0; lcv < samples; lcv++ ) {
		int l = 0, r = 0;
		int ch;


		paprium_s.echo_l[paprium_s.echo_ptr] = 0;
		paprium_s.echo_r[paprium_s.echo_ptr] = 0;


		paprium_music_synth(&l, &r);
		paprium_sfx_voice(&l, &r);


		paprium_s.echo_ptr = (paprium_s.echo_ptr+1) % (48000/6);

		l += paprium_s.echo_l[paprium_s.echo_ptr];
		r += paprium_s.echo_r[paprium_s.echo_ptr];


		l = (l * paprium_s.sfx_volume) / 0x100;
		r = (r * paprium_s.sfx_volume) / 0x100;


		if( paprium_s.audio_flags & 0x08 ) {  /* gain */
			l = l * 4 / 2;
			r = r * 4 / 2;
		}


		if( l > 32767 ) l = 32767;
		else if( l < -32768 ) l = -32768;

		if( r > 32767 ) r = 32767;
		else if( r < -32768 ) r = -32768;


		blip_add_delta_fast(snd.blips[3], lcv, l-paprium_s.out_l, r-paprium_s.out_r);


		paprium_s.out_l = l;
		paprium_s.out_r = r;
	}


	paprium_music_sheet();


#if DEBUG_MODE
	for( lcv = 0; lcv < 8; lcv++ ) {
		if( !paprium_s.sfx[lcv].decay ) continue;

	sprintf(error_str, "[%d] %d %d %d - %d %d\n", lcv, paprium_s.sfx[lcv].loop, paprium_s.sfx[lcv].volume, paprium_s.sfx[lcv].size, paprium_s.sfx[lcv].decay, paprium_s.sfx[lcv].decay2);
	log_cb(RETRO_LOG_ERROR, error_str);

		paprium_s.sfx[lcv].decay2 += paprium_s.sfx[lcv].decay;
		while( paprium_s.sfx[lcv].decay2 >= 2 ) {  /* stage1 intro sound fade-out time ?? 2 seems decent */
			paprium_s.sfx[lcv].volume -= 1;
			if( paprium_s.sfx[lcv].volume < 0 ) paprium_s.sfx[lcv].volume = 0;

			paprium_s.sfx[lcv].decay2 -= 2;
		}
	}
#endif


	blip_end_frame(snd.blips[3], samples);


	paprium_track_last = paprium_s.music_track;
}


static void paprium_decoder_lz_rle(uint src, uint8 *dst)
{
	int size = 0;
	int len, lz, rle, code;


#if 0
	sprintf(error_str, "LZ-RLE %X\n", src);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	while(1) {
		int type = cart.rom[(src++)^1];

		code = type >> 6;
		len = type & 0x3F;


		if( (code == 0) && (len == 0))
			break;

		else if( code == 1 )
			rle = cart.rom[(src++)^1];

		else if( code == 2 )
			lz = size - cart.rom[(src++)^1];


		while( len-- > 0 ) {
			switch(code) {
			case 0: dst[(size++)^1] = cart.rom[(src++)^1]; break;
			case 1: dst[(size++)^1] = rle; break;
			case 2: dst[(size++)^1] = dst[(lz++)^1]; break;
			case 3: dst[(size++)^1] = 0; break;
			}
		}
	}

#if 0
	sprintf(error_str, "STOP %X - %X\n", src, size);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	paprium_s.decoder_size = size;
}


static void paprium_decoder_lzo(uint src, uint8 *dst)
{
	int size = 0;
	int len, lz, raw;
	int state = 0;


#if 0
	sprintf(error_str, "LZO %X\n", src);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	while(1) {
		int code = cart.rom[(src++)^1];

		if( code & 0x80 ) goto code_80;
		if( code & 0x40 ) goto code_40;
		if( code & 0x20 ) goto code_20;
		if( code & 0x10 ) goto code_10;


code_00:
		if( state == 0 ) {
			raw = code & 0x0F;

			if( raw == 0 ) {
				int extra = 0;

				while(1) {
					raw = cart.rom[(src++)^1];
					if( raw ) break;

					extra += 255;
				}

				raw += extra;
				raw += 15;
			}
			raw += 3;

			len = 0;
			state = 4;
			goto copy_loop;
		}

		else if( state < 4 ) {
			raw = code & 0x03;
			lz = (code >> 2) & 0x03;

			lz += cart.rom[(src++)^1] << 2;
			lz += 1;

			len = 2;
			goto copy_loop;
		}

		else {
			raw = code & 0x03;
			lz = (code >> 2) & 0x03;

			lz += cart.rom[(src++)^1] << 2;
			lz += 2049;

			len = 3;
			goto copy_loop;
		}


code_10:
		len = code & 0x07;

		if( len == 0 ) {
			int extra = 0;

			while(1) {
				len = cart.rom[(src++)^1];
				if( len ) break;

				extra += 255;
			}

			len += extra;
			len += 7;
		}
		len += 2;


		lz = ((code >> 3) & 1) << 14;

		code = cart.rom[(src++)^1];
		raw = code & 0x03;
		lz += code >> 2;

		lz += cart.rom[(src++)^1] << 6;
		lz += 16384;

		if( lz == 16384 ) break;
		goto copy_loop;


code_20:
		len = code & 0x1F;

		if( len == 0 ) {
			int extra = 0;

			while(1) {
				len = cart.rom[(src++)^1];
				if( len ) break;

				extra += 255;
			}

			len += extra;
			len += 31;
		}
		len += 2;


		code = cart.rom[(src++)^1];
		raw = code & 0x03;

		lz = code >> 2;
		lz += cart.rom[(src++)^1] << 6;
		lz += 1;

		goto copy_loop;


code_40:
		raw = code & 0x03;
		len = ((code >> 5) & 1) + 3;
		lz = (code >> 2) & 0x07;

		lz += cart.rom[(src++)^1] << 3;
		lz += 1;

		goto copy_loop;


code_80:
		raw = code & 0x03;
		len = ((code >> 5) & 0x03) + 5;
		lz = (code >> 2) & 0x07;

		lz += cart.rom[(src++)^1] << 3;
		lz += 1;


copy_loop:
		if( len > 0 )
			state = raw;
		else
			state = 4;

		
		lz = size - lz;

		while(1) {
			if( len > 0 ) {
				dst[(size++)^1] = dst[(lz++)^1];
				len--;
			}

			else if( raw > 0 ) {
				dst[(size++)^1] = cart.rom[(src++)^1];
				raw--;
			}

			else
				break;
		}		
	}


#if 0
	sprintf(error_str, "END %X - %X\n", src, size);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	paprium_s.decoder_size = size;
}


static void paprium_decoder_type(int src, uint8 *dst)
{
	int type = cart.rom[(src++)^1];

	if( type == 0x80 )
		paprium_decoder_lz_rle(src, dst);

	else if( type == 0x81 )
		paprium_decoder_lzo(src, dst);

	else {
#if DEBUG_MODE
		sprintf(error_str, "%X - Decoder_Type %X\n", src-1, type);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "paprium_decoder_init", 0);
#endif
	}
}


static int paprium_decoder(int mode)
{
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM Decoder %02X -- %04X %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		mode, *(uint16 *)(paprium_s.ram + 0x1E10), *(uint16 *)(paprium_s.ram + 0x1E12), *(uint16 *)(paprium_s.ram + 0x1E14));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	int offset = *(uint16 *)(paprium_s.ram + 0x1E10);
	int ptr = (*(uint16 *)(paprium_s.ram + 0x1E12) << 16) + *(uint16 *)(paprium_s.ram + 0x1E14);


#if DEBUG_MODE
	if( mode != 2 && mode != 7 ) {
		sprintf(error_str, "Mode %X", mode);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "Decoder_Init", 0);
	}
#endif


	paprium_decoder_type(ptr, paprium_s.decoder_ram + offset);


	paprium_s.decoder_mode = mode;
	paprium_s.decoder_ptr = offset;


#if DEBUG_MODE
	if( paprium_cmd_count != 3 ) {
		sprintf(error_str, "Decoder %d count\n", paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static int paprium_decoder_copy(int arg)
{
	/* *(uint16 *)(paprium_s.ram + 0x1E10); */
	int offset = *(uint16 *)(paprium_s.ram + 0x1E12);
	int size = *(uint16 *)(paprium_s.ram + 0x1E14);


#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM Decoder_Copy %02X [%X] -- %04X %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg, paprium_s.decoder_ptr,
		*(uint16 *)(paprium_s.ram + 0x1E10), *(uint16 *)(paprium_s.ram + 0x1E12), *(uint16 *)(paprium_s.ram + 0x1E14));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


#if DEBUG_MODE
	if( *(uint16 *)(paprium_s.ram + 0x1E10) ) {
		sprintf(error_str, "paprium_decoder 1E10 = %X", *(uint16 *)(paprium_s.ram + 0x1E10));
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0,error_str,"decoder",0);
	}
#endif


	paprium_s.decoder_ptr = offset;
	paprium_s.decoder_size = size;


#if DEBUG_MODE
	if( paprium_cmd_count != 3 ) {
		sprintf(error_str, "Decoder_Copy %d count\n", paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void paprium_sprite(int index)
{
	int lcv, spr_x, spr_y, count;
	int spritePtr, gfxPtr;

	int dmaSize = 0;
	int dmaPtr = *(uint16*) (paprium_s.ram + 0x1F16);

	int spriteCount = *(uint16*) (paprium_s.ram + 0x1F18);

	int anim = *(uint16*) (paprium_s.ram + 0xF80 + index*16);
	int nextAnim = *(uint16*) (paprium_s.ram + 0xF82 + index*16);
	int obj = *(uint16*) (paprium_s.ram + 0xF84 + index*16) & 0x7FFF;
	//*(uint16*) (paprium_s.ram + 0xF86 + index*16);
	int objAttr = *(uint16*) (paprium_s.ram + 0xF88 + index*16);
	int reset = *(uint16*) (paprium_s.ram + 0xF8A + index*16);
	int pos_x = *(uint16*) (paprium_s.ram + 0xF8C + index*16);
	int pos_y = *(uint16*) (paprium_s.ram + 0xF8E + index*16);

	int src = paprium_s.draw_src;
	int vram = paprium_s.draw_dst;
	int flip_h = objAttr & 0x800;
	int animFlags;


	int animPtr = *(uint32*) (paprium_obj_ram + (obj+1)*4);
	int framePtr = paprium_s.obj[index];


	if( index != 0x30 ) {
		//return;
	}


#if DEBUG_MODE  /* frozen enemy */
	{
		int color = -1;
		int pri = -1;

		if(0) {}
		else if( (obj > 0x00 && obj < 0x30) && (anim == 5) ) color = 0;
		else if( (obj == 0x0D) && (anim == 3) ) color = 0;

		if( color != -1 ) {
			objAttr &= ~0x6000;

			switch(color) {
			case 0: break;
			case 1: objAttr += 0x2000; break;
			case 2: objAttr += 0x4000; break;
			case 3: objAttr += 0x6000; break;
			}
		}

		if( pri != -1 ) {
			objAttr &= ~0x8000;

			switch(pri) {
			case 0: break;
			case 1: objAttr += 0x8000; break;
			}
		}
	}
#endif


#if DEBUG_SPRITE
	uint8 *ptr = paprium_s.ram + 0xF80 + index*16;

	sprintf(error_str, "[%d] [%04X:%04X] DM Sprite %02X - %04X %04X %04X %04X %04X %04X X=%04X Y=%04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		index,
		*(uint16 *)(ptr+0), *(uint16 *)(ptr+2), *(uint16 *)(ptr+4), *(uint16 *)(ptr+6),
		*(uint16 *)(ptr+8), *(uint16 *)(ptr+10), *(uint16 *)(ptr+12), *(uint16 *)(ptr+14) );
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


reload:
	if( reset == 1 ) {
		framePtr = *(uint32*) (paprium_obj_ram + animPtr + anim*4);

		*(uint16*) (paprium_s.ram + 0xF8A + index*16) = 0;
	}
	if( (framePtr == 0) || (framePtr == -1) ) {
		*(uint16*) (paprium_s.ram + 0xF80 + index*16) = 0;
		return;
	}


	spritePtr  = paprium_obj_ram[framePtr + 0] << 0;
	spritePtr += paprium_obj_ram[framePtr + 1] << 8;
	spritePtr += paprium_obj_ram[framePtr + 2] << 16;
	animFlags = paprium_obj_ram[framePtr + 3];


#if DEBUG_SPRITE
	sprintf(error_str, "%X - %X %X %X - %X - %X %X\n",
		((obj+1) & 0x7FFF)*4,
		animPtr, anim, animPtr + anim*4,
		framePtr,
		spritePtr, animFlags);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	if( spritePtr == 0 ) return;


	framePtr += 4; 

	if( animFlags < 0x80 ) {
		if( nextAnim == 0xFFFF ) {
			int nextFrame;

			nextFrame  = paprium_obj_ram[framePtr + 0] << 0;
			nextFrame += paprium_obj_ram[framePtr + 1] << 8;
			nextFrame += paprium_obj_ram[framePtr + 2] << 16;

			framePtr = nextFrame;
		}
		else if( reset == 0 ) {
			anim = nextAnim;
			reset = 1;

			goto reload;
		}
	}

	paprium_s.obj[index] = framePtr;


	count = paprium_obj_ram[(spritePtr++)^1];
	int flags2 = paprium_obj_ram[(spritePtr++)^1];


#if 0
	sprintf(error_str, "%d - %X %X %X\n", count, src, vram, flags2);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	spr_x = pos_x;
	spr_y = pos_y;

	for( lcv = 0; lcv < count; lcv++ ) {
		int misc = ((paprium_obj_ram[spritePtr + 3] >> 4) & 0x0F);
		int size_x = ((paprium_obj_ram[spritePtr + 3] >> 2) & 0x03) + 1;
		int size_y = ((paprium_obj_ram[spritePtr + 3] >> 0) & 0x03) + 1;
		int tile = *(uint16*) (paprium_obj_ram + spritePtr + 4);
		int tileAttr = *(uint16*) (paprium_obj_ram + spritePtr + 6) & ~0x1FF;
		int ofs = *(uint16*) (paprium_obj_ram + spritePtr + 6) & 0x1FF;

		int tilePtr = paprium_tile_ptr + tile*4;
		int tileSize = size_x * size_y * 0x20;

		int sprAttr = 0;


		if( !flip_h )
			spr_x += (int8) paprium_obj_ram[spritePtr + 1];
		else
			spr_x -= (int8) paprium_obj_ram[spritePtr + 1];

		spr_y += (int8) paprium_obj_ram[spritePtr + 0];
		spritePtr += 8;


		if( tile == 0 ) continue;
		if( spriteCount >= 94 ) continue;


		sprAttr  = (tileAttr & 0x8000) ? 0x8000 : (objAttr & 0x8000);
		sprAttr += (tileAttr & 0x6000) ? (tileAttr & 0x6000) : (objAttr & 0x6000);
		sprAttr += (tileAttr & 0x1800) ^ (objAttr & 0x1800);


		if( (spr_y >= 128) && (spr_y + size_y*8 < 368) ) {
			if( (!flip_h && ((spr_x + size_x*8 >= 128) && spr_x < 448)) ||
				 (flip_h && ((spr_x - size_x*8 < 448) && spr_x >= 128)) ) {
				if( spriteCount < 80 ) {
					*(uint16*) (paprium_s.ram + 0xB00 + spriteCount*8) = spr_y & 0x3FF;
					*(uint16*) (paprium_s.ram + 0xB02 + spriteCount*8) = ((size_x-1) << 10) + ((size_y-1) << 8) + (spriteCount+1);
					*(uint16*) (paprium_s.ram + 0xB04 + spriteCount*8) = sprAttr + ((vram / 0x20) & 0x7FF);
					*(uint16*) (paprium_s.ram + 0xB06 + spriteCount*8) = (spr_x - ((!flip_h) ? 0 : size_x*8)) & 0x1FF;

#if DEBUG_SPRITE
					sprintf(error_str, "%d / %X %X %X %X\n",
						spriteCount,
						*(uint16*) (paprium_s.ram + 0xB00 + (spriteCount-0)*8),
						*(uint16*) (paprium_s.ram + 0xB02 + (spriteCount-0)*8),
						*(uint16*) (paprium_s.ram + 0xB04 + (spriteCount-0)*8),
						*(uint16*) (paprium_s.ram + 0xB06 + (spriteCount-0)*8)
					);
					log_cb(RETRO_LOG_ERROR, error_str);
#endif
				}
				else {
					if( spriteCount == 80 ) {  /* sprite 0 lock */
						paprium_s.ram[0xD7A] = 1;  /* exp.s list */

						memcpy(paprium_s.exps_ram, paprium_s.ram + 0xB00, 8);
						paprium_s.exps_ram[2] = 14;  /* normal list */

						spriteCount++;
					}

					*(uint16*) (paprium_s.exps_ram + 0 + (spriteCount-80)*8) = spr_y & 0x3FF;
					*(uint16*) (paprium_s.exps_ram + 2 + (spriteCount-80)*8) = ((size_x-1) << 10) + ((size_y-1) << 8) + ((spriteCount-80)+1);
					*(uint16*) (paprium_s.exps_ram + 4 + (spriteCount-80)*8) = sprAttr + ((vram / 0x20) & 0x7FF);
					*(uint16*) (paprium_s.exps_ram + 6 + (spriteCount-80)*8) = (spr_x - ((!flip_h) ? 0 : size_x*8)) & 0x1FF;

#if DEBUG_SPRITE
					sprintf(error_str, "%d / %X %X %X %X\n",
						spriteCount,
						*(uint16*) (paprium_s.ram + 0x1F20 + (spriteCount-80)*8),
						*(uint16*) (paprium_s.ram + 0x1F22 + (spriteCount-80)*8),
						*(uint16*) (paprium_s.ram + 0x1F24 + (spriteCount-80)*8),
						*(uint16*) (paprium_s.ram + 0x1F26 + (spriteCount-80)*8)
					);
					log_cb(RETRO_LOG_ERROR, error_str);
#endif
				}

				spriteCount++;

#if DEBUG_SPRITE
				sprintf(error_str, "%d %d / %d %d %X %X / %X %X %X\n", spriteCount, count, spr_x, spr_y, tileAttr, objAttr, src, vram, tile);
				log_cb(RETRO_LOG_ERROR, error_str);
#endif
			}
		}



		int ptr = paprium_tile_ptr + (*(uint16*)(cart.rom + tilePtr) << 16) + *(uint16*)(cart.rom + tilePtr + 2);
		static uint8 tile_ram[0x10000];

		paprium_decoder_type(ptr, tile_ram);
		memcpy(paprium_s.ram + src, tile_ram + ofs * 0x20, tileSize);


		//if( !tiledupe && dmaPtr < dmalimit && vram < vramlimit && tile < tilelimit)

		*(uint16*) (paprium_s.ram + 0x1400 + dmaPtr*16) = 0x8F02;
		*(uint16*) (paprium_s.ram + 0x1402 + dmaPtr*16) = 0x9300 + ((tileSize >> 1) & 0xFF);
		*(uint16*) (paprium_s.ram + 0x1404 + dmaPtr*16) = 0x9500 + ((src >> 1) & 0xFF);
		*(uint16*) (paprium_s.ram + 0x1406 + dmaPtr*16) = 0x9400 + ((tileSize >> 9) & 0xFF);
		*(uint16*) (paprium_s.ram + 0x1408 + dmaPtr*16) = 0x9700;
		*(uint16*) (paprium_s.ram + 0x140A + dmaPtr*16) = 0x9600 + ((src >> 9) & 0xFF);
		*(uint16*) (paprium_s.ram + 0x140C + dmaPtr*16) = 0x4000 + (vram & 0x3FFF);
		*(uint16*) (paprium_s.ram + 0x140E + dmaPtr*16) = 0x0080 + (vram >> 14);

		*(uint16*) (paprium_s.ram + 0x1F16) = ++dmaPtr;
		*(uint16*) (paprium_s.ram + 0x1F18) = spriteCount;


		src += tileSize;
		dmaSize += tileSize;
		vram += tileSize;
	}


	paprium_s.draw_src = src;
	paprium_s.draw_dst = vram;
}


static void paprium_sprite_init(int arg)
{
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM Sprite_Init %02X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	memset(paprium_s.ram + 0x1F20, 0, 14*8);


#if DEBUG_MODE
	if( paprium_cmd_count != 0 ) {
		sprintf(error_str, ">>  Sprite_Init %d count\n", paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void paprium_sprite_start(int arg)
{
#if DEBUG_SPRITE
	sprintf(error_str, "[%d] [%04X:%04X] DM Sprite_Start %02X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	int count = *(uint16*)(paprium_s.ram + 0x1F18);

	*(uint16*) (paprium_s.ram + 0x1F16) = 0x0001;  /* dma count */

	*(uint16*) (paprium_s.ram + 0x1400) = 0x8f02;
	*(uint16*) (paprium_s.ram + 0x1402) = 0x9340;
	*(uint16*) (paprium_s.ram + 0x1404) = 0x9580;
	*(uint16*) (paprium_s.ram + 0x1406) = 0x9401;
	*(uint16*) (paprium_s.ram + 0x1408) = 0x9700;
	*(uint16*) (paprium_s.ram + 0x140a) = 0x9605;
	*(uint16*) (paprium_s.ram + 0x140c) = 0x7000;
	*(uint16*) (paprium_s.ram + 0x140e) = 0x0083;


	if( count < 80 )
		memcpy(paprium_s.ram + 0x1F20, paprium_s.ram + 0xB00, 14*8);
	else
		memcpy(paprium_s.ram + 0x1F20, paprium_s.exps_ram, 14*8);


	paprium_s.draw_src = 0x2000;
	paprium_s.draw_dst = 0x200;


#if DEBUG_MODE
	if( paprium_cmd_count != 0 ) {
		sprintf(error_str, ">>  Sprite_Start %d count\n", paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void paprium_sprite_stop(int arg)
{
#if DEBUG_SPRITE
	sprintf(error_str, "[%d] [%04X:%04X] DM Sprite_Stop %02X -- %d sprites\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg, *(uint16*)(paprium_s.ram + 0x1f18));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	int count = *(uint16*)(paprium_s.ram + 0x1F18);

	if( count == 0 ) {
		memset(paprium_s.ram + 0xB00, 0, 8);
		memset(paprium_s.exps_ram, 0, 8);
	}
	else if( count <= 80 ) {
		paprium_s.ram[0xB02 + (count-1)*8] = 0;

		if( count <= 14 )
			paprium_s.exps_ram[2 + (count-1)*8] = 0;
	}
	else
		paprium_s.exps_ram[2 + (count-81)*8] = 0;


	if( arg == 2 )
		*(uint16*) (paprium_s.ram + 0x1F1C) = 1;  /* exp.s - force draw */


#if DEBUG_MODE
	if( paprium_cmd_count != 0 ) {
		sprintf(error_str, ">>  Sprite_Stop %d count\n", paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void paprium_sprite_pause(int arg)
{
#if DEBUG_SPRITE
	sprintf(error_str, "[%d] [%04X:%04X] DM Sprite_Pause %02X -- %d sprites\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg, *(uint16*)(paprium_s.ram + 0x1f18));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	int count = *(uint16*)(paprium_s.ram + 0x1F18);

	if( count == 0 )
		memset(paprium_s.ram + 0xB00, 0, 8);


#if DEBUG_MODE
	if( paprium_cmd_count != 0 ) {
		sprintf(error_str, ">>  Sprite_Pause %d count\n", paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void paprium_scaler_init(int arg)
{
	int row, col, out;
	static uint8 temp[0x800];

#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM Scaler_Init %02X - %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg,
		*(uint16 *)(paprium_s.ram + 0x1E10), *(uint16 *)(paprium_s.ram + 0x1E12));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	int ptr = (*(uint16 *)(paprium_s.ram + 0x1E10) << 16) + *(uint16 *)(paprium_s.ram + 0x1E12);
	paprium_decoder_type(ptr, temp);


	out = 0;

	for( col = 0; col < 64; col++ ) {
		for( row = 0; row < 64; row++ ) {
			if( col & 1 )
				paprium_s.scaler_ram[out] = temp[row*32 + (col/2)^1] & 0x0F;
			else
				paprium_s.scaler_ram[out] = temp[row*32 + (col/2)^1] >> 4;

			out++;
		}
	}


#if DEBUG_MODE
	if( paprium_cmd_count != 2 ) {
		sprintf(error_str, ">>  Scaler_Init %d count\n", paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void paprium_scaler(int arg)
{
	int row, col;

	int left = *(uint16 *)(paprium_s.ram + 0x1E10);
	int right = *(uint16 *)(paprium_s.ram + 0x1E12);
	int scale = *(uint16 *)(paprium_s.ram + 0x1E14);
	int ptr = *(uint16 *)(paprium_s.ram + 0x1E16);
	int step = 64 * 0x10000 / scale;
	int ptr_frac = 0;


#if DEBUG_SPRITE
	sprintf(error_str, "[%d] [%04X:%04X] DM Scaler %02X --  %04X %04X %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg, *(uint16 *)(paprium_s.ram + 0x1E10), *(uint16 *)(paprium_s.ram + 0x1E12), *(uint16 *)(paprium_s.ram + 0x1E14), *(uint16 *)(paprium_s.ram + 0x1E16));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	for( col = 0; col < 128; col++ ) {
		int base = (col & 4) ? 0x600 : 0x200;
		int out = ((col / 8) * 64) + ((col & 2) / 2);

		for( row = 0; row < 64; row += 2 ) {
			if( (col >= left) && (col < right) ) {
				if(col & 1)
					paprium_s.ram[base + out^1] += paprium_s.scaler_ram[row*64 + ptr] & 0x0F;
				else
					paprium_s.ram[base + out^1] = paprium_s.scaler_ram[row*64 + ptr] << 4;
			}
			else if( (col & 1) == 0 )
				paprium_s.ram[base + out^1] = 0;

			out += 2;
		}

		if( (col >= left) && (col < right) && (ptr < 64) ) {
			ptr_frac += 0x10000;

			while( ptr_frac >= step ) {
				ptr++;
				ptr_frac -= step;
			}
		}
	}


#if DEBUG_MODE
	if( paprium_cmd_count != 4 ) {
		sprintf(error_str, ">>  Scaler %d count\n", paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void paprium_sram_read(int bank)
{
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM Sram_Read %02X -- %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		bank, *(uint16 *)(paprium_s.ram + 0x1E10));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	int offset = *(uint16 *)(paprium_s.ram + 0x1E10);

	if( (bank >= 1) && (bank <= 4) )
		memcpy(paprium_s.ram + offset, sram.sram + ((bank-1) * 0x780), 0x780);


#if DEBUG_MODE
	if( paprium_cmd_count != 1 ) {
		sprintf(error_str, ">>  Sram_Read %d count\n", paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void paprium_sram_write(int bank)
{
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM Sram_Write %02X -- %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		bank, *(uint16 *)(paprium_s.ram + 0x1E10), *(uint16 *)(paprium_s.ram + 0x1E12));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	/* *(uint16 *)(paprium_s.ram + 0x1E10) */
	int offset = *(uint16 *)(paprium_s.ram + 0x1E12);

	if( (bank >= 1) && (bank <= 4) )
		memcpy(sram.sram + ((bank-1) * 0x780), paprium_s.ram + offset, 0x780);


#if DEBUG_MODE
	if( *(uint16 *)(paprium_s.ram + 0x1E10) != 0xBEEF ) {
		sprintf(error_str, ">>  SRAM write 1E10 %X\n", *(uint16 *)(paprium_s.ram + 0x1E10));
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "sram write", 0);
	}

	if( paprium_cmd_count != 1 ) {
		sprintf(error_str, ">>  Sram_Write %d count\n", paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void paprium_mapper(int arg)
{
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM Mapper %02X -- %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg, *(uint16 *)(paprium_s.ram + 0x1E10), *(uint16 *)(paprium_s.ram + 0x1E12));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif


	memcpy(paprium_s.ram + 0x8000, cart.rom + 0x8000, 0x8000);  /* troll */


#if DEBUG_MODE
	if( paprium_cmd_count != 2 ) {
		sprintf(error_str, ">>  Mapper %d count\n", paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void paprium_boot(int arg)
{
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM Boot %02X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	paprium_music_ptr = (*(uint16*)(paprium_s.ram + 0x1E10) << 16) + *(uint16*)(paprium_s.ram + 0x1E12);
	/* (*(uint16*)(paprium_s.ram + 0x1E14) << 16) + *(uint16*)(paprium_s.ram + 0x1E16); */
	paprium_wave_ptr = (*(uint16*)(paprium_s.ram + 0x1E18) << 16) + *(uint16*)(paprium_s.ram + 0x1E1A);
	/* (*(uint16*)(paprium_s.ram + 0x1E1C) << 16) + *(uint16*)(paprium_s.ram + 0x1E1E); */
	paprium_sfx_ptr = (*(uint16*)(paprium_s.ram + 0x1E20) << 16) + *(uint16*)(paprium_s.ram + 0x1E22);
	paprium_sprite_ptr = (*(uint16*)(paprium_s.ram + 0x1E24) << 16) + *(uint16*)(paprium_s.ram + 0x1E26);
	paprium_tile_ptr = (*(uint16*)(paprium_s.ram + 0x1E28) << 16) + *(uint16*)(paprium_s.ram + 0x1E2A);

	/* paprium_wave_unpack(paprium_wave_ptr, paprium_wave_ram); */
	paprium_decoder_type(paprium_sprite_ptr, paprium_obj_ram);

	paprium_s.decoder_size = 0;


#if DEBUG_MODE
	paprium_cmd_count = 0;
#endif
}


static void paprium_EC(int arg)
{
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM EC %02X = %02X %02X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		arg, *(uint16 *)(paprium_s.ram + 0x1E10), *(uint16 *)(paprium_s.ram + 0x1E12));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

#if DEBUG_MODE
	if( *(uint16 *)(paprium_s.ram + 0x1E10) ) {
		sprintf(error_str, ">>  EC %02X\n", *(uint16 *)(paprium_s.ram + 0x1E10));
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "ec", 0);
	}
#endif

#if DEBUG_MODE
	if( paprium_cmd_count != 2 ) {
		sprintf(error_str, ">>  EC %d count\n", paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void paprium_music(int track)
{
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM Music %02X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		track);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	track &= 0x7F;


	int ptr = (*(uint16*)(cart.rom + paprium_music_ptr + track*4)<<16) + (*(uint16*)(cart.rom + paprium_music_ptr + track*4 + 2));
	paprium_decoder_type(paprium_music_ptr + ptr, paprium_s.music_ram);
	paprium_s.decoder_size = 0;


	paprium_s.music_section = 0;
	paprium_s.music_segment = 0;
	paprium_s.audio_tick = 0;


	memset(paprium_s.music, 0, sizeof(paprium_s.music));
	for( int ch = 0; ch < 26; ch++ ) {
		paprium_voice_t *voice = paprium_s.music + ch;

		voice->panning = 0x80;
		voice->volume = 0x80;
		voice->program = paprium_s.music_ram[0x2A + ch^1];
	}


#if DEBUG_MODE
	if( paprium_cmd_count != 0 ) {
		sprintf(error_str, ">>  Music %d count\n", paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif


#if 1
	paprium_load_mp3(track, 1);
#endif
}


static void paprium_music_volume(int level)
{
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM Music_Volume %02X -- %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		level, *(uint16 *)(paprium_s.ram + 0x1E10), *(uint16 *)(paprium_s.ram + 0x1E12));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	paprium_s.music_volume = level;
	/* *(uint16 *)(paprium_s.ram + 0x1E10); */
	/* *(uint16 *)(paprium_s.ram + 0x1E12); */


#if 0
	// stream mp3 / ogg, manage tempo
#endif


#if DEBUG_MODE
	if(*(uint16 *)(paprium_s.ram + 0x1E10) != 0x80) {
		sprintf(error_str, ">>  Music_Volume = %X\n", *(uint16 *)(paprium_s.ram + 0x1E10));
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "ooo", 0);
	}

	if(*(uint16 *)(paprium_s.ram + 0x1E12) != 0x00 && *(uint16 *)(paprium_s.ram + 0x1E12) != 0x08) {
		sprintf(error_str, ">>  Music_Volume-2 = %X\n", *(uint16 *)(paprium_s.ram + 0x1E12));
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "ooo", 0);
	}

	if( paprium_cmd_count != 2 ) {
		sprintf(error_str, ">>  Music_Volume %d count\n", paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void paprium_music_setting(int flag)
{
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM Music_Setting %02X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		flag);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	if( flag == 8 )
		paprium_s.music_segment = -1;

	else if( flag == 0 )
		paprium_s.music_segment = -1;


#if DEBUG_MODE
	else {
		sprintf(error_str, ">>  Music_Setting = %X\n", flag);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "ooo", 0);
	}
#endif


#if DEBUG_MODE
	if( paprium_cmd_count != 0 ) {
		sprintf(error_str, ">>  Music_Setting %d count\n", paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void paprium_music_special(int flag)
{
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM Music_Special %02X = %04X %04X %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		flag, *(uint16 *)(paprium_s.ram + 0x1E10), *(uint16 *)(paprium_s.ram + 0x1E12), *(uint16 *)(paprium_s.ram + 0x1E14), *(uint16 *)(paprium_s.ram + 0x1E16));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	if( flag == 1 ) {
		//*(uint16 *)(paprium_s.ram + 0x1E10)
		//*(uint16 *)(paprium_s.ram + 0x1E12)

#if DEBUG_MODE
		if( *(uint16 *)(paprium_s.ram + 0x1E10) != 0x40 ) {
			sprintf(error_str, ">>  Music_Special_01 %X count\n", *(uint16 *)(paprium_s.ram + 0x1E10));
			log_cb(RETRO_LOG_ERROR, error_str);
			MessageBoxA(0, error_str, "count", 0);
		}

		if( *(uint16 *)(paprium_s.ram + 0x1E12) != 0x08 ) {
			sprintf(error_str, ">>  Music_Special_01-2 %X count\n", *(uint16 *)(paprium_s.ram + 0x1E10));
			log_cb(RETRO_LOG_ERROR, error_str);
			MessageBoxA(0, error_str, "count", 0);
		}

		if( paprium_cmd_count != 2 ) {
			sprintf(error_str, ">>  Music_Special-01 %d count\n", paprium_cmd_count);
			log_cb(RETRO_LOG_ERROR, error_str);
			MessageBoxA(0, error_str, "count", 0);
		}
#endif
	}

	else if( flag == 2 ) {
		//*(uint16 *)(paprium_s.ram + 0x1E10)  /* 4 = crisis, 0 = normal ? */

#if DEBUG_MODE
		if( *(uint16 *)(paprium_s.ram + 0x1E10) != 4 && *(uint16 *)(paprium_s.ram + 0x1E10) != 0 ) {
			sprintf(error_str, ">>  Music_Special_02 %X count\n", *(uint16 *)(paprium_s.ram + 0x1E10));
			log_cb(RETRO_LOG_ERROR, error_str);
			MessageBoxA(0, error_str, "count", 0);
		}

		if( paprium_cmd_count != 1 ) {
			sprintf(error_str, ">>  Music_Special-02 %d count\n", paprium_cmd_count);
			log_cb(RETRO_LOG_ERROR, error_str);
			MessageBoxA(0, error_str, "count", 0);
		}
#endif
	}

	else if( flag == 4 ) {
		//*(uint16 *)(paprium_s.ram + 0x1E10)  /* 81 = blu pill */

#if DEBUG_MODE
		if( *(uint16 *)(paprium_s.ram + 0x1E10) != 0x80 && *(uint16 *)(paprium_s.ram + 0x1E10) != 0x81 ) {
			sprintf(error_str, ">>  Music_Special_04 %X count\n", *(uint16 *)(paprium_s.ram + 0x1E10));
			log_cb(RETRO_LOG_ERROR, error_str);
			MessageBoxA(0, error_str, "count", 0);
		}

		if( paprium_cmd_count != 1 ) {
			sprintf(error_str, ">>  Music_Special-04 %d count\n", paprium_cmd_count);
			log_cb(RETRO_LOG_ERROR, error_str);
			MessageBoxA(0, error_str, "count", 0);
		}
#endif
	}

	else if( flag == 6 ) {  /* sax man */
		//*(uint16 *)(paprium_s.ram + 0x1E10)
		//*(uint16 *)(paprium_s.ram + 0x1E12)
		//*(uint16 *)(paprium_s.ram + 0x1E14)

#if DEBUG_MODE
		if( *(uint16 *)(paprium_s.ram + 0x1E10) != 1 ) {
			sprintf(error_str, ">>  Music_Special_06 %X count\n", *(uint16 *)(paprium_s.ram + 0x1E10));
			log_cb(RETRO_LOG_ERROR, error_str);
			MessageBoxA(0, error_str, "count", 0);
		}

		if( paprium_cmd_count != 3 ) {
			sprintf(error_str, ">>  Music_Special-06 %d count\n", paprium_cmd_count);
			log_cb(RETRO_LOG_ERROR, error_str);
			MessageBoxA(0, error_str, "count", 0);
		}
#endif
	}

	else if( flag == 7 ) {  /* blu pill - stage title */
		//*(uint16 *)(paprium_s.ram + 0x1E10)

#if DEBUG_MODE
		if( *(uint16 *)(paprium_s.ram + 0x1E10) != 0x6C && *(uint16 *)(paprium_s.ram + 0x1E10) != 0xA0 ) {
			sprintf(error_str, ">>  Music_Special_07 %X count\n", *(uint16 *)(paprium_s.ram + 0x1E10));
			log_cb(RETRO_LOG_ERROR, error_str);
			MessageBoxA(0, error_str, "count", 0);
		}

		if( paprium_cmd_count != 1 ) {
			sprintf(error_str, ">>  Music_Special-06 %d count\n", paprium_cmd_count);
			log_cb(RETRO_LOG_ERROR, error_str);
			MessageBoxA(0, error_str, "count", 0);
		}
#endif
	}

#if DEBUG_MODE
	else {
		sprintf(error_str, ">>  Music special = %X\n", flag);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "ooo", 0);
	}
#endif
}


static void paprium_audio_setting(int flags)
{
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM Audio_Settings %02X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		flags);
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	paprium_s.audio_flags = flags;

	paprium_s.ram[0x1801] = flags & 0x01;  /* dac */

	paprium_s.ram[0x1800]  = (flags & 0x01) ? 0x80 : 0x00;  /* dac */
	paprium_s.ram[0x1800] += (flags & 0x02) ? 0x40 : 0x00;  /* ntsc */


#if DEBUG_MODE
	if( paprium_cmd_count != 0 ) {
		sprintf(error_str, ">>  Audio_Settings %d count\n", paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void paprium_sfx_volume(int level)
{
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM Sfx_Volume %02X -- %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		level, *(uint16 *)(paprium_s.ram + 0x1E10));
	log_cb(RETRO_LOG_ERROR, error_str);
#endif

	paprium_s.sfx_volume = level;
	/* *(uint16 *)(paprium_s.ram + 0x1E10); */
	/* *(uint16 *)(paprium_s.ram + 0x1E12); */


#if DEBUG_MODE
	if(*(uint16 *)(paprium_s.ram + 0x1E10) != 0x80) {
		sprintf(error_str, ">>  Sfx_Volume-1 = %X\n", *(uint16 *)(paprium_s.ram + 0x1E10));
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "ooo", 0);
	}

	if(*(uint16 *)(paprium_s.ram + 0x1E12) != 0x00 && paprium_cmd_count == 2) {
		sprintf(error_str, ">>  Sfx_Volume-2 = %X\n", *(uint16 *)(paprium_s.ram + 0x1E12));
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "ooo", 0);
	}
#endif


#if DEBUG_MODE
	if( paprium_cmd_count != 1 && paprium_cmd_count != 2 ) {
		sprintf(error_str, ">>  Sfx_Volume %d count\n", paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
}


static void paprium_cmd(int data)
{
	int cmd = data >> 8;
	int lcv;
	int retaddr;
	int argscnt;


	data &= 0xFF;

	if( m68k.prev_pc == 0xb4394 ) {
		retaddr = m68k_read_immediate_32(m68k.dar[15]+16);
		argscnt = 4;
	}
	else {
		retaddr = m68k_read_immediate_32(m68k.dar[15]+4);
		argscnt = 0;
	}



	switch(cmd) {
	case 0x81:
	case 0x83:
	case 0x95:
	case 0x96:
	case 0xA4:
	case 0xA9:
	case 0xB6:
		goto print_0;

	case 0xD0:
		goto print_2;
		goto print_5;

	case 0xE7:  /* T-574120-0 = after demo loop */
		goto print_9;

	case 0xD5:  /* secret room */
	case 0xEC:
		goto print_2;

	case 0x84: paprium_mapper(data); break;
	case 0x88: paprium_audio_setting(data); break;
	case 0x8C: paprium_music(data); break;
	case 0x8D: paprium_music_setting(data); break;
	case 0xAD: paprium_sprite(data); break;
	case 0xAE: paprium_sprite_start(data); break;
	case 0xAF: paprium_sprite_stop(data); break;
	case 0xB0: paprium_sprite_init(data); break;
	case 0xB1: paprium_sprite_pause(data); break;
	case 0xC6: paprium_boot(data); break;
	case 0xC9: paprium_music_volume(data); break;
	case 0xCA: paprium_sfx_volume(data); break;
	case 0xD1: goto sfx_play;
	case 0xD2: goto sfx_off;
	case 0xD3: goto sfx_loop;
	case 0xD6: paprium_music_special(data); break;
	case 0xDA: paprium_decoder(data); break;
	case 0xDB: paprium_decoder_copy(data); break;
	case 0xDF: paprium_sram_read(data); break;
	case 0xE0: paprium_sram_write(data); break;
	//case 0xF2: paprium_block_viewer(data); break;
	case 0xF4: paprium_scaler_init(data); break;
	case 0xF5: paprium_scaler(data); break;

	default: goto print_x;
	}


done:
	*(uint16*)(paprium_s.ram + 0x1FEA) &= 0x7FFF;
	return;



print_0:
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM %02X %02X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		cmd, data);
	log_cb(RETRO_LOG_ERROR, error_str);

	if( paprium_cmd_count != 0 ) {
		sprintf(error_str, "%X %d count\n", cmd, paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
	goto done;


print_1:
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM %02X %02X -- %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		cmd, data,
		*(uint16 *)(paprium_s.ram + 0x1E10));
	log_cb(RETRO_LOG_ERROR, error_str);

	if( paprium_cmd_count != 1 ) {
		sprintf(error_str, "%X %d count\n", cmd, paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
	goto done;


print_2:
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM %02X %02X -- %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		cmd, data,
		*(uint16 *)(paprium_s.ram + 0x1E10), *(uint16 *)(paprium_s.ram + 0x1E12));
	log_cb(RETRO_LOG_ERROR, error_str);

	if( paprium_cmd_count != 2 ) {
		sprintf(error_str, "%X %d count\n", cmd, paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
	goto done;


print_4:
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM %02X %02X -- %04X %04X %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		cmd, data,
		*(uint16 *)(paprium_s.ram + 0x1E10), *(uint16 *)(paprium_s.ram + 0x1E12), *(uint16 *)(paprium_s.ram + 0x1E14), *(uint16 *)(paprium_s.ram + 0x1E16));
	log_cb(RETRO_LOG_ERROR, error_str);


	if( paprium_cmd_count != 4 ) {
		sprintf(error_str, "%X %d count\n", cmd, paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
	goto done;


print_5:
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM %02X %02X -- %04X %04X %04X %04X %04X\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		cmd, data,
		*(uint16 *)(paprium_s.ram + 0x1E10), *(uint16 *)(paprium_s.ram + 0x1E12), *(uint16 *)(paprium_s.ram + 0x1E14), *(uint16 *)(paprium_s.ram + 0x1E16), *(uint16 *)(paprium_s.ram + 0x1E18));
	log_cb(RETRO_LOG_ERROR, error_str);

	if( paprium_cmd_count != 5 ) {
		sprintf(error_str, "%X %d count\n", cmd, paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
	goto done;


print_9:
#if DEBUG_MODE
	sprintf(error_str, "[%d] [%04X:%04X] DM %02X %02X -- %04X %04X %04X %04X %04X ..\n",
		v_counter,
		(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
		cmd, data,
		*(uint16 *)(paprium_s.ram + 0x1E10), *(uint16 *)(paprium_s.ram + 0x1E12), *(uint16 *)(paprium_s.ram + 0x1E14), *(uint16 *)(paprium_s.ram + 0x1E16), *(uint16 *)(paprium_s.ram + 0x1E18));
	log_cb(RETRO_LOG_ERROR, error_str);

	if( paprium_cmd_count != 9 ) {
		sprintf(error_str, "%X %d count\n", cmd, paprium_cmd_count);
		log_cb(RETRO_LOG_ERROR, error_str);
		MessageBoxA(0, error_str, "count", 0);
	}
#endif
	goto done;


print_x:
#if DEBUG_MODE
	if( argscnt == 4 ) {
		sprintf(error_str, "[%d] [%04X:%04X] DM %02X %02X -- %04X %04X %04X %04X\n",
			v_counter,
			(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
			cmd, data,
			*(uint16 *)(paprium_s.ram + 0x1E10), *(uint16 *)(paprium_s.ram + 0x1E12), *(uint16 *)(paprium_s.ram + 0x1E14), *(uint16 *)(paprium_s.ram + 0x1E16));
		log_cb(RETRO_LOG_ERROR, error_str);
	}

	else {
		sprintf(error_str, "[%d] [%04X:%04X] DM %02X %02X ## %04X %04X %04X %04X\n",
			v_counter,
			(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff,
			cmd, data,
			*(uint16 *)(paprium_s.ram + 0x1E10), *(uint16 *)(paprium_s.ram + 0x1E12), *(uint16 *)(paprium_s.ram + 0x1E14), *(uint16 *)(paprium_s.ram + 0x1E16));
		log_cb(RETRO_LOG_ERROR, error_str);
	}

	sprintf(error_str, "DM %02X %02X\n", cmd, data);
	log_cb(RETRO_LOG_ERROR, error_str);
	MessageBoxA(0, error_str, "DM", 0);
#endif
	goto done;


sfx_play:
	{
		int chan, vol, pan, flags;
		int ptr, size, type;
		int lcv;
		int newch = 0, maxtime = 0;
		const int rates[] = {1,2,4,5,8,9};
		uint8 *sfx = paprium_sfx_ptr + cart.rom;


#if DEBUG_MODE
		sprintf(error_str, "[%d] [%04X:%04X <== %04X:%04X] DM Sfx_Play %02X -- %04X %04X %04X %04X\n",
			v_counter,
			(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff, (retaddr>>16)&0xffff, retaddr&0xffff,
			data, *(uint16 *)(paprium_s.ram + 0x1E10), *(uint16 *)(paprium_s.ram + 0x1E12), *(uint16 *)(paprium_s.ram + 0x1E14), *(uint16 *)(paprium_s.ram + 0x1E16));
		log_cb(RETRO_LOG_ERROR, error_str);
#endif

		chan = *(uint16 *)(paprium_s.ram + 0x1E10);
		vol = *(uint16 *)(paprium_s.ram + 0x1E12);
		pan = *(uint16 *)(paprium_s.ram + 0x1E14);
		flags = *(uint16 *)(paprium_s.ram + 0x1E16);


#if DEBUG_MODE
		if(flags & 0x100) {
			sprintf(error_str, "Sfx flags %X\n", flags);
			//MessageBoxA(0, error_str, "Sfx flags", MB_OK );			
			log_cb(RETRO_LOG_ERROR, error_str);
		}  /* stage1 alarm = 8100 */
		//else if( flags <= 0x8100 ) {}
		else if( flags & 0x8000 ) {}
		else if( flags & 0x4000 ) {}
		else if( flags & 0x2000 ) {}
		else if( flags & 0x800 ) {}
		//else if( flags & 0x100 ) {}
		else if( flags & 0x400 ) {}
		else if( flags == 0 ) {}
		else {
			sprintf(error_str, "Sfx flags %X\n", flags);
			log_cb(RETRO_LOG_ERROR, error_str);
			MessageBoxA(0, error_str, "Sfx flags", MB_OK );
		}


		if( chan > 0x80 ) MessageBoxA(0, "Sfx channel", "Warn", MB_OK );
#endif


		ptr = (*(uint16 *)(sfx + data*8) << 16) | (*(uint16 *)(sfx + data*8 + 2));
		size = (*(uint8 *)(sfx + data*8 + 4) << 16) | (*(uint16 *)(sfx + data*8 + 6));
		type = *(uint8 *)(sfx + data*8 + 5);


#if DEBUG_MODE
		sprintf(error_str, "%X %X | %X %X %X | %X %X %X\n",
			paprium_sfx_ptr + data*8, paprium_sfx_ptr + ptr,
			ptr, size, type,
			rates[type >> 4], type & 0x03, type & 0x0C);
		log_cb(RETRO_LOG_ERROR, error_str);
#endif


		for( lcv = 0; lcv < 8; lcv++, chan >>= 1 ) {
			if( (chan & 1) == 0 ) continue;

			if( paprium_s.sfx[lcv].size ) {
				if( maxtime < paprium_s.sfx[lcv].time ) {
					maxtime = paprium_s.sfx[lcv].time;
					newch = lcv;
				}
				continue;
			}

			newch = lcv;
			break;
		}


		paprium_s.sfx[newch].volume = vol;
		paprium_s.sfx[newch].panning = pan;
		paprium_s.sfx[newch].type = type;

		paprium_s.sfx[newch].num = data;
		paprium_s.sfx[newch].flags = flags;

		paprium_s.sfx[newch].loop = 0;
		paprium_s.sfx[newch].count = 0;
		paprium_s.sfx[newch].time = 0;
		paprium_s.sfx[newch].tick = 0;
		paprium_s.sfx[newch].decay = 0;

		if( flags & 0x4000 )
			paprium_s.sfx[newch].echo = (paprium_s.echo_pan++) & 1;

		paprium_s.sfx[newch].ptr = ptr;
		paprium_s.sfx[newch].start = ptr;
		paprium_s.sfx[newch].size = size;
	}
	goto done;


sfx_loop:
	{
		int lcv;


#if DEBUG_MODE
		sprintf(error_str, "[%d] [%04X:%04X <== %04X:%04X] DM Sfx_Loop %02X -- %04X %04X %04X\n",
			v_counter,
			(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff, (retaddr>>16)&0xffff, retaddr&0xffff,
			data, *(uint16 *)(paprium_s.ram + 0x1E10), *(uint16 *)(paprium_s.ram + 0x1E12), *(uint16 *)(paprium_s.ram + 0x1E14));
		log_cb(RETRO_LOG_ERROR, error_str);
#endif


		for( lcv = 0; lcv < 8; lcv++, data >>= 1 ) {
			if( (data & 1) == 0 ) continue;

			paprium_s.sfx[lcv].volume = *(uint16 *)(paprium_s.ram + 0x1E10);
			paprium_s.sfx[lcv].panning = *(uint16 *)(paprium_s.ram + 0x1E12);
			paprium_s.sfx[lcv].decay = *(uint16 *)(paprium_s.ram + 0x1E14);

			paprium_s.sfx[lcv].loop = 1;

			break;
		}
	}
	goto done;


sfx_off:
	{
		int flags;


#if DEBUG_MODE
		sprintf(error_str, "[%d] [%04X:%04X <== %04X:%04X] DM Sfx_Off %02X -- %04X\n",
			v_counter,
			(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff, (retaddr>>16)&0xffff, retaddr&0xffff,
			data, *(uint16 *)(paprium_s.ram + 0x1E10));
		log_cb(RETRO_LOG_ERROR, error_str);
#endif


		flags = *(uint16 *)(paprium_s.ram + 0x1E10);


		if(0) {}
		//else if( flags == 6 ) {}
		else if( flags == 0 ) {}
		else {} //MessageBoxA(0, "Sfx_Off flags", "Warn", MB_OK );


		for( lcv = 0; lcv < 8; lcv++ ) {
			if( !(data & (1 << lcv) ) ) continue;

			if( flags == 0 ) {
				paprium_s.sfx[lcv].size = 0;
			}

			paprium_s.sfx[lcv].decay = flags;
			paprium_s.sfx[lcv].loop = 0;

			break;
		}
	}
	goto done;
}


static int paprium_addr_test(uint32 address, int mode)
{
#if DEBUG_MODE == 0
	return 0;
#endif


	if( address < 0x100 ) return 0;  /* exception table */

	if( skip_boot1 ) {
		if( address == 0x14CE ) {
			skip_boot1 = 0;
		}
		return 0;
	}


#if DEBUG_MODE
	if( mode == 0 ) {
		if( address >= 0x8000 ) return 0;
	}
#endif


#if DEBUG_MODE
	if( address >= 0x2000 && address < 0x8000 ) return 0;
#endif

#if DEBUG_MODE
	if( address >= 0x200 && address < 0xb00 ) return 0;
#endif

#if DEBUG_MODE
	if( address >= 0xb00 && address < 0xf00 ) return 0;
	if( address >= 0x1f20 && address < 0x1f90 ) return 0;  /* exp.s */
#endif

#if DEBUG_MODE
	if( address >= 0xF80 && address < 0x1290 ) return 0;  /* OBJ list */
#endif

#if 0
	if( address >= 0x1290 && address < 0x1400 ) {
		MessageBoxA(0, "address", "me", 0);
		return 0;
	}
#endif

#if 0
	if( address >= 0x1B40 && address < 0x1b90 ) {
		MessageBoxA(0, "address 2", "me", 0);
		return 1;
	}
#endif

#if 0
	if( address >= 0xd80 && address < 0xf80 ) {
		MessageBoxA(0, "address 3", "me", 0);
		return 1;
	}
#endif

#if DEBUG_MODE
	if( address >= 0x1400 && address < 0x1800 ) return 0;  /* DMA list */	
#endif

#if DEBUG_MODE
	if( address >= 0x1800 && address < 0x1A00 ) return 0;  /* DAC list ?? */
#endif

#if DEBUG_MODE
	if( address == 0x1F10 ) return 0;  /* DMA size = header [10h] + packet [x words] */
	if( address == 0x1F16 ) return 0;  /* DMA transfer count */
	if( address == 0x1F18 ) return 0;  /* OBJ count */
	if( address == 0x1F12 ) return 0;  /* sprite ram available ??? 0B00 or 1200 */
#endif

#if DEBUG_MODE
	if( address >= 0x1b98 && address < 0x1c00 ) return 0;  /* soundshock x26 */
#endif


	if( address == 0x1FEA ) return 0;  /* DM command */


	//if( address == 0x1FE4 ) return 0;  /* DM ?? */
	//if( address == 0x1FE6 ) return 0;  /* DM ?? */

	return 1;
}


static uint32 paprium_r8(uint32 address)
{
	int data = paprium_s.ram[address^1];


#if DEBUG_MODE
	if( paprium_addr_test(address, 0) ) {
		sprintf(error_str, "[%04X:%04X] R8 %04X = %04X\n",
			(m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff, address, data);

		log_cb(RETRO_LOG_ERROR, error_str);
	}
#endif


	if( address == 0x1800 )
		data = 0;

	else if( address >= 0x1880 && address < 0x1b00 ) {
		data = rand() % 256;
		//paprium_s.ram[address^1] = data;  /* dma blowout if no limit */
	}


	return data;
}


static uint32 paprium_r16(uint32 address)
{
	int data = 0;


	if( (address == 0xC000) && (paprium_s.decoder_size > 0) ) {
#if DEBUG_MODE
		sprintf(error_str, "[%d] Decoder %X %X @ C000\n", v_counter, paprium_s.decoder_ptr, paprium_s.decoder_size);
		log_cb(RETRO_LOG_ERROR, error_str);
#endif

		int max, size;

		if( paprium_s.decoder_mode == 2 )
			max = 0x4000;

		else if( paprium_s.decoder_mode == 7 )
			max = 0x800;


		size = (paprium_s.decoder_size > max) ? max : paprium_s.decoder_size;

		memcpy(paprium_s.ram + 0xC000, paprium_s.decoder_ram + paprium_s.decoder_ptr, size);

		paprium_s.decoder_ptr += size;
		paprium_s.decoder_size -= size;
	}


	switch( address ) {
	case 0x1FE4:
		data = 0xFFFF;
		data &= ~(1<<2);
		data &= ~(1<<6);
		break;

	case 0x1FE6:
		data = 0xFFFF;
		data &= ~(1<<14);
		data &= ~(1<<8);  /* sram */
		data &= ~(1<<9);  /* sram */
		break;
	
	case 0x1FEA:
		data = 0xFFFF;
		data &= ~(1<<15);
		break;

	default:
		data = *(uint16 *)(paprium_s.ram + address);
		break;
	}


#if DEBUG_MODE
	if( paprium_addr_test(address, 0) ) {
		sprintf(error_str, "[%d] [%04X:%04X] R16 %04X = %04X\n",
			v_counter, (m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff, address, data);

		log_cb(RETRO_LOG_ERROR, error_str);
	}
#endif

	return data;
}


static void paprium_w8(uint32 address, uint32 data)
{
#if DEBUG_MODE
	if( paprium_addr_test(address, 1) ) {
		sprintf(error_str, "[%d] [%04X:%04X] W8 %04X = %02X\n",
			v_counter, (m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff, address, data);

		log_cb(RETRO_LOG_ERROR, error_str);
	}
#endif

	paprium_s.ram[address^1] = data;
}


static void paprium_w16(uint32 address, uint32 data)
{
#if DEBUG_MODE
	if( paprium_addr_test(address, 1) ) {
		sprintf(error_str, "[%d] [%04X:%04X] W16 %04X = %04X\n",
			v_counter, (m68k.prev_pc>>16)&0xffff, m68k.prev_pc&0xffff, address, data);

		log_cb(RETRO_LOG_ERROR, error_str);
	}

	if( address >= 0x1E10 && address <= 0x1E30 ) paprium_cmd_count++;
#endif

	*(uint16 *)(paprium_s.ram + address) = data;

	if( address == 0x1FEA ) {
		paprium_cmd(data);

		paprium_cmd_count = 0;
	}
}


static uint32 paprium_io_r8(uint32 address)
{
	if( address == 0xA14101 )
		return paprium_tmss;

	ctrl_io_read_byte(address);
}


static void paprium_io_w8(uint32 address, uint32 data)
{
	if( address >= 0xA130F3 && address <= 0xA130FF )  /* no bank mapper */
		return;

	if( address == 0xA14101 ) {
		paprium_tmss = data;
		return;
	}

	ctrl_io_write_byte(address, data);
}


static void paprium_map()
{
    m68k.memory_map[0x00].base    = paprium_s.ram;
    m68k.memory_map[0x00].read8   = paprium_r8;
    m68k.memory_map[0x00].read16  = paprium_r16;
    m68k.memory_map[0x00].write8  = paprium_w8;
    m68k.memory_map[0x00].write16 = paprium_w16;

	m68k.memory_map[0xA1].read8  = paprium_io_r8;
	m68k.memory_map[0xA1].write8 = paprium_io_w8;

	zbank_memory_map[0x00].read  = paprium_r8;
	zbank_memory_map[0x00].write = paprium_w8;
}


static void paprium_init()
{
	int ptr;

	paprium_map();


	memset(&paprium_s, 0, sizeof(paprium_s));
	memcpy(paprium_s.ram, cart.rom, 0x10000);


#if 1  /* fast loadstate */
	ptr = (*(uint16*)(cart.rom + 0xaf77c) << 16) + *(uint16*)(cart.rom + 0xaf77e);

	paprium_music_ptr = (*(uint16*)(cart.rom + 0x10054)<< 16) + *(uint16*)(cart.rom + 0x10056);
	// (*(uint16*)(cart.rom + 0x10058)<< 16) + *(uint16*)(cart.rom + 0x1005a);
	paprium_wave_ptr = (*(uint16*)(cart.rom + ptr + 0x774)<< 16) + *(uint16*)(cart.rom + ptr + 0x776);
	// (*(uint16*)(cart.rom + 0x1005c)<< 16) + *(uint16*)(cart.rom + 0x1005e);
	paprium_sfx_ptr = (*(uint16*)(cart.rom + ptr + 0x778)<< 16) + *(uint16*)(cart.rom + ptr + 0x77a);
	paprium_sprite_ptr = (*(uint16*)(cart.rom + 0x10014)<< 16) + *(uint16*)(cart.rom + 0x10016);
	paprium_tile_ptr = (*(uint16*)(cart.rom + ptr + 0x780)<< 16) + *(uint16*)(cart.rom + ptr + 0x782);

	paprium_decoder_type(paprium_sprite_ptr, paprium_obj_ram);
	paprium_s.decoder_size = 0;


	if(0) {  /* www.wavpack.com */
		/* warn: adding payload after 8MB ROM breaks Mega CD */

		FILE *fp;

#ifdef _WIN32
		sprintf(error_str, "%s\\paprium\\paprium.wav", g_rom_dir);
#else
		sprintf(error_str, "%s/paprium/paprium.wav", g_rom_dir);
#endif

		fp = fopen(error_str, "rb");
		if(!fp) {
			//MessageBoxA(0,"help","r",0);
			exit(1);
		}
		fseek(fp, 0x2C, SEEK_SET);
		int size = fread(paprium_wave_ram, 1, 0x180000, fp);
		for(int lcv = 0; lcv < size; lcv +=2 ) {
			int data = *(uint16*)(paprium_wave_ram + lcv);
			data = ((data & 0xFF00) >> 8) + ((data & 0x00FF) << 8);
			*(uint16*)(paprium_wave_ram + lcv) = data;
		}
		fclose(fp);
	}

	*(uint16*)(paprium_s.ram + 0x192) = 0x3634;  /* 6-button, multitap */
#endif


	paprium_s.ram[0x1D1D^1] = 0x04;  /* rom ok - dynamic patch */
	paprium_s.ram[0x1D2C^1] = 0x67;


#if 1  /* boot hack */
	*(uint16*) (paprium_s.ram + 0x1560) = 0x4EF9;
	*(uint16*) (paprium_s.ram + 0x1562) = 0x0001;
	*(uint16*) (paprium_s.ram + 0x1564) = 0x0100;
#endif


	fast_dma_hack = 1;  /* skip vram management */


#if DEBUG_CHEAT  /* cheat - big hurt */
	*(uint16*) (cart.rom + 0x9FE38 + 6) = 0x007F;	/* Tug */
	*(uint16*) (cart.rom + 0x9FE58 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9FF18 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9FF38 + 6) = 0x007F;

	*(uint16*) (cart.rom + 0x9FB58 + 6) = 0x007F;	/* Alex */
	*(uint16*) (cart.rom + 0x9FB78 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9FBF8 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9FC18 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9FCB8 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9FCD8 + 6) = 0x007F;

	*(uint16*) (cart.rom + 0x9F758 + 6) = 0x007F;	/* Dice */
	*(uint16*) (cart.rom + 0x9F778 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9F798 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9F7B8 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9F7D8 + 6) = 0x007F;
	*(uint16*) (cart.rom + 0x9F898 + 6) = 0x007F;
#endif


#if 1  /* WM text - pre-irq delay */
	*(uint16*)(cart.rom + 0xb9094) = 0x2079;
	*(uint16*)(cart.rom + 0xb9096) = 0x000a;
	*(uint16*)(cart.rom + 0xb9098) = 0xf85c;

	*(uint16*)(cart.rom + 0xb909a) = 0x20bc;
	*(uint16*)(cart.rom + 0xb909c) = 0x0000;
	*(uint16*)(cart.rom + 0xb909e) = 0x0003;

	*(uint16*)(cart.rom + 0xb90a0) = 0x20bc;
	*(uint16*)(cart.rom + 0xb90a2) = 0x0000;
	*(uint16*)(cart.rom + 0xb90a4) = 0x0003;

	*(uint16*)(cart.rom + 0xb90a6) = 0x20bc;
	*(uint16*)(cart.rom + 0xb90a8) = 0x0000;
	*(uint16*)(cart.rom + 0xb90aa) = 0x0003;

	*(uint16*)(cart.rom + 0xb90ac) = 0x20bc;
	*(uint16*)(cart.rom + 0xb90ae) = 0x0000;
	*(uint16*)(cart.rom + 0xb90b0) = 0x0003;

	*(uint16*)(cart.rom + 0xb90b2) = 0x20bc;
	*(uint16*)(cart.rom + 0xb90b4) = 0x0000;
	*(uint16*)(cart.rom + 0xb90b6) = 0x0003;
#endif


	srand((uintptr_t) &ptr);  /* discard time library */
	paprium_s.echo_pan = rand();


	paprium_s.music_segment = -1;
}


static void paprium_reset()
{
	paprium_init();

	mp3dec_init(&paprium_mp3d);

	mp3dec_init(&paprium_mp3d_boss1);
	mp3dec_init(&paprium_mp3d_boss2);
	mp3dec_init(&paprium_mp3d_boss3);
	mp3dec_init(&paprium_mp3d_boss4);
	paprium_load_mp3_boss();


	//extern int trace_start;
	//trace_start = 1;
	skip_boot1 = 1;
}
