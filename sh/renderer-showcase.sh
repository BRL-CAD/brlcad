#!/bin/sh
#                   R E N D E R E R - S H O W C A S E . S H
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###
#
# Generate a Cornell-box gallery of BRL-CAD raytracing output styles.
#
# The output directory contains raw renderer output, PNG previews, a
# contact sheet, logs, a manifest, and an HTML index.
#

set -u

usage()
{
    cat <<EOF
Usage: $0 [options]

Options:
  -B, --build-dir DIR   BRL-CAD build directory (default: SOURCE/.build)
  -o, --output-dir DIR  Showcase output directory (default: BUILD/renderer-showcase)
  -s, --size PIXELS     Square render size (default: 256)
  -P, --processors N    RT processor count (default: 1)
  -p, --perspective DEG Perspective angle (default: 55)
  -h, --help            Print this help
EOF
}

die()
{
    echo "renderer-showcase: ERROR: $*" 1>&2
    exit 1
}

log()
{
    echo "$*"
}

html_escape()
{
    printf '%s' "$1" | sed \
	-e 's/&/\&amp;/g' \
	-e 's/</\&lt;/g' \
	-e 's/>/\&gt;/g' \
	-e 's/"/\&quot;/g'
}

quote_args()
{
    out=
    for arg
    do
	case "$arg" in
	    *[!A-Za-z0-9_.,:=/+@%-]*|'')
		qarg=`printf '%s' "$arg" | sed "s/'/'\\\\''/g"`
		out="$out '$qarg'"
		;;
	    *)
		out="$out $arg"
		;;
	esac
    done
    printf '%s' "$out"
}

script_dir=`dirname "$0"`
src_dir=`CDPATH= cd "$script_dir/.." && pwd` || exit 1
build_dir="$src_dir/.build"
out_dir=
size=256
cpus=1
perspective=55

while test $# -gt 0 ; do
    case "$1" in
	-B|--build-dir)
	    test $# -gt 1 || die "$1 requires a directory"
	    build_dir="$2"
	    shift 2
	    ;;
	-o|--output-dir)
	    test $# -gt 1 || die "$1 requires a directory"
	    out_dir="$2"
	    shift 2
	    ;;
	-s|--size)
	    test $# -gt 1 || die "$1 requires a pixel size"
	    size="$2"
	    shift 2
	    ;;
	-P|--processors)
	    test $# -gt 1 || die "$1 requires a processor count"
	    cpus="$2"
	    shift 2
	    ;;
	-p|--perspective)
	    test $# -gt 1 || die "$1 requires an angle"
	    perspective="$2"
	    shift 2
	    ;;
	-h|--help)
	    usage
	    exit 0
	    ;;
	*)
	    die "unknown option: $1"
	    ;;
    esac
done

case "$size" in
    ''|*[!0-9]*) die "size must be a positive integer" ;;
esac
case "$cpus" in
    ''|*[!0-9]*) die "processor count must be a positive integer" ;;
esac
test "$size" -gt 0 || die "size must be greater than zero"
test "$cpus" -gt 0 || die "processor count must be greater than zero"

build_dir=`CDPATH= cd "$build_dir" && pwd` || die "cannot enter build directory: $build_dir"
if test "x$out_dir" = "x" ; then
    out_dir="$build_dir/renderer-showcase"
