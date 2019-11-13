// ztmu.h
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

// terminal manipulation utilities

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#include <map>
#include <vector>
#include <string>
#include <optional>
#include <string_view>


namespace ztmu
{
	enum class Key
	{
		// start above 0xFF, so anything below we can just use as normal chars.
		ESCAPE = 0x100,
		ENTER,
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

	enum class HandlerAction
	{
		CONTINUE,   // do nothing, and continue reading the line.
		RETURN,     // as if enter was pressed -- finish the line and give it back.
		QUIT        // abandon ship -- return EOF.
	};

	struct State
	{
		void clear();

		std::optional<std::string> read();
		std::optional<std::vector<std::string>> readContinuation(const std::string& seed = "");

		void setPrompt(const std::string& prompt);
		void setContPrompt(const std::string& prompt);
		void setWrappedPrompt(const std::string& prompt);

		void useUniqueHistory(bool enable);
		void addPreviousInputToHistory();
		void loadHistory(const std::vector<std::vector<std::string>>& h);
		std::vector<std::vector<std::string>> getHistory();

		void move_cursor_left(int n);
		void move_cursor_right(int n);

		void move_cursor_up(int n);
		void move_cursor_down(int n);

		void move_cursor_home();
		void move_cursor_end();

		void delete_left(int n);
		void delete_right(int n);

		// the handler will return true to continue reading, false to stop and return all input.
		void setKeyHandler(Key k, std::function<HandlerAction (State*, Key)> handler);
		void unsetKeyHandler(Key k);

		std::string getCurrentLine();
		void setCurrentLine(const std::string& s);

		void setMessageOnControlC(const std::string& msg);
		void enableExitOnEmptyControlC();

		std::string promptString;
		std::string contPromptString;
		std::string wrappedPromptString;

		size_t normPL = 0;
		size_t contPL = 0;
		size_t wrapPL = 0;

		size_t cursor = 0;
		size_t byteCursor = 0;

		size_t lineIdx = 0;
		size_t wrappedLineIdx = 0;
		size_t cachedNWLForCurrentLine = 0;

		size_t termWidth = 0;
		size_t termHeight = 0;

		std::vector<std::string> lines;
		std::map<Key, std::function<HandlerAction (State*, Key)>> keyHandlers;

		std::vector<std::vector<std::string>> history;
		std::vector<std::string> savedCurrentInput;
		bool uniqueHistory = true;
		size_t historyIdx = 0;

		bool wasAborted = false;
		bool emptyCtrlCExits = false;
		std::string uselessControlCMsg;
	};

	size_t getTerminalWidth();
	size_t displayedTextLength(const std::string_view& str);
}





namespace ztmu {
namespace detail
{
	struct CursorPosition
	{
		int x = -1;
		int y = -1;
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

	size_t getTerminalWidth();
	size_t displayedTextLength(const std::string_view& str);
	bool read_line(State* st, int promptMode, std::string seed);
	void cursor_left(State* st, bool refresh = true);
	void cursor_right(State* st, bool refresh = true);
	void refresh_line(State* st, std::string* oldLine = 0);
	void cursor_up(State* st);
	void cursor_down(State* st);
	void cursor_home(State* st);
	void cursor_end(State* st, bool refresh = true);
	void delete_left(State* st);
	void delete_right(State* st);
	size_t convertCursorToByteCursor(const char* bytes, size_t cursor);
















#if ZTMU_CREATE_IMPL // || true

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


	#if 0
		template <typename... Args>
		void ztmu_dbg(const char* fmt, Args&&... args)
		{
			if(!isatty(STDERR_FILENO))
				fprintf(stderr, "%s", zpr::sprint(fmt, args...).c_str());
		}
	#else
		template <typename... Args>
		void ztmu_dbg(const char* fmt, Args&&... args) { }
	#endif






	static constexpr const char  ESC = '\x1b';
	static constexpr const char* CSI = "\x1b[";

	static bool didRegisterAtexit = false;
	static bool isInRawMode = false;

	static inline void platform_write(const std::string_view& sv)
	{
		platform_write(sv.data(), sv.size());
	}

	static inline std::string moveCursorUp(int n);
	static inline std::string moveCursorDown(int n);
	static inline std::string moveCursorLeft(int n);
	static inline std::string moveCursorRight(int n);

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

	// static std::string setCursorPosition(const CursorPosition& pos)
	// {
	// 	if(pos.x <= 0 || pos.y <= 0)
	// 		return "";

	// 	return zpr::sprint("%s%d;%dH", CSI, pos.y, pos.x);
	// }











