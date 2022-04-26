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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>
#include <map>

#include "dosbox.h"
#include "logging.h"
#include "sdlmain.h"
#include "render.h"
#include "setup.h"
#include "inout.h"
#include "regs.h"
#include "bios.h"
#include "control.h"
#include "callback.h"
#include "cp437_uni.h"
#include "../ints/int10.h"

int altcp = 0, customcp = 0;

bool GFX_IsFullscreen();

using namespace std;

std::map<int, int> lowboxdrawmap {
    {1, 201}, {2, 187}, {3, 200}, {4, 188}, {5, 186}, {6, 205}, {0xe, 178},
    {0x10, 206}, {0x14, 177}, {0x15, 202}, {0x16, 203}, {0x17, 185}, {0x19, 204}, {0x1a, 176},
};
std::map<int, int> pc98boxdrawmap {
    {0xB4, 0x67}, {0xB9, 0x6e}, {0xBA, 0x46}, {0xBB, 0x56}, {0xBC, 0x5E}, {0xB3, 0x45}, {0xBF, 0x53},
    {0xC0, 0x57}, {0xC1, 0x77}, {0xC2, 0x6f}, {0xC3, 0x5f}, {0xC4, 0x43}, {0xC5, 0x7f}, {0xC8, 0x5A},
    {0xC9, 0x52}, {0xCA, 0x7e}, {0xCB, 0x76}, {0xCC, 0x66}, {0xCD, 0x44}, {0xD9, 0x5B}, {0xDA, 0x4f},
};
uint16_t cpMap_AX[32] = {
	0x0020, 0x00b6, 0x2195, 0x2194, 0x2191, 0x2193, 0x2192, 0x2190, 0x2500, 0x2502, 0x250c, 0x2510, 0x2518, 0x2514, 0x251c, 0x252c,
	0x2524, 0x2534, 0x253c, 0x2550, 0x2551, 0x2554, 0x2557, 0x255d, 0x255a, 0x2560, 0x2566, 0x2563, 0x2569, 0x256c, 0x00ab, 0x00bb
};

#if defined(USE_TTF)
#include "DOSBoxTTF.h"
#include "../gui/sdl_ttf.c"

#define MIN_PTSIZE 9

#ifdef _MSC_VER
# define MIN(a,b) ((a) < (b) ? (a) : (b))
# define MAX(a,b) ((a) > (b) ? (a) : (b))
#else
# define MIN(a,b) std::min(a,b)
# define MAX(a,b) std::max(a,b)
#endif

Render_ttf ttf;
bool char512 = true;
bool showbold = true;
bool showital = true;
bool showline = true;
bool showsout = false;
bool dbcs_sbcs = true;
bool printfont = true;
bool autoboxdraw = true;
bool halfwidthkana = true;
bool ttf_dosv = false;
bool lastmenu = true;
bool initttf = false;
bool copied = false;
bool firstset = true;
bool forceswk = false;
bool wpExtChar = false;
int wpType = 0;
int wpVersion = 0;
int wpBG = -1;
int wpFG = 7;
int lastset = 0;
int lastfontsize = 0;
int switchoutput = -1;

static unsigned long ttfSize = sizeof(DOSBoxTTFbi), ttfSizeb = 0, ttfSizei = 0, ttfSizebi = 0;
static void * ttfFont = DOSBoxTTFbi, * ttfFontb = NULL, * ttfFonti = NULL, * ttfFontbi = NULL;
int eurAscii, NonUserResizeCounter;
bool rtl, gbk, chinasea, switchttf, force_conversion, blinking;
extern uint8_t ccount;
extern uint16_t cpMap[512], cpMap_PC98[256];
uint16_t cpMap_copy[256];
static SDL_Color ttf_fgColor = {0, 0, 0, 0};
static SDL_Color ttf_bgColor = {0, 0, 0, 0};
static SDL_Rect ttf_textRect = {0, 0, 0, 0};
static SDL_Rect ttf_textClip = {0, 0, 0, 0};

ttf_cell curAttrChar[txtMaxLins*txtMaxCols];					// currently displayed textpage
ttf_cell newAttrChar[txtMaxLins*txtMaxCols];					// to be replaced by

typedef struct {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	uint8_t alpha;		// unused
} alt_rgb;
alt_rgb altBGR0[16], altBGR1[16];
int blinkCursor = -1;
static int prev_sline = -1;
static int charSet = 0;
static alt_rgb *rgbColors = (alt_rgb*)render.pal.rgb;
static bool blinkstate = false;
extern bool resetreq, colorChanged;
bool colorChanged = false, justChanged = false, staycolors = false, firstsize = true;

bool GFX_IsFullscreen();
void ttf_reset(void), resetFontSize(), setVGADAC(), OUTPUT_TTF_Select(int fsize), RENDER_Reset(void), KEYBOARD_Clear(), GFX_SwitchFullscreenNoReset(void), set_output(Section *sec, bool should_stretch_pixels);
bool ttfswitch=false, switch_output_from_ttf=false;

int menuwidth_atleast(int width), FileDirExistCP(const char *name), FileDirExistUTF8(std::string &localname, const char *name);
void AdjustIMEFontSize(void), initcodepagefont(void), MSG_Init(void), DOSBox_SetSysMenu(void), resetFontSize(void), RENDER_CallBack( GFX_CallBackFunctions_t function );
bool isDBCSCP(void), InitCodePage(void), CodePageGuestToHostUTF16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/), systemmessagebox(char const * aTitle, char const * aMessage, char const * aDialogType, char const * aIconType, int aDefaultButton);
SDL_Point get_initial_window_position_or_default(int default_val);

void GetMaxWidthHeight(unsigned int *pmaxWidth, unsigned int *pmaxHeight) {
    unsigned int maxWidth = sdl.desktop.full.width;
    unsigned int maxHeight = sdl.desktop.full.height;

    SDL_DisplayMode dm;
    if (SDL_GetDesktopDisplayMode(sdl.display_number?sdl.display_number-1:0,&dm) == 0) {
        maxWidth = dm.w;
        maxHeight = dm.h;
    }
    *pmaxWidth = maxWidth;
    *pmaxHeight = maxHeight;
}

void SetVal(const std::string& secname, const std::string& preval, const std::string& val) {
    Section* sec = control->GetSection(secname);
    if(sec) {
        std::string real_val=preval+"="+val;
        sec->HandleInputline(real_val);
    }
}

void GFX_SetResizeable(bool enable);
SDL_Window * GFX_SetSDLSurfaceWindow(uint16_t width, uint16_t height);