fi
case "$out_dir" in
    /*) ;;
    *) out_dir=`pwd`/"$out_dir" ;;
esac

bin_dir="$build_dir/bin"
test -d "$bin_dir" || die "missing build bin directory: $bin_dir"

find_tool()
{
    tool="$bin_dir/$1"
    test -x "$tool" || die "missing executable: $tool"
    echo "$tool"
}

RT=`find_tool rt`
RTEDGE=`find_tool rtedge`
RTDEPTH=`find_tool rtdepth`
RTXRAY=`find_tool rtxray`
RTHIDE=`find_tool rthide`
BWPNG=`find_tool bw-png`
PNGPIX=`find_tool png-pix`
PIXPNG=`find_tool pix-png`
PIXTILE=`find_tool pixtile`
GENCOLOR=`find_tool gencolor`
FBCLEAR=`find_tool fbclear`
PLOT3FB=`find_tool plot3-fb`
FBPNG=`find_tool fb-png`

db="$build_dir/share/db/cornell.g"
if test ! -f "$db" ; then
    db="$build_dir/db/cornell.g"
fi
test -f "$db" || die "missing Cornell database. Build the cornell.g target first."

rm -rf "$out_dir"
mkdir -p "$out_dir/png" "$out_dir/raw" "$out_dir/log" "$out_dir/tile" || exit 1

view_script="$out_dir/cornell.view"
cat > "$view_script" <<EOF
viewsize 6.000000000000000e+03;
orientation 0.000000000000000e+00 9.961946980917455e-01 8.715574274765824e-02 0.000000000000000e+00;
eye_pt 2.780000000000000e+02 3.709445330007912e+02 -4.544232590366239e+02;
start 0; clean;
end;
EOF

manifest="$out_dir/manifest.tsv"
cat > "$manifest" <<EOF
id	title	png	description	command
EOF

add_item()
{
    id="$1"
    title="$2"
    png="$3"
    desc="$4"
    cmd="$5"

    printf '%s\t%s\t%s\t%s\t%s\n' "$id" "$title" "$png" "$desc" "$cmd" >> "$manifest"
}

run_rt_png()
{
    id="$1"
    title="$2"
    desc="$3"
    shift 3

    png="$out_dir/png/$id.png"
    logfile="$out_dir/log/$id.log"

    log "... rendering $id"
    "$RT" -B -M "-p$perspective" "-s$size" "-P$cpus" "$@" -o "$png" "$db" all.g < "$view_script" > "$logfile" 2>&1 \
	|| die "$id failed; see $logfile"
    test -s "$png" || die "$id did not create $png"

    arg_display=`quote_args "$@"`
    add_item "$id" "$title" "png/$id.png" "$desc" "rt -B -M -p$perspective -s$size -P$cpus$arg_display -o png/$id.png cornell.g all.g < cornell.view"
}

run_edge_png()
{
    id="$1"
    title="$2"
    desc="$3"
    shift 3

    png="$out_dir/png/$id.png"
    logfile="$out_dir/log/$id.log"

    log "... rendering $id"
    "$RTEDGE" -B -M "-p$perspective" "-s$size" "-P$cpus" "$@" -o "$png" "$db" all.g < "$view_script" > "$logfile" 2>&1 \
	|| die "$id failed; see $logfile"
    test -s "$png" || die "$id did not create $png"

    arg_display=`quote_args "$@"`
    add_item "$id" "$title" "png/$id.png" "$desc" "rtedge -B -M -p$perspective -s$size -P$cpus$arg_display -o png/$id.png cornell.g all.g < cornell.view"
}

run_bw_png()
{
    id="$1"
    title="$2"
    desc="$3"
    renderer="$4"
    renderer_name="$5"
    shift 5

    bw="$out_dir/raw/$id.bw"
    png="$out_dir/png/$id.png"
    logfile="$out_dir/log/$id.log"

    log "... rendering $id"
    "$renderer" -B -M "-p$perspective" "-s$size" "-P$cpus" "$@" -o "$bw" "$db" all.g < "$view_script" > "$logfile" 2>&1 \
	|| die "$id failed; see $logfile"
    "$BWPNG" "-s$size" "$bw" > "$png" 2>> "$logfile" \
	|| die "$id PNG conversion failed; see $logfile"
    test -s "$png" || die "$id did not create $png"

    arg_display=`quote_args "$@"`
    add_item "$id" "$title" "png/$id.png" "$desc" "$renderer_name -B -M -p$perspective -s$size -P$cpus$arg_display -o raw/$id.bw cornell.g all.g < cornell.view"
}

run_rthide_png()
{
    id="$1"
    title="$2"
    desc="$3"

    plot="$out_dir/raw/$id.plot3"
    fb="$out_dir/raw/$id.fb"
    png="$out_dir/png/$id.png"
    logfile="$out_dir/log/$id.log"

    log "... rendering $id"
    "$RTHIDE" -B -M "-p$perspective" "-s$size" "-P$cpus" -o "$plot" "$db" all.g < "$view_script" > "$logfile" 2>&1 \
	|| die "$id failed; see $logfile"
    "$FBCLEAR" -F "$fb" "-s$size" 255 255 255 >> "$logfile" 2>&1 \
	|| die "$id framebuffer clear failed; see $logfile"
    "$PLOT3FB" -F "$fb" -o "-s$size" < "$plot" >> "$logfile" 2>&1 \
	|| die "$id plot3-fb conversion failed; see $logfile"
    "$FBPNG" -F "$fb" "-s$size" "$png" >> "$logfile" 2>&1 \
	|| die "$id fb-png conversion failed; see $logfile"
    test -s "$png" || die "$id did not create $png"

    add_item "$id" "$title" "png/$id.png" "$desc" "rthide -B -M -p$perspective -s$size -P$cpus -o raw/$id.plot3 cornell.g all.g < cornell.view; plot3-fb -F raw/$id.fb -o -s$size < raw/$id.plot3; fb-png -F raw/$id.fb -s$size png/$id.png"
}

run_rt_png rt_l0 "rt -l0 full lighting" "Full lighting model with scene light, material color, shadows, and reflection." -l0
run_rt_png rt_l1 "rt -l1 eye diffuse" "Simple diffuse lighting from the eye, useful for debugging form." -l1
run_rt_png rt_l2 "rt -l2 normals" "Surface normals encoded as RGB colors." -l2
run_rt_png rt_l3 "rt -l3 three-light" "Three-light diffuse debug model." -l3
run_rt_png rt_l4 "rt -l4 curvature" "Inverse curvature debug display. The Cornell box is mostly planar, so this is intentionally flat." -l4
run_rt_png rt_l5 "rt -l5 principal direction" "Principal curvature direction debug display." -l5
run_rt_png rt_l6 "rt -l6 UV" "UV coordinate test-map style." -l6
run_rt_png rt_l7 "rt -l7 photon map" "Photon-mapping global illumination using a small deterministic photon count for quick showcase runs." -l7,1024,0,8,60.0,1,0,0,0,1.0,
run_rt_png rt_l8 "rt -l8 heat graph" "Heat-graph mode; current PNG file output matches the shaded image while heat timing is reported in the log." -l8
run_rt_png rt_haze "rt haze" "Full lighting with atmospheric haze enabled." -m 0.0008,0.55,0.72,0.92
run_rt_png rt_cutaway "rt cut plane" "Full lighting with a view-wide cutting plane." -k 0,1,0,260

run_edge_png rtedge_default "rtedge default" "Default edge rendering on a dark background."
run_edge_png rtedge_colored "rtedge colored" "Custom foreground and background colors." -c "set fg=255/220/0 bg=20/22/26"
run_edge_png rtedge_regions "rtedge region colors" "Region-aware edge rendering using region colors." -c "set rc=1 dr=1"

run_bw_png rtdepth "rtdepth" "Depth map rendered as grayscale." "$RTDEPTH" rtdepth
run_bw_png rtxray "rtxray" "Pseudo X-ray thickness image rendered as grayscale." "$RTXRAY" rtxray
run_rthide_png rthide "rthide hidden line" "Hidden-line plot3 output rasterized with plot3-fb and fb-png."

cols=4
count=`awk 'NR > 1 {c++} END {print c + 0}' "$manifest"`
rows=$(( (count + cols - 1) / cols ))
total=$(( rows * cols ))
sheet_w=$(( cols * size ))
sheet_h=$(( rows * size ))

log "... building contact sheet"
i=0
{
    read -r header
    while IFS='	' read -r id title png desc cmd ; do
    tile=`printf '%s/tile/%03d.pix' "$out_dir" "$i"`
    "$PNGPIX" "$out_dir/$png" > "$tile" 2>> "$out_dir/log/contact-sheet.log" \
	|| die "png-pix failed for $png"
    i=$(( i + 1 ))
    done
} < "$manifest"

while test "$i" -lt "$total" ; do
    tile=`printf '%s/tile/%03d.pix' "$out_dir" "$i"`
    "$GENCOLOR" "-s$size" 32 32 32 > "$tile" 2>> "$out_dir/log/contact-sheet.log" \
	|| die "failed to create blank contact-sheet tile"
    i=$(( i + 1 ))
done

sheet_pix="$out_dir/contact-sheet.pix"
sheet_png="$out_dir/contact-sheet.png"
(
    set --
    j=0
    while test "$j" -lt "$total" ; do
	tile=`printf '%s/tile/%03d.pix' "$out_dir" "$j"`
	set -- "$@" "$tile"
	j=$(( j + 1 ))
    done
    "$PIXTILE" "-s$size" "-W$sheet_w" "-N$sheet_h" "$@" > "$sheet_pix" 2>> "$out_dir/log/contact-sheet.log"
) || die "pixtile failed; see $out_dir/log/contact-sheet.log"

"$PIXPNG" "-w$sheet_w" "-n$sheet_h" "$sheet_pix" > "$sheet_png" 2>> "$out_dir/log/contact-sheet.log" \
    || die "pix-png failed for contact sheet; see $out_dir/log/contact-sheet.log"

html="$out_dir/index.html"
cat > "$html" <<EOF
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>BRL-CAD Renderer Showcase - Cornell Box</title>
<style>
:root {
  color-scheme: light dark;
  --bg: #f5f5f2;
  --fg: #181916;
  --muted: #666b61;
  --line: #d6d8d0;
  --card: #ffffff;
}
@media (prefers-color-scheme: dark) {
  :root {
    --bg: #151613;
    --fg: #eceee7;
    --muted: #a8ada1;
    --line: #343830;
    --card: #1d1f1a;
  }
}
body {
  margin: 0;
  font: 14px/1.45 system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  color: var(--fg);
  background: var(--bg);
}
main {
  max-width: 1180px;
  margin: 0 auto;
  padding: 28px 20px 48px;
}
h1 {
  margin: 0 0 8px;
  font-size: 30px;
  letter-spacing: 0;
}
p {
  color: var(--muted);
  margin: 0 0 18px;
}
.sheet {
  display: block;
  width: min(100%, ${sheet_w}px);
  height: auto;
  border: 1px solid var(--line);
  background: #202020;
}
.grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(210px, 1fr));
  gap: 16px;
  margin-top: 22px;
}
figure {
  margin: 0;
  border: 1px solid var(--line);
  background: var(--card);
}
figure img {
  display: block;
  width: 100%;
  aspect-ratio: 1 / 1;
  object-fit: contain;
  background: #111;
}
figcaption {
  padding: 10px 12px 12px;
}
figcaption strong,
figcaption span,
figcaption code {
  display: block;
}
figcaption span {
  color: var(--muted);
  min-height: 40px;
}
code {
  margin-top: 8px;
  padding-top: 8px;
  border-top: 1px solid var(--line);
  color: var(--muted);
  white-space: normal;
  overflow-wrap: anywhere;
  font-size: 12px;
}
</style>
</head>
<body>
<main>
<h1>BRL-CAD Renderer Showcase - Cornell Box</h1>
<p>Generated from <code>$db</code> at ${size}x${size} using the saved Cornell view matrix from <code>db/cornell.rt</code>.</p>
<a href="contact-sheet.png"><img class="sheet" src="contact-sheet.png" alt="Renderer contact sheet"></a>
<div class="grid">
EOF

{
    read -r header
    while IFS='	' read -r id title png desc cmd ; do
    title_h=`html_escape "$title"`
    desc_h=`html_escape "$desc"`
    cmd_h=`html_escape "$cmd"`
    cat >> "$html" <<EOF
<figure id="$id">
  <a href="$png"><img src="$png" alt="$title_h"></a>
  <figcaption>
    <strong>$title_h</strong>
    <span>$desc_h</span>
    <code>$cmd_h</code>
  </figcaption>
</figure>
EOF
    done
} < "$manifest"

cat >> "$html" <<EOF
</div>
</main>
</body>
</html>
EOF

log "Showcase written to: $out_dir"
log "Open: $html"

exit 0

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
