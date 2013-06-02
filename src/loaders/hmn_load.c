/* Extended Module Player
 * Copyright (C) 1996-2013 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU Lesser General Public License. See COPYING.LIB
 * for more information.
 */

#include "loader.h"
#include "mod.h"
#include "period.h"
#include "hmn_extras.h"

/*
 * From http://www.livet.se/mahoney/:
 *
 * Most modules from His Master's Noise uses special chip-sounds or
 * fine-tuning of samples that never was a part of the standard NoiseTracker
 * v2.0 command set. So if you want to listen to them correctly use an Amiga
 * emulator and run the demo! DeliPlayer does a good job of playing them
 * (there are some occasional error mostly concerning vibrato and portamento
 * effects, but I can live with that!), and it can be downloaded from
 * http://www.deliplayer.com
 */

/*
 * From http://www.cactus.jawnet.pl/attitude/index.php?action=readtext&issue=12&which=12
 *
 * [Bepp] For your final Amiga release, the music disk His Master's Noise,
 * you developed a special version of NoiseTracker. Could you tell us a
 * little about this project?
 *
 * [Mahoney] I wanted to make a music disk with loads of songs, without being
 * too repetitive or boring. So all of my "experimental features" that did not
 * belong to NoiseTracker v2.0 were put into a separate version that would
 * feature wavetable sounds, chord calculations, off-line filter calculations,
 * mixing, reversing, sample accurate delays, resampling, fades - calculations
 * that would be done on a standard setup of sounds instead of on individual
 * modules. This "compression technique" lead to some 100 songs fitting on two
 * standard 3.5" disks, written by 22 different composers. I'd say that writing
 * a music program does give you loads of talented friends - you should try
 * that yourself someday!
 */

/*
 * From: Pex Tufvesson
 * To: Claudio Matsuoka
 * Date: Sat, Jun 1, 2013 at 4:16 AM
 * Subject: Re: A question about (very) old stuff
 *
 * (...)
 * If I remember correctly, these chip sounds were done with several short
 * waveforms, and an index table that was loopable that would choose which
 * waveform to play each frame. And, you didn't have to "draw" every
 * waveform in the instrument - you would choose which waveforms to draw
 * and the replayer would (at startup) interpolate the waveforms that you
 * didn't draw.
 *
 * In the special noisetracker, you could draw all of these waveforms, draw
 * the index table, and the instrument would be stored in one of the
 * "patterns" of the song.
 */

static int hmn_test(FILE *, char *, const int);
static int hmn_load(struct module_data *, FILE *, const int);

const struct format_loader hmn_loader = {
	"His Master's Noisetracker (MOD)",
	hmn_test,
	hmn_load
};

/* His Master's Noise M&K! will fail in regular Noisetracker loading
 * due to invalid finetune values.
 */
#define MAGIC_FEST	MAGIC4('F', 'E', 'S', 'T')
#define MAGIC_MK	MAGIC4('M', '&', 'K', '!')

static int hmn_test(FILE * f, char *t, const int start)
{
	int magic;

	fseek(f, start + 1080, SEEK_SET);
	magic = read32b(f);

	if (magic != MAGIC_FEST && magic != MAGIC_MK)
		return -1;

	fseek(f, start + 0, SEEK_SET);
	read_title(f, t, 20);

	return 0;
}

struct mupp {
	uint8 prgon;
	uint8 pattno;
	uint8 dataloopstart;
	uint8 dataloopend;
};

