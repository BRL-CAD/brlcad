# BRL-CAD RMF/STIG Cat 1 Security Audit Dashboard
**Last Updated:** 2026-09-19 03:23:59 UTC

## Overall Progress
- **Total C/C++ Files:** 3493
- **Files Reviewed:** 274 (7.8%)
- **Files Pending Review:** 3219
- **Total Issues Identified:** 174

### Issues by Severity Potential
| Severity Level | Count | Description |
|:---:|:---:|:---|
| **3 (High)** | 11 | Likely exploit or crash potential; widespread/library exposure |
| **2 (Medium)** | 122 | Possible exploit or crash under specific circumstances |
| **1 (Low)** | 41 | Localized / low-impact vulnerability |

### Issues by Verification Status
- **Confirmed:** 0
- **Fixed (Committed):** 173
- **Pending Verification:** 1
- **Disproven:** 0

## Subsystem Review Progress
| Subsystem | Total Files | Reviewed | Progress | Issues Found |
|:---|:---:|:---:|:---:|:---:|
| `bench` | 5 | 0 | 0.0% | 0 |
| `doc` | 19 | 0 | 0.0% | 0 |
| `include` | 315 | 2 | 0.6% | 1 |
| `misc` | 55 | 0 | 0.0% | 0 |
| `regress` | 21 | 0 | 0.0% | 0 |
| `src/adrt` | 51 | 0 | 0.0% | 0 |
| `src/archer` | 1 | 0 | 0.0% | 0 |
| `src/art` | 6 | 0 | 0.0% | 0 |
| `src/brlman` | 1 | 0 | 0.0% | 0 |
| `src/bwish` | 3 | 0 | 0.0% | 0 |
| `src/conv/3dm` | 1 | 0 | 0.0% | 0 |
| `src/conv/asc` | 5 | 0 | 0.0% | 0 |
| `src/conv/bot_dump.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/comgeom` | 7 | 0 | 0.0% | 0 |
| `src/conv/conv-vg2g.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/csg` | 2 | 0 | 0.0% | 0 |
| `src/conv/dbupgrade.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/dxf` | 3 | 0 | 0.0% | 0 |
| `src/conv/enf-g.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/fast4-g.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/g-acad.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/g-dot.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/g-egg.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/g-nff.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/g-obj.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/g-shell-rect.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/g-var.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/g-vdb.cpp` | 1 | 0 | 0.0% | 0 |
| `src/conv/g-voxel.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/g-vrml.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/g-x3d.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/g-xxx.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/g-xxx_facets.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/g4-g5.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/g5-g4.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/gcv` | 1 | 0 | 0.0% | 0 |
| `src/conv/gltf` | 1 | 0 | 0.0% | 0 |
| `src/conv/iges` | 2 | 0 | 0.0% | 0 |
| `src/conv/intaval` | 8 | 0 | 0.0% | 0 |
| `src/conv/jack` | 2 | 0 | 0.0% | 0 |
| `src/conv/k-g` | 19 | 0 | 0.0% | 0 |
| `src/conv/nastran-g.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/nmg` | 1 | 0 | 0.0% | 0 |
| `src/conv/obj-g.c` | 1 | 0 | 0.0% | 0 |
| `src/conv/off` | 2 | 0 | 0.0% | 0 |
| `src/conv/patch` | 3 | 0 | 0.0% | 0 |
| `src/conv/ply` | 2 | 0 | 0.0% | 0 |
| `src/conv/raw` | 6 | 0 | 0.0% | 0 |
| `src/conv/shp` | 4 | 0 | 0.0% | 0 |
| `src/conv/step` | 600 | 0 | 0.0% | 0 |
| `src/conv/stl` | 2 | 0 | 0.0% | 0 |
| `src/conv/tankill` | 1 | 0 | 0.0% | 0 |
| `src/conv/vdeck` | 5 | 0 | 0.0% | 0 |
| `src/conv/walk_example.c` | 1 | 0 | 0.0% | 0 |
| `src/external` | 17 | 0 | 0.0% | 0 |
| `src/fb` | 29 | 0 | 0.0% | 0 |
| `src/fbserv` | 4 | 4 | 100.0% | 2 |
| `src/gtools` | 40 | 0 | 0.0% | 0 |
| `src/isst` | 9 | 0 | 0.0% | 0 |
| `src/launcher` | 8 | 0 | 0.0% | 0 |
| `src/libanalyze` | 40 | 0 | 0.0% | 0 |
| `src/libbg` | 232 | 34 | 14.7% | 27 |
| `src/libbn` | 41 | 41 | 100.0% | 31 |
| `src/libbrep` | 50 | 0 | 0.0% | 0 |
| `src/libbu` | 176 | 176 | 100.0% | 101 |
| `src/libbv` | 19 | 0 | 0.0% | 0 |
| `src/libdm` | 80 | 0 | 0.0% | 0 |
| `src/libfft` | 8 | 0 | 0.0% | 0 |
| `src/libgcv` | 111 | 0 | 0.0% | 0 |
| `src/libged` | 509 | 0 | 0.0% | 0 |
| `src/libicv` | 34 | 0 | 0.0% | 0 |
| `src/libnmg` | 54 | 0 | 0.0% | 0 |
| `src/liboptical` | 46 | 0 | 0.0% | 0 |
| `src/libpc` | 6 | 0 | 0.0% | 0 |
| `src/libpkg` | 10 | 10 | 100.0% | 3 |
| `src/libqtcad` | 29 | 0 | 0.0% | 0 |
| `src/librt` | 373 | 0 | 0.0% | 0 |
| `src/libtclcad` | 31 | 0 | 0.0% | 0 |
| `src/libtermio` | 2 | 0 | 0.0% | 0 |
| `src/libwdb` | 27 | 0 | 0.0% | 0 |
| `src/mged` | 55 | 0 | 0.0% | 0 |
| `src/nirt` | 2 | 0 | 0.0% | 0 |
| `src/proc-db` | 55 | 0 | 0.0% | 0 |
| `src/qged` | 34 | 0 | 0.0% | 0 |
| `src/remrt` | 7 | 7 | 100.0% | 2 |
| `src/rt` | 33 | 0 | 0.0% | 0 |
| `src/rtwizard` | 1 | 0 | 0.0% | 0 |
| `src/shapes` | 15 | 0 | 0.0% | 0 |
| `src/sig` | 39 | 0 | 0.0% | 0 |
| `src/util` | 90 | 0 | 0.0% | 0 |

