/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2023-2024  The DOSBox Staging Team
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

#include "title_bar.h"

#include "checks.h"
#include "control.h"
#include "cpu.h"
#include "dosbox.h"
#include "mapper.h"
#include "sdlmain.h"
#include "setup.h"
#include "string_utils.h"
#include "support.h"
#include "video.h"

#include <SDL.h>
#include <atomic>

CHECK_NARROWING();

// ***************************************************************************
// Datat types and storage
// ***************************************************************************

enum class ProgramDisplay : uint8_t { None, Name, Path, Segment };
enum class VersionDisplay : uint8_t { None, Simple, Detailed };

static struct TitlebarConfig {
	std::string titlebar_text = {};
	bool animated_rec         = false;
	bool show_cycles          = false;
	bool show_dosbox          = false;
	ProgramDisplay program    = ProgramDisplay::None;
	VersionDisplay version    = VersionDisplay::None;
} config = {};

static struct {
	bool audio_capture   = false;
	bool video_capture   = false;
	bool guest_os_booted = false;

	std::string segment_name   = {};
	std::string canonical_name = {}; // path + name + extension
	std::string hint_mouse     = {};

	int num_cycles = 0;

	// Title string variants in case of animated recording mark
	std::string title_frame_1 = {};
	std::string title_frame_2 = {};

	SDL_TimerID timer_id = {};

	std::atomic<bool> is_animation_running      = false;
	std::atomic<bool> animation_phase_alternate = false;
} state = {};

// Pre-calculated strings, to save some time when generating new title bar text
static struct {
	std::string cycles_ms = {};

	std::string tag_paused            = {};
	std::string tag_recording         = {}; // non-animated
	std::string tag_recording_frame_1 = {}; // animated
	std::string tag_recording_frame_2 = {};
} strings = {};

// ***************************************************************************
// String pre-calculation
// ***************************************************************************

static constexpr bool is_alpha_build()
{
	std::string_view version = VERSION;
	return version.find("alpha") != version.npos;
}

static constexpr bool is_debug_build()
{
#if !defined(NDEBUG)
	return true;
#else
	return false;
#endif
}

static void prepare_strings()
{
	if (!strings.cycles_ms.empty()) {
		return; // already prepared
	}

	// The U+23FA (Black Circle For Record) symbol would be more
	// suitable, but with some fonts it is larger than Latin
	// alphabet symbols - this (at least on KDE) leads to unpleasant
	// effect when it suddenly appears on the title bar
	const std::string MediumWhiteCircle = "\xe2\x9a\xaa"; // U+26AA
	const std::string MediumBlackCircle = "\xe2\x9a\xab"; // U+26AB

	const std::string Space     = std::string(" ");
	const std::string BeginTag  = std::string("[");
	const std::string EndTag    = std::string("]") + Space;
	const std::string Recording = std::string("REC");

	auto& cycles_ms             = strings.cycles_ms;
	auto& tag_paused            = strings.tag_paused;
	auto& tag_recording         = strings.tag_recording;
	auto& tag_recording_frame_1 = strings.tag_recording_frame_1;
	auto& tag_recording_frame_2 = strings.tag_recording_frame_2;

	cycles_ms     = Space + MSG_GetRaw("TITLEBAR_CYCLES_MS");
	tag_paused    = BeginTag + MSG_GetRaw("TITLEBAR_PAUSED") + EndTag;
	tag_recording = BeginTag + Recording + EndTag;

	tag_recording_frame_1 = BeginTag + MediumBlackCircle + Recording + EndTag;
	tag_recording_frame_2 = BeginTag + MediumWhiteCircle + Recording + EndTag;
}

// ***************************************************************************
// Title bar rendering
// ***************************************************************************

// Time each animation 'frame' lasts, in milliseconds. Lower = faster blinking.
constexpr uint32_t FrameTimeMs = 750;

static uint32_t animation_tick(uint32_t /* interval */, void* /* name */)
{
	if (!state.is_animation_running) {
		return FrameTimeMs;
	}

	state.animation_phase_alternate = !state.animation_phase_alternate;

	SDL_Event event = {};
	event.user.type = enum_val(SDL_DosBoxEvents::RefreshAnimatedTitle);
	event.user.type += sdl.start_event_id;

	SDL_PushEvent(&event);
	return FrameTimeMs;
}

static void start_animation()
{
	if (!state.is_animation_running) {
		state.animation_phase_alternate = false;
		SDL_AddTimer(FrameTimeMs / 2, animation_tick, nullptr);
		state.is_animation_running = true;
	}
}

