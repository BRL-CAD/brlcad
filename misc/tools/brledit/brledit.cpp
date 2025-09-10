/*
 * brledit: Minimalist text editor
 *
 * Although it has been extensively reworked with help from Github Copilot,
 * this editor is originally based on code from https://github.com/troglobit/mg
 * The keybindings were adjusted to use pico/nano style bindings for
 * consistency with other editors intended to be simple and friendly to new
 * users.
 *
 * Accordingly, since we have built this from an mg starting point, we maintain
 * that license for this editor as well:
 *
 * This is free and unencumbered software released into the public domain.
 *
 * Anyone is free to copy, modify, publish, use, compile, sell, or
 * distribute this software, either in source code form or as a compiled
 * binary, for any purpose, commercial or non-commercial, and by any
 * means.
 *
 * In jurisdictions that recognize copyright laws, the author or authors
 * of this software dedicate any and all copyright interest in the
 * software to the public domain. We make this dedication for the benefit
 * of the public at large and to the detriment of our heirs and
 * successors. We intend this dedication to be an overt act of
 * relinquishment in perpetuity of all present and future rights to this
 * software under copyright law.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * For more information, please refer to <http://unlicense.org/>
 */

#define NOMINMAX
#define LIBTERMIO_IMPLEMENTATION
#include "libtermio.h"

#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <cerrno>
#include <csignal>

#ifdef _WIN32
#  include <io.h>
#else
#  include <unistd.h>
#endif

/* Config */
static constexpr int STATUS_TIMEOUT_SECONDS = 4;
static constexpr int MIN_PAGE_SCROLL = 1;

/* Alternate screen RAII */
struct AltScreenRAII {
    AltScreenRAII()  { termio_write(STDOUT_FILENO, "\033[?1049h", 8); }
    ~AltScreenRAII() { termio_write(STDOUT_FILENO, "\033[?1049l", 8); }
};

#ifndef _WIN32
static void editor_signal_handler(int sig){
    termio_reset_tty(STDIN_FILENO);
    termio_write(STDOUT_FILENO,"\n",1);
    _exit(128+sig);
}
static void install_signals(){
    struct sigaction sa;
    sa.sa_handler = editor_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags=0;
    sigaction(SIGINT,&sa,nullptr);
    sigaction(SIGTERM,&sa,nullptr);
    sigaction(SIGHUP,&sa,nullptr);
}
#endif

/* Buffer: ASCII lines */
struct Buffer {
    std::vector<std::string> lines{std::string()};
    bool dirty=false;
    std::string filename;
    std::vector<std::string> cut_buffer;

    int num_lines() const { return (int)lines.size(); }
    void insert_line(int idx,const std::string &s){
        if (idx<0 || idx>num_lines()) return;
        lines.insert(lines.begin()+idx,s);
        dirty=true;
    }
    void delete_line(int idx){
        if (idx<0 || idx>=num_lines()) return;
        lines.erase(lines.begin()+idx);
        if (lines.empty()) lines.push_back(std::string());
        dirty=true;
    }
    void insert_char(int row,int col,char ch){
        if (row<0 || row>=num_lines()) return;
        auto &ln = lines[row];
        if (col<0) col=0;
        if (col>(int)ln.size()) col=(int)ln.size();
        ln.insert((size_t)col,1,ch);
        dirty=true;
    }
    void replace_char(int row,int col,char ch){
        if (row<0 || row>=num_lines()) return;
        auto &ln=lines[row];
        if (col<0 || col>=(int)ln.size()) return;
        ln[(size_t)col]=ch; dirty=true;
    }
    void delete_char(int row,int col){
        if (row<0 || row>=num_lines()) return;
        auto &ln=lines[row];
        if (col<0 || col>=(int)ln.size()) return;
        ln.erase((size_t)col,1);
        dirty=true;
    }
    bool load(const std::string &fname,std::string &err){
        std::ifstream ifs(fname);
        if(!ifs){ err="Cannot open file"; return false;}
        lines.clear();
        std::string l;
        while(std::getline(ifs,l)){
            lines.push_back(l);
        }
        if (lines.empty()) lines.push_back(std::string());
        filename=fname;
        dirty=false;
        return true;
    }
    bool save(const std::string &fname,std::string &err){
        std::ofstream ofs(fname);
        if(!ofs){ err="Cannot open file"; return false; }
        for (size_t i=0;i<lines.size();++i){
            ofs<<lines[i];
            if (i+1!=lines.size()) ofs<<"\n";
        }
        if(!ofs){ err="Write error"; return false; }
        filename=fname;
        dirty=false;
        return true;
    }
};

