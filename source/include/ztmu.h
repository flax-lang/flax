// ztmu.h
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

// terminal manipulation utilities

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#include <vector>
#include <string>
#include <optional>
#include <string_view>


namespace ztmu
{
	struct State
	{
		void clear();

		std::optional<std::string> read();
		std::optional<std::string> readContinuation();

		void setPrompt(const std::string& prompt);
		void setWrappedPrompt(const std::string& prompt);
		void setContinuationPrompt(const std::string& prompt);

		void move_cursor_left(int n);
		void move_cursor_right(int n);

		void move_cursor_up(int n);
		void move_cursor_down(int n);

		void move_cursor_home();
		void move_cursor_end();

		void delete_left(int n);
		void delete_right(int n);


		std::string promptString;
		std::string contPromptString;
		std::string wrappedPromptString;

		size_t cursor = 0;
		size_t byteCursor = 0;

		size_t lineIdx = 0;
		size_t wrappedLineIdx = 0;

		size_t termWidth = 0;
		size_t termHeight = 0;

		std::vector<std::string> lines;
	};
}





namespace ztmu {
namespace detail
{
	struct CursorPosition
	{
		int x = -1;
		int y = -1;
	};

	enum class Key
	{
		NONE = 0,
		ESCAPE,
		UP,
		DOWN,
		LEFT,
		RIGHT,
		HOME,
		END,
		DELETE,
		INSERT,
		PAGEUP,
		PAGEDOWN,
		F0, F1, F2, F3, F4, F5, F6, F7, F8, F9,
		F10, F11, F12, F13, F14, F15, F16, F17,
		F18, F19, F20, F21, F22, F23, F24
	};

	struct ControlSeq
	{
		ControlSeq() { }
		ControlSeq(char c) : lastChar(c) { }

		char lastChar;

		// only support max 8 params for now.
		int numParams = 0;
		int params[8];
	};

	struct KeyEvent
	{
		union {
			char key;
			Key escKey;
			ControlSeq ctrlSeq;
		};

		KeyEvent()
		{
			this->escKey = Key::NONE;
			this->isNormalChar = false;
			this->isControlSeq = false;
		}

		KeyEvent(const ControlSeq& cs)
		{
			this->ctrlSeq = cs;
			this->isNormalChar = false;
			this->isControlSeq = true;
		}

		KeyEvent(Key escKey)
		{
			this->escKey = escKey;
			this->isNormalChar = false;
			this->isControlSeq = false;
		}

		KeyEvent(char c)
		{
			this->key = c;
			this->isNormalChar = true;
			this->isControlSeq = false;
		}

		KeyEvent ctrl()
		{
			auto copy = *this;
			copy.isControlled = true;
			return copy;
		}

		KeyEvent alt()
		{
			auto copy = *this;
			copy.isAlted = true;
			return copy;
		}

		KeyEvent shift()
		{
			auto copy = *this;
			copy.isShifted = true;
			return copy;
		}


		bool isNormalChar = true;
		bool isControlSeq = false;

		bool isAlted = false;
		bool isShifted = false;
		bool isControlled = false;
	};


	static void leaveRawMode();
	static void enterRawMode();

	static int platform_read_one(char* c);
	static int platform_read(char* c, size_t len);
	static void platform_write(const char* s, size_t len);
	static void platform_write(const std::string_view& sv);

	static std::string moveCursorUp(int n);
	static std::string moveCursorDown(int n);
	static std::string moveCursorLeft(int n);
	static std::string moveCursorRight(int n);
	static CursorPosition getCursorPosition();
	static std::string setCursorPosition(const CursorPosition& pos);

	static size_t displayedTextLength(const std::string_view& str);
	static std::pair<KeyEvent, size_t> parseEscapeSequence(const std::string_view& str);





















#if ZTMU_CREATE_IMPL || true

// comes as a pair yo
#include "zpr.h"

#ifndef _WIN32

	#include <unistd.h>
	#include <signal.h>
	#include <termios.h>
	#include <sys/ioctl.h>

	static struct termios original_termios;


	static inline void platform_write(const char* s, size_t len)
	{
		write(STDOUT_FILENO, s, len);
	}

	static inline int platform_read_one(char* c)
	{
		return read(STDIN_FILENO, c, 1);
	}

	static inline int platform_read(char* c, size_t len)
	{
		return read(STDIN_FILENO, c, len);
	}

#else
	#include <io.h>

	static inline void platform_write(const char* s, size_t len)
	{
		_write(STDOUT_FILENO, s, len);
	}

	static inline int platform_read_one(char* c)
	{
		return _read(STDIN_FILENO, c, 1);
	}

	static inline int platform_read(char* c, size_t len)
	{
		return _read(STDIN_FILENO, c, len);
	}

#endif






	static constexpr const char  ESC = '\x1b';
	static constexpr const char* CSI = "\x1b[";