Bitu OUTPUT_TTF_SetSize(int /*width*/, int /*height*/) {
    bool text=CurMode->type==0||CurMode->type==2||CurMode->type==M_TEXT;
    if (text) {
        sdl.clip.x = sdl.clip.y = 0;
        sdl.draw.width = sdl.clip.w = ttf.cols*ttf.width;
        sdl.draw.height = sdl.clip.h = ttf.lins*ttf.height;
        ttf.inUse = true;
    } else
        ttf.inUse = false;
    int bx = 0, by = 0;
    if (sdl.display_number>0) {
        int displays = SDL_GetNumVideoDisplays();
        SDL_Rect bound;
        for( int i = 1; i <= displays; i++ ) {
            bound = SDL_Rect();
            SDL_GetDisplayBounds(i-1, &bound);
            if (i == sdl.display_number) {
                bx = bound.x;
                by = bound.y;
                break;
            }
        }
        SDL_DisplayMode dm;
        if (SDL_GetDesktopDisplayMode(sdl.display_number?sdl.display_number-1:0,&dm) == 0) {
            bx += (dm.w - sdl.draw.width - sdl.clip.x)/2;
            by += (dm.h - sdl.draw.height - sdl.clip.y)/2;
        }
    }
    if (ttf.inUse && ttf.fullScrn) {
        unsigned int maxWidth, maxHeight;
        GetMaxWidthHeight(&maxWidth, &maxHeight);
        //GFX_SetResizeable(false);
        sdl.window = GFX_SetSDLSurfaceWindow(maxWidth, maxHeight);
        sdl.surface = sdl.window?SDL_GetWindowSurface(sdl.window):NULL;
        const auto default_val = SDL_WINDOWPOS_CENTERED_DISPLAY(sdl.display_number);
        const auto pos = get_initial_window_position_or_default(default_val);
        if (pos.x == (int)default_val || pos.y == (int)default_val) {
            if (sdl.display_number==0) SDL_SetWindowPosition(sdl.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            else SDL_SetWindowPosition(sdl.window, bx, by);
        }
    } else {
        //GFX_SetResizeable(false);
        sdl.window = GFX_SetSDLSurfaceWindow(sdl.draw.width + sdl.clip.x, sdl.draw.height + sdl.clip.y);
        sdl.surface = sdl.window?SDL_GetWindowSurface(sdl.window):NULL;
        const auto default_val = SDL_WINDOWPOS_CENTERED_DISPLAY(sdl.display_number);
        const auto pos = get_initial_window_position_or_default(default_val);
        if (firstsize && (pos.x == (int)default_val && pos.y == (int)default_val) && text) {
            firstsize=false;
            if (sdl.display_number==0) SDL_SetWindowPosition(sdl.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            else SDL_SetWindowPosition(sdl.window, bx, by);
        }
    }
	if (!sdl.surface)
		E_Exit("SDL: Failed to create surface");
    void set_transparency(const Section_prop * section);
    set_transparency(static_cast<const Section_prop *>(control->GetSection("sdl")));
	SDL_ShowCursor(!ttf.fullScrn);
	sdl.active = true;

    return GFX_CAN_32 | GFX_SCALING;
}

void setVGADAC() {
    if (ttf.inUse&&IS_VGA_ARCH) {
        std::map<uint8_t,int> imap;
        for (uint8_t i = 0; i < 0x10; i++) {
            IO_ReadB(mem_readw(BIOS_VIDEO_PORT)+6);
            IO_WriteB(VGAREG_ACTL_ADDRESS, i+32);
            imap[i]=IO_ReadB(VGAREG_ACTL_READ_DATA);
            IO_WriteB(VGAREG_DAC_WRITE_ADDRESS, imap[i]);
            IO_WriteB(VGAREG_DAC_DATA, altBGR1[i].red*63/255);
            IO_WriteB(VGAREG_DAC_DATA, altBGR1[i].green*63/255);
            IO_WriteB(VGAREG_DAC_DATA, altBGR1[i].blue*63/255);
        }
    }
}

/* NTS: EGA/VGA etc have at least 16 DOS colors. Check also CGA etc. */
bool setColors(const char *colorArray, int n) {
    if (!colorChanged)
        for (uint8_t i = 0; i < 0x10; i++) {
            altBGR1[i].red=rgbColors[i].red;
            altBGR1[i].green=rgbColors[i].green;
            altBGR1[i].blue=rgbColors[i].blue;
        }
    staycolors = strlen(colorArray) && *colorArray == '+';
    const char* nextRGB = colorArray + (staycolors?1:0);
	uint8_t * altPtr = (uint8_t *)altBGR1;
	int rgbVal[3] = {-1,-1,-1};
	for (int colNo = 0; colNo < (n>-1?1:16); colNo++) {
		if (n>-1) altPtr+=4*n;
		if (sscanf(nextRGB, " ( %d , %d , %d)", &rgbVal[0], &rgbVal[1], &rgbVal[2]) == 3) {	// Decimal: (red,green,blue)
			for (int i = 0; i< 3; i++) {
				if (rgbVal[i] < 0 || rgbVal[i] > 255)
					return false;
				altPtr[i] = rgbVal[i];
			}
			while (*nextRGB != ')')
				nextRGB++;
			nextRGB++;
		} else if (sscanf(nextRGB, " #%6x", &rgbVal[0]) == 1) {							// Hexadecimal
			if (rgbVal[0] < 0)
				return false;
			for (int i = 0; i < 3; i++) {
				altPtr[2-i] = rgbVal[0]&255;
				rgbVal[0] >>= 8;
			}
			nextRGB = strchr(nextRGB, '#') + 7;
		} else
			return false;
		altPtr += 4;
	}
	for (int i = n>-1?n:0; i < (n>-1?n+1:16); i++) {
		altBGR0[i].blue = (altBGR1[i].blue*2 + 128)/4;
		altBGR0[i].green = (altBGR1[i].green*2 + 128)/4;
		altBGR0[i].red = (altBGR1[i].red*2 + 128)/4;
	}
    setVGADAC();
    colorChanged=justChanged=true;
	return true;
}

bool readTTFStyle(unsigned long& size, void*& font, FILE * fh) {
    long pos = ftell(fh);
    if (pos != -1L) {
        size = pos;
        font = malloc((size_t)size);
        if (font && !fseek(fh, 0, SEEK_SET) && fread(font, 1, (size_t)size, fh) == (size_t)size) {
            fclose(fh);
            return true;
        }
    }
    return false;
}

std::string failName="";
bool readTTF(const char *fName, bool bold, bool ital) {
	FILE * ttf_fh = NULL;
	std::string exepath = "";
	char ttfPath[1024];

	strcpy(ttfPath, fName);													// Try to load it from working directory
	strcat(ttfPath, ".ttf");
	ttf_fh = fopen(ttfPath, "rb");
	if (!ttf_fh) {
		strcpy(ttfPath, fName);
		ttf_fh = fopen(ttfPath, "rb");
	}
    if (!ttf_fh) {
        exepath=GetExecutablePath().string();
        if (exepath.size()) {
            strcpy(strrchr(strcpy(ttfPath, exepath.c_str()), CROSS_FILESPLIT)+1, fName);	// Try to load it from where DOSBox-X was started
            strcat(ttfPath, ".ttf");
            ttf_fh = fopen(ttfPath, "rb");
            if (!ttf_fh) {
                strcpy(strrchr(strcpy(ttfPath, exepath.c_str()), CROSS_FILESPLIT)+1, fName);
                ttf_fh = fopen(ttfPath, "rb");
            }
        }
    }
    if (!ttf_fh) {
        std::string config_path;
        Cross::GetPlatformConfigDir(config_path);
        struct stat info;
        if (!stat(config_path.c_str(), &info) && (info.st_mode & S_IFDIR)) {
            strcpy(ttfPath, config_path.c_str());
            strcat(ttfPath, fName);
            strcat(ttfPath, ".ttf");
            ttf_fh = fopen(ttfPath, "rb");
            if (!ttf_fh) {
                strcpy(ttfPath, config_path.c_str());
                strcat(ttfPath, fName);
                ttf_fh = fopen(ttfPath, "rb");
            }
        }
    }
    if (!ttf_fh) {
        std::string basedir = static_cast<Section_prop *>(control->GetSection("printer"))->Get_string("fontpath");
        if (basedir.back()!='\\' && basedir.back()!='/') basedir += CROSS_FILESPLIT;
        strcpy(ttfPath, basedir.c_str());
        strcat(ttfPath, fName);
        strcat(ttfPath, ".ttf");
        ttf_fh = fopen(ttfPath, "rb");
        if (!ttf_fh) {
            strcpy(ttfPath, basedir.c_str());
            strcat(ttfPath, fName);
            ttf_fh = fopen(ttfPath, "rb");
        }
    }
    if (!ttf_fh) {
        char fontdir[300];
#if defined(WIN32)
        strcpy(fontdir, "C:\\WINDOWS\\fonts\\");
        struct stat wstat;
        if(stat(fontdir,&wstat) || !(wstat.st_mode & S_IFDIR)) {
            char dir[MAX_PATH];
            if (GetWindowsDirectory(dir, MAX_PATH)) {
                strcpy(fontdir, dir);
                strcat(fontdir, "\\fonts\\");
            }
        }
#elif defined(LINUX)
        strcpy(fontdir, "/usr/share/fonts/");
#elif defined(MACOSX)
        strcpy(fontdir, "/Library/Fonts/");
#else
        strcpy(fontdir, "/fonts/");
#endif
        strcpy(ttfPath, fontdir);
        strcat(ttfPath, fName);
        strcat(ttfPath, ".ttf");
        ttf_fh = fopen(ttfPath, "rb");
        if (!ttf_fh) {
            strcpy(ttfPath, fontdir);
            strcat(ttfPath, fName);
            ttf_fh = fopen(ttfPath, "rb");
#if defined(LINUX) || defined(MACOSX)
            if (!ttf_fh) {
#if defined(LINUX)
                strcpy(fontdir, "/usr/share/fonts/truetype/");
#else
                strcpy(fontdir, "/System/Library/Fonts/");
#endif
                strcpy(ttfPath, fontdir);
                strcat(ttfPath, fName);
                strcat(ttfPath, ".ttf");
                ttf_fh = fopen(ttfPath, "rb");
                if (!ttf_fh) {
                    strcpy(ttfPath, fontdir);
                    strcat(ttfPath, fName);
                    ttf_fh = fopen(ttfPath, "rb");
                    if (!ttf_fh) {
                        std::string in;
#if defined(LINUX)
                        in = "~/.fonts/";
#else
                        in = "~/Library/Fonts/";
#endif
                        Cross::ResolveHomedir(in);
                        strcpy(ttfPath, in.c_str());
                        strcat(ttfPath, fName);
                        strcat(ttfPath, ".ttf");
                        ttf_fh = fopen(ttfPath, "rb");
                        if (!ttf_fh) {
                            strcpy(ttfPath, in.c_str());
                            strcat(ttfPath, fName);
                            ttf_fh = fopen(ttfPath, "rb");
                        }
                    }
                }
            }
#endif
        }
    }
    if (ttf_fh) {
        if (!fseek(ttf_fh, 0, SEEK_END)) {
            if (bold && ital && readTTFStyle(ttfSizebi, ttfFontbi, ttf_fh))
                return true;
            else if (bold && !ital && readTTFStyle(ttfSizeb, ttfFontb, ttf_fh))
                return true;
            else if (!bold && ital && readTTFStyle(ttfSizei, ttfFonti, ttf_fh))
                return true;
            else if (readTTFStyle(ttfSize, ttfFont, ttf_fh))
                return true;
        }
        fclose(ttf_fh);
    }
    if (!failName.size()||failName.compare(fName)) {
        failName=std::string(fName);
        bool alllower=true;
        for (size_t i=0; i<strlen(fName); i++) {if ((unsigned char)fName[i]>0x7f) alllower=false;break;}
        std::string message=alllower?("Could not load font file: "+std::string(fName)+(strlen(fName)<5||strcasecmp(fName+strlen(fName)-4, ".ttf")?".ttf":"")):"Could not load the specified font file.";
        //systemmessagebox("Warning", message.c_str(), "ok","warning", 1);
    }
	return false;
}

void SetBlinkRate(Section_prop* section) {
    const char * blinkCstr = section->Get_string("blinkc");
    unsigned int num=-1;
    if (!strcasecmp(blinkCstr, "false")||!strcmp(blinkCstr, "-1")) blinkCursor = -1;
    else if (1==sscanf(blinkCstr,"%u",&num)&&num>=0&&num<=7) blinkCursor = num;
    else blinkCursor = 4;
}

void CheckTTFLimit() {
    ttf.lins = MAX(24, MIN(IS_VGA_ARCH?txtMaxLins:60, (int)ttf.lins));
    ttf.cols = MAX(40, MIN(IS_VGA_ARCH?txtMaxCols:160, (int)ttf.cols));
    if (ttf.cols*ttf.lins>16384) {
        if (lastset==1) {
            ttf.lins=16384/ttf.cols;
            SetVal("ttf", "lins", std::to_string(ttf.lins));
        } else if (lastset==2) {
            ttf.cols=16384/ttf.lins;
            SetVal("ttf", "cols", std::to_string(ttf.cols));
        } else {
            ttf.lins = 25;
            ttf.cols = 80;
        }
    }
}

uint16_t cpMap[512] = { // Codepage is standard 437
	0x0020, 0x263a, 0x263b, 0x2665, 0x2666, 0x2663, 0x2660, 0x2219, 0x25d8, 0x25cb, 0x25d9, 0x2642, 0x2640, 0x266a, 0x266b, 0x263c,
	0x25ba, 0x25c4, 0x2195, 0x203c, 0x00b6, 0x00a7, 0x25ac, 0x21a8, 0x2191, 0x2193, 0x2192, 0x2190, 0x221f, 0x2194, 0x25b2, 0x25bc,
	0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f,
	0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f,
	0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f,
	0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f,
	0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f,
	0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x2302,
	0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7, 0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
	0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9, 0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
	0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba, 0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,		// 176 - 223 line/box drawing
	0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f, 0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
	0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b, 0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
	0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4, 0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
	0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248, 0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x0020,

	0x211e, 0x2120, 0x2122, 0x00ae, 0x00a9, 0x00a4, 0x2295, 0x2299, 0x2297, 0x2296, 0x2a38, 0x21d5, 0x21d4, 0x21c4, 0x2196, 0x2198,		// Second bank for WP extended charset
	0x2197, 0x2199, 0x22a4, 0x22a5, 0x22a2, 0x22a3, 0x2213, 0x2243, 0x2260, 0x2262, 0x00b3, 0x00be, 0x0153, 0x0152, 0x05d0, 0x05d1,
	0x0000, 0x2111, 0x211c, 0x2200, 0x2207, 0x29cb, 0x0394, 0x039b, 0x03a0, 0x039e, 0x03a8, 0x05d2, 0x03b3, 0x03b7, 0x03b9, 0x03ba,
	0x03bb, 0x03bd, 0x03c1, 0x03a5, 0x03c9, 0x03be, 0x03b6, 0x02bf, 0x03c5, 0x03c8, 0x03c2, 0x2113, 0x03c7, 0x222e, 0x0178, 0x00cf,
	0x00cb, 0x2227, 0x00c1, 0x0107, 0x0106, 0x01f5, 0x013a, 0x00cd, 0x0139, 0x0144, 0x0143, 0x00d3, 0x0155, 0x0154, 0x015b, 0x015a,
	0x00da, 0x00fd, 0x00dd, 0x017a, 0x0179, 0x00c0, 0x00c8, 0x00cc, 0x00d2, 0x00d9, 0x0151, 0x0150, 0x0171, 0x0170, 0x016f, 0x016e,
	0x010b, 0x010a, 0x0117, 0x0116, 0x0121, 0x0120, 0x0130, 0x017c, 0x017b, 0x00c2, 0x0109, 0x0108, 0x00ca, 0x011d, 0x011c, 0x0125,
	0x0124, 0x00ce, 0x0135, 0x0134, 0x00d4, 0x015d, 0x015c, 0x00db, 0x0175, 0x0174, 0x0177, 0x0176, 0x00e3, 0x00c3, 0x0129, 0x0128,
	0x00f5, 0x00d5, 0x0169, 0x0168, 0x0103, 0x0102, 0x011f, 0x011e, 0x016d, 0x016c, 0x010d, 0x010c, 0x010f, 0x010e, 0x011b, 0x011a,
	0x013e, 0x013d, 0x0148, 0x0147, 0x0159, 0x0158, 0x0161, 0x0160, 0x0165, 0x0164, 0x017e, 0x017d, 0x00f0, 0x0122, 0x0137, 0x0136,
	0x013c, 0x013b, 0x0146, 0x0145, 0x0157, 0x0156, 0x015f, 0x015e, 0x0163, 0x0162, 0x00df, 0x0133, 0x0132, 0x00f8, 0x00d8, 0x2218,
	0x0123, 0x0000, 0x01f4, 0x01e6, 0x2202, 0x0000, 0x0020, 0x2309, 0x2308, 0x230b, 0x230a, 0x23ab, 0x2320, 0x2321, 0x23a9, 0x23a8,
	0x23ac, 0x01e7, 0x2020, 0x2021, 0x201e, 0x222b, 0x222a, 0x2282, 0x2283, 0x2288, 0x2289, 0x2286, 0x2287, 0x220d, 0x2209, 0x2203,
	0x21d1, 0x21d3, 0x21d0, 0x21d2, 0x25a1, 0x2228, 0x22bb, 0x2234, 0x2235, 0x2237, 0x201c, 0x201d, 0x2026, 0x03b8, 0x0101, 0x0113,
	0x0100, 0x0112, 0x012b, 0x012a, 0x014d, 0x014c, 0x016b, 0x016a, 0x0105, 0x0104, 0x0119, 0x0118, 0x012f, 0x012e, 0x0173, 0x0172,
	0x0111, 0x0110, 0x0127, 0x0126, 0x0167, 0x0166, 0x0142, 0x0141, 0x0140, 0x013f, 0x00fe, 0x00de, 0x014a, 0x014b, 0x0149, 0x0000
};

int setTTFCodePage() {
    if (!copied) {
        memcpy(cpMap_copy,cpMap,sizeof(cpMap[0])*256);
        copied=true;
    }
    int cp = dos.loaded_codepage;
    if (cp) {
        LOG_MSG("Loaded system codepage: %d\n", cp);
        char text[2];
        uint16_t uname[4], wcTest[256];
        for (int i = 0; i < 256; i++) {
            text[0]=i;
            text[1]=0;
            uname[0]=0;
            uname[1]=0;
            if (cp == 932 && halfwidthkana) forceswk=true;
            if (cp == 932 || cp == 936 || cp == 949 || cp == 950 || cp == 951) dos.loaded_codepage = 437;
            /*if (CodePageGuestToHostUTF16(uname,text)) {
                wcTest[i] = uname[1]==0?uname[0]:i;
                if (cp == 932 && lowboxdrawmap.find(i)!=lowboxdrawmap.end() && TTF_GlyphIsProvided(ttf.SDL_font, wcTest[i]))
                    cpMap[i] = wcTest[i];
            }*/
            forceswk=false;
            if (cp == 932 || cp == 936 || cp == 949 || cp == 950 || cp == 951) dos.loaded_codepage = cp;
        }
        uint16_t unimap;
        int notMapped = 0;
        for (int y = ((customcp&&dos.loaded_codepage==customcp)||(altcp&&dos.loaded_codepage==altcp)?0:8); y < 16; y++)
            for (int x = 0; x < 16; x++) {
                if (y<8 && (wcTest[y*16+x] == y*16+x || wcTest[y*16+x] == cp437_to_unicode[y*16+x])) unimap = cpMap_copy[y*16+x];
                else unimap = wcTest[y*16+x];
                if (!TTF_GlyphIsProvided(ttf.SDL_font, unimap)) {
                    cpMap[y*16+x] = 0;
                    notMapped++;
                    LOG_MSG("Unmapped character: %3d - %4x", y*16+x, unimap);
                } else
                    cpMap[y*16+x] = unimap;
            }
        if (eurAscii != -1 && TTF_GlyphIsProvided(ttf.SDL_font, 0x20ac))
            cpMap[eurAscii] = 0x20ac;
        //initcodepagefont();
#if defined(WIN32) && !defined(HX_DOS)
        //DOSBox_SetSysMenu();
#endif
        if (cp == 932 && halfwidthkana) resetFontSize();
        //if (cp == 936) mainMenu.get_item("ttf_extcharset").check(gbk).refresh_item(mainMenu);
        //else if (cp == 950 || cp == 951) mainMenu.get_item("ttf_extcharset").check(chinasea).refresh_item(mainMenu);
        //else mainMenu.get_item("ttf_extcharset").check(gbk&&chinasea).refresh_item(mainMenu);
        return notMapped;
    } else
        return -1;
}

void GFX_SelectFontByPoints(int ptsize) {
	bool initCP = true;
	if (ttf.SDL_font != 0) {
		TTF_CloseFont(ttf.SDL_font);
		initCP = false;
	}
	if (ttf.SDL_fontb != 0) TTF_CloseFont(ttf.SDL_fontb);
	if (ttf.SDL_fonti != 0) TTF_CloseFont(ttf.SDL_fonti);
	if (ttf.SDL_fontbi != 0) TTF_CloseFont(ttf.SDL_fontbi);
	SDL_RWops *rwfont = SDL_RWFromConstMem(ttfFont, (int)ttfSize);
	ttf.SDL_font = TTF_OpenFontRW(rwfont, 1, ptsize);
    if (ttfSizeb>0) {
        SDL_RWops *rwfont = SDL_RWFromConstMem(ttfFontb, (int)ttfSizeb);
        ttf.SDL_fontb = TTF_OpenFontRW(rwfont, 1, ptsize);
    } else
        ttf.SDL_fontb = NULL;
    if (ttfSizei>0) {
        SDL_RWops *rwfont = SDL_RWFromConstMem(ttfFonti, (int)ttfSizei);
        ttf.SDL_fonti = TTF_OpenFontRW(rwfont, 1, ptsize);
    } else
        ttf.SDL_fonti = NULL;
    if (ttfSizebi>0) {
        SDL_RWops *rwfont = SDL_RWFromConstMem(ttfFontbi, (int)ttfSizebi);
        ttf.SDL_fontbi = TTF_OpenFontRW(rwfont, 1, ptsize);
    } else
        ttf.SDL_fontbi = NULL;
    ttf.pointsize = ptsize;
	TTF_GlyphMetrics(ttf.SDL_font, 65, NULL, NULL, NULL, NULL, &ttf.width);
	ttf.height = TTF_FontAscent(ttf.SDL_font)-TTF_FontDescent(ttf.SDL_font);
	if (ttf.fullScrn) {
        unsigned int maxWidth, maxHeight;
        GetMaxWidthHeight(&maxWidth, &maxHeight);
		ttf.offX = (maxWidth-ttf.width*ttf.cols)/2;
		ttf.offY = (maxHeight-ttf.height*ttf.lins)/2;
	}
	else
		ttf.offX = ttf.offY = 0;
	if (initCP) setTTFCodePage();
}

void SetOutputSwitch(const char *outputstr) {
        if (!strcasecmp(outputstr, "texture"))
            switchoutput = 6;
        else if (!strcasecmp(outputstr, "texturenb"))
            switchoutput = 7;
        else if (!strcasecmp(outputstr, "texturepp"))
            switchoutput = 8;
        else
#if C_OPENGL
        if (!strcasecmp(outputstr, "openglpp"))
            switchoutput = 5;
        else if (!strcasecmp(outputstr, "openglnb"))
            switchoutput = 4;
        else if (!strcasecmp(outputstr, "opengl")||!strcasecmp(outputstr, "openglnq"))
            switchoutput = 3;
        else
#endif
        if (!strcasecmp(outputstr, "surface"))
            switchoutput = 0;
        else
            switchoutput = -1;
}

void OUTPUT_TTF_Select(int fsize) {
    if (!initttf&&TTF_Init()) {											// Init SDL-TTF
        std::string message = "Could not init SDL-TTF: " + std::string(SDL_GetError());
        //systemmessagebox("Error", message.c_str(), "ok","error", 1);
        sdl.desktop.want_type = SCREEN_SURFACE;
        return;
    }
    int fontSize = 0;
    int winPerc = 0;
    if (fsize==3)
        winPerc = 100;
    else if (fsize>=MIN_PTSIZE)
        fontSize = fsize;
    else {
        Section_prop * ttf_section=static_cast<Section_prop *>(control->GetSection("ttf"));
        const char * fName = ttf_section->Get_string("font");
        const char * fbName = ttf_section->Get_string("fontbold");
        const char * fiName = ttf_section->Get_string("fontital");
        const char * fbiName = ttf_section->Get_string("fontboit");
        LOG_MSG("SDL: TTF activated %s", fName);
        //force_conversion = true;
        int cp = dos.loaded_codepage;
        bool trysgf = false;
        if (!*fName) {
            std::string mtype(static_cast<Section_prop *>(control->GetSection("dosbox"))->Get_string("machine"));
            //if (InitCodePage()&&isDBCSCP()) trysgf = true;
        }
        //force_conversion = false;
        dos.loaded_codepage = cp;
        if (trysgf) failName = "SarasaGothicFixed";
        if ((!*fName&&!trysgf)||!readTTF(*fName?fName:failName.c_str(), false, false)) {
            ttfFont = DOSBoxTTFbi;
            ttfFontb = NULL;
            ttfFonti = NULL;
            ttfFontbi = NULL;
            ttfSize = sizeof(DOSBoxTTFbi);
            ttfSizeb = ttfSizei = ttfSizebi = 0;
            ttf.DOSBox = true;
            std::string message="";
            if (*fbName)
                message="A valid ttf.font setting is required for the ttf.fontbold setting: "+std::string(fbName);
            else if (*fiName)
                message="A valid ttf.font setting is required for the ttf.fontital setting: "+std::string(fiName);
            else if (*fbiName)
                message="A valid ttf.font setting is required for the ttf.fontboit setting: "+std::string(fbiName);
            if (fsize==0&&message.size())
                ;//systemmessagebox("Warning", message.c_str(), "ok","warning", 1);
        } else {
            if (!*fbName||!readTTF(fbName, true, false)) {
                ttfFontb = NULL;
                ttfSizeb = 0;
            }
            if (!*fiName||!readTTF(fiName, false, true)) {
                ttfFonti = NULL;
                ttfSizei = 0;
            }
            if (!*fbiName||!readTTF(fbiName, true, true)) {
                ttfFontbi = NULL;
                ttfSizebi = 0;
            }
        }
        const char * colors = ttf_section->Get_string("colors");
        if (*colors) {
            if (!setColors(colors,-1)) {
                LOG_MSG("Incorrect color scheme: %s", colors);
                //setColors("#000000 #0000aa #00aa00 #00aaaa #aa0000 #aa00aa #aa5500 #aaaaaa #555555 #5555ff #55ff55 #55ffff #ff5555 #ff55ff #ffff55 #ffffff",-1);
            }
        } else if (IS_EGAVGA_ARCH) {
            alt_rgb *rgbcolors = (alt_rgb*)render.pal.rgb;
            std::string str = "";
            char value[30];
            for (int i = 0; i < 16; i++) {
                sprintf(value,"#%02x%02x%02x",rgbcolors[i].red,rgbcolors[i].green,rgbcolors[i].blue);
                str+=std::string(value)+" ";
            }
            if (str.size()) {
                setColors(str.c_str(),-1);
                colorChanged=justChanged=false;
            }
        }
        SetBlinkRate(ttf_section);
        const char *wpstr=ttf_section->Get_string("wp");
        wpType=0;
        wpVersion=0;
        if (strlen(wpstr)>1) {
            if (!strncasecmp(wpstr, "WP", 2)) wpType=1;
            else if (!strncasecmp(wpstr, "WS", 2)) wpType=2;
            else if (!strncasecmp(wpstr, "XY", 2)) wpType=3;
            else if (!strncasecmp(wpstr, "FE", 2)) wpType=4;
            if (strlen(wpstr)>2&&wpstr[2]>='1'&&wpstr[2]<='9') wpVersion=wpstr[2]-'0';
        }
        wpBG = ttf_section->Get_int("wpbg");
        wpFG = ttf_section->Get_int("wpfg");
        if (wpFG<0) wpFG = 7;
        winPerc = ttf_section->Get_int("winperc");
        if (winPerc>100||(fsize==2&&GFX_IsFullscreen())||(fsize!=1&&fsize!=2&&(static_cast<Section_prop *>(control->GetSection("sdl"))->Get_bool("fullscreen")))) winPerc=100;
        else if (winPerc<25) winPerc=25;
        if ((fsize==1||switchttf)&&winPerc==100) {
            winPerc=60;
            if (switchttf&&GFX_IsFullscreen()) GFX_SwitchFullScreen();
        }
        fontSize = ttf_section->Get_int("ptsize");
        char512 = ttf_section->Get_bool("char512");
        showbold = ttf_section->Get_bool("bold");
        showital = ttf_section->Get_bool("italic");
        showline = ttf_section->Get_bool("underline");
        showsout = ttf_section->Get_bool("strikeout");
        printfont = ttf_section->Get_bool("printfont");
        dbcs_sbcs = ttf_section->Get_bool("autodbcs");
        autoboxdraw = ttf_section->Get_bool("autoboxdraw");
        halfwidthkana = ttf_section->Get_bool("halfwidthkana");
        ttf_dosv = ttf_section->Get_bool("dosvfunc");
        SetOutputSwitch(ttf_section->Get_string("outputswitch"));
        rtl = ttf_section->Get_bool("righttoleft");
        ttf.lins = ttf_section->Get_int("lins");
        ttf.cols = ttf_section->Get_int("cols");
        if (fsize&&!IS_EGAVGA_ARCH) ttf.lins = 25;
        if (CurMode->type!=M_TEXT) {
            if (ttf.cols<1) ttf.cols=80;
            if (ttf.lins<1) ttf.lins=25;
            CheckTTFLimit();
        } else if (firstset) {
            bool alter_vmode=false;
            uint16_t c=0, r=0;
            c=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
            r=(uint16_t)(IS_EGAVGA_ARCH?real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS):24)+1;
            if (ttf.lins<1||ttf.cols<1)	{
                if (ttf.cols<1)
                    ttf.cols = c;
                else {
                    ttf.cols = MAX(40, MIN(txtMaxCols, (int)ttf.cols));
                    if (ttf.cols != c) alter_vmode = true;
                }
                if (ttf.lins<1)
                    ttf.lins = r;
                else {
                    ttf.lins = MAX(24, MIN(txtMaxLins, (int)ttf.lins));
                    if (ttf.lins != r) alter_vmode = true;
                }
            } else {
                CheckTTFLimit();
                if (ttf.cols != c || ttf.lins != r) alter_vmode = true;
            }
            if (alter_vmode) {
                for (Bitu i = 0; ModeList_VGA[i].mode <= 7; i++) {								// Set the cols and lins in video mode 2,3,7
                    ModeList_VGA[i].twidth = ttf.cols;
                    ModeList_VGA[i].theight = ttf.lins;
                }
            }
            firstset=false;
        } else {
            uint16_t c=0, r=0;
            c=real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS);
            r=(uint16_t)(IS_EGAVGA_ARCH?real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS):24)+1;
            ttf.cols=c;
            ttf.lins=r;
        }
    }

    if (winPerc == 100)
        ttf.fullScrn = true;
    else
        ttf.fullScrn = false;

    unsigned int maxWidth, maxHeight;
    GetMaxWidthHeight(&maxWidth, &maxHeight);