## Identified Issues Summary
| ID | Severity | Category | File & Location | Verification | Nature of Issue |
|:---|:---:|:---|:---|:---:|:---|
| `SEC-0001` | **Sev 3** | Invalid Free / Crasher | `src/libpkg/pkg.c:1876-1923` | `FIXED` | pkg_waitfor fails to clear pc->pkc_buf on excess message length, causing subsequent pkg_close to call free on caller stack memory. |
| `SEC-0002` | **Sev 2** | Uninitialized Memory Access | `src/libpkg/pkg.c:1960-1968` | `FIXED` | pkg_bwaitfor returns uninitialized heap buffer instead of NULL when network read fails or disconnects prematurely. |
| `SEC-0003` | **Sev 1** | NULL Pointer Dereference / Crasher | `src/libpkg/example_qt/server.cpp:46-59` | `PENDING` | PKGServer destructor unconditionally dereferences NULL client pointer and frees uninitialized pkc_inbuf. |
| `SEC-0004` | **Sev 3** | Heap Out-of-Bounds Read / Crasher | `src/fbserv/server.c:432-540` | `FIXED` | fbserv packet handlers fail to validate packet payload length against pixel count before calling fb_write, causing heap out-of-bounds read. |
| `SEC-0005` | **Sev 3** | Integer Overflow / Heap Buffer Overflow | `src/fbserv/server.c:476-580` | `FIXED` | Integer multiplication overflow in width*height in fb_readrect and fb_bwreadrect leads to heap buffer overflow during pixel read. |
| `SEC-0006` | **Sev 2** | Integer Signedness / Out-of-Bounds Memory Access | `src/remrt/rtsrv.c:891-971` | `FIXED` | ph_lines fails to validate pixel range bounds, causing signed integer underflow in (b-a+1)*3 to wrap to huge size_t in pkg_2send, triggering out-of-bounds heap reads, and leaks buffer. |
| `SEC-0007` | **Sev 2** | Unhandled Input Validation / Denial of Service | `src/remrt/remrt.c:3803-3806` | `FIXED` | ph_pixels parses incoming MSG_PIXELS without checking packet length or magic header, allowing malformed network packets to trigger out-of-bounds reads or terminate remrt via bu_bomb. |
| `SEC-0008` | **Sev 2** | Out-of-Bounds Memory Access / Buffer Underflow | `src/libbu/parse.c:87-118` | `FIXED` | bu_struct_wrap_buf and PARSE_CK_GETPUT do not verify buffer length is at least 8 bytes, causing negative index access and out-of-bounds read on buffers with small or zero length. |
| `SEC-0009` | **Sev 2** | Format String / Type Confusion | `src/libbu/vls_vprintf.c:186-226` | `FIXED` | vls_vprintf uses bitwise XOR (^=) instead of bitwise AND NOT (&= ~) to clear length modifiers, corrupting flags and causing incorrect type extraction from va_list. |
| `SEC-0010` | **Sev 2** | Off-by-One / Buffer Corruption | `src/libbu/vls.c:433-465` | `FIXED` | bu_vls_substr calls bu_vls_putc with NUL, embedding an unwanted NUL byte and off-by-one corrupting vls_len. |
| `SEC-0011` | **Sev 2** | Integer Overflow / Heap Overflow | `src/libbu/malloc.c:84-145,375-388` | `FIXED` | alloc() and bu_pool_alloc() lack integer multiplication overflow checks on cnt*size, which can cause undersized memory allocations and subsequent heap buffer overflows. |
| `SEC-0012` | **Sev 2** | Buffer Underflow / Out-of-bounds Pointer | `src/libbu/argv.c:97,175,268-295` | `FIXED` | bu_argv_from_string computed pointer at strlen(lp)-1 on potentially empty string causing integer underflow and out-of-bounds pointer arithmetic. |
| `SEC-0013` | **Sev 2** | Buffer Over-read | `src/libbu/str.c:61-75` | `FIXED` | bu_strlcatm uses unbounded strlen on dst to detect un-terminated buffers, causing out-of-bounds read. |
| `SEC-0014` | **Sev 2** | Null Pointer Dereference / Integer Overflow | `src/libbu/b64.c:216-284` | `FIXED` | bu_b64 encode and decode functions lacked null pointer checks on input/output and cast lengths to signed 32-bit int. |
| `SEC-0015` | **Sev 2** | Format String / Scanset Parser Bug | `src/libbu/sscanf.c:312-320,365-375` | `FIXED` | bu_vsscanf omitted literal ] and ^] from partFmt when extracting scanset specifiers, causing malformed format strings and scan failure. |
| `SEC-0016` | **Sev 2** | Null Pointer Dereference / Integer Overflow | `src/libbu/bitv.c:95-105,285-295,440-445,625-630` | `FIXED` | bu_hex_to_bitv and bu_binary_to_bitv2 crashed on NULL input, and bu_bitv_shift_vector had integer overflow UB on INT_MIN. |
| `SEC-0017` | **Sev 2** | Null Pointer Dereference / Memory Safety | `src/libbu/hash.c:125-135,180-190,270-280` | `FIXED` | bu_hash_get crashed on NULL key in _nhash_keycmp, and bu_hash_create/bu_hash_set lacked allocation failure handling. |
| `SEC-0018` | **Sev 3** | Memory Access Violation / Buffer Overflow / Out-of-bounds Pointer | `include/bu/ptbl.h, src/libbu/ptbl.c:105,119-120; 63,99,214,240,266,290` | `FIXED` | BU_PTBL_FOR macro and BU_PTBL_LASTADDR cause out-of-bounds pointer calculation and segfault on empty tables with NULL buffers, and table reallocations lack integer multiplication overflow checks. |
| `SEC-0019` | **Sev 2** | Out-of-bounds Bit Array Access / Resource Leak / Undefined Behavior | `src/libbu/affinity.c:74,90-104,108` | `FIXED` | parallel_set_affinity permitted negative cpu indexing into CPU_SET and negative bitshift on Windows, leaked Mach thread port rights, and passed wrong policy type for Mach thread affinity. |
| `SEC-0020` | **Sev 2** | NULL Pointer Dereference / Process Signal Safety | `src/libbu/backtrace.c:324,596,659-661,742` | `FIXED` | bu_backtrace_app called bu_strlcpy on potentially NULL gdb_path, used negative file descriptors in write, and reaped arbitrary child processes via wait(NULL). |
| `SEC-0021` | **Sev 2** | NULL Pointer Dereference / Buffer Underflow / Crasher | `src/libbu/cmd.c:50-66` | `FIXED` | bu_cmd lacked null checks on cmds, argv, and argv[cmd_index], allowed negative cmd_index causing buffer underflow, and executed unverified function pointers. |
| `SEC-0022` | **Sev 2** | Integer Multiplication Overflow / Crasher | `src/libbu/damlevlim.cpp:68-159` | `FIXED` | bu_editdist computed buffer allocation size as (n+1)*(m+1) using signed 32-bit int, causing signed integer multiplication overflow, out-of-bounds access, and std::length_error crash on large strings. |
| `SEC-0023` | **Sev 2** | Heap Buffer Overflow / NULL Pointer Dereference / TOCTOU | `src/libbu/dirent.c, src/libbu/datetime.cpp:36-88; 173-197` | `FIXED` | bu_file_list called opendir(NULL) when path is NULL and suffered from heap buffer overflow if directory entries were added concurrently between passes; bu_utctime crashed on NULL vls_gmtime. |
| `SEC-0024` | **Sev 2** | Buffer Overflow / Underflow / NULL Pointer Dereference / Infinite Loop | `src/libbu/dir.c:69-75, 108, 169, 186, 248, 254, 395-405, 480-535, 656-665, 684-740` | `FIXED` | dir routines passed caller buffer len to fixed stack arrays risking stack buffer overflow, permitted underflow pointer reads in _bu_dir_join_path, crashed on NULL result in vdir, and risked infinite loop or crash in bu_mkdir/bu_dirclear on invalid paths. |
| `SEC-0025` | **Sev 3** | Heap Use-After-Free / Double-Free / Thread Safety | `src/libbu/dylib.c:38-40, 70-87, 105-120` | `FIXED` | bu_dlunload freed the global handles table without resetting handles to NULL or max_handles to 0, leading to dangling pointer use-after-free heap writes on subsequent bu_dlclose calls and double-free on subsequent bu_dlunload calls; routines lacked thread synchronization. |
| `SEC-0026` | **Sev 1** | Invalid File Descriptor / Parameter Validation | `src/libbu/fchmod.cpp:62-75` | `FIXED` | bu_fchmod passed unvalidated negative file descriptors to OS handles routines triggering CRT parameter validation errors or unexpected failures. |
| `SEC-0027` | **Sev 2** | Memory Leak / Resource Leak / NULL Pointer Dereference / Logic Flaw | `src/libbu/file.cpp:79-110, 126-166, 193, 227-240, 360, 603-640` | `FIXED` | bu_file_same leaked allocated realpath strings on match and crashed on NULL paths, file_compare_info had an assignment instead of comparison operator bug, bu_file_exists leaked file descriptors, and stream seek/tell functions crashed on NULL streams. |
| `SEC-0028` | **Sev 2** | NULL Pointer Dereference / Crasher | `src/libbu/fnmatch.c:308-311` | `FIXED` | bu_path_match immediately dereferenced pattern and string without NULL pointer checks, causing immediate SIGSEGV crashes on NULL inputs. |
| `SEC-0029` | **Sev 2** | Invalid Pointer Cast / NULL Pointer Dereference / Buffer Overflow | `src/libbu/gethostname.c:49, 81-84, 89` | `FIXED` | bu_gethostname passed constant (LPDWORD)MAXPATHLEN as pointer to GetComputerName causing invalid pointer crash, left procfs file pointer unchecked causing null stream crash, and lacked entry bounds validation. |
| `SEC-0030` | **Sev 2** | NULL Pointer Dereference / Buffer Over-read / Logic Flaw | `src/libbu/getopt.c:34-39, 48-56, 65-72, 84, 98` | `FIXED` | bu_getopt dereferenced NULL nargv elements and NULL *nargv in tell macro, read out of bounds on '\0' matched in ostr via strchr, and misidentified bare '-' as an option. |
| `SEC-0031` | **Sev 2** | NULL Pointer Dereference / Crasher | `src/libbu/glob.c:63, 84, 110, 161, 195, 240, 259, 286` | `FIXED` | bu_glob internal helpers and callbacks dereferenced NULL pattern, path, directory entry, or stat buffer pointers without checking for NULL. |
| `SEC-0032` | **Sev 2** | Out-of-bounds Array Access / Integer Underflow | `src/libbu/heap.c:136, 200-216, 255` | `FIXED` | bu_heap_get bypassed allocation size range check when DEBUG was not defined causing out-of-bounds heap bin indexing and underflow to SIZE_MAX when sz == 0, and lacked cpu id bounds clamping. |
| `SEC-0033` | **Sev 2** | Out-of-bounds Memory Write / NULL Pointer Dereference | `src/libbu/hist.c:52, 82-100, 117-128` | `FIXED` | bu_hist_range permitted negative or out-of-bounds 'a' indexing on NaN inputs causing heap write access violation, and bu_hist_init/bu_hist_pr crashed on NULL inputs. |
| `SEC-0034` | **Sev 2** | Signal Handler Leak / Out-of-bounds Array Access / Logic Flaw | `src/libbu/interrupt.c:78-83, 100-116, 137-163` | `FIXED` | interrupt_restore_signal never restored signal handlers when no signal was received leaving temporary handler active indefinitely, checked wrong handler pointer, and lacked bounds check on signum. |
| `SEC-0035` | **Sev 2** | Buffer Underflow / Integer Overflow UB / Out-of-bounds Read | `src/libbu/num.c:35-58, 87-90, 106` | `FIXED` | bu_num_print read out-of-bounds at tbl_start[-1] due to reversed condition ordering in trailing newline scan and crashed on empty strings; char_length had signed integer overflow UB on INT64_MIN and truncated 64-bit int to long. |
| `SEC-0036` | **Sev 2** | NULL Pointer Dereference / Integer Overflow | `src/libbu/log.c:148, 156, 204, 222, 335` | `FIXED` | bu_flog lacked null pointer and empty string checks on fmt and fp, causing NULL pointer dereference crashes in fwrite, and bu_log helper routines lacked null pointer guards. |
| `SEC-0037` | **Sev 2** | NULL Pointer Dereference / Integer Underflow | `src/libbu/mappedfile.c:360, 544-550, 577` | `FIXED` | bu_open_mapped_file_with_path crashed with SIGSEGV on NULL name or path, bu_free_mapped_files suffered from unsigned integer underflow on index removal, and bu_open_mapped_file allowed invalid file sizes. |
| `SEC-0038` | **Sev 2** | NULL Pointer Dereference / Memory Safety | `src/libbu/observer.c:36, 58, 110, 168, 194, 215` | `FIXED` | bu_observer_cmd and helper routines dereferenced NULL argv or NULL observer lists without validation, and bu_observer_free left dangling pointers in the observer list. |
| `SEC-0039` | **Sev 2** | Integer Underflow / NULL Pointer Dereference | `src/libbu/lex.c:42, 131, 155, 257, 268` | `FIXED` | lex_getone allowed negative *used integer underflow on number fallback, and bu_lex crashed on NULL token, rtstr, or NULL key strings. |
| `SEC-0040` | **Sev 2** | Buffer Underflow / Concurrency Hazard / Integer Bounds | `src/libbu/path_normalize.c:47, 86-88, 95` | `FIXED` | bu_path_normalize used non-thread-safe static buffer, lacked lower-bound check in backward component scan allowing pointer underflow before resolved buffer, and used unchecked signed pointer arithmetic for MAXPATHLEN bounds check. |
| `SEC-0041` | **Sev 2** | NULL Pointer Dereference / Undefined Bitshift / Out-of-bounds Read | `src/libbu/printb.c:36, 40-52` | `FIXED` | bu_vls_printb dereferenced NULL vls and bits strings, shifted signed 1L by negative or out-of-range bit numbers causing undefined behavior, and read out-of-bounds on strings ending after bit numbers. |
| `SEC-0042` | **Sev 2** | NULL Pointer Dereference / Path Truncation Flaw | `src/libbu/path.c:177-180, 211-218, 242-258` | `FIXED` | bu_path_to_argv dereferenced NULL ac pointer causing crash, and bu_path_component corrupted file paths by stripping directories when a dot appeared in directory names under BU_PATH_EXTLESS and BU_PATH_EXT. |
| `SEC-0043` | **Sev 2** | Crash / Unhandled Exception / NULL Function Pointer Dereference | `src/libbu/parallel_cpp11thread.cpp:28-51` | `FIXED` | parallel_cpp11thread crashed on NULL func callback and caused std::terminate() aborts if thread creation threw an exception while previous threads remained joinable. |
| `SEC-0044` | **Sev 2** | Concurrency Hazard / Unchecked Buffer Overflow | `src/libbu/progname.c:74-79, 140-143, 155-190` | `FIXED` | bu_argv0_full_path and bu_getprogname returned static non-thread-safe buffers and accessed bu_progname unsynchronized with concurrent bu_setprogname writes, and bu_argv0_full_path unchecked bu_getcwd return and appended with potential buffer overflow. |
| `SEC-0045` | **Sev 2** | NULL Pointer Dereference / Crash | `src/libbu/realpath.c:38-66` | `FIXED` | bu_file_realpath dereferenced NULL path pointer into realpath, GetFullPathName, and bu_strlcpy fallback causing immediate SIGSEGV crashes. |
| `SEC-0046` | **Sev 2** | Format String Flaw / Uninitialized Variable Memory Corruption | `src/libbu/scan.c:38, 58-60, 68-73, 89-93` | `FIXED` | bu_scan_fastf_t constructed dynamic scanf format strings from user delimiter allowing format string injection, and read uninitialized stack variable 'len' into offset when sscanf failed to reach %n specifier. |
| `SEC-0047` | **Sev 2** | Out-of-bounds Array Access / Concurrency Race Condition | `src/libbu/semaphore.c:260-286, 298-301, 340-343` | `FIXED` | bu_semaphore_acquire and bu_semaphore_release lacked upper bounds checks on semaphore index causing out-of-bounds array access and integer overflow on UINT_MAX, and bu_semaphore_free destroyed mutexes without synchronizing on bu_init_lock. |
| `SEC-0048` | **Sev 2** | NULL Pointer Dereference / Memory Safety | `src/libbu/sha1.c:53, 110, 126, 150` | `FIXED` | SHA1Transform, SHA1Init, SHA1Update, and SHA1Final lacked NULL pointer validation on context, data, digest, state, and buffer pointers causing SIGSEGV crashes. |
| `SEC-0049` | **Sev 1** | Integer Overflow UB / Logic Flaw | `src/libbu/snooze.cpp:35-42` | `FIXED` | bu_snooze lacked upper bound clamping on 64-bit useconds causing integer overflow when converted to nanoseconds internally in std::chrono durations. |
| `SEC-0050` | **Sev 2** | Memory Access Flaw / Divide by Zero | `src/libbu/sort.c:156-189, 381-402` | `FIXED` | bu_sort lacked checks for NULL a, NULL cmp, zero element size es == 0 (causing division by zero in introsort), and min(a, b) macro lacked parameter parentheses. |
| `SEC-0051` | **Sev 2** | Buffer Underflow / Pointer Overflow | `src/libbu/tbl.c:381-432` | `FIXED` | bu_tbl_printf suffered from backward scan buffer underflow dereferencing buf[-1] and unbounded runaway pointer increment last += 2 over-reading memory. |
| `SEC-0052` | **Sev 2** | Null Pointer Dereference / Memory Management | `src/libbu/tc.c:33-200, 428-456` | `FIXED` | bu_thrd_create dereferenced NULL thr pointer and lacked guard on NULL func; mutex and condition variable routines lacked NULL checks; used raw malloc/free. |
| `SEC-0053` | **Sev 2** | Buffer Over-read / Null Pointer Dereference | `src/libbu/tcllist.c:538, 554-565` | `FIXED` | bu_argv_from_tcl_list crashed on NULL arguments; _bu_tcl_copy_collapse passed strlen(src) instead of remaining count to _bu_tcl_parse_backslash, allowing read out of bounds past list element. |
| `SEC-0054` | **Sev 3** | Uninitialized Memory / Heap Corruption / Concurrency | `src/libbu/temp.c:85-151, 155-175` | `FIXED` | temp_add_to_list failed to copy initial_temp_files when reallocating from 8 to 16 elements, reading uninitialized memory and calling close() on garbage file descriptors at exit; global temp list lacked thread synchronization. |
| `SEC-0055` | **Sev 2** | Buffer Underflow / Null Pointer Dereference | `src/libbu/units.c:237-244, 256-270, 390-440` | `FIXED` | units_name_matches underflowed bu_vls_strlen - 1 to SIZE_MAX on empty strings/whitespace, causing out-of-bounds heap read; bu_units_conversion, bu_mm_value, and bu_mm_cvt crashed on NULL pointers. |
| `SEC-0056` | **Sev 1** | Null Pointer Dereference / Integer Overflow UB | `src/libbu/units_dehumanize.c:80-92, 131-137` | `FIXED` | bu_dehumanize_number crashed on NULL str or size and performed signed multiplication without pre-overflow bounds check. |
| `SEC-0057` | **Sev 2** | Null Pointer Dereference / Integer Overflow UB | `src/libbu/units_humanize.c:74-82, 128-133` | `FIXED` | bu_humanize_number dereferenced buf[0] before validating buf != NULL, causing crash when called with a NULL buffer; negation of INT64_MIN caused signed integer overflow UB. |
| `SEC-0058` | **Sev 2** | Buffer Overflow / Null Pointer Dereference / Logic Flaw | `src/libbu/uuid.c:56, 89, 118-124, 134-161` | `FIXED` | bu_uuid_decode suffered critical 16-byte buffer overflow writing 32 bytes to a 16-byte array on non-adjacent hex strings; bu_uuid_create crashed on NULL uuid and corrupted UUIDv5 bits with 0x5F instead of 0x50; bu_uuid_encode crashed on NULL pointers. |
| `SEC-0059` | **Sev 2** | File Descriptor Leak / Uninitialized Data / Null Dereference | `src/libbu/vfont.c:42, 60-100, 140-150, 184-196, 201-207` | `FIXED` | vfont_get passed potential NULL from bu_dir() to snprintf %s, left 256th dispatch entry uninitialized (i < 255), leaked file descriptors on read failures in get_font, and vfont_free crashed on NULL pointer. |
| `SEC-0060` | **Sev 2** | Integer Overflow / Null Pointer Dereference | `src/libbu/vlb.c:34-160` | `FIXED` | bu_vlb_write lacked integer overflow protection on capacity addition and crashed when given NULL start; bu_vlb_init, bu_vlb_initialize, bu_vlb_free, and bu_vlb_print crashed on NULL pointers. |
| `SEC-0061` | **Sev 2** | Undefined Behavior / Memory Leak / Null Pointer Dereference | `src/libbu/cache.cpp:110-140, 220-250, 480-510` | `FIXED` | bu_cache_close freed bu_cache_impl containing std::mutex with BU_PUT instead of delete causing destructor bypass and undefined behavior; bu_cache_keys leaked keystr memory; bu_cache_write_commit underflowed mv_size on negative lengths; NULL txn crashed transactions. |
| `SEC-0062` | **Sev 2** | Null Pointer Dereference / Float Validity | `src/libbu/color.cpp:52-95, 120-160` | `FIXED` | bu_color_from_str, bu_rgb_to_hsv, and bu_hsv_to_rgb dereferenced NULL pointers unconditionally without validation; bu_color_from_rgb_floats lacked bounds and finite checks. |
| `SEC-0063` | **Sev 2** | Buffer Overflow / Unsigned Underflow | `src/libbu/convert.c:120-160, 200-240` | `FIXED` | bu_cv_fmt_cookie decremented buflen below zero on small buffers (1-2 bytes), wrapping to SIZE_MAX and writing out of bounds; bu_cv_w_cookie and byte swap routines crashed on NULL or empty input. |
| `SEC-0064` | **Sev 2** | File Descriptor Leak / Null Pointer Dereference | `src/libbu/crashreport.c:110-140, 210-230` | `FIXED` | bu_crashreport leaked file descriptor when ferror(fp) was true, and formatted potential NULL return from ctime(). |
| `SEC-0065` | **Sev 2** | Memory Leak / Null Pointer Dereference / Unchecked Input | `src/libbu/env.c:140-190, 240-270` | `FIXED` | editor_not_compatible leaked component vls and dereferenced NULL candidate; editor_file_check lacked NULL checks; bu_setenv lacked POSIX environment variable name validation. |
| `SEC-0066` | **Sev 2** | Null Pointer Dereference / Buffer Over-read / Type Mismatch | `src/libbu/opt.c:215-260, 480-520` | `FIXED` | opt_describe_internal_ascii crashed when arg_helpstr or help_string was NULL; docbook describe had curr vs d typo; opt_process over-read empty candidate; bu_opt_color had unsigned char type mismatch. |
| `SEC-0067` | **Sev 2** | Out-of-Bounds Memory Access | `src/libbu/parallel.c:280-310` | `FIXED` | PARALLEL_PUT accessed mapping[id] without validating 0 <= id < MAX_PSW*MAX_PSW, causing out-of-bounds memory corruption. |
| `SEC-0068` | **Sev 2** | Null Pointer Dereference / Memory Leak / Buffer Overflow | `src/libbu/process.c:350-380, 520-560, 1220-1250` | `FIXED` | bu_process_read unconditionally dereferenced *count = ... even if caller passed NULL; bu_process_exec leaked heap av on every execution; bu_interactive passed unchecked fd to FD_SET causing buffer overflow. |
| `SEC-0069` | **Sev 2** | Null Pointer Dereference | `src/libbu/uce-dirent.h:348-354, 578-583, 608-612` | `FIXED` | opendir, rewinddir, and _initdir relied solely on assert() in debug mode, crashing with NULL pointer dereference in release builds when given NULL dirname or DIR stream. |
| `SEC-0070` | **Sev 2** | Unhandled Exception / Denial of Service Crash | `src/libbu/vls_incr.cpp:136-155, 180-188` | `FIXED` | bu_vls_incr accessed ivar.str(1) and nvar.str(1) without verifying match group count, throwing std::out_of_range which bypassed catch (const std::regex_error&) and crashed the application. |
| `SEC-0071` | **Sev 2** | Null Pointer Dereference | `src/libbu/whereami.c:95-135` | `FIXED` | _bu_getExecutablePath dereferenced pname[0] without checking if _bu_progname_raw() returned NULL, and copied memory into out when out == NULL. |
| `SEC-0072` | **Sev 2** | Infinite Loop / Denial of Service | `src/libbu/whereis.c:135-155` | `FIXED` | bu_whereis bypassed pointer advancement on dirlen > MAXPATHLEN-2 with continue;, causing an infinite loop hanging the process at 100% CPU. |
| `SEC-0073` | **Sev 2** | Infinite Loop / Memory Leak | `src/libbu/which.c:91-115` | `FIXED` | which_path suffered infinite loop on oversized directories and empty matches, and leaked initial_path every time an executable was not found. |
| `SEC-0074` | **Sev 2** | Null Pointer Dereference | `src/libbu/xdr.c:31-88` | `FIXED` | bu_pshort, bu_gshort, bu_plong, bu_glong, and bu_plonglong dereferenced msgp without validating msgp != NULL, causing immediate segmentation fault. |
| `SEC-0075` | **Sev 2** | Memory Leak / Null Pointer Dereference | `src/libbu/tests/dylib/dylib.c:35-85` | `FIXED` | bu_file_list allocated filenames and bu_vls plugin_pattern allocated memory which was never freed; dylib_close_plugins dereferenced NULL pointer when called with plugins == NULL. |
| `SEC-0076` | **Sev 2** | Null Pointer Dereference | `src/libbu/tests/dylib/plugin_1.cpp:22-28` | `FIXED` | calc dereferenced *result without checking if *result == NULL, causing crash on NULL buffer pointer. |
| `SEC-0077` | **Sev 2** | Null Pointer Dereference | `src/libbu/tests/dylib/plugin_2.cpp:22-28` | `FIXED` | calc dereferenced *result without checking if *result == NULL, causing crash on NULL buffer pointer. |
| `SEC-0078` | **Sev 2** | Stack Buffer Overflow / Format String UB | `src/libbu/tests/test_dirname.c:58-82` | `FIXED` | bu_strlcpy used strlen(input)+1 instead of destination buffer size, causing stack buffer overflow on strings exceeding 1000 bytes; passed NULL to printf format string. |
| `SEC-0079` | **Sev 1** | Undefined Behavior / Crash | `src/libbu/tests/test_encode.c:70-90` | `FIXED` | Passed NULL str directly to %*s and %s in printf when testing test_encode(NULL), causing undefined behavior and potential crash. |
| `SEC-0080` | **Sev 1** | Memory Leak / Unchecked Return | `src/libbu/tests/test_datetime.c:243-345` | `FIXED` | result VLS was allocated and never freed across multiple date/time tests, leaking memory; sscanf return value was unvalidated. |
| `SEC-0081` | **Sev 2** | Null Pointer Dereference / Crash | `src/libbu/tests/test_basename.c:55-65, 130-140` | `FIXED` | get_system_output(NULL) called basename(NULL) which causes segmentation fault on glibc implementations; passed NULL input to bu_log format strings. |
| `SEC-0082` | **Sev 2** | Heap Buffer Overflow / Off-by-one | `src/libbu/tests/test_progname.c:63-71, 107-115, 149-157, 189-197` | `FIXED` | Allocated strlen(path) bytes via bu_calloc without space for null terminator before calling bu_strlcpy, causing buffer overflow; leaked allocated buffer. |
| `SEC-0083` | **Sev 2** | Use-After-Free / Memory Leak / Float Comparison | `src/libbu/tests/test_mappedfile.c:70-74, 385-397` | `FIXED` | Accessed mfp->buf after closing mapped file via bu_close_mapped_file, performed raw float equality on double num, and leaked fname VLS. |
| `SEC-0084` | **Sev 2** | Process Resource Leak / NULL Pointer Dereference | `src/libbu/tests/test_process.c:172-184, 521-536, 680-705` | `FIXED` | Failed to wait on or close process handles on error paths leaking processes, and attempted to close/read NULL file handles when fopen failed. |
| `SEC-0085` | **Sev 2** | Array Out-of-Bounds / Crash | `src/libbu/tests/test_parallel.c:70-80, 115-125` | `FIXED` | Indexed counter array with cpu ID up to ncpu in tally without bound check, and omitted NULL data check in parallel recursion callback. |
| `SEC-0086` | **Sev 2** | Out-of-Bounds Memory Read / Pointer Type-Punning | `src/libbu/tests/test_opt.c:58-64, 492-498` | `FIXED` | Read av[-1] when av was decremented without checking bounds, and type-punned unsigned int array to unsigned char pointer violating strict aliasing. |
| `SEC-0087` | **Sev 2** | NULL Pointer Dereference / Memory Leak | `src/libbu/tests/test_hook.c:45-55, 120-145` | `FIXED` | Callback dereferenced clientdata pointer without NULL check and hook list structures were allocated and never freed across tests. |
| `SEC-0088` | **Sev 2** | Out-of-Bounds Read / NULL Pointer Dereference | `src/libbu/tests/test_glob.c:95-110, 195-205` | `FIXED` | Read buffer out of bounds when checking trailing slash on short paths and called glob operations without validating NULL context pointers. |
| `SEC-0089` | **Sev 1** | Resource Leak / Unchecked Return Value | `src/libbu/tests/test_env.c:80-92, 128-135` | `FIXED` | Did not check fopen return value before writing and failed to validate bu_strdup return value leaking memory on error. |
| `SEC-0090` | **Sev 1** | NULL Pointer Dereference / Format Flaw | `src/libbu/tests/test_escape.c:72-85, 140-150` | `FIXED` | Passed unverified pointer to %s format specifier in printf calls and omitted explicit return code in test helper. |
| `SEC-0091` | **Sev 1** | Format String Flaw / NULL Dereference | `src/libbu/tests/test_path_component.c:62-75` | `FIXED` | Passed NULL pointer to %24s in bu_log formatting calls, which crashes strict C runtime environments. |
| `SEC-0092` | **Sev 1** | Memory Leak | `src/libbu/tests/test_humanize_number.c:115-135` | `FIXED` | Allocated string representations of flags and scale inside loop iterations without freeing them, causing repeated memory leaks. |
| `SEC-0093` | **Sev 1** | Memory Leak / NULL Pointer Dereference | `src/libbu/tests/test_file.c:70-75, 233-241` | `FIXED` | cleanup() function leaked fname VLS allocation on return, and realpath test did not check if rpath was NULL before comparing. |
| `SEC-0094` | **Sev 1** | Unchecked Return Value / Array Bounds | `src/libbu/tests/test_file_mime.c:45-56` | `FIXED` | Did not check sscanf return count allowing uninitialized integer use, and allowed negative context index causing out-of-bounds array access. |
| `SEC-0095` | **Sev 1** | NULL Pointer Dereference | `src/libbu/tests/test_observer.c:45-55` | `FIXED` | Missing NULL check on observer callback event structure pointer before dereferencing. |
| `SEC-0096` | **Sev 1** | NULL Pointer Dereference | `src/libbu/tests/test_parallel_concurrency.cpp:40-48` | `FIXED` | Thread worker function dereferenced user data pointer without checking for NULL. |
| `SEC-0097` | **Sev 1** | Resource Leak / Empty String Check | `src/libbu/tests/test_path_match.cpp:38-52` | `FIXED` | ifstream remained open without explicit close and empty lines were parsed without validation. |
| `SEC-0098` | **Sev 2** | POSIX/Darwin API Resolution / Failure to Canonicalize | `src/libbu/realpath.c:20-27` | `FIXED` | Under strict POSIX/XOPEN flags on macOS without _DARWIN_C_SOURCE, realpath bound to legacy conformance symbol which failed with EPERM, breaking path canonicalization across libbu. |
| `SEC-0099` | **Sev 2** | Memory Management / Resource Leak | `src/libbu/tests/test_realpath.c:47-79` | `FIXED` | Heap memory leak of allocated path string, unchecked sscanf return value, and potential NULL printf argument in test_realpath.c. |
| `SEC-0100` | **Sev 1** | Memory Access / NULL Pointer Dereference | `src/libbu/tests/test_semaphore.c:48-62` | `FIXED` | Unchecked callback argument pargs in multi-threaded function increment_thread could lead to NULL pointer dereference. |
| `SEC-0101` | **Sev 2** | Memory Access / Out-of-bounds Read | `src/libbu/tests/test_semaphore_registry.cpp:60-63` | `FIXED` | Unguarded argv[0] dereference passed to bu_setprogname when argv could be NULL or empty. |
| `SEC-0102` | **Sev 2** | Error Handling / Logic Error | `src/libbu/tests/test_semchk.cpp:40-75` | `FIXED` | Undeclared identifier av[0] passed to bu_setprogname, unhandled file stream failure in process_file, and unguarded argv dereference. |
| `SEC-0103` | **Sev 2** | Memory Access / Out-of-bounds Read | `src/libbu/tests/test_snooze.c:44-48` | `FIXED` | Unguarded argv[0] dereference passed to bu_setprogname when argv could be NULL or empty. |
| `SEC-0104` | **Sev 1** | Integer Wraparound / Underflow | `src/libbu/tests/test_sort.c:45-95` | `FIXED` | Integer subtraction bug in comparison function comp_1 (*(unsigned int *)num1 - *(unsigned int *)num2) caused unsigned underflow and reversed sort order; comp_4 lacked NULL pointer checks; sscanf return was unchecked. |
| `SEC-0105` | **Sev 2** | Memory Access / Out-of-bounds Read | `src/libbu/tests/test_static_init.cpp:44-48` | `FIXED` | Unguarded argv[0] dereference passed to bu_setprogname when argv could be NULL or empty. |
| `SEC-0106` | **Sev 2** | Memory Management / Resource Leak | `src/libbu/tests/test_str.c:115-180` | `FIXED` | Memory leaks of allocated dst buffer across test_bu_strlcatm, test_bu_strlcpym, and test_bu_strdupm, unchecked sscanf, and negative integer cast to size_t. |
| `SEC-0107` | **Sev 2** | Format String / NULL Pointer Dereference | `src/libbu/tests/test_str_isprint.c:70-98` | `FIXED` | Passing NULL string pointer to %10s format specifier in bu_log caused undefined behavior / crash; unchecked sscanf. |
| `SEC-0108` | **Sev 2** | Uninitialized Memory Access | `src/libbu/tests/test_subprocess.cpp:45-55` | `FIXED` | Stack buffer char line[25] was uninitialized before std::cin.get(line, 25), leading to uninitialized memory read if stream was empty or in error state. |
| `SEC-0109` | **Sev 1** | Concurrency / Out-of-bounds Memory Access | `src/libbu/tests/test_temp_filename.c:45-78` | `FIXED` | Passed &names (char ***) instead of names (char **) to bu_parallel, causing worker threads with cpu > 1 to read and write invalid stack memory outside the array; names[cpu - 1] caused underflow on cpu = 0; off-by-one heap allocation len + 1 caused string truncation. |
| `SEC-0110` | **Sev 2** | Memory Access / NULL Pointer Dereference | `src/libbu/tests/test_units.c:48-58` | `FIXED` | Missing NULL pointer validation for csv and token in helper function units_csv_has_token. |
| `SEC-0111` | **Sev 2** | Logic Error / False-Pass Test Suite Flaw | `src/libbu/tests/test_uuid.c:55-90` | `FIXED` | Typo in expected UUID string paired with inverted condition !bu_strcmp caused test suite to return 0 (PASS) on mismatch while returning 1 (FAIL) if it matched; missing error logging. |
| `SEC-0112` | **Sev 2** | Memory Access / Out-of-bounds Read | `src/libbu/tests/test_version.c:35-48` | `FIXED` | Missing common.h include and unguarded av[0] dereference passed to bu_setprogname. |
| `SEC-0113` | **Sev 2** | Memory Access / Out-of-bounds Read | `src/libbu/tests/test_vlb.c:42-46` | `FIXED` | Unguarded argv[0] dereference passed to bu_setprogname when argv could be NULL or empty. |
| `SEC-0114` | **Sev 2** | Memory Management / Resource Leak | `src/libbu/tests/test_vls.c:115-680` | `FIXED` | Heap memory leaks across 13 test functions using bu_vls_strdup solely for comparison without freeing the returned string; out-of-bounds argv access in test_bu_vls_substr; unchecked sscanf. |
| `SEC-0115` | **Sev 2** | Input Validation / Integer Parsing | `src/libbu/tests/test_vls_incr.c:58-85` | `FIXED` | Unguarded argv dereference and unvalidated strtol conversion where invalid characters or out-of-range counts were unhandled. |
| `SEC-0116` | **Sev 2** | Memory Management / Resource Leak | `src/libbu/tests/test_vls_incr_uniq.cpp:50-93` | `FIXED` | Memory leak of struct bu_vls name on exit; heap allocation leak of std::set on early exit; potential NULL pointer dereference in StrCmp. |
| `SEC-0117` | **Sev 2** | Memory Access / Out-of-bounds Read | `src/libbu/tests/test_vls_simplify.c:56-85` | `FIXED` | Unguarded argv dereference before bu_setprogname, potential NULL pointer dereference in bu_log format string. |
| `SEC-0118` | **Sev 1** | Buffer Overflow / Stack Vulnerability | `src/libbu/tests/test_vls_vprintf.c:55-175` | `FIXED` | Unbounded vsprintf into fixed 1024-byte stack buffer buffer[1024] created an unconstrained stack buffer overflow vulnerability; unchecked sscanf and unguarded argv. |
| `SEC-0119` | **Sev 1** | Numerical Stability / NaN Generation | `src/libbn/anim.c:304-332` | `FIXED` | anim_mat2quat negative square root NaN generation caused by floating-point rounding when testing !ZERO(square); passed minute negative numbers into sqrt() resulting in NaN quaternion components. |
| `SEC-0120` | **Sev 1** | Buffer Overflow | `src/libbn/anim.c:153-176` | `FIXED` | anim_mat_printf stack buffer overflow risk with fixed 128-byte character buffer when formatting 16 double-precision float values with custom prefix strings. |
| `SEC-0121` | **Sev 2** | Null Pointer Dereference | `src/libbn/anim.c:180-680` | `FIXED` | Missing NULL pointer checks across anim_mat_print, anim_dir2mat, anim_dirn2mat, anim_steer_mat, anim_v_permute, anim_v_unpermute, anim_tran, anim_mat2zyx, and anim_mat2ypr. |
| `SEC-0122` | **Sev 2** | Null Pointer Dereference | `src/libbn/complex.c:54-85` | `FIXED` | bn_cx_div and bn_cx_sqrt dereferenced destination and operand pointers without validating against NULL. |
| `SEC-0123` | **Sev 1** | Stack Buffer Overflow | `src/libbn/mat.c:1544-1578` | `FIXED` | bn_opt_mat looped up to argc when argc > 15, writing into fixed 16-element stack array fastf_t mtmp[16], overflowing the stack frame on excess arguments. |
| `SEC-0124` | **Sev 1** | Out-of-bounds Read | `src/libbn/mat.c:1500-1540` | `FIXED` | bn_opt_mat out-of-bounds read when checking trailing closing brace with bu_vls_addr(&str)[bu_vls_strlen(&str)-1] without verifying bu_vls_strlen > 0. |
| `SEC-0125` | **Sev 1** | Buffer Overflow | `src/libbn/mat.c:80-140` | `FIXED` | bn_mat_print_guts wrote into fixed obuf array without checking remaining capacity against bu_vls length, leading to potential buffer overflow. |
| `SEC-0126` | **Sev 1** | Concurrency Data Race / Logic Bug | `src/libbn/mat.c:1000-1060` | `FIXED` | bn_wrt_point_direc declared transformation matrices as static local variables, causing data corruption across concurrent raytracing threads, and had inverted translation vector origin_to_pt. |
| `SEC-0127` | **Sev 2** | Division by Zero | `src/libbn/mat.c:1100-1200` | `FIXED` | persp_mat and deering_persp_mat performed division by near_plane, right-left, and bottom-top without checking for zero denominators. |
| `SEC-0128` | **Sev 2** | Memory Access / Use-After-Free Prevention | `src/libbn/msr.c:120-135` | `FIXED` | bn_gauss_free lacked NULL pointer guard and failed to clear magic identifier before freeing heap structure. |
| `SEC-0129` | **Sev 3** | Dead Code | `src/libbn/mt19937ar.c:170-257` | `FIXED` | Dead #if 0 code block with test main function embedded within shared library source file. |
| `SEC-0130` | **Sev 1** | Heap Memory Corruption | `src/libbn/multipoly.c:80-101` | `FIXED` | bn_multipoly_grow realloc corruption passing outer pointer array P->cf instead of row array P->cf[i] with sizeof(double*) instead of sizeof(double), overwriting row pointers with pointer array address. |
| `SEC-0131` | **Sev 1** | Out-of-bounds Access | `src/libbn/multipoly.c:80-101` | `FIXED` | bn_multipoly_grow failed to update P->dgrs and P->dgrt dimensions after growing, leaving stale dimensions that caused subsequent operations to access out of bounds or fail to allocate. |
| `SEC-0132` | **Sev 1** | Out-of-bounds Read | `src/libbn/multipoly.c:120-132` | `FIXED` | bn_multipoly_add out-of-bounds read when polynomials had differing degrees, reading beyond allocated row bounds, and dimension calculation typo Max(p1->dgrt, p2->dgrs) instead of p2->dgrt. |
| `SEC-0133` | **Sev 1** | Logic / Numerical Error | `src/libbn/multipoly.c:140-155` | `FIXED` | bn_multipoly_mul overwrote accumulated product terms using = instead of +=, destroying earlier polynomial term contributions sharing identical combined powers. |
| `SEC-0134` | **Sev 1** | Resource Leak | `src/libbn/multipoly.c:70-75` | `FIXED` | Missing bn_multipoly_free function causing 100% memory leak of all 2D dynamically allocated bivariate polynomial matrices. |
| `SEC-0135` | **Sev 2** | Null Pointer Dereference / Dead Code | `src/libbn/noise.c:248-430` | `FIXED` | Missing NULL pointer validation on point and result in bn_noise_perlin, bn_noise_vec, and spectral functions; 22 lines of dead #if 0 code in bn_noise_mf. |
| `SEC-0136` | **Sev 1** | Out-of-bounds Access / Allocation Flaw | `src/libbn/noise.c:460-545` | `FIXED` | build_spec_tbl and find_spec_wgt allowed negative or zero octaves, leading to invalid memory allocation size (int)(octaves+1) <= 0 and out-of-bounds weight table indexing. |
| `SEC-0137` | **Sev 1** | Resource Leak / Typo | `src/libbn/numgen.c:52-72` | `FIXED` | bn_numgen_create allocated generator on the heap before validating dim > 0 and seed < 0, leaking the allocated structure on early return; nperiodic typo in flag check. |
| `SEC-0138` | **Sev 1** | Memory Corruption / Pointer Aliasing | `src/libbn/poly.c:50-168` | `FIXED` | In-place pointer aliasing in bn_poly_mul, bn_poly_add, and bn_poly_sub when destination pointer equaled one of the operand polynomials caused coefficients to be overwritten before being read. |
| `SEC-0139` | **Sev 1** | Buffer Overflow / Unsigned Underflow | `src/libbn/poly.c:170-197` | `FIXED` | bn_poly_synthetic_division assigned -1 to unsigned size_t quo->dgr when divisor degree exceeded dividend degree, underflowing to SIZE_MAX and triggering an unbounded loop writing past the 7-element coefficient array. |
| `SEC-0140` | **Sev 2** | Null Pointer Dereference | `src/libbn/poly.c:200-488` | `FIXED` | Missing NULL pointer checks in bn_poly_quadratic_roots, bn_poly_cubic_roots, bn_poly_quartic_roots, bn_pr_poly, and bn_pr_roots. |
| `SEC-0141` | **Sev 2** | Memory Access Violation / Numerical Robustness | `src/libbn/qmath.c:84-331` | `FIXED` | In quat_mat2quat, negative diagonal difference caused NaN and division-by-zero on non-orthogonal matrices; acos() called on unconstrained dot products causing NaN domain errors; missing NULL pointer checks across quaternion math operations. |
| `SEC-0142` | **Sev 2** | Denial of Service / Memory Access Violation | `src/libbn/randsph.c:50-120` | `FIXED` | Potential infinite loop in _bn_unit_sph_sample when bn_randmt() yields exactly 0.5; possible NaN from sqrt(1 - S) when S slightly exceeds 1 due to floating-point rounding; unchecked return value from bn_sobol_next in _bn_unit_sph_sample_sobol. |
| `SEC-0143` | **Sev 2** | Null Pointer Dereference | `src/libbn/sobol.c:148-325` | `FIXED` | bn_sobol_create did not check the return status of sobol_init, creating partially initialized structures with NULL member pointers when sdim was invalid; rightzero32 had undefined behavior if invoked with 0xffffffffU; _sobol_urand and bn_sobol_next lacked NULL pointer checks. |
| `SEC-0144` | **Sev 1** | Buffer Overflow / Resource Leak | `src/libbn/sphmap.c:35-315` | `FIXED` | Heap buffer overflow in bn_spm_pix_load which allocated nx*nx*3 bytes instead of nx*ny*3 while reading nx*ny*3 bytes; permanent memory leak of buffer on fread failure; out-of-bounds array reads and writes in bn_spm_read, bn_spm_write, and bn_spm_get when u or v >= 1.0 or negative; bn_spm_free checked magic number before checking for NULL pointer. |
| `SEC-0145` | **Sev 2** | Uninitialized Memory Read | `src/libbn/str.c:42-177` | `FIXED` | bn_decode_mat, bn_decode_quat, and bn_decode_vect copied uninitialized stack memory into destination buffers when sscanf matched fewer items than required or failed; missing NULL pointer checks across decode and encode routines. |
| `SEC-0146` | **Sev 2** | Null Pointer Dereference / Resource Leak | `src/libbn/tabdata.c:35-1215` | `FIXED` | Second pass fopen in bn_read_table_and_tabdata was unchecked, causing NULL pointer dereference in bu_fgets if reopening failed; memory leak of allocated table/tabdata on read failure; unsigned integer underflow in bn_table_delete_sample_pnts when i > j; NULL pointer dereference in bn_table_free and bn_tabdata_free. |
| `SEC-0147` | **Sev 2** | Undefined Behavior | `src/libbn/ulp.c:118-204` | `FIXED` | Referenced non-existent union member d in union flt_bits (val.d += 1 and minVal.d = 1 << 23); undefined behavior in signed 32-bit shift 1 << 52 when setting 64-bit exponent. |
| `SEC-0148` | **Sev 1** | Resource Leak | `src/libbn/wavelet.c:170-625` | `FIXED` | Permanent memory leaks in make_wlt_haar_2d_decompose, make_wlt_haar_2d_reconstruct, and make_wlt_haar_2d_decompose2 when tbuffer/tbuf is NULL because allocated buffers were never freed; missing NULL buffer and zero dimension validation across wavelet routines. |
| `SEC-0149` | **Sev 1** | Null Pointer Dereference | `src/libbn/tests/test_util.c:48-265` | `FIXED` | Missing NULL pointer checks in test helper and test dispatch functions (mat_close, vect_close, hvect_close, finite_vec, orthonormal_rotation, normalize_quat, make_table, make_tabdata, bn_api_single, and bn_api_dispatch) when called with NULL pointers or empty argument lists. |
| `SEC-0150` | **Sev 1** | Out-of-Bounds Read | `src/libbg/pca.cpp:55-80` | `FIXED` | bg_pca computed Thin U on a 3xnpnts matrix using JacobiSVD. When npnts < 3, Thin U has fewer than 3 columns, causing out-of-bounds column reads and assertions when indexing svd.matrixU()(row, 1) and svd.matrixU()(row, 2). |
| `SEC-0151` | **Sev 2** | Logic / Format String Flaw | `src/libbg/pointgen.c:35-60` | `FIXED` | bg_sph_sample referenced undeclared variable sample instead of pnts[i], used %d format specifier for size_t integers in bu_log, checked sample instead of center for translation offset, and lacked defensive NULL pointer validation on pnts. |
| `SEC-0152` | **Sev 2** | NULL Pointer Dereference | `src/libbg/sat.cpp:44-175` | `FIXED` | bg_sat_line_aabb, bg_sat_line_obb, bg_sat_tri_aabb, bg_sat_tri_obb, bg_sat_aabb_obb, and bg_sat_obb_obb called v3_from_array and axis_and_extent_from_vect without verifying point_t and vect_t pointer arguments, leading to null pointer dereferences. |
| `SEC-0153` | **Sev 2** | NULL Pointer Dereference | `src/libbg/lseg_lseg.cpp:37-45` | `FIXED` | bg_distsq_lseg3_lseg3 unpacked input point arrays without null pointer validation and lacked extern C linkage in its C++ definition. |
| `SEC-0154` | **Sev 2** | NULL Pointer Dereference | `src/libbg/lseg_pt.cpp:36-45` | `FIXED` | bg_distsq_lseg3_pt indexed point coordinates without checking for null pointers. |
| `SEC-0155` | **Sev 2** | NULL Pointer Dereference / Domain Error | `src/libbg/tri_pt.cpp:37-60` | `FIXED` | bg_tri_closest_pt indexed triangle vertices and sample points without null pointer checks and computed std::sqrt(result.sqrDistance) without non-negative clamping. |
| `SEC-0156` | **Sev 2** | NULL Pointer Dereference | `src/libbg/tri_ray.cpp:43-70` | `FIXED` | intersect_triangle and vec3_from_array dereferenced point and vector arguments without checking for null pointers. |
| `SEC-0157` | **Sev 2** | Divide by Zero / NULL Pointer Dereference | `src/libbg/util.c:50-145` | `FIXED` | coplanar_2d_coord_sys scaled origin by 1.0/n without verifying n >= 3, leading to division by zero if n <= 0 and undefined behavior for degenerate point sets. coplanar_3d_to_2d and coplanar_2d_to_3d lacked null checks on pointer parameters and count validation. |
| `SEC-0158` | **Sev 3** | NULL Pointer Dereference / Deprecated Keyword | `src/libbg/aabb_ray.c:31-143` | `FIXED` | bg_ray_invdir lacked null check on dir; bg_isect_aabb_ray lacked null checks on invdir, aabb_min, aabb_max, and opt; wrote into *r_min and *r_max without verifying destination pointers were non-null; used deprecated register storage class keywords. |
| `SEC-0159` | **Sev 3** | Validation Flaw | `src/libbg/obr.cpp:95-100` | `FIXED` | bg_3d_coplanar_obr accepted point count < 3 even though a 3D coplanar coordinate system requires at least 3 points to determine a plane normal. |
| `SEC-0160` | **Sev 1** | Buffer Overflow / Stack Buffer Corruption & Divide by Zero | `src/libbg/polygon.c:144-205` | `FIXED` | Operator precedence flaw in bg_3d_polygon_centroid where *cent[1] and *cent[2] evaluate to *(cent[1]) and *(cent[2]), writing 24 and 48 bytes out of bounds past caller's point_t stack/heap memory; uninitialized accumulation; division by zero when polygon is perpendicular to XY or XZ projection planes. |
| `SEC-0161` | **Sev 2** | Out-of-bounds Read / Memory Leak | `src/libbg/chull.c:50-185` | `FIXED` | Out-of-bounds read and negative deque index in bg_polyline_2d_chull when vertex count n < 3; permanent memory leaks in bg_3d_coplanar_chull of points_tmp and overwritten hull_2d pointer; missing NULL checks. |
| `SEC-0162` | **Sev 2** | Out-of-bounds Read / Memory Leak | `src/libbg/chull2.cpp:50-230` | `FIXED` | Out-of-bounds read and negative deque index in bg_polyline_2d_chull2 for n < 3; permanent memory leak of polyline in bg_2d_chull2; missing NULL parameter checks. |
| `SEC-0163` | **Sev 2** | Divide by Zero / Thread-Safety Race Condition | `src/libbg/clip.c:60-140` | `FIXED` | Division by zero in bg_lseg_clip for vertical or horizontal line segments; thread-safety race condition in bg_ray_vclip due to static local variables diff, sv, st, mindist, maxdist causing concurrent raytracing thread conflicts. |
| `SEC-0164` | **Sev 2** | Memory Leak / Unchecked Return Value | `src/libbg/polygon_triangulate.cpp:873-970` | `FIXED` | Unchecked return value of fopen in bg_tri_plot_2d causing NULL pointer dereference; permanent memory leaks of holes_array container and tri_out_pts in bg_polygon_triangulate; missing validation on polygon contour points. |
| `SEC-0165` | **Sev 2** | NULL Pointer Dereference | `src/libbg/tri_tri.c:180-715` | `FIXED` | bg_tri_tri_isect_with_line unconditionally wrote to *coplanar, *isectpt1, and *isectpt2 without verifying destination pointers were non-null; missing NULL vertex validation across all triangle-triangle intersection routines. |
| `SEC-0166` | **Sev 2** | NULL Pointer Dereference / Divide by Zero | `src/libbg/polygon_op.cpp:45-470` | `FIXED` | Unchecked NULL pointer dereferences in bg_find_polygon_area, bg_polygons_overlap, load_polygon, and clipping routines; potential division by zero on zero scale factor. |
| `SEC-0167` | **Sev 2** | Integer Underflow / Out-of-bounds Read | `src/libbg/polygon_point_in.c:40-51` | `FIXED` | bg_pnt_in_polygon underflows j = nvert - 1 when nvert == 0 and allows nvert < 3 which is geometrically invalid for polygons; missing NULL pointer checks on pnts and test. |
| `SEC-0168` | **Sev 3** | Out-of-bounds Read / Degenerate Buffer Access | `src/libbg/chull3d.cpp:115-210` | `FIXED` | bg_3d_chull and bg_3d_chull2 did not check for empty vertexBuffer or undersized indexBuffer before allocating and indexing; out-of-bounds access in bg_3d_chull2 if face index exceeded vmap size. |
| `SEC-0169` | **Sev 3** | Spatial Partitioning Stride Corruption / Resource Leak | `src/libbg/vert_tree.c:140-180, 260-430` | `FIXED` | Stride indexing corruption in bg_vert_tree_add_w_norm where vertex coordinate data was accessed with stride 3 instead of stride 6 (ptr->vleaf.index * 3 vs index * 6), causing spatial partitioning corruption and reading wrong coordinate/normal data; permanent memory leaks in bg_vert_tree_destroy which failed to free the_array when the_tree was NULL and failed to free the bg_vert_tree container structure. |
| `SEC-0170` | **Sev 2** | Truncated Integer Division / Overflow | `src/libbg/decimate.cpp:55-70, 160-175` | `FIXED` | Truncated integer division in trimesh_decimate_simple (1/cbin.size()) where 1 / cbin.size() evaluated to integer 0 for any bin with >1 point, collapsing the point accumulator to world origin [0,0,0] instead of the bin centroid; missing n_ifaces integer overflow guard. |
| `SEC-0171` | **Sev 2** | Out-of-bounds Read / Unhandled Exception | `src/libbg/delaunator.hpp:40-55, 240-305` | `FIXED` | Out-of-bounds vector read in sum() on empty vector (indexing x[0]); out-of-bounds vector read and crash in Delaunator constructor when n < 3 or when seed points i0 or i1 are uninitialized (accessing coords[2 * INVALID_INDEX]); unhandled exception in bg_polygon_triangulate when Delaunator encounters degenerate collinear input. |
| `SEC-0172` | **Sev 2** | Unchecked Error Code / Parameter Validation | `src/libbg/ballpivot.cpp:30-75` | `FIXED` | Unchecked negative return code in bg_3d_ballpivot (if (nfaces == 0) instead of if (nfaces <= 0)), allowing negative face count to be passed into bg_trimesh_3d_gc; missing validation for radii_cnt < 0 or radii_cnt > 0 with NULL radii; missing initial NULL assignment to output pointers. |
| `SEC-0173` | **Sev 1** | Unchecked File Operation / Null Dereference | `src/libbg/RTree.h:465-480, 1740-1800` | `FIXED` | Unchecked fopen() return value in RTree::plot() and RTree::plot2d() leading to NULL pointer dereference on file write failure; missing NULL check on filename in RTFileStream::OpenRead and RTFileStream::OpenWrite; unguarded VertexDataSource count on NULL pointer in QuickHull.hpp. |
| `SEC-0174` | **Sev 1** | Defensive Parameter Validation | `src/libbg/polygon_triangulate.cpp:560-585` | `FIXED` | Missing parameter validation in bg_detria for faces, num_faces, poly, poly_pnts, pts, holes_array, holes_npts, and steiner, which could cause segmentation faults when called with NULL or undersized inputs. |