/* Editor state */
struct Editor {
    Buffer buf;
    int cursor_row=0;
    int cursor_col=0;
    int rowoff=0;
    int coloff=0;
    int rows=24, cols=80;
    bool running=true;
    bool overwrite=false;
    std::string status_msg;
    std::chrono::steady_clock::time_point status_time;
    std::string last_search;
    int goal_col=0;
    int last_status_lines=1;

    void set_status(const std::string &s){
        status_msg=s;
        status_time=std::chrono::steady_clock::now();
    }
} E;

/* Help entries */
static const struct HelpEntry {
    const char *key;
    const char *label;
} HELP_ENTRIES[] = {
    {"^X","Exit"},{"^O","WriteOut"},{"^W","Search"},{"^K","Cut"},
    {"^U","Uncut"},{"^Y","PgUp"},{"^V","PgDn"},{"^_","Goto"},
    {"INS","OVR"}
};
static constexpr int NUM_HELP= (int)(sizeof(HELP_ENTRIES)/sizeof(HELP_ENTRIES[0]));

/* Build left section of status line: filename, modified flag, overwrite, status message */
static std::string build_status_left() {
    std::string s = (E.buf.filename.empty() ? "[No Name]" : E.buf.filename);
    if (E.buf.dirty) s += " [+]";
    if (E.overwrite) s += " [OVR]";
    if (!E.status_msg.empty()) {
        s += " | ";
        s += E.status_msg;
    }
    return s;
}

/* Build right-aligned Ln/Col string */
static std::string build_status_right() {
    // Cursor rows/cols are 0-based internally
    std::string r = "Ln " + std::to_string(E.cursor_row + 1) +
                    ", Col " + std::to_string(E.cursor_col + 1);
    return r;
}

/* Build a flattened one-line help string */
static std::string build_help_flat() {
    std::string h;
    for (int i=0;i<NUM_HELP;i++) {
        if (i) h += "  ";
        h += HELP_ENTRIES[i].key;
        h += " ";
        h += HELP_ENTRIES[i].label;
    }
    return h;
}

/* Wrapped help line count for multi-line layout */
static int help_wrapped_lines(int width){
    if (width < 1) width = 1;
    int lines=1,w=0;
    for (int i=0;i<NUM_HELP;i++){
        int entrylen=(int)strlen(HELP_ENTRIES[i].key)+1+(int)strlen(HELP_ENTRIES[i].label);
        int sep=(w?2:0);
        if (w && w+sep+entrylen > width){
            lines++; w=0;
        }
        w += sep+entrylen;
    }
    return lines;
}

/* Decide how many status lines (1 == everything single line, or >1 multi) */
static bool can_single_line_status() {
    std::string left = build_status_left();
    std::string right = build_status_right();
    std::string help = build_help_flat();

    int right_len = (int)right.size();
    if (right_len >= E.cols) return false; // can't even place right part fully
    int avail = E.cols - right_len - 1; // leave at least one space before right part
    if (avail <= 0) return false;

    int left_len = (int)left.size();
    int help_len = (int)help.size();

    // Need left + (if help not empty, 2 spaces + help) to fit in avail.
    // If it doesn't, we go multi-line.
    if (left_len + (help_len ? (2 + help_len) : 0) <= avail) return true;
    return false;
}

static int current_status_lines(){
    if (can_single_line_status()) return 1;
    // 1 line for status + wrapped help lines below
    return 1 + help_wrapped_lines(E.cols);
}