#if defined(WIN32)
    if (!ttf.fullScrn) {																			// 3D borders
        maxWidth -= GetSystemMetrics(SM_CXBORDER)*2;
        maxHeight -= GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYBORDER)*2;
    }
#endif
    int	curSize = fontSize>=MIN_PTSIZE?fontSize:30;													// no clear idea what would be a good starting value
    int lastGood = -1;
    int trapLoop = 0;

    if (fontSize<MIN_PTSIZE) {
        while (curSize != lastGood) {
            GFX_SelectFontByPoints(curSize);
            if (ttf.cols*ttf.width <= maxWidth && ttf.lins*ttf.height <= maxHeight) {				// if it fits on screen
                lastGood = curSize;
                float coveredPerc = float(100*ttf.cols*ttf.width/maxWidth*ttf.lins*ttf.height/maxHeight);
                if (trapLoop++ > 4 && coveredPerc <= winPerc)										// we can get into a +/-/+/-... loop!
                    break;
                curSize = (int)(curSize*sqrt((float)winPerc/coveredPerc));							// rounding down is ok
                if (curSize < MIN_PTSIZE)															// minimum size = 9
                    curSize = MIN_PTSIZE;
            } else if (--curSize < MIN_PTSIZE)														// silly, but OK, one never can tell..
                E_Exit("Cannot accommodate a window for %dx%d", ttf.lins, ttf.cols);
        }
        if (ttf.DOSBox && curSize > MIN_PTSIZE)														// make it even for DOSBox-X internal font (a bit nicer)
            curSize &= ~1;
    }
    
    GFX_SelectFontByPoints(curSize);
    if (fontSize>=MIN_PTSIZE && 100*ttf.cols*ttf.width/maxWidth*ttf.lins*ttf.height/maxHeight > 100)
        E_Exit("Cannot accommodate a window for %dx%d", ttf.lins, ttf.cols);
    if (ttf.SDL_font && ttf.width) {
        int widthb, widthm, widthx, width0, width1, width9;
        widthb = widthm = widthx = width0 = width1 = width9 = 0;
        TTF_GlyphMetrics(ttf.SDL_font, 'B', NULL, NULL, NULL, NULL, &widthb);
        TTF_GlyphMetrics(ttf.SDL_font, 'M', NULL, NULL, NULL, NULL, &widthm);
        TTF_GlyphMetrics(ttf.SDL_font, 'X', NULL, NULL, NULL, NULL, &widthx);
        TTF_GlyphMetrics(ttf.SDL_font, '0', NULL, NULL, NULL, NULL, &width0);
        if (abs(ttf.width-widthb)>1 || abs(ttf.width-widthm)>1 || abs(ttf.width-widthx)>1 || abs(ttf.width-width0)>1) LOG_MSG("TTF: The loaded font is not monospaced.");
        int cp=dos.loaded_codepage;
        //if (!cp) InitCodePage();
        /*if (isDBCSCP() && dbcs_sbcs) {
            int width = 0, height = 0;
            TTF_GlyphMetrics(ttf.SDL_font, 0x4E00, NULL, NULL, NULL, NULL, &width1);
            TTF_GlyphMetrics(ttf.SDL_font, 0x4E5D, NULL, NULL, NULL, NULL, &width9);
            if (width1 <= ttf.width || width9 <= ttf.width) LOG_MSG("TTF: The loaded font may not support DBCS characters.");
            else if ((ttf.width*2 != width1 || ttf.width*2 != width9) && ttf.width == widthb && ttf.width == widthm && ttf.width == widthx && ttf.width == width0) LOG_MSG("TTF: The loaded font is not monospaced.");
        }*/
        dos.loaded_codepage = cp;
    }
    resetreq=false;
    sdl.desktop.want_type = SCREEN_TTF;
}