	static bool didRegisterAtexit = false;
	static bool isInRawMode = false;

	static inline void platform_write(const std::string_view& sv)
	{
		platform_write(sv.data(), sv.size());
	}

	static inline std::string moveCursorUp(int n)
	{
		if(n == 0)  return "";
		if(n < 0)   return moveCursorDown(-n);
		else        return zpr::sprint("%s%dA", CSI, n);
	}

	static inline std::string moveCursorDown(int n)
	{
		if(n == 0)  return "";
		if(n < 0)   return moveCursorUp(-n);
		else        return zpr::sprint("%s%dB", CSI, n);
	}

	static inline std::string moveCursorLeft(int n)
	{
		if(n == 0)  return "";
		if(n < 0)   return moveCursorRight(-n);
		else        return zpr::sprint("%s%dD", CSI, n);
	}

	static inline std::string moveCursorRight(int n)
	{
		if(n == 0)  return "";
		if(n < 0)   return moveCursorLeft(-n);
		else        return zpr::sprint("%s%dC", CSI, n);
	}

	static CursorPosition getCursorPosition()
	{
		platform_write(zpr::sprint("%s6n", CSI));

		// ok, time to read it back. format is CSI [ rows ; cols R
		char buf[33] { 0 };
		for(size_t i = 0; i < 32; i++)
		{
			if(read(STDIN_FILENO, buf + i, 1) != 1)
				break;

			if(buf[i] == 'R')
				break;
		}

		// give up.
		if(buf[0] != ESC || buf[1] != '[')
			return CursorPosition();

		CursorPosition ret;
		int gotted = sscanf(&buf[2], "%d;%d", &ret.y, &ret.x);
		if(gotted != 2)
			return CursorPosition();

		return ret;
	}

	static std::string setCursorPosition(const CursorPosition& pos)
	{
		if(pos.x <= 0 || pos.y <= 0)
			return "";

		return zpr::sprint("%s%d;%dH", CSI, pos.y, pos.x);
	}











	static std::pair<KeyEvent, size_t> parseEscapeSequence(const std::string_view& str)
	{
		if(str.size() < 2 || str[0] != ESC)
			return std::make_pair(KeyEvent(), 0);

		auto read_digits = [](const std::string_view& s, int* out) -> size_t {
			int ret = 0;
			size_t cons = 0;
			for(char c : s)
			{
				if(!isdigit(c))
					break;

				cons++;
				ret = (10 * ret) + (c - '0');
			}

			*out = ret;
			return cons;
		};




		if(str[1] == ESC)
		{
			return std::make_pair(KeyEvent(Key::ESCAPE), 2);
		}
		else if(str[1] == 'N' || str[1] == 'O' || str[1] == 'c')
		{
			return std::make_pair(KeyEvent(ControlSeq(str[1])), 2);
		}
		else if(str[1] == 'P' || str[1] == ']' || str[1] == 'X' || str[1] == '^' || str[1] == '_')
		{
			// read till the terminator (ESC\).
			size_t cons = 2;
			while(cons + 1 < str.size() && (str[cons] != ESC || str[cons + 1] != '\\'))
				cons++;

			return std::make_pair(ControlSeq(str[1]), cons);
		}
		else if(str[1] == '[')
		{
			// parse the params.
			ControlSeq csi;

			size_t cons = 2;
			while(csi.numParams < 8)
			{
				int n = 0;
				size_t rd = read_digits(str.substr(cons), &n);
				if(rd > 0)
				{
					csi.params[csi.numParams++] = n;
					cons += rd;

					if(cons < str.size() && str[cons] == ';')
					{
						cons++;
						continue;
					}
					else
					{
						break;
					}
				}
				else
				{
					break;
				}
			}

			csi.lastChar = str[cons++];
			switch(csi.lastChar)
			{
				case '~': {
					auto ke = KeyEvent();
					switch(csi.params[0])
					{
						case 1:  ke = KeyEvent(Key::HOME);      break;
						case 2:  ke = KeyEvent(Key::INSERT);    break;
						case 3:  ke = KeyEvent(Key::DELETE);    break;
						case 4:  ke = KeyEvent(Key::END);       break;
						case 5:  ke = KeyEvent(Key::PAGEUP);    break;
						case 6:  ke = KeyEvent(Key::PAGEDOWN);  break;
						case 7:  ke = KeyEvent(Key::HOME);      break;
						case 8:  ke = KeyEvent(Key::END);       break;
						case 10: ke = KeyEvent(Key::F0);        break;
						case 11: ke = KeyEvent(Key::F1);        break;
						case 12: ke = KeyEvent(Key::F2);        break;
						case 13: ke = KeyEvent(Key::F3);        break;
						case 14: ke = KeyEvent(Key::F4);        break;
						case 15: ke = KeyEvent(Key::F5);        break;
						case 17: ke = KeyEvent(Key::F6);        break;
						case 18: ke = KeyEvent(Key::F7);        break;
						case 19: ke = KeyEvent(Key::F8);        break;
						case 20: ke = KeyEvent(Key::F9);        break;
						case 21: ke = KeyEvent(Key::F10);       break;
						case 23: ke = KeyEvent(Key::F11);       break;
						case 24: ke = KeyEvent(Key::F12);       break;
						case 25: ke = KeyEvent(Key::F13);       break;
						case 26: ke = KeyEvent(Key::F14);       break;
						case 28: ke = KeyEvent(Key::F15);       break;
						case 29: ke = KeyEvent(Key::F16);       break;
						case 31: ke = KeyEvent(Key::F17);       break;
						case 32: ke = KeyEvent(Key::F18);       break;
						case 33: ke = KeyEvent(Key::F19);       break;
						case 34: ke = KeyEvent(Key::F20);       break;
					}

					if(ke.escKey != Key::NONE && csi.numParams == 2)
					{
						int mods = csi.params[1] - 1;
						if(mods & 0x1)  ke = ke.shift();
						if(mods & 0x2)  ke = ke.alt();
						if(mods & 0x4)  ke = ke.ctrl();
						if(mods & 0x8)  ke = ke.alt();  // actually meta
					}

					return std::make_pair(ke, cons);
				}

				case 'A': return std::make_pair(KeyEvent(Key::UP),    cons);
				case 'B': return std::make_pair(KeyEvent(Key::DOWN),  cons);
				case 'C': return std::make_pair(KeyEvent(Key::RIGHT), cons);
				case 'D': return std::make_pair(KeyEvent(Key::LEFT),  cons);
				case 'F': return std::make_pair(KeyEvent(Key::END),   cons);
				case 'H': return std::make_pair(KeyEvent(Key::HOME),  cons);

				case 'P': return std::make_pair(KeyEvent(Key::F1), cons);
				case 'Q': return std::make_pair(KeyEvent(Key::F2), cons);
				case 'R': return std::make_pair(KeyEvent(Key::F3), cons);
				case 'S': return std::make_pair(KeyEvent(Key::F4), cons);
			}



			return std::make_pair(KeyEvent(csi), cons);
		}
		else
		{
			return std::make_pair(KeyEvent(), 0);
		}
	}


