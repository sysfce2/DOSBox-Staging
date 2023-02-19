/*
 *  Copyright (C) 2019-2023  The DOSBox Staging Team
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

#ifndef DOSBOX_DOSBOX_H
#define DOSBOX_DOSBOX_H

#include "config.h"
#include "compiler.h"
#include "types.h"

#include <memory>

int sdl_main(int argc, char *argv[]);

// The shutdown_requested bool is a conditional break in the parse-loop and
// machine-loop. Set it to true to gracefully quit in expected circumstances.
extern bool shutdown_requested;

// The E_Exit function throws an exception to quit. Call it in unexpected
// circumstances.
[[noreturn]] void E_Exit(const char *message, ...)
        GCC_ATTRIBUTE(__format__(__printf__, 1, 2));

void MSG_Add(const char*,const char*); // add messages (in UTF-8) to the language file
const char* MSG_Get(char const *);     // get messages (adapted to current code page) from the language file
const char* MSG_GetRaw(char const *);  // get messages (in UTF-8, without ANSI preprocessing) from the language file
bool MSG_Exists(const char*);

class Section;

typedef Bitu (LoopHandler)(void);

const char *DOSBOX_GetDetailedVersion() noexcept;
double DOSBOX_GetUptime();

void DOSBOX_RunMachine();
void DOSBOX_SetLoop(LoopHandler * handler);
void DOSBOX_SetNormalLoop();

void DOSBOX_Init(void);

class Config;
using config_ptr_t = std::unique_ptr<Config>;
extern config_ptr_t control;

enum SVGACards {
	SVGA_None,
	SVGA_S3,
	SVGA_TsengET4K,
	SVGA_TsengET3K,
	SVGA_ParadisePVGA1A
}; 

enum S3Card {
        S3_Generic,   // Generic emulation, minimal set

        S3_86C928,    // 86C928    [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/S3%20Graphics%2c%20Ltd/S3%2086C928%20GUI%20Accelerator%20%281992%2d09%29%2epdf]
        S3_Vision864, // Vision864 [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/S3%20Graphics%2c%20Ltd/S3%20Vision864%20Graphics%20Accelerator%20%281994%2d10%29%2epdf]
        S3_Vision868, // Vision868 [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/S3%20Graphics%2c%20Ltd/S3%20Vision868%20Multimedia%20Accelerator%20%281995%2d04%29%2epdf]
        S3_Vision964, // Vision964 [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/S3%20Graphics%2c%20Ltd/S3%20Vision964%20Graphics%20Accelerator%20%281995%2d01%29%2epdf]
        S3_Vision968, // Vision968 [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/S3%20Graphics%2c%20Ltd/S3%20Vision968%20Multimedia%20Accelerator%20%281995%2d04%29%2epdf]
        S3_Trio32,    // Trio32    [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/S3%20Graphics%2c%20Ltd/S3%20Trio32%e2%88%95Trio64%20Integrated%20Graphics%20Accelerators%20%281995%2d03%29%2epdf]
        S3_Trio64,    // Trio64    [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/S3%20Graphics%2c%20Ltd/S3%20Trio32%e2%88%95Trio64%20Integrated%20Graphics%20Accelerators%20%281995%2d03%29%2epdf]
        S3_Trio64V,   // Trio64V+  [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/S3%20Graphics%2c%20Ltd/S3%20Trio64V%2b%20Integrated%20Graphics%20and%20Video%20Accelerator%20%281995%2d07%29%2epdf]

        // all cards beyond this point have S3D acceleration

        S3_ViRGE,     // ViRGE     [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/S3%20Graphics%2c%20Ltd/S3%20ViRGE%20Integrated%203D%20Accelerator%20%281996%2d08%29%2epdf]
        S3_ViRGEVX    // ViRGE VX  [http://hackipedia.org/browse.cgi/Computer/Platform/PC%2c%20IBM%20compatible/Video/VGA/SVGA/S3%20Graphics%2c%20Ltd/S3%20ViRGE%E2%88%95VX%20Integrated%203D%20Accelerator%20(1996-06).pdf]
};

extern SVGACards svgaCard;
extern S3Card    s3Card;

extern bool mono_cga;

enum MachineType {
	// In age-order: Hercules is the oldest and VGA is the newest
	MCH_HERC  = 1 << 0,
	MCH_CGA   = 1 << 1,
	MCH_TANDY = 1 << 2,
	MCH_PCJR  = 1 << 3,
	MCH_EGA   = 1 << 4,
	MCH_VGA   = 1 << 5,
};

extern MachineType machine;

inline bool is_machine(const int type) {
	return machine & type;
}
#define IS_TANDY_ARCH ((machine==MCH_TANDY) || (machine==MCH_PCJR))
#define IS_EGAVGA_ARCH ((machine==MCH_EGA) || (machine==MCH_VGA))
#define IS_VGA_ARCH (machine==MCH_VGA)
#define TANDY_ARCH_CASE MCH_TANDY: case MCH_PCJR
#define EGAVGA_ARCH_CASE MCH_EGA: case MCH_VGA
#define VGA_ARCH_CASE MCH_VGA

#ifndef DOSBOX_LOGGING_H
#include "logging.h"
#endif // the logging system.

#endif /* DOSBOX_DOSBOX_H */