static void stop_animation()
{
	if (state.is_animation_running) {
		SDL_RemoveTimer(state.timer_id);
		state.is_animation_running = false;
	}
}

static std::string get_app_name_str()
{
	std::string result = DOSBOX_NAME;
	switch (config.version) {
	case VersionDisplay::Simple:
		result += " ";
		result += VERSION;
		break;
	case VersionDisplay::Detailed:
		result += " ";
		result += DOSBOX_GetDetailedVersion();
		break;
	default: // includes VersionDisplay::None:
		break;
	}

	const bool show_alpha = is_alpha_build() &&
	                        (config.version == VersionDisplay::None);

	if (is_debug_build()) {
		return (show_alpha ? result + " (alpha, debug build)"
		                   : result + " (debug build)");
	} else {
		return (show_alpha ? result + " (alpha)" : result);
	}
}

static std::string get_program_name()
{
	std::string result = {};

	auto strip_path = [](std::string& name) {
		const auto position = name.rfind('\\');
		if (position != std::string::npos) {
			name = name.substr(position + 1);
		}
	};

	switch (config.program) {
	case ProgramDisplay::Name:
		result = state.canonical_name;
		strip_path(result);
		break;
	case ProgramDisplay::Path:
		result = state.canonical_name;
		break;
	case ProgramDisplay::Segment:
		return state.segment_name;
	default:
		return result;
	}

	if (result.empty() && !state.segment_name.empty()) {
		// Most likely due to Windows 3.1x running in enhanced mode
		return state.segment_name;
	}

	return result;
}

static std::string get_dosbox_program_display()
{
	std::string title = {};

	if (config.program == ProgramDisplay::None) {
		title = config.titlebar_text;
	} else {
		title = get_program_name();
	}
	trim(title);

	if (title.empty()) {
		return get_app_name_str();
	} else if (config.show_dosbox) {
		return title + " - " + get_app_name_str();
	}

	return title;
}

static std::string get_cycles_display()
{
	std::string cycles = {};

	if (!config.show_cycles) {
		return cycles;
	}

	if (!CPU_CycleAutoAdjust) {
		cycles = format_str(" - %d", state.num_cycles);
	} else if (CPU_CycleLimit > 0) {
		cycles = format_str(" - max %d%% limit %d",
		                    state.num_cycles,
		                    CPU_CycleLimit);
	} else {
		cycles = format_str(" - max %d%%", state.num_cycles);
	}

	return cycles + strings.cycles_ms;
}

static std::string get_mouse_hint()
{
	return state.hint_mouse;
}

// Also responsible for starting/stopping the animation
static void maybe_add_rec_pause_mark(std::string& title_str)
{
	bool should_stop_animation = state.is_animation_running;

	if (sdl.is_paused) {
		title_str = strings.tag_paused + title_str;
	} else if (state.audio_capture || state.video_capture) {
		if (config.animated_rec) {
			auto& frame_1 = state.title_frame_1;
			auto& frame_2 = state.title_frame_2;
			frame_1 = strings.tag_recording_frame_1 + title_str;
			frame_2 = strings.tag_recording_frame_2 + title_str;
			start_animation();
			should_stop_animation = false;
		} else {
			title_str = strings.tag_recording + title_str;
		}
	}

	if (should_stop_animation) {
		stop_animation();
	}
}

void GFX_RefreshAnimatedTitle()
{
	if (!state.is_animation_running) {
		return;
	}

	if (state.animation_phase_alternate) {
		SDL_SetWindowTitle(sdl.window, state.title_frame_1.c_str());
	} else {
		SDL_SetWindowTitle(sdl.window, state.title_frame_2.c_str());
	}
}

void GFX_RefreshTitle()
{
	prepare_strings();

	std::string title_str = get_dosbox_program_display() +
	                        get_cycles_display() +
	                        get_mouse_hint();
	maybe_add_rec_pause_mark(title_str);

	if (state.is_animation_running) {
		GFX_RefreshAnimatedTitle();
	} else {
		SDL_SetWindowTitle(sdl.window, title_str.c_str());
	}
}

// ***************************************************************************
// External notifications and setter functions
// ***************************************************************************

void GFX_NotifyBooting()
{
	state.guest_os_booted = true;
	GFX_RefreshTitle();
}

void GFX_SetAudioCaptureMark(const bool is_capturing)
{
	if (state.audio_capture != is_capturing) {
		state.audio_capture = is_capturing;
		GFX_RefreshTitle();
	}
}