static void draw_wrapped_help_lines(int start_row) {
    int cur_row = start_row;
    int w = 0;
    termio_move_cursor(STDOUT_FILENO, cur_row, 1);
    for (int i=0;i<NUM_HELP;i++) {
        int entrylen = (int)strlen(HELP_ENTRIES[i].key) + 1 + (int)strlen(HELP_ENTRIES[i].label);
        int sep = (w?2:0);
        if (w && w + sep + entrylen > E.cols) {
            // pad remainder
            for (int j=w;j<E.cols;j++) termio_write(STDOUT_FILENO," ",1);
            cur_row++;
            w=0;
            termio_move_cursor(STDOUT_FILENO, cur_row, 1);
        }
        if (w && sep) {
            termio_write(STDOUT_FILENO,"  ",2);
            w += 2;
        }
        termio_write(STDOUT_FILENO, HELP_ENTRIES[i].key, strlen(HELP_ENTRIES[i].key));
        termio_write(STDOUT_FILENO, " ", 1);
        termio_write(STDOUT_FILENO, HELP_ENTRIES[i].label, strlen(HELP_ENTRIES[i].label));
        w += entrylen;
    }
    // Pad end of last help line
    for (int j=w;j<E.cols;j++) termio_write(STDOUT_FILENO," ",1);
}

static void draw_status(bool flash=false){
    int slines = current_status_lines();
    E.last_status_lines = slines;
    termio_show_cursor(STDOUT_FILENO,0);

    std::string left  = build_status_left();
    std::string right = build_status_right();
    int right_len = (int)right.size();

    if (slines == 1) {
        // Single line: left + help + right-aligned Ln/Col
        std::string help = build_help_flat();
        int avail = E.cols - right_len - 1;
        if (avail < 0) avail = 0;

        std::string line;
        // Entire left must fit (if not, truncate)
        if ((int)left.size() > avail) {
            left.resize(avail);
            help.clear(); // no room for help if left took entire avail
        }

        line = left;
        if (!help.empty()) {
            if ((int)line.size() + 2 + (int)help.size() <= avail) {
                if (!line.empty()) line += "  ";
                line += help;
            } else {
                // Not enough room for help after all; ignore help on single line
            }
        }

        // Now construct full buffer
        // We'll fill a char buffer with spaces, then overlay left/help and right
        std::string out(E.cols,' ');
        // Copy left/help segment
        for (size_t i=0;i<line.size() && (int)i<avail;i++)
            out[i] = line[i];
        // Copy right-aligned Ln/Col at end
        if (right_len < E.cols) {
            int start = E.cols - right_len;
            for (int i=0;i<right_len;i++)
                out[start + i] = right[i];
        }
        if (flash) termio_flash_status_line(STDOUT_FILENO, E.rows, E.cols);
        termio_move_cursor(STDOUT_FILENO,E.rows,1);
        termio_write(STDOUT_FILENO,out.c_str(),out.size());
    } else {
        // Multi-line:
        // First line: status-left truncated if needed, right-aligned Ln/Col
        int status_row = E.rows - slines + 1;
        std::string out(E.cols,' ');
        if (right_len >= E.cols) {
            // If the right string somehow is longer, just show its tail
            right = right.substr(right_len - E.cols);
            right_len = (int)right.size();
        }
        int right_start = E.cols - right_len;
        for (int i=0;i<right_len;i++)
            out[right_start + i] = right[i];

        // Left area: up to right_start-1
        int left_space = right_start - 1; // leave one space gap
        if (left_space < 0) left_space = 0;
        if ((int)left.size() > left_space)
            left.resize(left_space);
        for (size_t i=0; i<left.size(); i++)
            out[i] = left[i];

        if (flash) termio_flash_status_line(STDOUT_FILENO,status_row,E.cols);
        termio_move_cursor(STDOUT_FILENO,status_row,1);
        termio_write(STDOUT_FILENO,out.c_str(),out.size());

        // Help lines below (wrapped)
        draw_wrapped_help_lines(status_row + 1);
    }

    termio_show_cursor(STDOUT_FILENO,1);
}