	static size_t parseEscapeSequence(const std::string_view& str)
	{
		if(str.size() < 2 || str[0] != ESC)
			return 0;

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
			return 2;
		}
		else if(str[1] == 'N' || str[1] == 'O' || str[1] == 'c')
		{
			return 2;
		}
		else if(str[1] == 'P' || str[1] == ']' || str[1] == 'X' || str[1] == '^' || str[1] == '_')
		{
			// read till the terminator (ESC\).
			size_t cons = 2;
			while(cons + 1 < str.size() && (str[cons] != ESC || str[cons + 1] != '\\'))
				cons++;

			return cons;
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
			return cons;
		}
		else
		{
			return 0;
		}
	}


	inline size_t displayedTextLength(const std::string_view& str)
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
					size_t k = parseEscapeSequence(sv.substr(i));
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

        // disable bracketed paste:
        platform_write("\x1b[?2004l");
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

		// local modes - echoing off, canonical off, no extended functions,
		// no signal chars (^Z,^C)
		raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);

		// control chars - set return condition: min number of bytes and timer.
		// We want read to return every single byte, without timeout.
		raw.c_cc[VMIN] = 1;
		raw.c_cc[VTIME] = 0;

		/* put terminal in raw mode after flushing */
		if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0)
			return;

		// enable bracketed paste:
		platform_write("\x1b[?2004h");
		isInRawMode = true;
	}

	inline size_t getTerminalWidth()
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

	inline size_t convertCursorToByteCursor(const char* bytes, size_t cursor)
	{
		size_t bc = 0;
		for(size_t i = 0; i < cursor; i++)
			bc = 1 + findEndOfUTF8CP(reinterpret_cast<const uint8_t*>(bytes), bc);

		return bc;
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

	// static std::pair<size_t, size_t> calculate_right_codepoints(int n, const std::string_view& sv, size_t cursor, size_t byteCursor)
	// {
	// 	for(int i = 0; i < n; i++)
	// 	{
	// 		if(byteCursor < sv.size())
	// 		{
	// 			auto x = byteCursor;
	// 			auto l = findEndOfUTF8CP(reinterpret_cast<const uint8_t*>(sv.data()), x);

	// 			byteCursor += (l - x + 1);
	// 			cursor += 1;
	// 		}
	// 	}

	// 	return { cursor, byteCursor };
	// }



	static size_t get_cursor_line_offset(size_t termWidth, size_t num, size_t firstPromptLen, size_t subsequentPromptLen, bool* didWrap = 0)
	{
		size_t hcursor = firstPromptLen;
		size_t x = num;

		while(true)
		{
			auto left = termWidth - hcursor;
			auto todo = std::min(left, x);
			x -= todo;

			if(x == 0)
			{
				hcursor += todo;
				if(hcursor == termWidth)
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

	static size_t calculate_wli(State* st, size_t len)
	{
		size_t done = 0;
		size_t remaining = len;

		size_t lc = 1;
		size_t wli = 0;

		while(true)
		{
			auto left = st->termWidth - (lc == 1 ? (st->lineIdx == 0 ? st->normPL : st->contPL) : st->wrapPL);
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
	}

	static size_t calculate_nwl(State* st, size_t lineIdx, size_t len)
	{
		auto width = st->termWidth;

		size_t lines = 1;
		while(len > 0)
		{
			auto left = width - (lines == 1 ? (lineIdx == 0 ? st->normPL : st->contPL) : st->wrapPL);
			if(left == len)
				lines += 1;

			len -= std::min(left, len);
			if(len == 0)
				break;

			lines += 1;
		}

		return lines;
	}


	// returns (linesBelowCursor, finalHcursor)
	static std::pair<size_t, size_t> _refresh_line(State* st, size_t lineIdx, bool skip_cursor, const std::string& oldLine,
		bool defer_flush, std::string* commandBuffer)
	{
		std::string commands;
		auto qcmd = [&commands](const std::string_view& c) {
			commands += c;
		};

		auto flushcmd = [&commands]() {
			platform_write(commands);
			commands.clear();
		};


		auto getPromptForLine = [&](size_t wli) -> std::pair<std::string, size_t> {
			if(lineIdx == 0)
			{
				if(wli == 0) return { st->promptString, st->normPL };
				else         return { st->wrappedPromptString, st->wrapPL };
			}
			else
			{
				if(wli == 0) return { st->contPromptString, st->contPL };
				else         return { st->wrappedPromptString, st->wrapPL };
			}
		};


		std::string_view currentLine = st->lines[lineIdx];

		size_t numWrappingLines = calculate_nwl(st, st->lineIdx, displayedTextLength(currentLine));

		// if we're operating on the current line, shun bian cache the NWL count.
		// we'll use this for cursor_down among other things.
		if(st->lineIdx == lineIdx)
			st->cachedNWLForCurrentLine = numWrappingLines;

		size_t old_NWL = calculate_nwl(st, st->lineIdx, displayedTextLength(oldLine));

		auto new_wli = calculate_wli(st, st->cursor);

		qcmd(moveCursorLeft(9999));

		ztmu_dbg("nwl: %zu, old_nwl: %zu, new_wli: %zu, old_wli: %zu\n", numWrappingLines, old_NWL, new_wli, st->wrappedLineIdx);

		// what we want to do here is go all the way to the bottom of the string (regardless of cursor position), and clear it.
		// then go up one line and repeat, basically clearing the whole thing. we don't clear lines above the cursor!
		{
			auto nwl = std::max(numWrappingLines, old_NWL);

			if(numWrappingLines > st->wrappedLineIdx)
			{
				// the the current position (just the vcursor):
				auto vcursor = getCursorPosition().y;
				auto tomove = nwl - st->wrappedLineIdx - 1;

				// ztmu_dbg("height: %zu, vc: %zu, tm: %zu\n", st->termHeight, vcursor, tomove);
				if(st->termHeight - vcursor < tomove)
				{
					ztmu_dbg("need to scroll\n");
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
			auto hcursor = get_cursor_line_offset(st->termWidth, st->cursor, st->normPL, st->wrapPL, &didWrap);
			ztmu_dbg("didWrap: %d\n", didWrap);

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

				alrPrinted = st->wrapPL;
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
				ztmu_dbg("hcursor = %zu, remaining = %zu, bc = %zu, lbc = %zu, len = %zu, line = '%.*s', pr = '%s', plen = %zu\n",
					hcursor, rem, st->byteCursor, leftByteCursor, currentLine.length(), static_cast<int>(currentLine.length()),
					currentLine.data(), pstr.c_str(), plen);

				ztmu_dbg("leftportion = '%s'\n", std::string(leftPortion).c_str());

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

				auto left = st->termWidth - (printedLines == 0 ? alrPrinted : st->wrapPL);
				auto todo = std::min(left, remaining);

				qcmd(adv_and_cons(todo));
				remaining -= todo;

				if(remaining == 0)
				{
					// clear the rest of the line (cursor to end)
					if(todo != left)
						qcmd(zpr::sprint("%s0K", CSI));

					ztmu_dbg("broke\n");
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

			if(!skip_cursor)
			{
				// ok time to move up... we know that we padded with spaces all the way to the edge
				// so we can move up by printedLines, and left by width - hcursor
				qcmd(moveCursorUp(static_cast<int>(printedLines)));
				qcmd(moveCursorLeft(9999));
				qcmd(moveCursorRight(hcursor));
			}

			ztmu_dbg("hcursor = %zu\n", hcursor);

			st->wrappedLineIdx = calculate_wli(st, st->cursor);
			ztmu_dbg("updated wli: %zu\n\n", st->wrappedLineIdx);

			if(defer_flush) *commandBuffer = commands;
			else            flushcmd();

			return std::make_pair(printedLines, hcursor);
		}
	}

	inline void refresh_line(State* st, std::string* oldLine)
	{
		// refresh the top line manually:

		ztmu_dbg("\n\n** refreshing line %d...\n", st->lineIdx);
		auto [ down, hcursor ] = _refresh_line(st, st->lineIdx, /* skip_cursor: */ false,
			oldLine ? *oldLine : st->lines[st->lineIdx], /* defer_flush: */ false, /* cmd_buffer: */ nullptr);

		down += 1;
		ztmu_dbg("down = %zu\n", down);

		// this is not re-entrant!!!
		auto old_wli = st->wrappedLineIdx;
		auto old_bcr = st->byteCursor;
		auto old_cur = st->cursor;

		size_t downAccum = 0;

		std::string buffer;

		// refresh every line including and after the current line.
		for(size_t i = st->lineIdx + 1; i < st->lines.size(); i++)
		{
			st->wrappedLineIdx = 0;
			st->byteCursor = 0;
			st->cursor = 0;

			// see if we need to scroll. note that _refresh_line will scroll for the current line -- ONLY IF IT WRAPS
			// but because we are drawing all lines, we need space for the next line as well.
			if(getCursorPosition().y == st->termHeight)
				buffer += zpr::sprint("%s1S", CSI); // one should be enough.

			buffer += moveCursorDown(down);
			downAccum += down;

			ztmu_dbg("** refreshing line %zu...\n", i);

			std::string buf;
			down = _refresh_line(st, i, /* skip_cursor: */ true, st->lines[i], /* defer_flush: */ true,
				&buf).first + 1;

			buffer += buf;

			ztmu_dbg("down = %zu\n", down);
		}

		if(downAccum > 0)
		{
			platform_write(zpr::sprint("%s%s%s%s", buffer, moveCursorUp(downAccum), moveCursorLeft(9999),
				moveCursorRight(hcursor)));
		}

		st->wrappedLineIdx = old_wli;
		st->byteCursor = old_bcr;
		st->cursor = old_cur;
	}



	static size_t _calculate_total_nwl_for_all_lines(State* st, size_t startingLineIdx = 0)
	{
		size_t down = 0;
		for(size_t i = startingLineIdx; i < st->lines.size(); i++)
			down += calculate_nwl(st, i, displayedTextLength(st->lines[i]));

		return down;
	}


	inline void cursor_home(State* st)
	{
		st->cursor = 0;
		st->byteCursor = 0;

		refresh_line(st);
	}

	inline void cursor_end(State* st, bool refresh)
	{
		st->cursor = displayedTextLength(getCurLine(st));
		st->byteCursor = getCurLine(st).size();

		if(refresh)
			refresh_line(st);
	}


	// note: internal code doesn't use delete_left or delete_right, so we don't need two
	// versions that handle lines.
	inline void delete_left(State* st)
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
		else if(st->lineIdx > 0)
		{
			st->lineIdx -= 1;

			st->cursor = displayedTextLength(st->lines[st->lineIdx]);
			st->byteCursor = st->lines[st->lineIdx].size();
			st->wrappedLineIdx = calculate_wli(st, st->cursor);

			// here's the thing: the remaining pieces of this line needs to be appended
			// to the previous line! (+1 here cos we already -= 1 above)
			auto remaining = st->lines[st->lineIdx + 1];
			st->lines[st->lineIdx].insert(st->byteCursor, remaining);

			// before we refresh, we need to go all the way to the bottom and erase the last physical line,
			// so that it won't be lingering later on.
			{
				auto down = _calculate_total_nwl_for_all_lines(st, st->lineIdx) - 2;
				ztmu_dbg("** down = %zu\n", down);

				// physical cursor needs to move up one, so move up one more than we moved down.
				platform_write(zpr::sprint("%s%s2K%s", moveCursorDown(down), CSI, moveCursorUp(down + 1)));
			}

			// then we need to erase this line from the lines!
			st->lines.erase(st->lines.begin() + st->lineIdx + 1);

			// need to refresh.
			refresh_line(st);
		}
	}

	inline void delete_right(State* st)
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
		else if(st->lineIdx + 1 < st->lines.size())
		{
			// we don't move anything. all the text from the next line gets appended to the current line.
			auto nextLine = st->lines[st->lineIdx + 1];
			st->lines[st->lineIdx].insert(st->byteCursor, nextLine);

			// before we refresh, we need to go all the way to the bottom and erase the last physical line,
			// so that it won't be lingering later on.
			{
				auto down = _calculate_total_nwl_for_all_lines(st, st->lineIdx) - 1;
				platform_write(zpr::sprint("%s%s2K%s", moveCursorDown(down), CSI, moveCursorUp(down)));
			}

			// then we need to erase the next line from the lines!
			st->lines.erase(st->lines.begin() + st->lineIdx + 1);

			// need to refresh.
			refresh_line(st);
		}
	}

	// bound to C-k by default
	static void delete_line_right(State* st)
	{
		if(st->byteCursor < st->lines[st->lineIdx].size())
		{
			st->lines[st->lineIdx].erase(st->byteCursor);
			refresh_line(st);
		}
		else if(st->lineIdx + 1 < st->lines.size())
		{
			// just call delete_right, which will collapse this line.
			delete_right(st);
		}
	}






	// we differentiate between _cursor_left() and cursor_left() -- the latter only moves along
	// a single line ("logical line"), while the latter will move to the previous line if the current
	// line is exhausted. user-inputs call cursor_left(), while internal users should call _cursor_left().

	static void _cursor_left(State* st, int n, bool refresh = true)
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

	inline void cursor_left(State* st, bool refresh)
	{
		if(st->cursor > 0)
		{
			_cursor_left(st, 1, refresh);
		}
		else if(st->lineIdx > 0)
		{
			st->lineIdx -= 1;

			// physical cursor needs to move up one.
			platform_write(moveCursorUp(1));

			st->cursor = displayedTextLength(st->lines[st->lineIdx]);
			st->byteCursor = st->lines[st->lineIdx].size();
			st->wrappedLineIdx = calculate_wli(st, st->cursor);

			// need to refresh.
			refresh_line(st);
		}
	}


	// same deal with _cursor_right as for _cursor_left.
	static void _cursor_right(State* st, int n, bool refresh = true)
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

	inline void cursor_right(State* st, bool refresh)
	{
		if(st->byteCursor < getCurLine(st).size())
		{
			_cursor_right(st, 1, refresh);
		}
		else if(st->lineIdx + 1 < st->lines.size())
		{
			st->lineIdx += 1;

			// physical cursor needs to move down one.
			platform_write(moveCursorDown(1));

			st->cursor = 0;
			st->byteCursor = 0;
			st->wrappedLineIdx = 0;

			// need to refresh.
			refresh_line(st);
		}
	}


	// shared stuff that history-manipulating people need to do, basically moving the
	// cursor all the way to the end of the text, including the physical cursor.
	static void _move_cursor_to_end_of_input(State* st)
	{
		// we want the cursor to be at the very end of the last line of input!

		// but first, go to the very beginning, and refresh -- this draws all the lines.
		st->lineIdx = 0;
		st->cursor = 0;
		st->byteCursor = 0;
		st->wrappedLineIdx = 0;

		refresh_line(st);

		// then we can move to the bottom and refresh again.
		{
			size_t down = _calculate_total_nwl_for_all_lines(st) - 1;

			platform_write(moveCursorDown(down));
		}

		st->cursor = displayedTextLength(st->lines.back());
		st->byteCursor = convertCursorToByteCursor(st->lines.back().c_str(), st->cursor);

		st->lineIdx = st->lines.size() - 1;
		st->wrappedLineIdx = calculate_wli(st, st->cursor);

		refresh_line(st);
	}

	inline void cursor_up(State* st)
	{
		if(st->cursor > 0 && st->wrappedLineIdx > 0)
		{
			size_t promptL = st->lineIdx == 0 ? st->normPL : st->contPL;

			// first, get the current horz cursor position:
			auto hcursor = get_cursor_line_offset(st->termWidth, st->cursor, promptL, st->wrapPL);

			// because wrappedLineIdx > 0, we know we're wrapping. this is the number of chars on the current
			// line (of text!), to the left of the cursor.
			auto h_curlength = hcursor - st->wrapPL;

			// this is how many extra chars to go left.
			auto rightmargin = st->termWidth - hcursor;

			// this will stop when we can't go further, so there's no need to limit.
			_cursor_left(st, h_curlength + rightmargin);

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
			ztmu_dbg("going up...\n");

			std::string_view prevLine = st->lines[st->lineIdx - 1];
			size_t prevLineLen = displayedTextLength(prevLine);

			// first, get the current horz cursor position. for the current line, we must always be
			// in a continuation if are able to go upwards.
			auto hcursor = get_cursor_line_offset(st->termWidth, st->cursor, st->contPL, st->wrapPL);

			// now, we get the offset of the previous line:
			auto prev_hcursor = get_cursor_line_offset(st->termWidth, prevLineLen,
				st->lineIdx > 1 ? st->contPL : st->normPL, st->wrapPL);

			auto prev_right_margin = st->termWidth - prev_hcursor;

			st->cursor = prevLineLen;
			st->byteCursor = convertCursorToByteCursor(prevLine.data(), st->cursor);
			ztmu_dbg("c: %zu, bc: %zu, margin: %zu, pl: '%s'\n", st->cursor, st->byteCursor,
				prev_right_margin, std::string(prevLine).c_str());

			st->lineIdx -= 1;
			st->wrappedLineIdx = calculate_wli(st, st->cursor);

			// and then we move the cursor.
			platform_write(moveCursorUp(1));
			_cursor_left(st, st->termWidth - hcursor - prev_right_margin, /* refresh: */ false);
			refresh_line(st);

			ztmu_dbg("c: %zu, bc: %zu, margin: %zu, pl: '%s'\n", st->cursor, st->byteCursor,
				prev_right_margin, std::string(prevLine).c_str());
		}
		else if(st->history.size() > 0 && st->historyIdx < st->history.size())
		{
			ztmu_dbg("## history up (%zu)\n", st->historyIdx);

			// save it, so we can go back to it.
			if(st->historyIdx == 0)
				st->savedCurrentInput = st->lines;

			// ok, so we're already at the top of the thing, so we should clear the screen from here
			// to the bottom.
			platform_write(zpr::sprint("%s0J", CSI));

			// set the index, set the lines, and refresh.
			st->historyIdx += 1;
			st->lines = st->history[st->history.size() - st->historyIdx];

			ztmu_dbg("setting: \n");
			for(const auto& l : st->lines)
				ztmu_dbg("'%s'\n", l);

			ztmu_dbg("\n");

			_move_cursor_to_end_of_input(st);
		}
	}

	inline void cursor_down(State* st)
	{
		if(st->byteCursor < getCurLine(st).size() && (st->lines.size() == 1 || st->wrappedLineIdx + 1 < st->cachedNWLForCurrentLine))
		{
			// works on a similar principle as cursor_up.

			auto hcursor = get_cursor_line_offset(st->termWidth, st->cursor,
				st->lineIdx == 0 ? st->normPL : st->contPL, st->wrapPL);

			// again, we make a similar assumption -- that only the first and second prompts can differ; the second and
			// subsequent wrapping prompts must have the same length.

			auto rightmargin = st->termWidth - hcursor;

			// in the next line, the prompt will always be a wrapping prompt.
			auto leftmargin = hcursor - st->wrapPL;

			_cursor_right(st, rightmargin + leftmargin);
		}
		else if(st->lineIdx + 1 < st->lines.size())
		{
			ztmu_dbg("going down...\n");

			std::string_view nextLine = st->lines[st->lineIdx + 1];
			size_t nextLineLen = displayedTextLength(nextLine);

			// first, get the current horz cursor position:
			auto hcursor = get_cursor_line_offset(st->termWidth, st->cursor,
				st->lineIdx == 0 ? st->normPL:  st->contPL, st->wrapPL);

			// ok, the next line will definitely be a continuation prompt. so, see how many chars into the line
			// we'll actually put ourselves -- and handle the edge case of negative values!
			auto next_leftChars = (hcursor < st->contPL
				? 0     // snap to the beginning of the line, then.
				: std::min(hcursor - st->contPL, nextLineLen)
			);

			// ok, time to do the real work.
			st->cursor = next_leftChars;
			st->byteCursor = convertCursorToByteCursor(nextLine.data(), st->cursor);
			st->lineIdx += 1;

			// now move the cursor and refresh!
			platform_write(moveCursorDown(1));
			refresh_line(st);
		}
		else if(st->historyIdx > 0)
		{
			ztmu_dbg("## history down (%zu)\n", st->historyIdx);

			// somewhat similar to moving up in history. first, we will already be at the bottom of
			// all lines. so, we need to move the physical and virtual cursor to the top of the entire
			// input. to do this, calculate the total NWL of all lines:
			{
				// we must do this for the current input state, before we change into the history.
				auto up = _calculate_total_nwl_for_all_lines(st) - 1;

				// as we move up, we must clear the entire line, to get rid of the lingering text.
				for(size_t i = 0; i < up; i++)
				{
					platform_write(zpr::sprint("%s2K", CSI));
					platform_write(moveCursorUp(1));
				}
			}

			// ok, now we can change:
			st->historyIdx -= 1;
			if(st->historyIdx == 0) st->lines = st->savedCurrentInput, st->savedCurrentInput.clear();
			else                    st->lines = st->history[st->history.size() - st->historyIdx];

			// now, fix it.
			_move_cursor_to_end_of_input(st);
		}
	}





	// this is ugly!!!
	static State* currentStateForSignal = 0;
	inline bool read_line(State* st, int promptMode, std::string seed)
	{
		constexpr char CTRL_A       = '\x01';
		constexpr char CTRL_C       = '\x03';
		constexpr char CTRL_D       = '\x04';
		constexpr char CTRL_E       = '\x05';
		constexpr char CTRL_H       = '\x08';
		constexpr char CTRL_K       = '\x0B';
		constexpr char ENTER        = '\x0D';
		constexpr char BACKSPACE    = '\x7F';

		// NOT THREAD SAFE!!
		enterRawMode();
		currentStateForSignal = st;
		st->termWidth = getTerminalWidth();
		st->termHeight = getTerminalHeight();

		st->wrappedLineIdx = 0;
		{
			// TODO: fix this. probably pull out calc_wli from the refresh function.
			// if your seed exceeds the terminal width, then you deserve it.
			st->lines.push_back(seed);
			st->cursor = seed.size();
			st->byteCursor = convertCursorToByteCursor(seed.c_str(), st->cursor);
		}

		platform_write(promptMode == 0 ? st->promptString : st->contPromptString);
		platform_write(seed);

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


		auto commit_line = [&](bool refresh = true) {
			// move the cursor to the end, refresh the line, then leave raw mode -- this makes sure
			// that we leave the cursor in a nice place after the call to this function returns.
			cursor_end(st, refresh);

			// see how many lines we need to go down.
			size_t down = 0;
			for(size_t i = st->lineIdx + 1; i < st->lines.size(); i++)
				down += calculate_nwl(st, i, displayedTextLength(st->lines[i]));

			platform_write(moveCursorDown(down));
			platform_write(moveCursorLeft(9999));
		};

		st->wasAborted = false;

		bool eof = false;
		while(true)
		{
		// lol
		loop_top:

			char c = 0;
			if(platform_read_one(&c) <= 0)
				break;

			ztmu_dbg("[%d]", static_cast<int>(c));
			switch(c)
			{
				case CTRL_C: {
					st->wasAborted = true;

					if(st->lines.size() == 1 && st->lines[0].empty())
					{
						if(st->emptyCtrlCExits)
						{
							st->wasAborted = true;
							eof = true;
							goto finish;
						}
						else
						{
							platform_write(st->uselessControlCMsg);
							goto finish_now;
						}
					}
					else
					{
						commit_line(/* refresh: */ false);

						// if we are multi-line, we can return eof. if not, just return empty.
						if(st->lines.size() > 1)
							eof = true;

						st->clear();
						goto finish_now;
					}
				}

				// this is a little complex; control-D is apparently both EOF: when the buffer is empty,
				// and delete-left: when it is not...
				case CTRL_D: {
					// if the buffer is empty, then quit.
					if(getCurLine(st).empty() && st->lineIdx == 0)
					{
						st->wasAborted = true;

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

				// backspace
				case CTRL_H: [[fallthrough]];
				case BACKSPACE: {
					delete_left(st);
				} break;

				case CTRL_K: {
					// delete to end of line.
					delete_line_right(st);
				} break;

				// enter
				case ENTER: {
					if(auto fn = st->keyHandlers[Key::ENTER]; fn)
					{
						auto res = fn(st, Key::ENTER);
						switch(res)
						{
							case HandlerAction::CONTINUE:
								goto loop_top;

							case HandlerAction::QUIT:
								eof = true;
								goto finish;

							default:
								break;
						}
					}

					goto finish;
				}



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

							if(n == 200)
							{
								ztmu_dbg("PASTE!\n");

								// keep reading until we find an esc.
								std::string pasted;

								char x = 0;
								while(platform_read_one(&x) > 0)
								{
									pasted += x;
									if(pasted.rfind("\x1b[201~") != -1)
										break;
								}

								// get rid of the thing at the end.
								pasted.erase(pasted.rfind("\x1b[201~"));

								ztmu_dbg("PASTED: '%s'\n", pasted);

								auto oldline = getCurLine(st);

								auto len = displayedTextLength(pasted);

								getCurLine(st).insert(st->byteCursor, pasted);

								st->cursor += len;
								st->byteCursor += pasted.size();

								ztmu_dbg("c: %zu, bc: %zu\n", st->cursor, st->byteCursor);


								refresh_line(st, &oldline);

								goto loop_top;
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
							case 'C': cursor_right(st); break;
							case 'D': cursor_left(st); break;
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

					// also: for handlers, we don't bother about giving you codepoints, so... tough luck.
					// TODO.
					if(auto fn = st->keyHandlers[static_cast<Key>(uc)]; fn)
					{
						switch(fn(st, static_cast<Key>(uc)))
						{
							case HandlerAction::CONTINUE:
								break;

							case HandlerAction::QUIT: eof = true; [[fallthrough]];
							case HandlerAction::RETURN: [[fallthrough]];
							default:
								goto finish;
						}
					}

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
		commit_line();

	finish_now:
		// restore the signal state, and reset the terminal to normal mode.
		currentStateForSignal = 0;
		if(didSetSignalHandler)
			sigaction(SIGWINCH, &old_sa, nullptr);

		leaveRawMode();

		platform_write("\n");
		return eof;
	}


#endif

}
}


namespace ztmu
{
	inline size_t displayedTextLength(const std::string_view& str)
	{
		return detail::displayedTextLength(str);
	}

	inline size_t getTerminalWidth()
	{
		return detail::getTerminalWidth();
	}

	inline void State::clear()
	{
		// reset the thing.
		this->savedCurrentInput.clear();
		this->lines.clear();
		this->lineIdx = 0;

		this->cachedNWLForCurrentLine = 0;
		this->wrappedLineIdx = 0;
	}

	inline void State::setPrompt(const std::string& prompt)
	{
		this->promptString = prompt;
		this->normPL = detail::displayedTextLength(prompt);
	}

	inline void State::setContPrompt(const std::string& prompt)
	{
		this->contPromptString = prompt;
		this->contPL = detail::displayedTextLength(prompt);
	}

	inline void State::setWrappedPrompt(const std::string& prompt)
	{
		this->wrappedPromptString = prompt;
		this->wrapPL = detail::displayedTextLength(prompt);
	}

	inline void State::move_cursor_left(int n)
	{
		if(n < 0)   move_cursor_right(-n);

		for(int i = 0; i < n; i++)
			detail::cursor_left(this, /* refresh: */ false);

		detail::refresh_line(this);
	}

	inline void State::move_cursor_right(int n)
	{
		if(n < 0)   move_cursor_left(-n);

		for(int i = 0; i < n; i++)
			detail::cursor_right(this, /* refresh: */ false);

		detail::refresh_line(this);
	}

	inline void State::move_cursor_up(int n)
	{
		if(n < 0)   move_cursor_down(-n);

		for(int i = 0; i < n; i++)
			detail::cursor_up(this);

		detail::refresh_line(this);
	}

	inline void State::move_cursor_down(int n)
	{
		if(n < 0)   move_cursor_up(-n);

		for(int i = 0; i < n; i++)
			detail::cursor_down(this);

		detail::refresh_line(this);
	}


	inline void State::move_cursor_home()
	{
		detail::cursor_home(this);
	}

	inline void State::move_cursor_end()
	{
		detail::cursor_end(this);
	}

	inline void State::delete_left(int n)
	{
		if(n <= 0) return;

		for(int i = 0; i < n; i++)
			detail::delete_left(this);
	}

	inline void State::delete_right(int n)
	{
		if(n <= 0) return;

		for(int i = 0; i < n; i++)
			detail::delete_right(this);
	}

	inline void State::setKeyHandler(Key k, std::function<HandlerAction (State*, Key)> handler)
	{
		this->keyHandlers[k] = handler;
	}

	inline void State::unsetKeyHandler(Key k)
	{
		this->keyHandlers[k] = std::function<HandlerAction (State*, Key)>();
	}

	inline std::string State::getCurrentLine()
	{
		return this->lines[this->lineIdx];
	}

	inline void State::setCurrentLine(const std::string& s)
	{
		auto old = this->lines[this->lineIdx];
		this->lines[this->lineIdx] = s;

		// we need to ensure the cursors are updated!
		this->cursor = std::min(this->cursor, detail::displayedTextLength(s));
		this->byteCursor = detail::convertCursorToByteCursor(s.c_str(), this->cursor);

		detail::refresh_line(this, &old);
	}

	inline void State::useUniqueHistory(bool enable)
	{
		this->uniqueHistory = enable;
	}

	// your responsibility to check if the input was empty!
	// well if you unique it then you'll only get one empty history entry, but still.
	inline void State::addPreviousInputToHistory()
	{
		if(this->wasAborted)
			return;

		// now, if we need to unique the history, then erase (if any) the
		// prior occurrence of that item. we shouldn't really need to check for more
		// than one, because any previous call to addHistory() should have already
		// left us with at most two.

		// obviously, we check for dupes before adding the current entry, because then we'll
		// just end up erasing the new entry like an idiot.
		if(auto it = std::find(this->history.begin(), this->history.end(), this->lines); it != this->history.end())
			this->history.erase(it);

		// this must be called before the next invocation of read() or readContinuation()
		// so that State->lines is still preserved.
		this->history.push_back(this->lines);
	}

	inline void State::loadHistory(const std::vector<std::vector<std::string>>& h)
	{
		this->history = h;
	}

	// it's up to you to serialise it!
	inline std::vector<std::vector<std::string>> State::getHistory()
	{
		return this->history;
	}

	inline void State::setMessageOnControlC(const std::string& msg)
	{
		this->uselessControlCMsg = msg;
	}

	inline void State::enableExitOnEmptyControlC()
	{
		this->emptyCtrlCExits = true;
	}

	inline std::optional<std::string> State::read()
	{
		// clear.
		this->clear();
		bool eof = detail::read_line(this, /* promptMode: */ 0, "");

		if(eof) return std::nullopt;
		else    return this->lines.back();
	}

	inline std::optional<std::vector<std::string>> State::readContinuation(const std::string& seed)
	{
		// don't clear
		this->lineIdx++;

		bool eof = detail::read_line(this, /* promptMode: */ 1, seed);

		if(eof) return std::nullopt;
		else    return this->lines;
	}
}


