void GFX_SetVideoCaptureMark(const bool is_capturing)
{
	if (state.video_capture != is_capturing) {
		state.video_capture = is_capturing;
		GFX_RefreshTitle();
	}
}

void GFX_SetProgramName(const std::string& segment_name,
                        const std::string& canonical_name)
{
	std::string segment_name_dos = segment_name;

	// Segment name might contain just about any character - adapt it
	for (size_t i = 0; i < segment_name_dos.size(); ++i) {
		if (!is_extended_printable_ascii(segment_name_dos[i])) {
			segment_name_dos[i] = '?';
		}
	}
	trim(segment_name_dos);

	// Store new names as UTF-8, refresh title bar
	dos_to_utf8(segment_name_dos, state.segment_name);
	dos_to_utf8(canonical_name, state.canonical_name);
	GFX_RefreshTitle();
}

void GFX_SetCycles(const int32_t cycles)
{
	if (cycles >= 0 && state.num_cycles != cycles) {
		state.num_cycles = cycles;
		GFX_RefreshTitle();
	}
}

void GFX_SetMouseHint(const MouseHint hint_id)
{
	static const std::string prexix = " - ";

	auto create_hint_str = [](const char* requested_name) {
		char hint_buffer[200] = {0};

		safe_sprintf(hint_buffer, MSG_GetRaw(requested_name), PRIMARY_MOD_NAME);
		return prexix + hint_buffer;
	};

	std::string hint = {};
	switch (hint_id) {
	case MouseHint::None:
		break;
	case MouseHint::CapturedHotkey:
		hint = create_hint_str("TITLEBAR_HINT_CAPTURED_HOTKEY");
		break;
	case MouseHint::CapturedHotkeyMiddle:
		hint = create_hint_str("TITLEBAR_HINT_CAPTURED_HOTKEY_MIDDLE");
		break;
	case MouseHint::ReleasedHotkey:
		hint = create_hint_str("TITLEBAR_HINT_RELEASED_HOTKEY");
		break;
	case MouseHint::ReleasedHotkeyMiddle:
		hint = create_hint_str("TITLEBAR_HINT_RELEASED_HOTKEY_MIDDLE");
		break;
	case MouseHint::ReleasedHotkeyAnyButton:
		hint = create_hint_str("TITLEBAR_HINT_RELEASED_HOTKEY_ANY_BUTTON");
		break;
	case MouseHint::SeamlessHotkey:
		hint = create_hint_str("TITLEBAR_HINT_SEAMLESS_HOTKEY");
		break;
	case MouseHint::SeamlessHotkeyMiddle:
		hint = create_hint_str("TITLEBAR_HINT_SEAMLESS_HOTKEY_MIDDLE");
		break;
	default: assert(false); break;
	}

	if (state.hint_mouse != hint) {
		state.hint_mouse = std::move(hint);
		GFX_RefreshTitle();
	}
}

// ***************************************************************************
// Lifecycle
// ***************************************************************************

