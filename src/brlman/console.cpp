/*                     C O N S O L E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#define LIBTERMIO_IMPLEMENTATION
#include "libtermio.h"

#include "html_text.h"

namespace {

constexpr int default_rows = 24;
constexpr int status_rows = 1;
constexpr size_t horizontal_step = 8;
#ifndef HAVE_WINDOWS_H
constexpr long input_poll_microseconds = 100000;
#endif
static volatile sig_atomic_t interrupted = 0;

void stop_pager(int)
{
    interrupted = 1;
}

#ifdef HAVE_WINDOWS_H
BOOL WINAPI stop_console(DWORD event)
{
    if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT)
	return FALSE;
    interrupted = 1;
    /* Wake ReadConsoleInput so restoration runs on the main thread. */
    INPUT_RECORD record = {};
    record.EventType = KEY_EVENT;
    record.Event.KeyEvent.bKeyDown = TRUE;
    record.Event.KeyEvent.wVirtualKeyCode = VK_ESCAPE;
    record.Event.KeyEvent.uChar.UnicodeChar = 27;
    DWORD written;
    WriteConsoleInputW(GetStdHandle(STD_INPUT_HANDLE), &record, 1, &written);
    return TRUE;
}
#endif

bool write_terminal(const std::string &text)
{
    return termio_write(STDOUT_FILENO, text.data(), text.size()) == (ssize_t)text.size();
}

int read_key()
{
#ifndef HAVE_WINDOWS_H
    /* A signal can arrive just before an input read. Bound that race so
     * cleanup does not depend on another keystroke after interruption. */
    while (!interrupted) {
	fd_set input;
	FD_ZERO(&input);
	FD_SET(STDIN_FILENO, &input);
	struct timeval timeout = {0, input_poll_microseconds};
	int ready = select(STDIN_FILENO + 1, &input, NULL, NULL, &timeout);
	if (ready > 0)
	    return termio_getch(STDIN_FILENO);
	if (ready < 0)
	    return TERMIO_KEY_NONE;
	if (termio_poll_resize(STDOUT_FILENO))
	    return TERMIO_KEY_RESIZE;
    }
    return TERMIO_KEY_NONE;
#else
    return termio_getch(STDIN_FILENO);
#endif
}

class terminal_session {
public:
    terminal_session()
    {
	interrupted = 0;
	termio_save_tty(STDIN_FILENO);
#ifdef HAVE_WINDOWS_H
	native_output = GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &output_mode) != 0;
#endif
	termio_enable_vt_output(STDOUT_FILENO);
	ready = termio_has_vt_output(STDOUT_FILENO) != 0;
#ifdef HAVE_WINDOWS_H
	DWORD mode = 0;
	if (GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode)) {
	    codepage = GetConsoleOutputCP();
	    if (ready && !SetConsoleOutputCP(CP_UTF8))
		ready = false;
	}
#endif
	if (!ready)
	    return;
#ifdef HAVE_WINDOWS_H
	old_int = std::signal(SIGINT, stop_pager);
	old_term = std::signal(SIGTERM, stop_pager);
	console_handler = SetConsoleCtrlHandler(stop_console, TRUE) != 0;
#else
	struct sigaction action = {};
	action.sa_handler = stop_pager;
	sigemptyset(&action.sa_mask);
	int_saved = sigaction(SIGINT, &action, &old_int) == 0;
	term_saved = sigaction(SIGTERM, &action, &old_term) == 0;
#endif
	termio_set_raw(STDIN_FILENO);
	raw = true;
	ready = termio_is_raw(STDIN_FILENO) != 0;
	if (ready) {
	    active = write_terminal("\033[?1049h\033[?25l");
	    ready = active;
	}
    }

    ~terminal_session()
    {
	restore();
    }

    void restore()
    {
	if (restored)
	    return;
	restored = true;
	if (active)
	    write_terminal("\033[0m\033[?25h\033[?1049l");
	if (raw)
	    termio_reset_tty(STDIN_FILENO);
#ifdef HAVE_WINDOWS_H
	else if (native_output)
	    SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), output_mode);
	if (old_int != SIG_ERR)
	    std::signal(SIGINT, old_int);
	if (old_term != SIG_ERR)
	    std::signal(SIGTERM, old_term);
	if (codepage)
	    SetConsoleOutputCP(codepage);
	if (console_handler)
	    SetConsoleCtrlHandler(stop_console, FALSE);
#else
	if (int_saved)
	    sigaction(SIGINT, &old_int, NULL);
	if (term_saved)
	    sigaction(SIGTERM, &old_term, NULL);
#endif
    }

    bool usable() const { return ready; }

private:
    bool ready = false;
    bool active = false;
    bool raw = false;
    bool restored = false;
#ifdef HAVE_WINDOWS_H
    void (*old_int)(int) = SIG_ERR;
    void (*old_term)(int) = SIG_ERR;
    UINT codepage = 0;
    bool console_handler = false;
    bool native_output = false;
    DWORD output_mode = 0;
#else
    struct sigaction old_int = {};
    struct sigaction old_term = {};
    bool int_saved = false;
    bool term_saved = false;
#endif
};