void ResetTTFSize(Bitu /*val*/) {
    resetFontSize();
}

void processWP(uint8_t *pcolorBG, uint8_t *pcolorFG) {
    charSet = 0;
    if (!wpType) return;
    uint8_t colorBG = *pcolorBG, colorFG = *pcolorFG;
    int style = 0;
    if (CurMode->mode == 7) {														// Mono (Hercules)
        style = showline && (colorFG&7) == 1 ? TTF_STYLE_UNDERLINE : TTF_STYLE_NORMAL;
        if ((colorFG&0xa) == colorFG && (colorBG&15) == 0)
            colorFG = 8;
        else if (colorFG&7)
            colorFG |= 7;
    }
    else if (wpType == 1) {															// WordPerfect
        if (showital && colorFG == 0xe && (colorBG&15) == (wpBG > -1 ? wpBG : 1)) {
            style = TTF_STYLE_ITALIC;
            colorFG = wpFG;
        }
        else if (showline && (colorFG == 1 || colorFG == wpFG+8) && (colorBG&15) == 7) {
            style = TTF_STYLE_UNDERLINE;
            colorBG = wpBG > -1 ? wpBG : 1;
            colorFG = colorFG == 1 ? wpFG : (wpFG+8);
        }
        else if (showsout && colorFG == 0 && (colorBG&15) == 3) {
            style = TTF_STYLE_STRIKETHROUGH;
            colorBG = wpBG > -1 ? wpBG : 1;
            colorFG = wpFG;
        }
    } else if (wpType == 2) {														// WordStar
        if (colorBG&8) {
            if (showline && colorBG&1)
                style |= TTF_STYLE_UNDERLINE;
            if (showital && colorBG&2)
                style |= TTF_STYLE_ITALIC;
            if (showsout && colorBG&4)
                style |= TTF_STYLE_STRIKETHROUGH;
            if (style)
                colorBG = wpBG > -1 ? wpBG : 0;
        }
    } else if (wpType == 3) {														// XyWrite
        if (showital && (colorFG == 10 || colorFG == 14) && colorBG != 12 && !(!showline && colorBG == 3)) {
            style = TTF_STYLE_ITALIC;
            if (colorBG == 3) {
                style |= TTF_STYLE_UNDERLINE;
                colorBG = wpBG > -1 ? wpBG : 1;
            }
            colorFG = colorFG == 10 ? wpFG : (wpFG+8);
        }
        else if (showline && (colorFG == 3 || colorFG == 0xb)) {
            style = TTF_STYLE_UNDERLINE;
            colorFG = colorFG == 3 ? wpFG : (wpFG+8);
        }
        else if (!showsout || colorBG != 4)
            style = TTF_STYLE_NORMAL;
        if (showsout && colorBG == 4 && colorFG != 12 && colorFG != 13) {
            style |= TTF_STYLE_STRIKETHROUGH;
            colorBG = wpBG > -1 ? wpBG : (wpVersion < 4 ? 0 : 1);
            if (colorFG != wpFG+8) colorFG = wpFG;
        }
    } else if (wpType == 4 && colorFG == wpFG) {									// FastEdit
        if (colorBG == 1 || colorBG == 5 || colorBG == 6 || colorBG == 7 || colorBG == 11 || colorBG == 12 || colorBG == 13 || colorBG == 15)
            colorFG += 8;
        if (colorBG == 2 || colorBG == 5) {
            if (showital) style |= TTF_STYLE_ITALIC;
        } else if (colorBG == 3 || colorBG == 6) {
            if (showline) style |= TTF_STYLE_UNDERLINE;
        } else if (colorBG == 4 || colorBG == 7) {
            if (showsout) style |= TTF_STYLE_STRIKETHROUGH;
        } else if (colorBG == 8 || colorBG == 11) {
            style |= (showital?TTF_STYLE_ITALIC:0) | (showline?TTF_STYLE_UNDERLINE:0);
        } else if (colorBG == 9 || colorBG == 12) {
            style |= (showital?TTF_STYLE_ITALIC:0) | (showsout?TTF_STYLE_STRIKETHROUGH:0);
        } else if (colorBG == 10 || colorBG == 13) {
            style |= (showline?TTF_STYLE_UNDERLINE:0) | (showsout?TTF_STYLE_STRIKETHROUGH:0);
        } else if (colorBG == 14 || colorBG == 15) {
            style |= (showital?TTF_STYLE_ITALIC:0) | (showline?TTF_STYLE_UNDERLINE:0) | (showsout?TTF_STYLE_STRIKETHROUGH:0);
        }
        colorBG = wpBG;
    }
    if (char512 && wpType == 1) {
        if (wpExtChar && (colorFG&8)) {	// WordPerfect high bit = second character set (if 512char active)
            charSet = 1;
            colorFG &= 7;
        }
    }
    if (showbold && (colorFG == wpFG+8 || (wpType == 1 && (wpVersion < 1 || wpVersion > 5 ) && colorFG == 3 && (colorBG&15) == (wpBG > -1 ? wpBG : 1)))) {
        if (ttf.SDL_fontbi != 0 || !(style&TTF_STYLE_ITALIC) || wpType == 4) style |= TTF_STYLE_BOLD;
        if ((ttf.SDL_fontbi != 0 && (style&TTF_STYLE_ITALIC)) || (ttf.SDL_fontb != 0 && !(style&TTF_STYLE_ITALIC)) || wpType == 4) colorFG = wpFG;
    }
    if (style)
        TTF_SetFontStyle(ttf.SDL_font, style);
    else
        TTF_SetFontStyle(ttf.SDL_font, TTF_STYLE_NORMAL);
    *pcolorBG = colorBG;
    *pcolorFG = colorFG;
}