void TITLEBAR_ReadConfig(Section* section)
{
	assert(section);
	const Section_prop* conf = dynamic_cast<Section_prop*>(section);
	assert(conf);
	if (!conf) {
		return;
	}

	bool warned_double_animation = false;
	bool warned_double_cycles    = false;
	bool warned_double_dosbox    = false;
	bool warned_double_program   = false;
	bool warned_double_version   = false;

	auto check_not_set_animation = [&]() {
		if (config.animated_rec && !warned_double_animation) {
			LOG_WARNING("SDL: 'animation' specified more than once in 'titlebar_content'");
			warned_double_animation = true;
		}
	};

	auto check_not_set_cycles = [&]() {
		if (config.show_cycles && !warned_double_cycles) {
			LOG_WARNING("SDL: 'cycles' specified more than once in 'titlebar_content'");
			warned_double_cycles = true;
		}
	};

	auto check_not_set_dosbox = [&]() {
		if (config.show_dosbox && !warned_double_dosbox) {
			LOG_WARNING("SDL: 'dosbox' specified more than once in 'titlebar_content'");
			warned_double_dosbox = true;
		}
	};

	auto check_not_set_program = [&]() {
		if (config.program != ProgramDisplay::None && !warned_double_program) {
			LOG_WARNING("SDL: 'program' specified more than once in 'titlebar_content'");
			warned_double_program = true;
		}
	};

	auto check_not_set_version = [&]() {
		if (config.version != VersionDisplay::None && !warned_double_version) {
			LOG_WARNING("SDL: 'version' specified more than once in 'titlebar_content'");
			warned_double_version = true;
		}
	};

	config = TitlebarConfig();

	const auto titlebar_content = split(conf->Get_string("titlebar_content"));
	for (const auto& setting : titlebar_content) {
		if (iequals(setting, "animation")) {
			check_not_set_animation();
			config.animated_rec = true;
			continue;
		}

		if (iequals(setting, "cycles")) {
			check_not_set_cycles();
			config.show_cycles = true;
			continue;
		}

		if (iequals(setting, "dosbox")) {
			check_not_set_dosbox();
			config.show_dosbox = true;
			continue;
		}

		if (iequals(setting, "program") ||
		    iequals(setting, "program=name")) {
			check_not_set_program();
			config.program = ProgramDisplay::Name;
			continue;
		}

		if (iequals(setting, "program=path")) {
			check_not_set_program();
			config.program = ProgramDisplay::Path;
			continue;
		}

		if (iequals(setting, "program=segment")) {
			check_not_set_program();
			config.program = ProgramDisplay::Segment;
			continue;
		}

		if (iequals(setting, "version") ||
		    iequals(setting, "version=simple")) {
			check_not_set_version();
			config.version = VersionDisplay::Simple;
			continue;
		}

		if (iequals(setting, "version=detailed")) {
			check_not_set_version();
			config.version = VersionDisplay::Detailed;
			continue;
		}

		LOG_WARNING("SDL: Unknown setting '%s' in 'titlebar_content'",
		            setting.c_str());
	}

	config.titlebar_text = conf->Get_string("titlebar_text");
	trim(config.titlebar_text);
}

void TITLEBAR_AddConfig(Section_prop &secprop)
{
	constexpr auto always = Property::Changeable::Always;

	Prop_string* prop_str = nullptr;

	prop_str = secprop.Add_string("titlebar_content", always,
	                              "animation cycles dosbox program=name");
	prop_str->Set_help(
	        "Space separated list of information to be displayed on the window title bar\n"
	        "('animation cycles dosbox program=name' by default). Possible information are:\n"
	        "  animation:          Animates title bar audio/video recording mark. Results\n"
	        "                      might depend on your screen font.\n"
	        "  cycles:             Show CPU cycles setting.\n"
	        "  dosbox:             DOSBox emulator name.\n"
	        "  program[=<value>]:  Display the name of a running program instead of\n"
	        "                      'titlebarbar_text'. The <value> can be one of:\n"
	        "                        name:     Program name, with file extension (default).\n"
	        "                        path:     Name, extension, and full absolute path.\n"
	        "                        segment:  Displays program memory segment name.\n"
	        " 	                  Note: With some software (like Windows 3.1x in enhanced\n"
	        "                      mode) it is impossible to recognize the full program name\n"
	        "                      or path; in such case 'segment' value is used instead.\n"
	        "  version[=<value>]:  Display DOSBox version information after emulator name.\n"
	        "                      The <value> can be one of:\n"
	        "                        simple:    Simple version format (default).\n"
	        "                        detailed:  Include GIT hash, if available.");

	prop_str = secprop.Add_string("titlebar_text", always, "");
	prop_str->Set_help(
	        "Text to display on the custom title bar (unset by default).");
}

void TITLEBAR_AddMessages()
{
	MSG_Add("TITLEBAR_CYCLES_MS", "cycles/ms");
	MSG_Add("TITLEBAR_PAUSED", "PAUSED");

	MSG_Add("TITLEBAR_HINT_CAPTURED_HOTKEY", "mouse captured, %s+F10 to release");
	MSG_Add("TITLEBAR_HINT_CAPTURED_HOTKEY_MIDDLE",
	        "mouse captured, %s+F10 or middle-click to release");
	MSG_Add("TITLEBAR_HINT_RELEASED_HOTKEY", "to capture the mouse press %s+F10");
	MSG_Add("TITLEBAR_HINT_RELEASED_HOTKEY_MIDDLE",
	        "to capture the mouse press %s+F10 or middle-click");
	MSG_Add("TITLEBAR_HINT_RELEASED_HOTKEY_ANY_BUTTON",
	        "to capture the mouse press %s+F10 or click any button");
	MSG_Add("TITLEBAR_HINT_SEAMLESS_HOTKEY", "seamless mouse, %s+F10 to capture");
	MSG_Add("TITLEBAR_HINT_SEAMLESS_HOTKEY_MIDDLE",
	        "seamless mouse, %s+F10 or middle-click to capture");
}