static void clamp_cursor(){
    if (E.cursor_row<0) E.cursor_row=0;
    if (E.cursor_row>=E.buf.num_lines()) E.cursor_row=E.buf.num_lines()-1;
    if (E.cursor_row<0) E.cursor_row=0;
    int len=(int)E.buf.lines[E.cursor_row].size();
    if (E.cursor_col<0) E.cursor_col=0;
    if (E.cursor_col>len) E.cursor_col=len;
}

static int cursor_disp_col(){ return E.cursor_col; }

static void draw_screen(){
    int slines=current_status_lines();
    E.last_status_lines=slines;
    int usable=E.rows - slines;
    if (usable<1) usable=1;

    for (int i=0;i<usable;i++){
        int idx=E.rowoff + i;
        termio_move_cursor(STDOUT_FILENO,i+1,1);
        if (idx < 0 || idx >= E.buf.num_lines()){
            termio_write(STDOUT_FILENO,"~",1);
            for (int k=1;k<E.cols;k++) termio_write(STDOUT_FILENO," ",1);
            continue;
        }
        const std::string &ln=E.buf.lines[idx];
        int len=(int)ln.size();
        int start=E.coloff;
        if (start>len) start=len;
        int remain=len-start;
        if (remain>E.cols) remain=E.cols;
        if (remain>0) termio_write(STDOUT_FILENO, ln.c_str()+start, (size_t)remain);
        for (int k=remain;k<E.cols;k++) termio_write(STDOUT_FILENO," ",1);
    }
    draw_status(false);
}

static void place_cursor(){
    int slines=current_status_lines();
    int usable=E.rows - slines;
    if (usable<1) usable=1;
    int row = E.cursor_row - E.rowoff + 1;
    if (row<1) row=1;
    if (row>usable) row=usable;
    int col = E.cursor_col - E.coloff + 1;
    if (col<1) col=1;
    if (col>E.cols) col=E.cols;
    termio_move_cursor(STDOUT_FILENO,row,col);
}

static void redraw_all(){
    termio_show_cursor(STDOUT_FILENO,0);
    draw_screen();
    place_cursor();
    termio_show_cursor(STDOUT_FILENO,1);
}

static std::string prompt(const std::string &msg){
    redraw_all();
    int slines=current_status_lines();
    int prow=E.rows - slines + 1;
    termio_move_cursor(STDOUT_FILENO,prow,1);
    std::string line=msg;
    if ((int)line.size()>E.cols) line.resize(E.cols);
    termio_write(STDOUT_FILENO,line.c_str(),line.size());
    termio_write(STDOUT_FILENO,"\033[K",3);
    std::string out;
    for(;;){
        int ch = termio_getch(STDIN_FILENO);
        if (ch==13 || ch==10) break;
        if (ch==27){ out.clear(); break; }
        if (ch==127 || ch==8){
            if (!out.empty()){
                out.pop_back();
                termio_write(STDOUT_FILENO,"\b \b",3);
            }
        } else if (ch>=32 && ch<127){
            if ((int)(msg.size()+out.size()) < E.cols-1){
                out.push_back((char)ch);
		char c = (char)ch;
                termio_write(STDOUT_FILENO,&c,1);
            }
        }
    }
    redraw_all();
    return out;
}

/* Search */
static bool search_forward(const std::string &term,int &fr,int &fc,int sr,int sc){
    if (term.empty()) return false;
    for (int r=sr;r<E.buf.num_lines();++r){
        const std::string &hay = E.buf.lines[r];
        size_t start=0;
        if (r==sr) {
            if (sc < (int)hay.size())
                start=(size_t)sc;
            else continue;
        }
        if (start>hay.size()) continue;
        size_t pos=hay.find(term,start);
        if (pos!=std::string::npos){
            fr=r; fc=(int)pos;
            return true;
        }
    }
    return false;
}
static void do_search(){
    std::string q=prompt("Search (^W): ");
    if (q.empty()){
        if (E.last_search.empty()){
            E.set_status("No prior search"); return;
        }
        q=E.last_search;
    }
    int fr,fc;
    if (search_forward(q,fr,fc,E.cursor_row,E.cursor_col+1)){
        E.cursor_row=fr; E.cursor_col=fc;
        E.goal_col=cursor_disp_col();
        clamp_cursor();
        E.last_search=q;
        E.set_status("Found: "+q);
    } else {
        E.set_status("Not found: "+q);
    }
}