bool hasfocus = true, lastfocus = true;
void GFX_EndTextLines(bool force) {
    if (!force&&CurMode->type!=M_TEXT) return;
    static uint8_t bcount = 0;
	Uint16 unimap[txtMaxCols+1];							// max+1 charaters in a line
	int xmin = ttf.cols;									// keep track of changed area
	int ymin = ttf.lins;
	int xmax = -1;
	int ymax = -1;
	ttf_cell *curAC = curAttrChar;							// pointer to old/current buffer
	ttf_cell *newAC = newAttrChar;							// pointer to new/changed buffer

	if (ttf.fullScrn && (ttf.offX || ttf.offY)) {
        unsigned int maxWidth, maxHeight;
        GetMaxWidthHeight(&maxWidth, &maxHeight);
        SDL_Rect *rect = &sdl.updateRects[0];
        rect->x = 0; rect->y = 0; rect->w = maxWidth; rect->h = ttf.offY;
        SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
        rect->w = ttf.offX; rect->h = maxHeight;
        SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
        rect->x = 0; rect->y = sdl.draw.height + ttf.offY; rect->w = maxWidth; rect->h = maxHeight - sdl.draw.height - ttf.offY;
        SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
        rect->x = sdl.draw.width + ttf.offX; rect->y = 0; rect->w = maxWidth - sdl.draw.width - ttf.offX; rect->h = maxHeight;
        SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
    }

	if (ttf.cursor < ttf.cols*ttf.lins)	// hide/restore (previous) cursor-character if we had one

//		if (cursor_enabled && (vga.draw.cursor.sline > vga.draw.cursor.eline || vga.draw.cursor.sline > 15))
//		if (ttf.cursor != vga.draw.cursor.address>>1 || (vga.draw.cursor.enabled !=  cursor_enabled) || vga.draw.cursor.sline > vga.draw.cursor.eline || vga.draw.cursor.sline > 15)
		if ((ttf.cursor != vga.draw.cursor.address>>1) || vga.draw.cursor.sline > vga.draw.cursor.eline || vga.draw.cursor.sline > 15) {
			curAC[ttf.cursor] = newAC[ttf.cursor];
            curAC[ttf.cursor].chr ^= 0xf0f0;	// force redraw (differs)
        }

	lastfocus = hasfocus;
	hasfocus =
	SDL_GetWindowFlags(sdl.window) & SDL_WINDOW_INPUT_FOCUS ? true : false;
	bool focuschanged = lastfocus != hasfocus, noframe = ttf.fullScrn;
	ttf_textClip.h = ttf.height;
	ttf_textClip.y = 0;
	for (unsigned int y = 0; y < ttf.lins; y++) {
		bool draw = false;
		ttf_textRect.y = ttf.offY+y*ttf.height;
		for (unsigned int x = 0; x < ttf.cols; x++) {
			if (((newAC[x] != curAC[x] || newAC[x].selected != curAC[x].selected || (colorChanged && (justChanged || draw)) || force) && (!newAC[x].skipped || force)) || (!y && focuschanged && noframe)) {
				draw = true;
				xmin = min((int)x, xmin);
				ymin = min((int)y, ymin);
				ymax = y;

				bool dw = false;
				const int x1 = x;
				uint8_t colorBG = newAC[x].bg;
				uint8_t colorFG = newAC[x].fg;
				processWP(&colorBG, &colorFG);
                if (newAC[x].selected) {
                    uint8_t color = colorBG;
                    colorBG = colorFG;
                    colorFG = color;
                }
                bool colornul = staycolors || (IS_VGA_ARCH && (altBGR1[colorBG&15].red > 4 || altBGR1[colorBG&15].green > 4 || altBGR1[colorBG&15].blue > 4 || altBGR1[colorFG&15].red > 4 || altBGR1[colorFG&15].green > 4 || altBGR1[colorFG&15].blue > 4) && rgbColors[colorBG].red < 5 && rgbColors[colorBG].green < 5 && rgbColors[colorBG].blue < 5 && rgbColors[colorFG].red < 5 && rgbColors[colorFG].green <5 && rgbColors[colorFG].blue < 5);
				ttf_textRect.x = ttf.offX+(rtl?(ttf.cols-x-1):x)*ttf.width;
				ttf_bgColor.r = !y&&!hasfocus&&noframe?altBGR0[colorBG&15].red:(colornul||(colorChanged&&!IS_VGA_ARCH)?altBGR1[colorBG&15].red:rgbColors[colorBG].red);
				ttf_bgColor.g = !y&&!hasfocus&&noframe?altBGR0[colorBG&15].green:(colornul||(colorChanged&&!IS_VGA_ARCH)?altBGR1[colorBG&15].green:rgbColors[colorBG].green);
				ttf_bgColor.b = !y&&!hasfocus&&noframe?altBGR0[colorBG&15].blue:(colornul||(colorChanged&&!IS_VGA_ARCH)?altBGR1[colorBG&15].blue:rgbColors[colorBG].blue);
				ttf_fgColor.r = !y&&!hasfocus&&noframe?altBGR0[colorFG&15].red:(colornul||(colorChanged&&!IS_VGA_ARCH)?altBGR1[colorFG&15].red:rgbColors[colorFG].red);
				ttf_fgColor.g = !y&&!hasfocus&&noframe?altBGR0[colorFG&15].green:(colornul||(colorChanged&&!IS_VGA_ARCH)?altBGR1[colorFG&15].green:rgbColors[colorFG].green);
				ttf_fgColor.b = !y&&!hasfocus&&noframe?altBGR0[colorFG&15].blue:(colornul||(colorChanged&&!IS_VGA_ARCH)?altBGR1[colorFG&15].blue:rgbColors[colorFG].blue);

                if (newAC[x].unicode) {
                    dw = newAC[x].doublewide;
                    unimap[x-x1] = newAC[x].chr;
                    curAC[x] = newAC[x];
                    x++;

                    if (dw) {
                        curAC[x] = newAC[x];
                        x++;
                        if (rtl) ttf_textRect.x -= ttf.width;
                    }
                }
                else {
                    uint8_t ascii = newAC[x].chr&255;
                    if(ttf_dosv && ascii == 0x5c)
                        ascii = 0x9d;
                    curAC[x] = newAC[x];
                    if (ascii > 175 && ascii < 179 && dos.loaded_codepage != 864 && dos.loaded_codepage != 874 && dos.loaded_codepage != 3021 && !(dos.loaded_codepage == 932 && halfwidthkana) && (dos.loaded_codepage < 1250 || dos.loaded_codepage > 1258) && !(altcp && dos.loaded_codepage == altcp) && !(customcp && dos.loaded_codepage == customcp)) {	// special: shade characters 176-178 unless PC-98
                        ttf_bgColor.b = (ttf_bgColor.b*(179-ascii) + ttf_fgColor.b*(ascii-175))>>2;
                        ttf_bgColor.g = (ttf_bgColor.g*(179-ascii) + ttf_fgColor.g*(ascii-175))>>2;
                        ttf_bgColor.r = (ttf_bgColor.r*(179-ascii) + ttf_fgColor.r*(ascii-175))>>2;
                        unimap[x-x1] = ' ';							// shaded space
                    } else {
                        unimap[x-x1] = cpMap[ascii+charSet*256];
                    }

                    x++;
                }

                {
                    unimap[x-x1] = 0;
                    xmax = max((int)(x-1), xmax);

                    SDL_Surface* textSurface = TTF_RenderUNICODE_Shaded(ttf.SDL_font, unimap, ttf_fgColor, ttf_bgColor, ttf.width*(dw?2:1));
                    ttf_textClip.w = (x-x1)*ttf.width;
                    SDL_BlitSurface(textSurface, &ttf_textClip, sdl.surface, &ttf_textRect);
                    SDL_FreeSurface(textSurface);
                    x--;
                }
			}
		}
		curAC += ttf.cols;
		newAC += ttf.cols;
	}
    if (!force) justChanged = false;
    // NTS: Additional fix is needed for the cursor in PC-98 mode; also expect further cleanup
	bcount++;
	if (vga.draw.cursor.enabled && vga.draw.cursor.sline <= vga.draw.cursor.eline && vga.draw.cursor.sline <= 16 && blinkCursor) {	// Draw cursor?
		unsigned int newPos = (unsigned int)(vga.draw.cursor.address>>1);
		if (newPos < ttf.cols*ttf.lins) {								// If on screen
			int y = newPos/ttf.cols;
			int x = newPos%ttf.cols;
			vga.draw.cursor.count++;

			if (blinkCursor>-1)
				vga.draw.cursor.blinkon = (vga.draw.cursor.count & 1<<blinkCursor) ? true : false;

			if (ttf.cursor != newPos || vga.draw.cursor.sline != prev_sline || ((blinkstate != vga.draw.cursor.blinkon) && blinkCursor>-1)) {				// If new position or shape changed, force draw
				if (blinkCursor>-1 && blinkstate == vga.draw.cursor.blinkon) {
					vga.draw.cursor.count = 4;
					vga.draw.cursor.blinkon = true;
				}
				prev_sline = vga.draw.cursor.sline;
				xmin = min(x, xmin);
				xmax = max(x, xmax);
				ymin = min(y, ymin);
				ymax = max(y, ymax);
			}
			blinkstate = vga.draw.cursor.blinkon;
			ttf.cursor = newPos;
//			if (x >= xmin && x <= xmax && y >= ymin && y <= ymax  && (GetTickCount()&0x400))	// If overdrawn previously (or new shape)
			if (x >= xmin && x <= xmax && y >= ymin && y <= ymax && !newAttrChar[ttf.cursor].skipped) {							// If overdrawn previously (or new shape)
				uint8_t colorBG = newAttrChar[ttf.cursor].bg;
				uint8_t colorFG = newAttrChar[ttf.cursor].fg;
				processWP(&colorBG, &colorFG);

				/* Don't do this in PC-98 mode, the bright pink cursor in EDIT.COM looks awful and not at all how the cursor is supposed to look --J.C. */
				if (blinking && colorBG&8) {
					colorBG-=8;
					if ((bcount/8)%2)
						colorFG=colorBG;
				}
				bool dw = newAttrChar[ttf.cursor].unicode && newAttrChar[ttf.cursor].doublewide;
				ttf_bgColor.r = colorChanged&&!IS_VGA_ARCH?altBGR1[colorBG&15].red:rgbColors[colorBG].red;
				ttf_bgColor.g = colorChanged&&!IS_VGA_ARCH?altBGR1[colorBG&15].green:rgbColors[colorBG].green;
				ttf_bgColor.b = colorChanged&&!IS_VGA_ARCH?altBGR1[colorBG&15].blue:rgbColors[colorBG].blue;
				ttf_fgColor.r = colorChanged&&!IS_VGA_ARCH?altBGR1[colorFG&15].red:rgbColors[colorFG].red;
				ttf_fgColor.g = colorChanged&&!IS_VGA_ARCH?altBGR1[colorFG&15].green:rgbColors[colorFG].green;
				ttf_fgColor.b = colorChanged&&!IS_VGA_ARCH?altBGR1[colorFG&15].blue:rgbColors[colorFG].blue;
				unimap[0] = newAttrChar[ttf.cursor].unicode?newAttrChar[ttf.cursor].chr:cpMap[newAttrChar[ttf.cursor].chr&255];
                if (dw) {
                    unimap[1] = newAttrChar[ttf.cursor].chr;
                    unimap[2] = 0;
                    xmax = max(x+1, xmax);
                } else
                    unimap[1] = 0;
				// first redraw character
				SDL_Surface* textSurface = TTF_RenderUNICODE_Shaded(ttf.SDL_font, unimap, ttf_fgColor, ttf_bgColor, ttf.width*(dw?2:1));
				ttf_textClip.w = ttf.width*(dw?2:1);
				ttf_textRect.x = ttf.offX+(rtl?(ttf.cols-x-(dw?2:1)):x)*ttf.width;
				ttf_textRect.y = ttf.offY+y*ttf.height;
				SDL_BlitSurface(textSurface, &ttf_textClip, sdl.surface, &ttf_textRect);
				SDL_FreeSurface(textSurface);
				if ((vga.draw.cursor.blinkon || blinkCursor<0)) {
                    // second reverse lower lines
                    textSurface = TTF_RenderUNICODE_Shaded(ttf.SDL_font, unimap, ttf_bgColor, ttf_fgColor, ttf.width*(dw?2:1));
                    ttf_textClip.y = (ttf.height*(vga.draw.cursor.sline>15?15:vga.draw.cursor.sline))>>4;
                    ttf_textClip.h = ttf.height - ttf_textClip.y;								// for now, cursor to bottom
                    ttf_textRect.y = ttf.offY+y*ttf.height + ttf_textClip.y;
                    SDL_BlitSurface(textSurface, &ttf_textClip, sdl.surface, &ttf_textRect);
                    SDL_FreeSurface(textSurface);
				}
			}
		}
	}
	if (xmin <= xmax) {												// if any changes
        SDL_Rect *rect = &sdl.updateRects[0];
        rect->x = ttf.offX+(rtl?(ttf.cols-xmax-1):xmin)*ttf.width; rect->y = ttf.offY+ymin*ttf.height; rect->w = (xmax-xmin+1)*ttf.width; rect->h = (ymax-ymin+1)*ttf.height;
        SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
    }
}

