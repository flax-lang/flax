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
#include <string_view>


namespace ztmu
{
	struct State
	{
		void clear();

		std::string read();
		std::string readContinuation();

		void setPrompt(const std::string& prompt);
		void setContinuationPrompt(const std::string& prompt);




		bool isContinuationMode = false;

		std::string promptString;
		std::string contPromptString;

		size_t cursor = 0;
		size_t byteCursor = 0;
		std::string currLine;

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
	static void platform_write(const std::string& s);
	static void platform_write(const char* s, size_t len);

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
	#include <termios.h>

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

	static inline void platform_write(const std::string& s)
	{
		platform_write(s.c_str(), s.size());
	}

	static inline std::string moveCursorUp(int n)
	{
		if(n < 0)   return moveCursorDown(-n);
		else        return zpr::sprint("%s%dA", CSI, n);
	}

	static inline std::string moveCursorDown(int n)
	{
		if(n < 0)   return moveCursorUp(-n);
		else        return zpr::sprint("%s%dB", CSI, n);
	}

	static inline std::string moveCursorLeft(int n)
	{
		if(n < 0)   return moveCursorRight(-n);
		else        return zpr::sprint("%s%dD", CSI, n);
	}

	static inline std::string moveCursorRight(int n)
	{
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




	static void refresh_line(State* st)
	{
		// let's not care about multiline for now.

		// kill the entire line, and move the cursor to the beginning as well.
		platform_write("\x1b[2K");
		platform_write(moveCursorLeft(9999));

		// print out the prompt.
		auto pr = st->isContinuationMode ? st->contPromptString : st->promptString;
		auto promptLen = displayedTextLength(pr);

		platform_write(pr);
		platform_write(st->currLine);

		// move the cursor to the right by the cursor.
		platform_write(moveCursorLeft(9999));
		platform_write(moveCursorRight(st->cursor + promptLen));
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


	static bool read_line(State* st)
	{
		constexpr char CTRL_C       = '\x03';
		constexpr char CTRL_D       = '\x04';
		constexpr char BACKSPACE_   = '\x08';
		constexpr char ENTER        = '\x0D';
		constexpr char BACKSPACE    = '\x7F';

		enterRawMode();

		auto cursor_home = [&st]() {
			st->cursor = 0;
			st->byteCursor = 0;

			refresh_line(st);
		};

		auto cursor_end = [&st]() {
			st->cursor = displayedTextLength(st->currLine);
			st->byteCursor = st->currLine.size();

			refresh_line(st);
		};

		auto cursor_left = [&st]() {
			if(st->cursor > 0)
			{
				auto x = st->byteCursor - 1;
				auto l = findBeginningOfUTF8CP(reinterpret_cast<const uint8_t*>(st->currLine.c_str()), x);
				st->byteCursor -= (x - l + 1);
				st->cursor -= 1;

				refresh_line(st);
			}
		};

		auto cursor_right = [&st]() {
			if(st->byteCursor < st->currLine.size())
			{
				auto x = st->byteCursor;
				auto l = findEndOfUTF8CP(reinterpret_cast<const uint8_t*>(st->currLine.c_str()), x);

				st->byteCursor += (l - x + 1);
				st->cursor += 1;

				refresh_line(st);
			}
		};



		auto delete_left = [&st]() {
			if(st->cursor > 0 && st->currLine.size() > 0)
			{
				auto x = st->byteCursor - 1;
				auto l = findBeginningOfUTF8CP(reinterpret_cast<const uint8_t*>(st->currLine.c_str()), x);

				st->currLine.erase(l, x - l + 1);
				st->byteCursor -= (x - l + 1);
				st->cursor -= 1;

				refresh_line(st);
			}
		};

		auto delete_right = [&st]() {
			if(st->byteCursor < st->currLine.size())
			{
				auto x = st->byteCursor;
				auto l = findEndOfUTF8CP(reinterpret_cast<const uint8_t*>(st->currLine.c_str()), x);

				st->currLine.erase(x, l - x + 1);

				// both cursors remain unchanged.
				refresh_line(st);
			}
		};







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

				// this is a little complex; if there's stuff in the buffer, then don't do anything
				// possibly do a bell -- swift and python both do this.
				case CTRL_D: {

					// if the buffer is empty, then quit.
					if(st->currLine.empty())
					{
						eof = true;
						goto finish;
					}
				} break;

				case CTRL_C: {
					if(st->currLine.empty())
					{
						eof = true;
						goto finish;
					}
					else
					{
						st->currLine.clear();
						st->cursor = 0;
						refresh_line(st);
					}
				}

				// backspace
				case BACKSPACE: [[fallthrough]];
				case BACKSPACE_: {
					delete_left();
				} break;

				// time for some fun.
				case ESC: {
					// there should be at least two more!
					char seq[2]; platform_read(seq, 2);

					if(seq[0] == '[')
					{
						if(seq[1] >= '0' && seq[1] <= '9')
						{
							int n = 0;

							// ok now time to read until a ~
							char x = seq[1];
							while(x != '~')
							{
								n = (10 * n) + (x - '0');
								platform_read_one(&x);
							}

							switch(n)
							{
								case 3: delete_right(); break;

								case 1: cursor_home(); break;
								case 7: cursor_home(); break;

								case 4: cursor_end(); break;
								case 8: cursor_end(); break;
							}
						}
						else
						{
							switch(seq[1])
							{
								case 'C': cursor_right(); break;
								case 'D': cursor_left(); break;
								case 'H': cursor_home(); break;
								case 'F': cursor_end(); break;
							}
						}
					}
					else if(seq[0] == 'O')
					{
						if(seq[1] == 'H')       cursor_home();
						else if(seq[1] == 'F')  cursor_end();
					}

				} break;

				// default: just append -- if it's not a control char.
				default: {
					uint8_t uc = static_cast<uint8_t>(c);
					if(uc >= 0x20 && uc <= 0x7F)
					{
						st->currLine.insert(st->currLine.begin() + st->byteCursor, c);

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

						st->currLine.insert(st->byteCursor, cp);

						st->byteCursor += cp.size();
						st->cursor += displayedTextLength(cp);
					}

					refresh_line(st);
				} break;
			}
		}

	finish:
		leaveRawMode();
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
	}

	void State::setPrompt(const std::string& prompt)
	{
		this->promptString = prompt;
	}

	void State::setContinuationPrompt(const std::string& prompt)
	{
		this->contPromptString = prompt;
	}

	std::string State::read()
	{
		// clear.
		this->clear();

		detail::platform_write(this->promptString);
		bool eof = detail::read_line(this);

		if(!eof)
		{
			this->lines.push_back(this->currLine);
			this->currLine.clear();

			return this->lines.back();
		}
		else
		{
			return "\x4";
		}
	}

	std::string State::readContinuation()
	{
		// don't clear.
		detail::platform_write(this->contPromptString);
		bool eof = detail::read_line(this);

		if(!eof)
		{
			this->lines.push_back(this->currLine);
			this->currLine.clear();

			return this->lines.back();
		}
		else
		{
			return "\x4";
		}
	}
}


