	static size_t displayedTextLength(const std::string_view& str)
	{
		auto utf8_len = [](const char* begin, const char* end) -> size_t {
			size_t len = 0;

			while(*begin && begin != end)
			{
				if((*begin & 0xC0) != 0x80)
					len += 1;

				begin++;
			}
			return len;
		};

		auto len = utf8_len(str.begin(), str.end());
		{
			size_t cons = 0;
			std::string_view sv = str;

			size_t i = 0;
			while(i < sv.size())
			{
				if(sv[i] == ESC)
				{
					size_t k = parseEscapeSequence(sv.substr(i)).second;
					i += k;
					cons += k;
				}
				else
				{
					i++;
				}
			}

			return len - cons;
		}
	}

	static void leaveRawMode()
	{
		if(isInRawMode && tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios) != -1)
        	isInRawMode = false;
	}

	static void enterRawMode()
	{
		if(!isatty(STDIN_FILENO))
			return;

		if(!didRegisterAtexit)
		{
			atexit([]() {
				leaveRawMode();
			});

			didRegisterAtexit = true;
		}

		if(tcgetattr(STDIN_FILENO, &original_termios) == -1)
			return;

		// copy the original
		termios raw = original_termios;

		// input modes: no break, no CR to NL, no parity check, no strip char,
		// no start/stop output control.
		raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);

		// output modes - disable post processing
		raw.c_oflag &= ~(OPOST);

		// control modes - set 8 bit chars
		raw.c_cflag |= (CS8);

		// local modes - choing off, canonical off, no extended functions,
		// no signal chars (^Z,^C)
		raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

		// control chars - set return condition: min number of bytes and timer.
		// We want read to return every single byte, without timeout.
		raw.c_cc[VMIN] = 1;
		raw.c_cc[VTIME] = 0;