FT_Face GetTTFFace() {
    if (ttf.inUse && ttfFont && ttfSize) {
        TTF_Font *font = TTF_OpenFontRW(SDL_RWFromConstMem(ttfFont, (int)ttfSize), 1, ttf.pointsize);
        return font?font->face:NULL;
    } else
        return NULL;
}

void resetFontSize() {
	if (ttf.inUse) {
		GFX_SelectFontByPoints(ttf.pointsize);
		GFX_SetSize(720+sdl.clip.x, 400+sdl.clip.y, GFX_CAN_32,sdl.draw.scalex,sdl.draw.scaley,sdl.draw.callback,1.0);
        
		wmemset((wchar_t*)curAttrChar, -1, ttf.cols*ttf.lins);
		if (ttf.fullScrn) {																// smaller content area leaves old one behind
			SDL_FillRect(sdl.surface, NULL, 0);
            SDL_Rect *rect = &sdl.updateRects[0];
            rect->x = 0; rect->y = 0; rect->w = 0; rect->h = 0;
            SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
		}
		GFX_EndTextLines(true);
	}
}

void decreaseFontSize() {
	int dec=ttf.DOSBox ? 2 : 1;
	if (ttf.inUse && ttf.pointsize >= MIN_PTSIZE + dec) {
		GFX_SelectFontByPoints(ttf.pointsize - dec);
		GFX_SetSize(720+sdl.clip.x, 400+sdl.clip.y, GFX_CAN_32,sdl.draw.scalex,sdl.draw.scaley,sdl.draw.callback,1.0);
		wmemset((wchar_t*)curAttrChar, -1, ttf.cols*ttf.lins);
		if (ttf.fullScrn) {																// smaller content area leaves old one behind
            ttf.fullScrn = false;
            resetFontSize();
        } else
            GFX_EndTextLines(true);
	}
}

