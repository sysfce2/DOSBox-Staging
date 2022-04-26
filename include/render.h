/*
 *  Copyright (C) 2002-2021  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_RENDER_H
#define DOSBOX_RENDER_H

// 0: complex scalers off, scaler cache off, some simple scalers off, memory requirements reduced
// 1: complex scalers off, scaler cache off, all simple scalers on
// 2: complex scalers off, scaler cache on
// 3: complex scalers on
#define RENDER_USE_ADVANCED_SCALERS 3

#include "../src/gui/render_scalers.h"

#define RENDER_SKIP_CACHE	16
//Enable this for scalers to support 0 input for empty lines
//#define RENDER_NULL_INPUT

#if defined(C_FREETYPE)
#define USE_TTF 1
#endif

struct RenderPal_t {
	struct {
		uint8_t red = 0;
		uint8_t green = 0;
		uint8_t blue = 0;
		uint8_t unused = 0;
	} rgb[256] = {};
	union {
		uint16_t b16[256];
		uint32_t b32[256] = {};
	} lut = {};
	bool changed = false;
	uint8_t modified[256] = {};
	uint32_t first = 0;
	uint32_t last = 0;
};

struct Render_t {
	struct {
		uint32_t width = 0;
		uint32_t start = 0;
		uint32_t height = 0;
		unsigned bpp = 0;
		bool dblw = false;
		bool dblh = false;
		double ratio = 0;
		double fps = 0;
	} src = {};
	struct {
		int count = 0;
		int max = 0;
		uint32_t index = 0;
		uint8_t hadSkip[RENDER_SKIP_CACHE] = {};
	} frameskip = {};
	struct {
		uint32_t size = 0;
		scalerMode_t inMode = {};
		scalerMode_t outMode = {};
		scalerOperation_t op = {};
		bool clearCache = false;
		bool forced = false;
		ScalerLineHandler_t lineHandler = nullptr;
		ScalerLineHandler_t linePalHandler = nullptr;
		ScalerComplexHandler_t complexHandler = nullptr;
		uint32_t blocks = 0;
		uint32_t lastBlock = 0;
		int outPitch = 0;
		uint8_t *outWrite = nullptr;
		uint32_t cachePitch = 0;
		uint8_t *cacheRead = nullptr;
		uint32_t inHeight = 0;
		uint32_t inLine = 0;
		uint32_t outLine = 0;
	} scale = {};
#if C_OPENGL
	char *shader_src = nullptr;
#endif
	RenderPal_t pal = {};
	bool updating = false;
	bool active = false;
	bool aspect = true;
	bool fullFrame = true;
};

#if defined(USE_TTF)
#include "SDL_ttf.h"
#define txtMaxCols 255
#define txtMaxLins 88
typedef struct {
	bool	inUse;
	TTF_Font *SDL_font;
	TTF_Font *SDL_fontb;
	TTF_Font *SDL_fonti;
	TTF_Font *SDL_fontbi;
	bool	DOSBox;								// is DOSBox-X internal TTF loaded, pointsizes should be even to look really nice
	int		pointsize;
	int		height;								// height of character cell
	int		width;								// width
	unsigned int		cursor;
	unsigned int		lins;								// number of lines 24-60
	unsigned int		cols;								// number of columns 80-160
	bool	fullScrn;							// in fake fullscreen
	int		offX;								// horizontal offset to center content
	int		offY;								// vertical ,,
} Render_ttf;

struct ttf_cell {
    uint16_t        chr;                        // unicode code point OR just the raw code point. set to ' ' (0x20) for empty space.
    unsigned int    fg:4;                       // foreground color (one of 16)
    unsigned int    bg:4;                       // background color (one of 16)
    unsigned int    doublewide:1;               // double-wide (e.g. PC-98 JIS), therefore skip next character cell.
    unsigned int    blink:1;                    // blink attribute
    unsigned int    boxdraw:1;                  // box-drawing attribute
    unsigned int    underline:1;                // underline attribute
    unsigned int    unicode:1;                  // chr is unicode code point
    unsigned int    skipped:1;                  // adjacent (ignored) cell to a doublewide
    unsigned int    selected:1;

    ttf_cell() {
        chr = 0x20;
        fg = 7;
        bg = 0;
        doublewide = 0;
        blink = 0;
        boxdraw = 0;
        underline = 0;
        unicode = 0;
        skipped = 0;
        selected = 0;
    }

    bool operator==(const ttf_cell &lhs) const {
        return  chr         == lhs.chr &&
                fg          == lhs.fg &&
                bg          == lhs.bg &&
                doublewide  == lhs.doublewide &&
                blink       == lhs.blink &&
                skipped     == lhs.skipped &&
                underline   == lhs.underline &&
                unicode     == lhs.unicode;
    }
    bool operator!=(const ttf_cell &lhs) const {
        return !(*this == lhs);
    }
};
// FIXME: Perhaps the TTF output code should just render unicode code points and vga_draw should do the code page conversion

extern ttf_cell curAttrChar[];					// currently displayed textpage
extern ttf_cell newAttrChar[];					// to be replaced by
extern Render_ttf ttf;
#endif

extern Render_t render;
extern ScalerLineHandler_t RENDER_DrawLine;
void RENDER_SetSize(uint32_t width,
                    uint32_t height,
                    unsigned bpp,
                    double fps,
                    double ratio,
                    bool dblw,
                    bool dblh);
bool RENDER_StartUpdate(void);
void RENDER_EndUpdate(bool abort);
void RENDER_SetPal(uint8_t entry,uint8_t red,uint8_t green,uint8_t blue);

#endif