		/* put terminal in raw mode after flushing */
		if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0)
			return;

		isInRawMode = true;
	}

	static size_t getTerminalWidth()
	{
		#ifdef _WIN32
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
			return csbi.srWindow.Right - csbi.srWindow.Left + 1;
		#else

			#ifdef _MSC_VER
			#else
				#pragma GCC diagnostic push
				#pragma GCC diagnostic ignored "-Wold-style-cast"
			#endif

			struct winsize w;
			ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
			return w.ws_col;

			#ifdef _MSC_VER
			#else
				#pragma GCC diagnostic pop
			#endif

		#endif
	}

	static size_t getTerminalHeight()
	{
		#ifdef _WIN32
			CONSOLE_SCREEN_BUFFER_INFO csbi;
			GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
			return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
		#else

			#ifdef _MSC_VER
			#else
				#pragma GCC diagnostic push
				#pragma GCC diagnostic ignored "-Wold-style-cast"
			#endif

			struct winsize w;
			ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
			return w.ws_row;

			#ifdef _MSC_VER
			#else
				#pragma GCC diagnostic pop
			#endif

		#endif
	}

	static std::string& getCurLine(State* st)
	{
		return st->lines[st->lineIdx];
	}

	static size_t findBeginningOfUTF8CP(const uint8_t* bytes, size_t i)
	{
		if(bytes[i] <= 0x7F)
			return i;

		while(i > 0 && (bytes[i] & 0xC0) == 0x80)
			i -= 1;

		return i;
	}

	static size_t findEndOfUTF8CP(const uint8_t* bytes, size_t i)
	{
		if(bytes[i] <= 0x7F)
			return i;

		if((bytes[i] & 0xE0) == 0xC0)
			return i + 1;

		else if((bytes[i] & 0xF0) == 0xE0)
			return i + 2;

		else if((bytes[i] & 0xF8) == 0xF0)
			return i + 3;

		return i;
	}


	static std::pair<size_t, size_t> calculate_left_codepoints(int n, const std::string_view& sv, size_t cursor, size_t byteCursor)
	{
		for(int i = 0; i < n; i++)
		{
			if(cursor > 0)
			{
				auto x = byteCursor - 1;
				auto l = findBeginningOfUTF8CP(reinterpret_cast<const uint8_t*>(sv.data()), x);
				byteCursor -= (x - l + 1);
				cursor -= 1;
			}
		}

		return { cursor, byteCursor };
	}

	static std::pair<size_t, size_t> calculate_right_codepoints(int n, const std::string_view& sv, size_t cursor, size_t byteCursor)
	{
		for(int i = 0; i < n; i++)
		{
			if(byteCursor < sv.size())
			{
				auto x = byteCursor;
				auto l = findEndOfUTF8CP(reinterpret_cast<const uint8_t*>(sv.data()), x);

				byteCursor += (l - x + 1);
				cursor += 1;
			}
		}

		return { cursor, byteCursor };
	}



	static size_t get_cursor_line_offset(State* st, size_t num, size_t firstPromptLen, size_t subsequentPromptLen, bool* didWrap = 0)
	{
		size_t hcursor = firstPromptLen;
		size_t x = num;

		while(true)
		{
			auto left = st->termWidth - hcursor;
			auto todo = std::min(left, x);
			x -= todo;

			if(x == 0)
			{
				hcursor += todo;
				if(hcursor == st->termWidth)
				{
					hcursor = subsequentPromptLen;
					if(didWrap) *didWrap = true;
				}

				break;
			}

			hcursor = subsequentPromptLen;
		}

		return hcursor;
	}



	static void refresh_line(State* st, std::string* oldLine = 0)
	{
		auto normPL = displayedTextLength(st->promptString);
		auto contPL = displayedTextLength(st->contPromptString);
		auto wrapPL = displayedTextLength(st->wrappedPromptString);

		if(!oldLine)
			oldLine = &getCurLine(st);

		std::string commands;
		auto qcmd = [&commands](const std::string_view& c) {
			commands += c;
		};

		auto flushcmd = [&commands]() {
			platform_write(commands);
			commands.clear();
		};


		auto calc_wrap = [&](size_t len) -> size_t {
			auto width = st->termWidth;

			size_t lines = 1;
			while(len > 0)
			{
				auto left = width - (lines == 1 ? normPL : wrapPL);
				if(left == len)
					lines += 1;

				len -= std::min(left, len);
				if(len == 0)
					break;

				lines += 1;
			}

			return lines;
		};

		auto calc_wli = [&](size_t len) -> size_t {
			size_t done = 0;
			size_t remaining = len;

			size_t lc = 1;
			size_t wli = 0;

			while(true)
			{
				auto left = st->termWidth - (lc == 1 ? normPL : wrapPL);
				auto todo = std::min(left, remaining);

				if(remaining == left)
					wli++;

				remaining -= todo;
				done += todo;

				if(remaining == 0)
					break;

				// if your cursor is beyond this point, then you are on the next line.
				if(st->cursor >= done + 1)
				{
					wli++;
					lc++;
				}
			}

			return wli;
		};

		auto getPromptForLine = [&](size_t wli) -> std::pair<std::string, size_t> {
			if(st->lineIdx == 0)
			{
				if(wli == 0) return { st->promptString, normPL };
				else         return { st->wrappedPromptString, wrapPL };
			}
			else
			{
				if(wli == 0) return { st->contPromptString, contPL };
				else         return { st->wrappedPromptString, wrapPL };
			}
		};


		std::string_view currentLine = getCurLine(st);

		size_t numWrappingLines = calc_wrap(displayedTextLength(currentLine));
		size_t old_NWL = calc_wrap(displayedTextLength(*oldLine));

		auto new_wli = calc_wli(st->cursor);

		// kill the entire line, and move the cursor to the beginning as well.
		// qcmd("\x1b[2K");
		qcmd(moveCursorLeft(9999));


		fprintf(stderr, "nwl: %zu, old_nwl: %zu, new_wli: %zu, old_wli: %zu\n", numWrappingLines, old_NWL, new_wli, st->wrappedLineIdx);

		// what we want to do here is go all the way to the bottom of the string (regardless of cursor position), and clear it.
		// then go up one line and repeat, basically clearing the whole thing. we don't clear lines above the cursor!
		{
			auto nwl = std::max(numWrappingLines, old_NWL);

			if(numWrappingLines > st->wrappedLineIdx)
			{
				// the the current position (just the vcursor):
				auto vcursor = getCursorPosition().y;
				auto tomove = nwl - st->wrappedLineIdx - 1;

				fprintf(stderr, "height: %zu, vc: %zu, tm: %zu\n", st->termHeight, vcursor, tomove);
				if(st->termHeight - vcursor < tomove)
				{
					fprintf(stderr, "need to scroll\n");
					// scroll the screen down -- but move the text *up*
					qcmd(zpr::sprint("%s%dS", CSI, tomove - (st->termHeight - vcursor)));
				}

				qcmd(moveCursorDown(tomove));
			}

			for(size_t i = 1; i < nwl - new_wli; i++)
			{
				qcmd("\x1b[2K");
				qcmd(moveCursorUp(1));
			}
		}


		// move to the leftmost position.
		qcmd(moveCursorLeft(9999));

		{
			// there's stuff on the left of the cursor that we need to print too, since we cleared the entire line.

			bool didWrap = false;
			size_t alrPrinted = 0;

			// TODO: we always use normPL here, but we should choose between normPL and contPL!
			auto hcursor = get_cursor_line_offset(st, st->cursor, normPL, wrapPL, &didWrap);
			fprintf(stderr, "didWrap: %d\n", didWrap);

			if(didWrap)
			{
				// we always use new_wli - 1, because we might be moving left-to-right or right-to-left!
				auto [ pstr, plen ] = getPromptForLine(new_wli - 1);

				auto leftCPs = st->termWidth - plen;
				auto leftByteCursor = calculate_left_codepoints(leftCPs, currentLine, st->cursor, st->byteCursor).second;
				auto leftPortion = currentLine.substr(leftByteCursor, st->byteCursor - leftByteCursor);
				currentLine.remove_prefix(st->byteCursor);

				// before proceeding, move the cursor up one, and to the left.
				qcmd(moveCursorUp(1));
				qcmd(moveCursorLeft(9999));

				qcmd(pstr);
				qcmd(leftPortion);

				// ok, now move the line down, and print the continuation prompt.
				qcmd(moveCursorDown(1));
				qcmd(moveCursorLeft(9999));
				qcmd(st->wrappedPromptString);

				alrPrinted = wrapPL;
			}
			else
			{
				auto [ pstr, plen ] = getPromptForLine(new_wli);

				auto leftCPs = hcursor - plen;
				auto leftByteCursor = calculate_left_codepoints(leftCPs, currentLine, st->cursor, st->byteCursor).second;

				// we need to print leftCPs of stuff *before* the current cursor.
				auto leftPortion = currentLine.substr(leftByteCursor, st->byteCursor - leftByteCursor);
				currentLine.remove_prefix(st->byteCursor);

				auto rem = displayedTextLength(currentLine);
				fprintf(stderr, "hcursor = %zu, remaining = %zu, bc = %zu, lbc = %zu, len = %zu, line = '%.*s', pr = '%s', plen = %zu\n",
					hcursor, rem, st->byteCursor, leftByteCursor, currentLine.length(), static_cast<int>(currentLine.length()),
					currentLine.data(), pstr.c_str(), plen);

				fprintf(stderr, "leftportion = '%s'\n", std::string(leftPortion).c_str());

				qcmd(pstr);
				qcmd(leftPortion);

				alrPrinted = plen + displayedTextLength(leftPortion);
			}

			size_t printedLines = 0;
			size_t remaining = displayedTextLength(currentLine);

			// clear to the right, just in case
			qcmd(zpr::sprint("%s0K", CSI));

			while(remaining > 0)
			{
				// ok, now we can print the rest of the string "normally".
				auto adv_and_cons = [&currentLine](size_t cons) -> std::string_view {
					// return the view from [here, cons) -- in terms of codepoints
					// and advance the line by cons codepoints.

					size_t bytes_to_adv = 0;
					for(size_t i = 0; i < cons; i++)
					{
						bytes_to_adv = 1 + findEndOfUTF8CP(reinterpret_cast<const uint8_t*>(currentLine.data()),
							bytes_to_adv);
					}

					auto ret = currentLine.substr(0, bytes_to_adv);
					currentLine.remove_prefix(bytes_to_adv);

					return ret;
				};

				auto left = st->termWidth - (printedLines == 0 ? alrPrinted : wrapPL);
				auto todo = std::min(left, remaining);

				qcmd(adv_and_cons(todo));
				remaining -= todo;

				if(remaining == 0)
				{
					// clear the rest of the line (cursor to end)
					if(todo != left)
						qcmd(zpr::sprint("%s0K", CSI));

					fprintf(stderr, "broke\n");
					break;
				}

				// if there's more:
				// move the cursor down and left.
				qcmd(moveCursorDown(1));
				qcmd(moveCursorLeft(9999));

				// print the wrap prompt.
				qcmd(st->wrappedPromptString);
				printedLines += 1;
			}

			qcmd(moveCursorLeft(9999));

			// ok time to move up... we know that we padded with spaces all the way to the edge
			// so we can move up by printedLines, and left by width - hcursor
			qcmd(moveCursorUp(static_cast<int>(printedLines)));
			qcmd(moveCursorLeft(9999));
			qcmd(moveCursorRight(hcursor));
			fprintf(stderr, "hcursor = %zu\n", hcursor);
		}

		st->wrappedLineIdx = calc_wli(st->cursor);
		fprintf(stderr, "updated wli: %zu\n\n", st->wrappedLineIdx);
		flushcmd();
	}

	static void cursor_home(State* st)
	{
		st->cursor = 0;
		st->byteCursor = 0;

		refresh_line(st);
	}

	static void cursor_end(State* st)
	{
		st->cursor = displayedTextLength(getCurLine(st));
		st->byteCursor = getCurLine(st).size();

		refresh_line(st);
	}

	static void cursor_left(State* st, int n, bool refresh = true)
	{
		for(int i = 0; i < n; i++)
		{
			if(st->cursor > 0)
			{
				auto x = st->byteCursor - 1;
				auto l = findBeginningOfUTF8CP(reinterpret_cast<const uint8_t*>(getCurLine(st).c_str()), x);
				st->byteCursor -= (x - l + 1);
				st->cursor -= 1;
			}
		}

		if(refresh)
			refresh_line(st);
	}

	static void cursor_right(State* st, int n, bool refresh = true)
	{
		for(int i = 0; i < n; i++)
		{
			if(st->byteCursor < getCurLine(st).size())
			{
				auto x = st->byteCursor;
				auto l = findEndOfUTF8CP(reinterpret_cast<const uint8_t*>(getCurLine(st).c_str()), x);

				st->byteCursor += (l - x + 1);
				st->cursor += 1;
			}
		}

		if(refresh)
			refresh_line(st);
	}

	static void delete_left(State* st)
	{
		if(st->cursor > 0 && getCurLine(st).size() > 0)
		{
			auto x = st->byteCursor - 1;
			auto l = findBeginningOfUTF8CP(reinterpret_cast<const uint8_t*>(getCurLine(st).c_str()), x);

			auto old = getCurLine(st);

			getCurLine(st).erase(l, x - l + 1);
			st->byteCursor -= (x - l + 1);
			st->cursor -= 1;

			refresh_line(st, &old);
		}
	}

	static void delete_right(State* st)
	{
		if(st->byteCursor < getCurLine(st).size())
		{
			auto x = st->byteCursor;
			auto l = findEndOfUTF8CP(reinterpret_cast<const uint8_t*>(getCurLine(st).c_str()), x);

			auto old = getCurLine(st);

			getCurLine(st).erase(x, l - x + 1);

			// both cursors remain unchanged.
			refresh_line(st, &old);
		}
	}

	static void cursor_up(State* st)
	{
		if(st->cursor > 0 && st->wrappedLineIdx > 0)
		{
			// TODO:
			size_t promptL = displayedTextLength(st->promptString);
			size_t wrapL = displayedTextLength(st->wrappedPromptString);

			// first, get the current horz cursor position:
			auto hcursor = get_cursor_line_offset(st, st->cursor, promptL, wrapL);

			// because wrappedLineIdx > 0, we know we're wrapping. this is the number of chars on the current
			// line (of text!), to the left of the cursor.
			auto h_curlength = hcursor - wrapL;

			// this is how many extra chars to go left.
			auto rightmargin = st->termWidth - hcursor;

			// this will stop when we can't go further, so there's no need to limit.
			// note: this doesn't manipulate the console cursor at all -- we only do that in
			// refresh_line, once!
			cursor_left(st, h_curlength + rightmargin);

			/*
				so what we want to do, ideally, is move "left" termWidth number of codepoints. HOWEVER,
			 	if we have such a situation:
				 * > some long str
				 | ing
				   ^

				if the cursor is there and we want to go "up", what ought to happen is that the cursor gets
				moved to the left extreme -- ie. before the "s" in "some". we will make and document an
				assumption here: all the wrapped prompts are the same length, and the only situation where
				you get this problem is when you move from the second line up to the first line. thus, we
				can use the length of the string, without having to do stupid weird math to figure out
				how many chars are on the previous line.
			*/
		}
		else if(st->lineIdx > 0)
		{
			// ok -- we are now into weird strange territory. move up into the previous continuation line...
		}
		else
		{
			// TODO: possibly handle history?
		}
	}

	static void cursor_down(State* st)
	{
		if(st->byteCursor < getCurLine(st).size())
		{
			// works on a similar principle as cursor_up.

			// TODO:
			size_t promptL = 0;
			{

			}

			promptL = displayedTextLength(st->promptString);
			size_t wrapL = displayedTextLength(st->wrappedPromptString);

			auto hcursor = get_cursor_line_offset(st, st->cursor, promptL, wrapL);

			// again, we make a similar assumption -- that only the first and second prompts can differ; the second and
			// subsequent wrapping prompts must have the same length.

			auto rightmargin = st->termWidth - hcursor;

			// in the next line, the prompt will always be a wrapping prompt.
			auto leftmargin = hcursor - wrapL;

			cursor_right(st, rightmargin + leftmargin);
		}
		else
		{
			// TODO: handle moving down into the next "line" list -- during continuations.
			// TODO: possibly handle history?
		}
	}





	// this is ugly!!!
	static State* currentStateForSignal = 0;
	static bool read_line(State* st, int promptMode)
	{
		constexpr char CTRL_A       = '\x01';
		constexpr char CTRL_C       = '\x03';
		constexpr char CTRL_D       = '\x04';
		constexpr char CTRL_E       = '\x05';
		constexpr char BACKSPACE_   = '\x08';
		constexpr char ENTER        = '\x0D';
		constexpr char BACKSPACE    = '\x7F';

		// NOT THREAD SAFE!!
		enterRawMode();
		currentStateForSignal = st;
		st->termWidth = getTerminalWidth();
		st->termHeight = getTerminalHeight();

		st->cursor = 0;
		st->byteCursor = 0;
		st->wrappedLineIdx = 0;
		st->lines.emplace_back("");

		platform_write(promptMode == 0 ? st->promptString : st->contPromptString);

		bool didSetSignalHandler = false;

		// time for some signalling!
		struct sigaction new_sa {
			.sa_flags = SA_RESTART,  // this is important, if not read() will return EINTR (interrupted by signal)
			.sa_handler = [](int sig) {
				if(currentStateForSignal)
				{
					currentStateForSignal->termWidth = getTerminalWidth();
					currentStateForSignal->termHeight = getTerminalHeight();
				}
			}
		};

		struct sigaction old_sa;
		if(sigaction(SIGWINCH, &new_sa, &old_sa) == 0)
			didSetSignalHandler = true;





		bool eof = false;
		while(true)
		{
			char c = 0;
			if(platform_read_one(&c) <= 0)
				break;

			// fprintf(stderr, "[%d]", c);
			switch(c)
			{
				// enter
				case ENTER: {
					goto finish;
				}

				// this is a little complex; control-D is apparently both EOF: when the buffer is empty,
				// and delete-left: when it is not...
				case CTRL_D: {
					// if the buffer is empty, then quit.
					if(getCurLine(st).empty())
					{
						eof = true;
						goto finish;
					}
					else
					{
						delete_left(st);
					}
				} break;

				case CTRL_A: {
					cursor_home(st);
				} break;

				case CTRL_E: {
					cursor_end(st);
				} break;

				case CTRL_C: {
					if(getCurLine(st).empty())
					{
						eof = true;
						goto finish;
					}
					else
					{
						auto old = getCurLine(st);
						getCurLine(st).clear();

						st->cursor = 0;
						st->byteCursor = 0;
						refresh_line(st, &old);
					}
				}

				// backspace
				case BACKSPACE: [[fallthrough]];
				case BACKSPACE_: {
					delete_left(st);
				} break;

				// time for some fun.
				case ESC: {
					// there should be at least two more!
					char seq[2]; platform_read(seq, 2);

					char thing = 0;
					if(seq[0] == '[')
					{
						if(seq[1] >= '0' && seq[1] <= '9')
						{
							int n = 0;

							// ok now time to read until a ~
							char x = seq[1];
							while(x >= '0' && x <= '9')
							{
								n = (10 * n) + (x - '0');
								platform_read_one(&x);
							}

							// we have a modifier -- for now, ignore them.
							if(x == ';')
							{
								// advance it.
								platform_read_one(&x);

								int mods = 0;
								while(x >= '0' && x <= '9')
								{
									mods = (10 * mods) + (x - '0');
									platform_read_one(&x);
								}

								thing = x;
							}
							else
							{
								thing = n;
							}
						}
						else
						{
							thing = seq[1];
						}

						switch(thing)
						{
							case 3: delete_right(st); break;

							case 1: cursor_home(st); break;
							case 7: cursor_home(st); break;

							case 4: cursor_end(st); break;
							case 8: cursor_end(st); break;

							case 'A': cursor_up(st); break;
							case 'B': cursor_down(st); break;
							case 'C': cursor_right(st, 1); break;
							case 'D': cursor_left(st, 1); break;
							case 'H': cursor_home(st); break;
							case 'F': cursor_end(st); break;
						}
					}
					else if(seq[0] == 'O')
					{
						if(seq[1] == 'H')       cursor_home(st);
						else if(seq[1] == 'F')  cursor_end(st);
					}

				} break;

				// default: just append -- if it's not a control char.
				default: {
					auto old = getCurLine(st);

					uint8_t uc = static_cast<uint8_t>(c);
					if(uc >= 0x20 && uc <= 0x7F)
					{
						getCurLine(st).insert(getCurLine(st).begin() + st->byteCursor, c);

						st->cursor += 1;
						st->byteCursor += 1;
					}
					else if(uc >= 0x20)
					{
						std::string cp;
						cp += c;

						// if it's unicode, then try and read even more.
						// see: https://en.wikipedia.org/wiki/UTF-8 for the bitmasks we're using.
						if((uc & 0xE0) == 0xC0)
						{
							// one extra byte
							char c = 0; platform_read_one(&c);
							cp += c;
						}
						else if((uc & 0xF0) == 0xE0)
						{
							// two extra bytes
							char buf[2]; platform_read(buf, 2);
							cp += buf;
						}
						else if((uc & 0xF8) == 0xF0)
						{
							// three extra bytes
							char buf[3]; platform_read(buf, 3);
							cp += buf;
						}

						getCurLine(st).insert(st->byteCursor, cp);

						st->byteCursor += cp.size();
						st->cursor += displayedTextLength(cp);
					}

					refresh_line(st, &old);
				} break;
			}
		}




	finish:
		// restore the signal state, and reset the terminal to normal mode.
		currentStateForSignal = 0;
		if(didSetSignalHandler)
			sigaction(SIGWINCH, &old_sa, nullptr);

		// move the cursor to the end, refresh the line, then leave raw mode -- this makes sure
		// that we leave the cursor in a nice place after the call to this function returns.
		cursor_end(st);
		leaveRawMode();

		platform_write("\n");
		return eof;
	}