/* Editing helpers */
static void cut_line(){
    if (E.cursor_row<0 || E.cursor_row>=E.buf.num_lines()) return;
    E.buf.cut_buffer.push_back(E.buf.lines[E.cursor_row]);
    E.buf.delete_line(E.cursor_row);
    if (E.cursor_row>=E.buf.num_lines()) E.cursor_row=E.buf.num_lines()-1;
    if (E.cursor_row<0) E.cursor_row=0;
    E.cursor_col=0;
    E.goal_col=cursor_disp_col();
    clamp_cursor();
    E.set_status("Line cut");
}
static void uncut_lines(){
    if (E.buf.cut_buffer.empty()) { E.set_status("Cut buffer empty"); return; }
    int at=E.cursor_row+1;
    for (size_t i=0;i<E.buf.cut_buffer.size();++i)
        E.buf.insert_line(at+(int)i,E.buf.cut_buffer[i]);
    E.set_status("Uncut");
}
static void goto_line(){
    std::string s=prompt("Goto line (^_): ");
    if (s.empty()) return;
    char *endp=nullptr;
    errno=0;
    long v=strtol(s.c_str(),&endp,10);
    if (endp==s.c_str() || *endp || errno || v<=0){ E.set_status("Invalid line"); return; }
    if (v>(long)E.buf.num_lines()) v=E.buf.num_lines();
    E.cursor_row=(int)v-1;
    int len=(int)E.buf.lines[E.cursor_row].size();
    int tgt = E.goal_col;
    if (tgt>len) tgt=len;
    E.cursor_col=tgt;
    clamp_cursor();
}
static void write_out(){
    std::string fname=E.buf.filename;
    if (fname.empty()){
        fname=prompt("Write Out (^O) - file name: ");
        if (fname.empty()){ E.set_status("Canceled"); return; }
    }
    std::string err;
    if (!E.buf.save(fname,err)){
        E.set_status("Save failed: "+err);
    } else {
        E.set_status("Wrote: "+fname);
    }
}
static bool confirm_exit(){
    if (!E.buf.dirty) return true;
    std::string ans=prompt("Unsaved changes! Save? (y/n/c): ");
    if (ans.empty()) return false;
    char c=(char)tolower((unsigned char)ans[0]);
    if (c=='y'){
        write_out();
        return !E.buf.dirty;
    } else if (c=='n'){
        return true;
    }
    return false;
}

