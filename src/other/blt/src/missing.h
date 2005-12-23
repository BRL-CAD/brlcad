#ifndef _MISSING_H
#define _MISSING_H

#include <winspool.h>

#ifndef DeleteBitmap
#define DeleteBitmap(hbm)       DeleteObject((HGDIOBJ)(HBITMAP)(hbm))
#endif
#ifndef DeleteBrush
#define DeleteBrush(hbr)	DeleteObject((HGDIOBJ)(HBRUSH)(hbr))
#endif
#ifndef DeleteFont
#define DeleteFont(hfont)	DeleteObject((HGDIOBJ)(HFONT)(hfont))
#endif
#ifndef DeletePalette
#define DeletePalette(hpal)     DeleteObject((HGDIOBJ)(HPALETTE)(hpal))
#endif
#ifndef DeletePen
#define DeletePen(hpen)		DeleteObject((HGDIOBJ)(HPEN)(hpen))
#endif
#ifndef SelectBitmap
#define SelectBitmap(hdc, hbm)  ((HBITMAP)SelectObject((hdc), (HGDIOBJ)(HBITMAP)(hbm)))
#endif
#ifndef SelectBrush
#define SelectBrush(hdc, hbr)   ((HBRUSH)SelectObject((hdc), (HGDIOBJ)(HBRUSH)(hbr)))
#endif
#ifndef SelectFont
#define SelectFont(hdc, hfont)  ((HFONT)SelectObject((hdc), (HGDIOBJ)(HFONT)(hfont)))
#endif
#ifndef SelectPen
#define SelectPen(hdc, hpen)    ((HPEN)SelectObject((hdc), (HGDIOBJ)(HPEN)(hpen)))
#endif
#ifndef GetNextWindow
#define GetNextWindow(hWnd,wCmd) GetWindow((hWnd),(wCmd))
#endif
#ifndef GetStockBrush
#define GetStockBrush(i)     ((HBRUSH)GetStockObject(i))
#endif
#ifndef GetStockPen
#define GetStockPen(i)       ((HPEN)GetStockObject(i))
#endif


#define DM_SPECVERSION 0x0401

#define DMPAPER_ISO_B4              42	/* B4 (ISO) 250 x 353 mm              */
#define DMPAPER_JAPANESE_POSTCARD   43	/* Japanese Postcard 100 x 148 mm     */
#define DMPAPER_9X11                44	/* 9 x 11 in                          */
#define DMPAPER_10X11               45	/* 10 x 11 in                         */
#define DMPAPER_15X11               46	/* 15 x 11 in                         */
#define DMPAPER_ENV_INVITE          47	/* Envelope Invite 220 x 220 mm       */
#define DMPAPER_RESERVED_48         48	/* RESERVED--DO NOT USE               */
#define DMPAPER_RESERVED_49         49	/* RESERVED--DO NOT USE               */
#define DMPAPER_LETTER_EXTRA        50	/* Letter Extra 9 \275 x 12 in        */
#define DMPAPER_LEGAL_EXTRA         51	/* Legal Extra 9 \275 x 15 in         */
#define DMPAPER_TABLOID_EXTRA       52	/* Tabloid Extra 11.69 x 18 in        */
#define DMPAPER_A4_EXTRA            53	/* A4 Extra 9.27 x 12.69 in           */
#define DMPAPER_LETTER_TRANSVERSE   54	/* Letter Transverse 8 \275 x 11 in   */
#define DMPAPER_A4_TRANSVERSE       55	/* A4 Transverse 210 x 297 mm         */
#define DMPAPER_LETTER_EXTRA_TRANSVERSE 56	/* Letter Extra Transverse 9\275 x 12 in */
#define DMPAPER_A_PLUS              57	/* SuperA/SuperA/A4 227 x 356 mm      */
#define DMPAPER_B_PLUS              58	/* SuperB/SuperB/A3 305 x 487 mm      */
#define DMPAPER_LETTER_PLUS         59	/* Letter Plus 8.5 x 12.69 in         */
#define DMPAPER_A4_PLUS             60	/* A4 Plus 210 x 330 mm               */
#define DMPAPER_A5_TRANSVERSE       61	/* A5 Transverse 148 x 210 mm         */
#define DMPAPER_B5_TRANSVERSE       62	/* B5 (JIS) Transverse 182 x 257 mm   */
#define DMPAPER_A3_EXTRA            63	/* A3 Extra 322 x 445 mm              */
#define DMPAPER_A5_EXTRA            64	/* A5 Extra 174 x 235 mm              */
#define DMPAPER_B5_EXTRA            65	/* B5 (ISO) Extra 201 x 276 mm        */
#define DMPAPER_A2                  66	/* A2 420 x 594 mm                    */
#define DMPAPER_A3_TRANSVERSE       67	/* A3 Transverse 297 x 420 mm         */
#define DMPAPER_A3_EXTRA_TRANSVERSE 68	/* A3 Extra Transverse 322 x 445 mm   */
#ifndef DMPAPER_LAST
#define DMPAPER_LAST                DMPAPER_A3_EXTRA_TRANSVERSE
#endif /*DMPAPER_LAST */

#define DMPAPER_USER                256

/* bin selections */
#ifndef DMPAPER_FIRST
#define DMBIN_FIRST         DMBIN_UPPER
#endif /*DMPAPER_FIRST*/

#define DMBIN_UPPER         1
#define DMBIN_ONLYONE       1
#define DMBIN_LOWER         2
#define DMBIN_MIDDLE        3
#define DMBIN_MANUAL        4
#define DMBIN_ENVELOPE      5
#define DMBIN_ENVMANUAL     6
#define DMBIN_AUTO          7
#define DMBIN_TRACTOR       8
#define DMBIN_SMALLFMT      9
#define DMBIN_LARGEFMT      10
#define DMBIN_LARGECAPACITY 11
#define DMBIN_CASSETTE      14
#define DMBIN_FORMSOURCE    15

#ifndef DMBIN_LAST 
#define DMBIN_LAST          DMBIN_FORMSOURCE
#endif /*DMBIN_LAST*/

#define DMBIN_USER          256	/* device specific bins start here */

/* print qualities */
#define DMRES_DRAFT         (-1)
#define DMRES_LOW           (-2)
#define DMRES_MEDIUM        (-3)
#define DMRES_HIGH          (-4)

#define DMTT_DOWNLOAD_OUTLINE 4	/* download TT fonts as outline soft fonts */


#endif /* _MISSING_H */