#endif

}
}


namespace ztmu
{
	void State::clear()
	{
		// reset the thing.
		this->lines.clear();
		this->lineIdx = 0;
	}

	void State::setPrompt(const std::string& prompt)
	{
		this->promptString = prompt;
	}

	void State::setWrappedPrompt(const std::string& prompt)
	{
		this->wrappedPromptString = prompt;
	}

	void State::setContinuationPrompt(const std::string& prompt)
	{
		this->contPromptString = prompt;
	}

	void State::move_cursor_left(int n)
	{
		if(n < 0)   move_cursor_right(-n);

		detail::cursor_left(this, n);
	}

	void State::move_cursor_right(int n)
	{
		if(n < 0)   move_cursor_left(-n);

		detail::cursor_right(this, n);
	}

	void State::move_cursor_up(int n)
	{
		if(n < 0)   move_cursor_down(-n);

		for(int i = 0; i < n; i++)
			detail::cursor_up(this);
	}

	void State::move_cursor_down(int n)
	{
		if(n < 0)   move_cursor_up(-n);

		for(int i = 0; i < n; i++)
			detail::cursor_down(this);
	}


	void State::move_cursor_home()
	{
		detail::cursor_home(this);
	}

	void State::move_cursor_end()
	{
		detail::cursor_end(this);
	}

	void State::delete_left(int n)
	{
		if(n <= 0) return;

		for(int i = 0; i < n; i++)
			detail::delete_left(this);
	}

	void State::delete_right(int n)
	{
		if(n <= 0) return;

		for(int i = 0; i < n; i++)
			detail::delete_right(this);
	}





	std::optional<std::string> State::read()
	{
		// clear.
		this->clear();
		bool eof = detail::read_line(this, /* promptMode: */ 0);

		if(eof) return std::nullopt;
		else    return this->lines.back();
	}

	std::optional<std::string> State::readContinuation()
	{
		// don't clear.
		this->lineIdx++;

		bool eof = detail::read_line(this, /* promptMode: */ 1);

		if(eof) return std::nullopt;
		else    return this->lines.back();
	}
}


