size_t next_match(const std::vector<brlman::text_line> &lines,
	const std::string &query, size_t top)
{
    if (query.empty())
	return top;
    for (size_t step = 1; step <= lines.size(); step++) {
	size_t candidate = (top + step) % lines.size();
	if (brlman::line_text(lines[candidate]).find(query) != std::string::npos)
	    return candidate;
    }
    return top;
}

}

namespace brlman {

bool console_interactive()
{
    const char *term = getenv("TERM");
    if (term && std::string(term) == "dumb")
	return false;
    return termio_is_terminal(STDIN_FILENO) && termio_is_terminal(STDOUT_FILENO);
}

bool console_dump(const std::vector<text_line> &lines)
{
    for (const auto &line : lines) {
	std::string text = line_text(line) + "\n";
#ifdef HAVE_WINDOWS_H
	HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD mode = 0;
	if (GetConsoleMode(output, &mode)) {
	    int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
		    text.data(), (int)text.size(), NULL, 0);
	    if (!count)
		return false;
	    std::vector<WCHAR> wide((size_t)count);
	    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
		    text.data(), (int)text.size(), wide.data(), count))
		return false;
	    DWORD written = 0;
	    if (!WriteConsoleW(output, wide.data(), (DWORD)count, &written, NULL) ||
		written != (DWORD)count)
		return false;
	    continue;
	}
#endif
	if (fwrite(text.data(), 1, text.size(), stdout) != text.size())
	    return false;
    }
    return fflush(stdout) == 0;
}

bool console_view(const html_document &document, size_t requested_width)
{
    int rows = default_rows;
    int cols = (int)default_width;
    termio_get_winsize(STDOUT_FILENO, &rows, &cols);
    terminal_session session;
    if (!session.usable()) {
	session.restore();
	return console_dump(document.format(requested_width ? requested_width : default_width));
    }

    size_t top = 0;
    size_t left = 0;
    std::string query;
    bool searching = false;
    size_t width = requested_width ? requested_width : (size_t)std::max(cols - 1, 1);
    auto lines = document.format(width);
    while (!interrupted) {
	size_t visible = (size_t)std::max(rows - status_rows, 0);
	if (lines.empty())
	    return true;
	top = std::min(top, lines.size() - 1);
	size_t display_width = (size_t)std::max(cols - 1, 1);
	std::string screen = "\033[H";
	for (size_t i = 0; i < visible; i++) {
	    screen += "\033[2K";
	    if (top + i < lines.size())
		screen += line_text(lines[top + i], left, display_width, true);
	    screen += "\r\n";
	}
	std::string status = searching ? "/" + query :
	    "q quit  arrows scroll  space/b page  / search  n next  " +
	    std::to_string(top + 1) + "/" + std::to_string(lines.size());
	text_line status_line{{{status, normal}}, 0};
	screen += "\033[2K\033[7m" + line_text(status_line, 0, display_width) + "\033[0m";
	if (!write_terminal(screen))
	    return false;

	errno = 0;
	int key = read_key();
	if (interrupted)
	    break;
	if (termio_poll_resize(STDOUT_FILENO) || key == TERMIO_KEY_RESIZE) {
	    size_t block = lines[top].block;
	    termio_get_winsize(STDOUT_FILENO, &rows, &cols);
	    width = requested_width ? requested_width : (size_t)std::max(cols - 1, 1);
	    lines = document.format(width);
	    left = 0;
	    top = 0;
	    while (top + 1 < lines.size() && lines[top].block < block)
		top++;
	    continue;
	}
	if (key == TERMIO_KEY_NONE) {
	    if (errno == EINTR)
		continue;
	    if (errno)
		return false;
	    break;
	}
	if (searching) {
	    if (key == '\n' || key == '\r') {
		searching = false;
		top = next_match(lines, query, top);
	    } else if (key == 27) {
		searching = false;
	    } else if (key == '\b' || key == 127) {
		if (!query.empty())
		    query.pop_back();
	    } else if (key >= ' ' && key < 127) {
		query += (char)key;
	    }
	    continue;
	}
	switch (key) {
	    case 'q': case 'Q': case 27: case 3: case 4:
		return true;
	    case TERMIO_KEY_UP: case 'k':
		if (top)
		    top--;
		break;
	    case TERMIO_KEY_DOWN: case 'j': case '\n': case '\r':
		top = std::min(top + 1, lines.size() - 1);
		break;
	    case TERMIO_KEY_PGDN: case ' ':
		top = std::min(top + visible, lines.size() - 1);
		break;
	    case TERMIO_KEY_PGUP: case 'b':
		top = top > visible ? top - visible : 0;
		break;
	    case TERMIO_KEY_HOME: case 'g':
		top = 0;
		break;
	    case TERMIO_KEY_END: case 'G':
		top = lines.size() > visible ? lines.size() - visible : 0;
		break;
	    case TERMIO_KEY_LEFT: case 'h':
		left = left > horizontal_step ? left - horizontal_step : 0;
		break;
	    case TERMIO_KEY_RIGHT: case 'l': {
		size_t limit = 0;
		for (const auto &line : lines) {
		    size_t length = text_width(line_text(line));
		    if (length > display_width)
			limit = std::max(limit, length - display_width);
		}
		left = std::min(left + horizontal_step, limit);
		break;
	    }
	    case '/':
		query.clear();
		searching = true;
		break;
	    case 'n':
		top = next_match(lines, query, top);
		break;
	    default:
		break;
	}
    }
    return true;
}

}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
