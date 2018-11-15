#!/bin/sh
#
# This script is in the public domain.
#
# Example script showing the combination of gcv/gdal and mged being
# used to make a .g file of the Hawaii islands chain from the
# University of Washington's Geomorphological Research Group terrain
# data files.  Illustrates warping and downsampling with gdalwarp and
# concating multiple terrain data sets into a single .g file.
#
# Data from http://gis.ess.washington.edu/data/raster/tenmeter/hawaii/index.html

# Cleanup
rm -f hawaii.g
rm -f *.bil *.dem
rm -f README*
rm -f *.blw *.hdr *.stx

# Unpack
for filename in *.zip; do
	unzip $filename
done
rm -f README*
rmdir oahu

# Big Island is UTM zone 5, reproject
# TODO - is this correct?  Big island isn't currently importing
# in the right relative location compared to the other islands...
gdalwarp -t_srs '+proj=utm +zone=4 +datum=WGS84' hawaii.bil bigisland_zone4.bil

# Downsample to a data density we can more easily manipulate in
# BRL-CAD dsp objects.
gdalwarp -tr 75 75 -r average bigisland_zone4.bil bigisland.tiff
gdalwarp -tr 75 75 -r average kahoolawe.bil  kahoolawe.tiff
gdalwarp -tr 75 75 -r average kauai.bil      kauai.tiff
gdalwarp -tr 75 75 -r average lanai.bil      lanai.tiff
gdalwarp -tr 75 75 -r average maui.bil       maui.tiff
gdalwarp -tr 75 75 -r average molokai.bil    molokai.tiff
gdalwarp -tr 75 75 -r average niihau.bil     niihau.tiff
gdalwarp -tr 75 75 -r average oahu.bil       oahu.tiff

# Make individual .g files for each island
gcv bigisland.tiff bigisland.g
gcv kahoolawe.tiff kahoolawe.g
gcv kauai.tiff     kauai.g
gcv lanai.tiff     lanai.g
gcv maui.tiff      maui.g
gcv molokai.tiff   molokai.g
gcv niihau.tiff    niihau.g
gcv oahu.tiff      oahu.g

# Merge all the islands into a single file
mged bigisland.g dbconcat kahoolawe.g
mged bigisland.g dbconcat kauai.g
mged bigisland.g dbconcat lanai.g
mged bigisland.g dbconcat maui.g
mged bigisland.g dbconcat molokai.g
mged bigisland.g dbconcat niihau.g
mged bigisland.g dbconcat oahu.g

# The big island doesn't quite end up where we want it relative to the other
# islands (TODO - are we not warping it correctly?)) but in the meantime we can
# adjust its position to be approximately correct.
mged bigisland.g "units km;e bigisland.s;sed bigisland.s; tra 640 0 0; accept"

# Done
mv bigisland.g hawaii.g

# Cleanup
rm kahoolawe.g kauai.g lanai.g maui.g molokai.g niihau.g oahu.g
rm *.tiff