void increaseFontSize() {
	int inc=ttf.DOSBox ? 2 : 1;
	if (ttf.inUse) {																	// increase fontsize
        unsigned int maxWidth, maxHeight;
        GetMaxWidthHeight(&maxWidth, &maxHeight);
#if defined(WIN32)
		if (!ttf.fullScrn) {															// 3D borders
			maxWidth -= GetSystemMetrics(SM_CXBORDER)*2;
			maxHeight -= GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYBORDER)*2;
		}
#endif
		GFX_SelectFontByPoints(ttf.pointsize + inc);
		if (ttf.cols*ttf.width <= maxWidth && ttf.lins*ttf.height <= maxHeight) {		// if it fits on screen
			GFX_SetSize(720+sdl.clip.x, 400+sdl.clip.y, GFX_CAN_32,sdl.draw.scalex,sdl.draw.scaley,sdl.draw.callback,1.0);
			wmemset((wchar_t*)curAttrChar, -1, ttf.cols*ttf.lins);						// force redraw of complete window
			GFX_EndTextLines(true);
		} else
			GFX_SelectFontByPoints(ttf.pointsize - inc);
	}
}

void TTF_IncreaseSize(bool pressed) {
    if (!pressed||ttf.fullScrn) return;
    increaseFontSize();
    return;
}