/* Input loop */
static void event_loop(){
    bool redraw=true;
    while (E.running){
        int prior_status_lines = current_status_lines();
        int prev_row=E.cursor_row;
        int prev_col=cursor_disp_col();

        if (redraw){
            redraw_all();
            redraw=false;
        } else {
            place_cursor();
            termio_show_cursor(STDOUT_FILENO,1);
        }

        if (!E.status_msg.empty() &&
            std::chrono::steady_clock::now() - E.status_time >
              std::chrono::seconds(STATUS_TIMEOUT_SECONDS)){
            E.status_msg.clear();
            draw_status(false);
        }

        int ch = termio_getch(STDIN_FILENO);

        switch (ch){
            case 24: /* ^X */
                if (confirm_exit()) E.running=false;
                redraw=true;
                break;
            case 15: /* ^O */
                write_out(); redraw=true; break;
            case 23: /* ^W */
                do_search(); redraw=true; break;
            case 11: /* ^K */
                cut_line(); redraw=true; break;
            case 21: /* ^U */
                uncut_lines(); redraw=true; break;
            case 25: /* ^Y */
            case TERMIO_KEY_PGUP: {
                int page=(E.rows - current_status_lines() - 1);
                if (page < MIN_PAGE_SCROLL) page=MIN_PAGE_SCROLL;
                E.cursor_row -= page;
                if (E.cursor_row<0) E.cursor_row=0;
                int len=(int)E.buf.lines[E.cursor_row].size();
                int tgt = E.goal_col;
                if (tgt>len) tgt=len;
                E.cursor_col=tgt;
                clamp_cursor(); redraw=true;
            } break;
            case 22: /* ^V */
            case TERMIO_KEY_PGDN: {
                int page=(E.rows - current_status_lines() - 1);
                if (page < MIN_PAGE_SCROLL) page=MIN_PAGE_SCROLL;
                E.cursor_row += page;
                if (E.cursor_row>=E.buf.num_lines()) E.cursor_row=E.buf.num_lines()-1;
                int len=(int)E.buf.lines[E.cursor_row].size();
                int tgt=E.goal_col;
                if (tgt>len) tgt=len;
                E.cursor_col=tgt;
                clamp_cursor(); redraw=true;
            } break;
            case 31: /* ^_ */
                goto_line(); redraw=true; break;

            case TERMIO_KEY_UP:
                if (E.cursor_row>0){
                    E.cursor_row--;
                    {
                        int len=(int)E.buf.lines[E.cursor_row].size();
                        int tgt=E.goal_col;
                        if (tgt>len) tgt=len;
                        E.cursor_col=tgt;
                    }
                    clamp_cursor();
                }
                break;
            case TERMIO_KEY_DOWN:
                if (E.cursor_row + 1 < E.buf.num_lines()){
                    E.cursor_row++;
                    {
                        int len=(int)E.buf.lines[E.cursor_row].size();
                        int tgt=E.goal_col;
                        if (tgt>len) tgt=len;
                        E.cursor_col=tgt;
                    }
                    clamp_cursor();
                }
                break;
            case TERMIO_KEY_LEFT:
                if (E.cursor_col>0) E.cursor_col--;
                else if (E.cursor_row>0){
                    E.cursor_row--;
                    E.cursor_col=(int)E.buf.lines[E.cursor_row].size();
                }
                E.goal_col=cursor_disp_col();
                clamp_cursor();
                break;
            case TERMIO_KEY_RIGHT: {
                int len=(int)E.buf.lines[E.cursor_row].size();
                if (E.cursor_col < len) E.cursor_col++;
                else if (E.cursor_row + 1 < E.buf.num_lines()){
                    E.cursor_row++;
                    E.cursor_col=0;
                }
                E.goal_col=cursor_disp_col();
                clamp_cursor();
            } break;
            case TERMIO_KEY_HOME:
                E.cursor_col=0;
                E.goal_col=cursor_disp_col();
                clamp_cursor();
                break;
            case TERMIO_KEY_END:
                E.cursor_col=(int)E.buf.lines[E.cursor_row].size();
                E.goal_col=cursor_disp_col();
                clamp_cursor();
                break;
            case TERMIO_KEY_INSERT:
                E.overwrite = !E.overwrite;
                E.set_status(E.overwrite ? "Overwrite ON" : "Overwrite OFF");
                redraw=true;
                break;
            case TERMIO_KEY_DELETE:
            case 4: /* ^D */ {
                int len=(int)E.buf.lines[E.cursor_row].size();
                if (E.cursor_col < len){
                    E.buf.delete_char(E.cursor_row,E.cursor_col);
                } else if (E.cursor_row+1 < E.buf.num_lines()){
                    E.buf.lines[E.cursor_row] += E.buf.lines[E.cursor_row+1];
                    E.buf.delete_line(E.cursor_row+1);
                }
                clamp_cursor(); redraw=true;
            } break;
            case 13: case 10: { /* Enter */
                std::string &line=E.buf.lines[E.cursor_row];
                int split = (E.cursor_col < (int)line.size()) ? E.cursor_col : (int)line.size();
                std::string rest=line.substr((size_t)split);
                line.erase((size_t)split);
                E.buf.insert_line(E.cursor_row+1,rest);
                E.cursor_row++; E.cursor_col=0;
                E.goal_col=cursor_disp_col();
                clamp_cursor(); redraw=true;
            } break;
            case 127: case 8: { /* Backspace */
                if (E.cursor_col>0){
                    E.cursor_col--;
                    E.buf.delete_char(E.cursor_row,E.cursor_col);
                } else if (E.cursor_row>0){
                    int plen=(int)E.buf.lines[E.cursor_row-1].size();
                    E.buf.lines[E.cursor_row-1] += E.buf.lines[E.cursor_row];
                    E.buf.delete_line(E.cursor_row);
                    E.cursor_row--;
                    E.cursor_col=plen;
                }
                E.goal_col=cursor_disp_col();
                clamp_cursor(); redraw=true;
            } break;
            default:
                if (ch>=32 && ch<127){
                    int len=(int)E.buf.lines[E.cursor_row].size();
                    if (E.overwrite && E.cursor_col < len)
                        E.buf.replace_char(E.cursor_row,E.cursor_col,(char)ch);
                    else
                        E.buf.insert_char(E.cursor_row,E.cursor_col,(char)ch);
                    E.cursor_col++;
                    E.goal_col=cursor_disp_col();
                    clamp_cursor();
                    redraw=true;
                }
                break;
        }

        /* Scroll adjustments */
        int slines=current_status_lines();
        int visible=E.rows - slines;
        if (visible<1) visible=1;
        if (E.cursor_row < E.rowoff){ E.rowoff=E.cursor_row; redraw=true; }
        if (E.cursor_row >= E.rowoff + visible){
            E.rowoff=E.cursor_row - (visible - 1);
            if (E.rowoff<0) E.rowoff=0;
            redraw=true;
        }
        int dcol=cursor_disp_col();
        if (dcol < E.coloff){ E.coloff=dcol; redraw=true; }
        if (dcol >= E.coloff + E.cols){
            E.coloff = dcol - E.cols + 1;
            redraw=true;
        }

        /* Resize */
        if (termio_poll_resize(STDOUT_FILENO)==1){
            termio_get_winsize(STDOUT_FILENO,&E.rows,&E.cols);
            if (E.rows<2) E.rows=2;
	    if (E.cols<1) E.cols=1;
            termio_runtime_sanitize(); /* re-disable mouse/paste after resize */
	    termio_drain_startup_noise(); /* also drain after a resize in case terminal re-issued stray events */
            redraw=true;
        }

        /* Efficient status refresh if only cursor moved without needing full redraw */
        if (!redraw){
            bool moved = (prev_row!=E.cursor_row)||(prev_col!=cursor_disp_col());
            if (moved){
                int now_lines=current_status_lines();
                if (now_lines!=prior_status_lines) {
                    redraw=true;
                } else {
                    draw_status(false);
                    place_cursor();
                    termio_show_cursor(STDOUT_FILENO,1);
                }
            }
        }
    }
}

