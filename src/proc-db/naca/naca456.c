/*                       N A C A 4 5 6 . C
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file naca456.c
 *
 * BRL-CAD's translation of the calculation of coordinates for NACA airfoils is
 * based on the public domain program naca456 written by Ralph Carmichael of
 * Public Domain Aeronautical Software (PDAS):
 *
 * http://www.pdas.com/naca456.html
 *
 * naca456 is in turn based off of earlier work by several authors at NASA,
 * documented in reports NASA TM X-3284, NASA TM X-3069 and NASA TM 4741. The
 * program naca456 is documented in the paper:
 *
 * Carmichael, Ralph L.: Algorithm for Calculating Coordinates of Cambered NACA
 * Airfoils At Specified Chord Locations. AIAA Paper 2001-5235, November 2001.
 *
 * Disclaimer, per the PDAS distribution:
 *
 * Although many of the works contained herein were developed by national
 * laboratories or their contractors, neither the U.S. Government nor Public
 * Domain Aeronautical Software make any warranty as to the accuracy or
 * appropriateness of the procedure to any particular application.
 *
 * The programs and descriptions have been collected and reproduced with great
 * care and attention to accuracy, but no guarantee or warranty is implied. All
 * programs are offered AS IS and Public Domain Aeronautical Software does not
 * give any express or implied warranty of any kind and any implied warranties are
 * disclaimed. Public Domain Aeronautical Software will not be liable for any
 * direct, indirect, special, incidental, or consequential damages arising out of
 * any use of this software. In no case should the results of any computational
 * scheme be used as a substitute for sound engineering practice and judgment.
 *
 */

#include "naca.h"
#include "bu/app.h"

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    if (!argc || !argv)
	return 1;

    return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