static int hmn_load(struct module_data *m, FILE * f, const int start)
{
	struct xmp_module *mod = &m->mod;
	int i, j;
	struct xmp_event *event;
	struct mod_header mh;
	struct mupp mupp[31];
	uint8 mod_event[4];
	int mupp_index, num_mupp;

	LOAD_INIT();

	/*
	 *    clr.b   $1c(a6) ;prog on/off
	 *    CMP.L   #'Mupp',-$16(a3,d4.l)
	 *    bne.s   noprgo
	 *    move.l  a0,-(a7)
	 *    move.b  #1,$1c(a6)      ;prog on
	 *    move.l  l697,a0
	 *    lea     $43c(a0),a0
	 *    moveq   #0,d2
	 *    move.b  -$16+$4(a3,d4.l),d2     ;pattno
	 *    mulu    #$400,d2
	 *    lea     (a0,d2.l),a0
	 *    move.l  a0,4(a6)        ;proginstr data-start
	 *    moveq   #0,d2
	 *    MOVE.B  $3C0(A0),$12(A6)
	 *    AND.B   #$7F,$12(A6)
	 *    move.b  $380(a0),d2
	 *    mulu    #$20,d2
	 *    lea     (a0,d2.w),a0
	 *    move.l  a0,$a(a6)       ;loopstartmempoi = startmempoi
	 *    move.B  $3(a3,d4.l),$13(a6)     ;volume
	 *    move.b  -$16+$5(a3,d4.l),8(a6)  ;dataloopstart
	 *    move.b  -$16+$6(a3,d4.l),9(a6)  ;dataloopend
	 *    move.w  #$10,$e(a6)     ;looplen
	 *    move.l  (a7)+,a0
	 *    MOVE.W  $12(A6),(A2)
	 *    AND.W   #$FF,(A2)
	 *    BRA.S   L505_LQ
	 */

	/*
	 * Wavetable structure is 22 * 32 byte waveforms and 32 byte
	 * wave control data with looping.
	 */
	memset(mupp, 0, 31 * sizeof (struct mupp));

	fread(&mh.name, 20, 1, f);
	num_mupp = 0;

	for (i = 0; i < 31; i++) {
		fread(&mh.ins[i].name, 22, 1, f);	/* Instrument name */
		if (memcmp(mh.ins[i].name, "Mupp", 4) == 0) {
			mupp[i].prgon = 1;
			mupp[i].pattno = mh.ins[i].name[4];
			mupp[i].dataloopstart = mh.ins[i].name[5];
			mupp[i].dataloopend = mh.ins[i].name[6];
			num_mupp++;
		}

		mh.ins[i].size = read16b(f);	/* Length in 16-bit words */
		mh.ins[i].finetune = read8(f);	/* Finetune (signed nibble) */
		mh.ins[i].volume = read8(f);	/* Linear playback volume */
		mh.ins[i].loop_start = read16b(f);	/* Loop start in 16-bit words */
		mh.ins[i].loop_size = read16b(f);	/* Loop size in 16-bit words */
	}
	mh.len = read8(f);
	mh.restart = read8(f);
	fread(&mh.order, 128, 1, f);
	fread(&mh.magic, 4, 1, f);

	mod->chn = 4;
	mod->ins = 31;
	mod->smp = mod->ins + 28 * num_mupp;
	mod->len = mh.len;
	mod->rst = mh.restart;
	memcpy(mod->xxo, mh.order, 128);

	for (i = 0; i < 128; i++) {
		if (mod->xxo[i] > mod->pat)
			mod->pat = mod->xxo[i];
	}

	mod->pat++;

	mod->trk = mod->chn * mod->pat;

	strncpy(mod->name, (char *)mh.name, 20);
	set_type(m, "%s (%4.4s)", "His Master's Noisetracker", mh.magic);
	MODULE_INFO();

	INSTRUMENT_INIT();

	for (i = 0; i < mod->ins; i++) {
		int num;

		if (mupp[i].prgon) {
			mod->xxi[i].nsm = num = 28;
			snprintf((char *)mod->xxi[i].name, XMP_NAME_SIZE,
				"Mupp pat=%02x (%02x,%02x)", mupp[i].pattno,
				mupp[i].dataloopstart, mupp[i].dataloopend);
			mod->xxi[i].extra = calloc(1, sizeof(struct hmn_extras));
			if (mod->xxi[i].extra == NULL)
				return -1;
			HMN_EXTRA(mod->xxi[i])->magic = HMN_EXTRAS_MAGIC;
		} else {
			num = 1;
			mod->xxi[i].nsm = mh.ins[i].size > 0 ? 1 : 0;
			copy_adjust(mod->xxi[i].name, mh.ins[i].name, 22);

			mod->xxs[i].len = 2 * mh.ins[i].size;
			mod->xxs[i].lps = 2 * mh.ins[i].loop_start;
			mod->xxs[i].lpe = mod->xxs[i].lps +
						2 * mh.ins[i].loop_size;
			mod->xxs[i].flg = mh.ins[i].loop_size > 1 ?
						XMP_SAMPLE_LOOP : 0;
		}

		mod->xxi[i].sub = calloc(sizeof(struct xmp_subinstrument), num);
		if (mod->xxi[i].sub == NULL)
			return -1;

		for (j = 0; j < num; j++) {
			mod->xxi[i].sub[j].fin =
					-(int8)(mh.ins[i].finetune << 4);
			mod->xxi[i].sub[j].vol = mh.ins[i].volume;
			mod->xxi[i].sub[j].pan = 0x80;
			mod->xxi[i].sub[j].sid = i;
		}
	}

	PATTERN_INIT();

	/* Load and convert patterns */
	D_(D_INFO "Stored patterns: %d", mod->pat);

	for (i = 0; i < mod->pat; i++) {
		PATTERN_ALLOC(i);
		mod->xxp[i]->rows = 64;
		TRACK_ALLOC(i);

		for (j = 0; j < (64 * 4); j++) {
			event = &EVENT(i, j % 4, j / 4);
			fread(mod_event, 1, 4, f);
			decode_noisetracker_event(event, mod_event);
		}
	}

	m->quirk |= QUIRK_MODRNG;

	/* Load samples */

	D_(D_INFO "Stored samples: %d", mod->smp);

	for (i = 0; i < 31; i++) {
		load_sample(m, f, SAMPLE_FLAG_FULLREP,
			    &mod->xxs[mod->xxi[i].sub[0].sid], NULL);
	}


	/* Load Mupp samples */

	mupp_index = 0;
	for (i = 0; i < 31; i ++) {
		struct hmn_extras *extra =
				(struct hmn_extras *)mod->xxi[i].extra;

		if (!mupp[i].prgon)
			continue;

		fseek(f, start + 1084 + 1024 * mupp[i].pattno, SEEK_SET);
		for (j = 0; j < 28; j++) {
			int k = 31 + 28 * mupp_index + j;
			mod->xxi[i].sub[j].sid = k;
			mod->xxs[k].len = 32;
			mod->xxs[k].lps = 0;
			mod->xxs[k].lpe = 32;
			mod->xxs[k].flg = XMP_SAMPLE_LOOP;
			load_sample(m, f, 0, &mod->xxs[k], NULL);
		}

		extra->dataloopstart = mupp[i].dataloopstart;
		extra->dataloopend = mupp[i].dataloopend;

		fread(extra->data, 1, 64, f);
		fread(extra->progvolume, 1, 64, f);

		mupp_index++;
	}

	return 0;
}