int main(int argc,char *argv[]){
    termio_save_tty(STDIN_FILENO);
#ifndef _WIN32
    install_signals();
#endif
    /* Initialize termio debug logging early (before mode changes) */
    termio_debug_init();
    try {
        /* Enable VT if possible (console) BEFORE writing any VT sequences */
        termio_enable_vt_output(STDOUT_FILENO);
        {
            AltScreenRAII alt;
            termio_set_raw(STDIN_FILENO);
            termio_runtime_sanitize();
            termio_drain_startup_noise();

            termio_get_winsize(STDOUT_FILENO,&E.rows,&E.cols);
            if (E.rows < 2) E.rows = 2;
            if (E.cols < 1) E.cols = 1;
            if (argc>1){
                std::string err;
                if (!E.buf.load(argv[1],err))
                    E.set_status(err);
                else
                    E.set_status(std::string("Loaded (") + termio_input_environment() + ")");
            } else {
                E.set_status(std::string("New file (") + termio_input_environment() + ")");
            }
            E.goal_col=cursor_disp_col();
            redraw_all();
            event_loop();
        } /* AltScreenRAII destroyed here (restores screen) */
        termio_reset_tty(STDIN_FILENO);
    } catch(...){
        termio_reset_tty(STDIN_FILENO);
        termio_write(STDOUT_FILENO,"\nException - terminal restored.\n",34);
        return 1;
    }
    termio_write(STDOUT_FILENO,"\n",1);
    return 0;
}