void TTF_DecreaseSize(bool pressed) {
    if (!pressed||ttf.fullScrn) return;
    decreaseFontSize();
    return;
}

bool setVGAColor(const char *colorArray, int i);
void ttf_reset_colors() {
    if (ttf.inUse) {
        SetVal("ttf", "colors", "");
        setColors("#000000 #0000aa #00aa00 #00aaaa #aa0000 #aa00aa #aa5500 #aaaaaa #555555 #5555ff #55ff55 #55ffff #ff5555 #ff55ff #ffff55 #ffffff",-1);
    } else {
        char value[128];
        for (int i=0; i<16; i++) {
            strcpy(value,i==0?"#000000":i==1?"#0000aa":i==2?"#00aa00":i==3?"#00aaaa":i==4?"#aa0000":i==5?"#aa00aa":i==6?"#aa5500":i==7?"#aaaaaa":i==8?"#555555":i==9?"#5555ff":i==10?"#55ff55":i==11?"#55ffff":i==12?"#ff5555":i==13?"#ff55ff":i==14?"#ffff55":"#ffffff");
            //setVGAColor(value, i);
        }
    }
}

void ResetColors_mapper_shortcut(bool pressed) {
    if (!pressed||(!ttf.inUse&&!IS_VGA_ARCH)) return;
    ttf_reset_colors();
}

void ttf_reset() {
    OUTPUT_TTF_Select(2);
    resetFontSize();
}

void ttf_setlines(int cols, int lins) {
    if (cols>0) SetVal("ttf", "cols", std::to_string(cols));
    if (lins>0) SetVal("ttf", "lins", std::to_string(lins));
    firstset=true;
    ttf_reset();
    real_writeb(BIOSMEM_SEG,BIOSMEM_NB_COLS,ttf.cols);
    if (IS_EGAVGA_ARCH) real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,ttf.lins-1);
    vga.draw.address_add = ttf.cols * 2;
}

void ttf_switch_on(bool ss=true) {
    if ((ss&&ttfswitch)||(!ss&&switch_output_from_ttf)) {
        uint16_t oldax=reg_ax;
        reg_ax=0x1600;
        CALLBACK_RunRealInt(0x2F);
        if (reg_al!=0&&reg_al!=0x80) {reg_ax=oldax;return;}
        reg_ax=oldax;
        SetVal("sdl", "output", "ttf");
        GFX_RegenerateWindow(control->GetSection("sdl"));
        if (ss) ttfswitch = false;
        else switch_output_from_ttf = false;
        if (ttf.fullScrn) {
            if (!GFX_IsFullscreen()) GFX_SwitchFullscreenNoReset();
            OUTPUT_TTF_Select(3);
            resetreq = true;
        }
        resetFontSize();
    }
}

void ttf_switch_off(bool ss=true) {
    if (!ss&&ttfswitch)
        ttf_switch_on();
    if (ttf.inUse) {
        std::string output="surface";
        if (switchoutput==0)
            output = "surface";
#if C_OPENGL
        else if (switchoutput==3)
            output = "opengl";
        else if (switchoutput==4)
            output = "openglnb";
        else if (switchoutput==5)
            output = "openglpp";
#endif
        else if (switchoutput==6)
            output = "texture";
        else if (switchoutput==7)
            output = "texturenb";
        else if (switchoutput==8)
            output = "texturepp";
        else {
#if C_OPENGL
            output = "opengl";
#else
            output = "texture";
#endif
        }
        KEYBOARD_Clear();
        SetVal("sdl", "output", output);
        GFX_RegenerateWindow(control->GetSection("sdl"));
        if (ss) ttfswitch = true;
        else switch_output_from_ttf = true;
        if (GFX_IsFullscreen()) GFX_SwitchFullscreenNoReset();
        RENDER_Reset();
    }
}
#endif
