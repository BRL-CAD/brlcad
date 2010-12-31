
/*
 * htmlimage.c ---
 *
 *     This file contains routines that manipulate images used in an HTML
 *     document.
 *
 *----------------------------------------------------------------------------
 * Copyright (c) 2005 Eolas Technologies Inc.
 * All rights reserved.
 *
 * This Open Source project was made possible through the financial support
 * of Eolas Technologies Inc.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <ORGANIZATION> nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
static const char rcsid[] = "$Id: htmlimage.c,v 1.70 2008/01/20 06:17:49 danielk1977 Exp $";

#include <assert.h>
#include "html.h"
#include "htmllayout.h"

/*----------------------------------------------------------------------------
 * OVERVIEW 
 *
 *     This file contains code for the "Html image server". Possibly "image 
 *     manager" would be a better name. The image server has two primary 
 *     goals:
 *
 *     * Manage loading images from the -imagecmd callback to ensure 
 *       that each image used by the document is only loaded once.
 *     * Manage resizing images to minimize the number of (expensive)
 *       resize operations that need to be performed.
 *
 * PUBLIC INTERFACE
 *
 *     Image Server Object:
 *    
 *         HtmlImageServerInit()
 *         HtmlImageServerShutdown()
 *
 *         HtmlImageServerGet()
 *
 *         HtmlImageServerSuspendGC()
 *         HtmlImageServerDoGC()
 *    
 *     Image Object:
 *    
 *         HtmlImageUnscaledName()
 *         HtmlImageScale()
 *         HtmlImageFree()
 *         HtmlImageAlphaChannel()
 *
 *         HtmlImageTile()
 *         HtmlImageImage()
 *         HtmlImagePixmap()
 *         HtmlImageTilePixmap()
 *
 * IMAGE CONVERSION ROUTINES
 *
 *     As well as the image server, this file also contains the following
 *     routine used to create a Tk image from an XImage (used by 
 *     the [widget image] command).
 * 
 *         HtmlXImageToImage()
 *----------------------------------------------------------------------------
 */

/*
 * Image-server object. 
 */
struct HtmlImageServer {
    HtmlTree *pTree;                 /* Pointer to owner HtmlTree object */
    Tcl_HashTable aImage;            /* Hash table of images by URL */
    int isSuspendGC;
};

/*
 * HtmlImage structures are stored in the Htmltree.aImage array. The index
 * to the array is the URI specified for the image. If the URI was loaded
 * via a stylesheet other than the default stylesheet, the URI will have
 * already been passed to the -uricmd callback to turn it into a full-path.
 */
struct HtmlImage2 {
    HtmlImageServer *pImageServer;   /* Image server that caches this image */
    const char *zUrl;                /* Hash table key */

    int isValid;                     /* True if HtmlImage.image is valid */
    int width;                       /* Width of HtmlImage2.image */
    int height;                      /* Height of HtmlImage2.image */
    Tk_Image image;                  /* Scaled (or unscaled) image */

    int iTileWidth;                  /* Width of tile image (if it exists) */
    int iTileHeight;                 /* Height of tile image (if it exists) */

    Pixmap pixmap;                   /* Pixmap of image */
    Pixmap tilepixmap;               /* Tile pixmap of image */
    Tcl_Obj *pCompressed;            /* Compressed image data */

    int nIgnoreChange;

    Tcl_Obj *pTileName;              /* Name of Tk tile image */
    Tk_Image tile;                   /* Tiled image, or zero */

    int eAlpha;                      /* An ALPHA_CHANNEL_XXX value */

    int nRef;                        /* Number of references to this struct */
    Tcl_Obj *pImageName;             /* Image name, if this is unscaled */
    Tcl_Obj *pDelete;                /* Delete script, if this is unscaled */
    HtmlImage2 *pUnscaled;           /* Unscaled image, if this is scaled */

    HtmlImage2 *pNext;               /* Next in list of scaled copies */
};

#define ALPHA_CHANNEL_UNKNOWN 0
#define ALPHA_CHANNEL_TRUE    1
#define ALPHA_CHANNEL_FALSE   2


/*
 *---------------------------------------------------------------------------
 *
 * HtmlImageServerInit --
 *
 *     Initialise the image-server for html widget pTree. A pointer to the
 *     initialised server is stored at HtmlTree.pImageServer.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void
HtmlImageServerInit(pTree)
    HtmlTree *pTree;
{
    HtmlImageServer *p;
    assert(!pTree->pImageServer);
    p = HtmlNew(HtmlImageServer);
    Tcl_InitHashTable(&p->aImage, TCL_STRING_KEYS);
    p->pTree = pTree;
    pTree->pImageServer = p;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlImageServerShutdown --
 *
 *     Shutdown and delete the image-server for html widget pTree. This
 *     function frees all resources allocated by HtmlImageServerInit().
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void 
HtmlImageServerShutdown(pTree)
    HtmlTree *pTree;
{
    HtmlImageServer *p = pTree->pImageServer;
#ifndef NDEBUG
    Tcl_HashSearch search;
    Tcl_HashEntry *pEntry = Tcl_FirstHashEntry(&p->aImage, &search);
    assert(!pEntry);
#endif
    HtmlFree(p);
    pTree->pImageServer = 0;
}

/*
 *---------------------------------------------------------------------------
 *
 * photoputblock --
 *
 *     This is a wrapper around Tk_PhotoPutBlock(). In tk 8.5, the 'interp'
 *     argument was added to the Tk_PhotoPutBlock() signature. This
 *     function deals with this API change.
 *
 *     Later: The trick is to define USE_COMPOSITELESS_PHOTO_PUT_BLOCK,
 *     which is now done in html.h.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
photoputblock(interp, handle, blockPtr, x, y, width, height, compRule)
    Tcl_Interp *interp;
    Tk_PhotoHandle handle;
    Tk_PhotoImageBlock *blockPtr;
    int x;
    int y;
    int width;
    int height;
    int compRule;
{
    Tk_PhotoPutBlock(handle, blockPtr, x, y, width, height);
}

static void
freeTile(pImage)
    HtmlImage2 *pImage;
{
    HtmlTree *pTree = pImage->pImageServer->pTree;
    int flags = TCL_GLOBAL_ONLY;
    Tcl_Obj *pScript;
    if (pImage->pTileName) {
        pScript = Tcl_NewStringObj("image delete", -1);
        Tcl_IncrRefCount(pScript);
        Tcl_ListObjAppendElement(0, pScript, pImage->pTileName);
        Tcl_EvalObjEx(pTree->interp, pScript, flags);
        Tcl_DecrRefCount(pScript);
    
        Tcl_DecrRefCount(pImage->pTileName);
        pImage->tile = 0;
        pImage->pTileName = 0;
    }
    if (pImage->tilepixmap) {
        assert(pImage->pixmap);
        Tk_FreePixmap(
            Tk_Display(pImage->pImageServer->pTree->tkwin), pImage->tilepixmap);
        pImage->tilepixmap = 0;
    }
}

#define UNSCALED(pImage) (                                       \
   ((pImage) && (pImage)->pUnscaled)?(pImage)->pUnscaled:pImage  \
)

static int
imageChangedCb(pTree, pNode, clientData)
    HtmlTree *pTree;
    HtmlNode *pNode;
    ClientData clientData;
{
    HtmlComputedValues *pV = HtmlNodeComputedValues(pNode);
    HtmlImage2 *pImage = (HtmlImage2 *)clientData;
    assert(!pImage->pUnscaled);
    if (pV) {
        HtmlImage2 *imBackgroundImage = pV->imBackgroundImage;
        if (imBackgroundImage == pImage) {
            int w = PIXELVAL_AUTO;
            int h = PIXELVAL_AUTO;
            HtmlImage2 *pNew = HtmlImageScale(imBackgroundImage, &w, &h, 1);
            HtmlImageFree(pV->imZoomedBackgroundImage);
            pV->imZoomedBackgroundImage = pNew;
        }
        if (pV->imReplacementImage==pImage || pV->imListStyleImage==pImage) {
            HtmlCallbackLayout(pTree, pNode);
        }
    }
    return HTML_WALK_DESCEND;
}

static void asyncPixmapify(clientData)
    ClientData clientData;
{
    HtmlImage2 *pImage = (HtmlImage2 *)clientData;
    HtmlImagePixmap(pImage);
}

static Tcl_Obj*
getImageCompressed(pImage)
    HtmlImage2 *pImage;
{
    if (!pImage->pCompressed) {
        Tcl_Interp *interp = pImage->pImageServer->pTree->interp;
        Tcl_Obj *apObj[3];
        apObj[0] = pImage->pImageName;
        apObj[1] = Tcl_NewStringObj("cget", -1);
        apObj[2] = Tcl_NewStringObj("-data", -1);
    
        Tcl_IncrRefCount(apObj[0]);
        Tcl_IncrRefCount(apObj[1]);
        Tcl_IncrRefCount(apObj[2]);
        if (TCL_OK == Tcl_EvalObjv(interp, 3, apObj, TCL_EVAL_GLOBAL)) {
	    int nData;
	    Tcl_Obj *pData = Tcl_GetObjResult(interp);
	    Tcl_GetByteArrayFromObj(pData, &nData);
	    if (nData>0){
                pImage->pCompressed = pData;
                Tcl_IncrRefCount(pData);
	    }
        }
        Tcl_DecrRefCount(apObj[2]);
        Tcl_DecrRefCount(apObj[1]);
        Tcl_DecrRefCount(apObj[0]);
    }
    return pImage->pCompressed;
}

static void
freeImageCompressed(pImage)
    HtmlImage2 *pImage;
{
    if (pImage->pCompressed) {
        Tcl_DecrRefCount(pImage->pCompressed);
        pImage->pCompressed = 0;
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * imageChanged --
 *
 *     Image-changed callback.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
static void 
imageChanged(clientData, x, y, width, height, imgWidth, imgHeight)
    ClientData clientData;
    int x;
    int y;
    int width;
    int height;
    int imgWidth;
    int imgHeight;
{
    HtmlImage2 *pImage = (HtmlImage2 *)clientData;
    if (pImage && !pImage->pUnscaled && !pImage->nIgnoreChange) {
        HtmlImage2 *p;
        HtmlTree *pTree = pImage->pImageServer->pTree;
        assert(pImage->image);

        for (p = pImage->pNext; p; p = p->pNext) {
            p->isValid = 0;
            assert(!p->pTileName);
        }
        freeTile(pImage);
        pImage->eAlpha = ALPHA_CHANNEL_UNKNOWN;

        /* Delete the pixmap/compressed-data representation */
        if( pImage->pixmap ){
            Tk_FreePixmap(Tk_Display(pTree->tkwin), pImage->pixmap);
            pImage->pixmap = 0;
        }
        freeImageCompressed(pImage);

        if (imgWidth!=pImage->width || imgHeight!=pImage->height) {
            pImage->width = imgWidth;
            pImage->height = imgHeight;
            HtmlWalkTree(pTree, 0, imageChangedCb, (ClientData)pImage);
        }

        Tcl_DoWhenIdle(asyncPixmapify, (ClientData)pImage);

        /* If the image contents have been modified but the size is
         * constant, then just redraw the display. This is lazy. If
         * there were an efficient way to determine the minimum region
         * to draw, then stuff like animated gifs would be much more
         * efficient.
         */
        HtmlCallbackDamage(pTree, 0, 0, 1000000, 1000000);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlImageServerGet --
 *
 *     Retrieve an HtmlImage2 object for the image at URL zUrl from 
 *     an image-server. The caller should match this call with a single
 *     HtmlImageFree() when the image object is no longer required.
 *
 *     If the image is not already in the cache, the Tcl script 
 *     configured as the widget -imagecmd is invoked. If this command
 *     raises an error or returns an invalid result, then this function 
 *     returns NULL. A Tcl back-ground error is propagated in this case 
 *     also.
 *
 * Results:
 *     Pointer to HtmlImage2 object containing the image from zUrl, or
 *     NULL, if zUrl was invalid for some reason.
 *
 * Side effects:
 *     May invoke -imagecmd script.
 *
 *---------------------------------------------------------------------------
 */
HtmlImage2* 
HtmlImageServerGet(p, zUrl)
    HtmlImageServer *p;
    const char *zUrl; 
{
    Tcl_Obj *pImageCmd = p->pTree->options.imagecmd;
    Tcl_Interp *interp = p->pTree->interp;
    Tcl_HashEntry *pEntry = 0;
    HtmlImage2 *pImage = 0;

    /* Try to find the requested image in the hash table. */
    if (pImageCmd) {
        int new_entry;
        pEntry = Tcl_CreateHashEntry(&p->aImage, zUrl, &new_entry);
        if (new_entry) {
            Tcl_Obj *pEval;
            Tcl_Obj *pResult;
            int rc;
            int nObj;
            Tcl_Obj **apObj = 0;
            Tk_Image img;
           
	    /* The image could not be found in the hash table and an 
             * -imagecmd callback is configured. The callback script 
             * must be executed to obtain an image. Build up a script 
             * in pEval and execute it. Put the result in variable pResult.
             */
            pEval = Tcl_DuplicateObj(pImageCmd);
            Tcl_IncrRefCount(pEval);
            Tcl_ListObjAppendElement(interp, pEval, Tcl_NewStringObj(zUrl, -1));
            rc = Tcl_EvalObjEx(interp, pEval, TCL_EVAL_DIRECT|TCL_EVAL_GLOBAL);
            Tcl_DecrRefCount(pEval);
            if (rc != TCL_OK) {
                goto image_get_out;
            }
            pResult = Tcl_GetObjResult(interp);
    
            /* Read the result into array apObj. If the result was
             * not a valid Tcl list, return NULL and raise a background
             * error about the badly formed list.
             */
            rc = Tcl_ListObjGetElements(interp, pResult, &nObj, &apObj);
            if (rc != TCL_OK) {
                goto image_get_out;
            }
            if (nObj==0) {
                Tcl_DeleteHashEntry(pEntry);
                goto image_unavailable;
            }

            pImage = HtmlNew(HtmlImage2);
            if (nObj == 1 || nObj == 2) {
                img = Tk_GetImage(
                    interp, p->pTree->tkwin, Tcl_GetString(apObj[0]),
                    imageChanged, pImage
                );
            }
            if ((nObj != 1 && nObj != 2) || !img) {
                Tcl_ResetResult(interp);
                Tcl_AppendResult(interp,  "-imagecmd returned bad value", 0);
                HtmlFree(pImage);
                pImage = 0;
                goto image_get_out;
            }

            Tcl_SetHashValue(pEntry, (ClientData)pImage);
            Tcl_IncrRefCount(apObj[0]);
            pImage->pImageName = apObj[0];
            if (nObj == 2) {
                Tcl_IncrRefCount(apObj[1]);
                pImage->pDelete = apObj[1];
            }
            pImage->pImageServer = p;
            pImage->zUrl = Tcl_GetHashKey(&p->aImage, pEntry);
            pImage->image = img;
            Tk_SizeOfImage(pImage->image, &pImage->width, &pImage->height);
            pImage->isValid = 1;
            HtmlImagePixmap(pImage);
        }
    }

image_get_out:
    pImage = (HtmlImage2 *)(pEntry ? Tcl_GetHashValue(pEntry) : 0);
    HtmlImageRef(pImage);
    if (!pImage && pImageCmd) {
        Tcl_BackgroundError(interp);
        Tcl_ResetResult(interp);
        assert(pEntry);
        Tcl_DeleteHashEntry(pEntry);
    }

image_unavailable:
    return pImage;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlImageUnscaledName --
 *
 *     Return the name of the Tk image that this HtmlImage2 contains
 *     a (possibly scaled) version of. i.e. the name returned by 
 *     the -imagecmd script.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
Tcl_Obj *HtmlImageUnscaledName(pImage)
    HtmlImage2 *pImage;
{
    Tcl_Obj *pRet = pImage->pImageName;
    if (pImage->pUnscaled) {
        pRet = pImage->pUnscaled->pImageName;
    }
    assert(pRet);
    return pRet;
}

void
HtmlImageSize(pImage, pWidth, pHeight)
    HtmlImage2 *pImage;    /* Image object */
    int *pWidth;           /* OUT: Image width */
    int *pHeight;          /* OUT: Image height */
{
    if (pWidth) *pWidth = pImage->width;
    if (pHeight) *pHeight = pImage->height;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlImageScale --
 *
 *     This function is used to scale an image to a specified width 
 *     and height, either or both of which may be "AUTO". A pointer to the
 *     new image object (which may be the same as the first) is returned.
 *
 *     Before this function is called *pWidth and *pHeight should be set 
 *     to the desired width of the returned image in pixels, or to AUTO.
 *     When this function exits, *pWidth and *pHeight are set to the 
 *     width and height of the returned image, respectively.
 *
 *     If one of *pWidth or *pHeight is AUTO on entry, then the image is 
 *     scaled maintaining the aspect ration according to the non-AUTO 
 *     dimension. If both *pWidth and *pHeight are AUTO, then the image
 *     is not scaled.
 *
 *     if the "doScale" argument is zero, then the returned image is 
 *     not actually scaled (it is always the same as the argument image).
 *     However *pWidth and *pHeight are still set as if the image were
 *     scaled. This is used to estimate the size of layouts that will
 *     never actually be drawn (i.e. max/min width of table column content).
 *
 *     Whether or not the image returned is the same as the image passed
 *     as an argument, the caller must eventually invoke HtmlImageFree() 
 *     exactly once on the returned pointer to deallocate resources.
 *
 * Results:
 *     Scaled image object.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
HtmlImage2 *
HtmlImageScale(pImage, pWidth, pHeight, doScale)
    HtmlImage2 *pImage;    /* Image object */
    int *pWidth;           /* IN/OUT: Image width */
    int *pHeight;          /* IN/OUT: Image height */
    int doScale;           /* True to actually scale image */
{
    HtmlImage2 *pUnscaled = pImage->pUnscaled;
    HtmlImage2 *pRet;
    int w, h;

    if (!pUnscaled) {
        pUnscaled = pImage;
    }
    assert(pUnscaled && pUnscaled->isValid);

    /* If either of *pWidth or *pHeight are AUTO, set them to the
     * corresponding pixel value based on the aspect and/or size 
     * of the unscaled image. 
     */
    assert(*pWidth  == PIXELVAL_AUTO || *pWidth >= 0);
    assert(*pHeight == PIXELVAL_AUTO || *pHeight >= 0);
    if (*pWidth == PIXELVAL_AUTO && *pHeight == PIXELVAL_AUTO) {
        double rZoom = pImage->pImageServer->pTree->options.zoom;
        *pWidth = (pUnscaled->width * rZoom);
        *pHeight = (pUnscaled->height * rZoom);
    } else if (PIXELVAL_AUTO == *pWidth) {
        *pWidth = 0;
        if (pUnscaled->height) {
            *pWidth = (*pHeight * pUnscaled->width) / pUnscaled->height;
        }
    } else if (PIXELVAL_AUTO == *pHeight) {
        *pHeight = 0;
        if (pUnscaled->width) {
            *pHeight = (*pWidth * pUnscaled->height) / pUnscaled->width;
        }
    }
    w = *pWidth;
    h = *pHeight;

    if(!doScale || w == 0 || h == 0) {
        return 0;
    }

    /* Search for a scaled copy of the same unscaled image that we can use. 
     * If one cannot be found, allocate a new image. Mark it as invalid.
     */
    for (pRet = pUnscaled; pRet; pRet = pRet->pNext) {
        if (pRet->width == 0 && pRet->height == h) {
            break;
        }
        if (pRet->width == w && pRet->height == h) {
            break;
        }
    }
    if (!pRet) {
        pRet = HtmlNew(HtmlImage2);
        pRet->pImageServer = pUnscaled->pImageServer;
        pRet->zUrl = pUnscaled->zUrl;
        pRet->pNext = pUnscaled->pNext;
        pUnscaled->pNext = pRet;
        pRet->width = w;
        pRet->height = h;
        pRet->pUnscaled = pUnscaled;
        pRet->pUnscaled->nRef++;
    } 
    pRet->nRef++;

    assert(pRet->isValid == 1 || pRet->isValid == 0);
    return pRet;
}

Tk_Image
HtmlImageImage(pImage)
    HtmlImage2 *pImage;    /* Image object */
{
    assert(pImage && (pImage->isValid == 1 || pImage->isValid == 0));
    if (!pImage->isValid) {
        /* pImage->image is invalid. This happens if the underlying Tk
         * image, or the image that this is a scaled copy of, is changed
         * or deleted. It also happens the first time this function is
         * called after a call to HtmlImageScale().
         */ 
        Tk_PhotoHandle photo;
        Tk_PhotoImageBlock block;
        Tcl_Interp *interp = pImage->pImageServer->pTree->interp;
        HtmlImage2 *pUnscaled = pImage->pUnscaled;

        if (pUnscaled->pixmap) {
            Tcl_Obj *apObj[4];
            int rc;

printf("TODO: BAD. Have to recreate image to make scaled copy.\n");

            apObj[0] = pUnscaled->pImageName;
            apObj[1] = Tcl_NewStringObj("configure", -1);
            apObj[2] = Tcl_NewStringObj("-data", -1);
            apObj[3] = pUnscaled->pCompressed;

            Tcl_IncrRefCount(apObj[1]);
            Tcl_IncrRefCount(apObj[2]);
            Tcl_IncrRefCount(apObj[3]);
            pUnscaled->nIgnoreChange++;
            rc = Tcl_EvalObjv(interp, 4, apObj, TCL_EVAL_GLOBAL);
            pUnscaled->nIgnoreChange--;
            assert(rc==TCL_OK);
            Tcl_IncrRefCount(apObj[3]);
            Tcl_DecrRefCount(apObj[2]);
            Tcl_DecrRefCount(apObj[1]);
        }

        assert(pUnscaled);
        if (!pImage->pImageName) {
            /* If pImageName is still NULL, then create a new photo
             * image to write the scaled data to. Todo: Is it possible
             * to do this without invoking a script, creating the Tcl
             * command etc.?
             */
            Tk_Window win = pImage->pImageServer->pTree->tkwin;
            Tcl_Interp *interp = pImage->pImageServer->pTree->interp;
            const char *z;

            Tcl_Eval(interp, "image create photo");
            pImage->pImageName = Tcl_GetObjResult(interp);
            Tcl_IncrRefCount(pImage->pImageName);
            assert(0 == pImage->pDelete);
            assert(0 == pImage->image);

            z = Tcl_GetString(pImage->pImageName);
            pImage->image = Tk_GetImage(interp, win, z, imageChanged, pImage);
        }
        assert(pImage->image);

        CHECK_INTEGER_PLAUSIBILITY(pImage->width);
        CHECK_INTEGER_PLAUSIBILITY(pImage->height);
        CHECK_INTEGER_PLAUSIBILITY(pUnscaled->width);
        CHECK_INTEGER_PLAUSIBILITY(pUnscaled->height);

        /* Write the scaled data into image pImage->image */
        photo = Tk_FindPhoto(interp, Tcl_GetString(pUnscaled->pImageName));
        if (photo) {
            Tk_PhotoGetImage(photo, &block);
        }
        if (photo && block.pixelPtr) { 
            int x, y;                /* Iterator variables */
            int w, h;                /* Width and height of unscaled image */
            int sw, sh;              /* Width and height of scaled image */
            Tk_PhotoHandle s_photo;
            Tk_PhotoImageBlock s_block;

            sw = pImage->width;
            sh = pImage->height;
            w = pUnscaled->width;
            h = pUnscaled->height;
            s_photo = Tk_FindPhoto(interp, Tcl_GetString(pImage->pImageName));

            s_block.pixelPtr = (unsigned char *)HtmlAlloc("temp", sw * sh * 4);
            s_block.width = sw;
            s_block.height = sh;
            s_block.pitch = sw * 4;
            s_block.pixelSize = 4;
            s_block.offset[0] = 0;
            s_block.offset[1] = 1;
            s_block.offset[2] = 2;
            s_block.offset[3] = 3;

            for (x=0; x<sw; x++) {
                int orig_x = ((x * w) / sw);
                for (y=0; y<sh; y++) {
                    unsigned char *zOrig;
                    unsigned char *zScale;
                    int orig_y = ((y * h) / sh);

                    zOrig = &block.pixelPtr[
                        orig_x * block.pixelSize + orig_y * block.pitch];
                    zScale = &s_block.pixelPtr[
                        x * s_block.pixelSize + y * s_block.pitch];

                    zScale[0] = zOrig[block.offset[0]];
                    zScale[1] = zOrig[block.offset[1]];
                    zScale[2] = zOrig[block.offset[2]];
                    zScale[3] = zOrig[block.offset[3]];
                }
            }
            photoputblock(interp, s_photo, &s_block, 0, 0, sw, sh, 0);
            HtmlFree(s_block.pixelPtr);
        } else {
            return HtmlImageImage(pImage->pUnscaled);
        }

        pImage->isValid = 1;
        if (pUnscaled->pixmap) {
            Tcl_Obj *apObj[4];

            apObj[0] = Tcl_NewStringObj("image", -1);
            apObj[1] = Tcl_NewStringObj("create", -1);
            apObj[2] = Tcl_NewStringObj("photo", -1);
            apObj[3] = pUnscaled->pImageName;

            Tcl_IncrRefCount(apObj[0]);
            Tcl_IncrRefCount(apObj[1]);
            Tcl_IncrRefCount(apObj[2]);
            pUnscaled->nIgnoreChange++;
            Tcl_EvalObjv(interp, 4, apObj, TCL_EVAL_GLOBAL);
            pUnscaled->nIgnoreChange--;
            Tcl_DecrRefCount(apObj[2]);
            Tcl_DecrRefCount(apObj[1]);
            Tcl_IncrRefCount(apObj[0]);
        }
    }

    return pImage->image;
}

#define N_TILE_PIXELS 4000

/*
 *---------------------------------------------------------------------------
 *
 * tilesize --
 *
 * Results:
 *     True if the image should use a tile, false otherwise. If true
 *     is returned, *pW and *pH are set to the pixel height and width
 *     of the required tile.
 *
 * Side effects:
 *     
 *
 *---------------------------------------------------------------------------
 */
static int tilesize(pImage, pW, pH)
    HtmlImage2* pImage;
    int *pW;
    int *pH;
{
    int iImageWidth = pImage->width;
    int iImageHeight = pImage->height;

    int xmul = 1;
    int ymul = 1;

    if (iImageWidth * iImageHeight > N_TILE_PIXELS) return 0;

    /* Figure out the eventual width and height of the tile. */
    while ((iImageWidth * iImageHeight * xmul * ymul) < N_TILE_PIXELS) {
        xmul = xmul + xmul;
        ymul = ymul + ymul;
    }

    *pW = pImage->width * xmul;
    *pH = pImage->height * ymul;
    return 1;
}

Pixmap
HtmlImageTilePixmap(pImage, pW, pH)
    HtmlImage2* pImage;
    int *pW;
    int *pH;
{
    if (HtmlImagePixmap(pImage)) {
        Tk_Window win;
        XGCValues gc_values;
        GC gc;
        int i, j;

        if( pImage->tilepixmap ){
            goto return_tile; 
        }

        if (!tilesize(pImage, &pImage->iTileWidth, &pImage->iTileHeight)) {
            goto return_original;
        }

        win = pImage->pImageServer->pTree->tkwin;
        pImage->tilepixmap = Tk_GetPixmap(Tk_Display(win), Tk_WindowId(win),
            pImage->iTileWidth, pImage->iTileHeight, Tk_Depth(win)
        );

        memset(&gc_values, 0, sizeof(XGCValues));
        gc = Tk_GetGC(win, 0, &gc_values);
        for (i = 0; i < pImage->iTileWidth; i += pImage->width){
            for (j = 0; j < pImage->iTileHeight; j += pImage->height){
                XCopyArea(Tk_Display(win), 
                     pImage->pixmap, pImage->tilepixmap, gc, 0, 0, 
                     pImage->width, pImage->height, 
                     i, j
                );
            }
        }
        Tk_FreeGC(Tk_Display(win), gc);
    }

return_tile:
    *pW = pImage->iTileWidth;
    *pH = pImage->iTileHeight;
    return pImage->tilepixmap;

return_original:
    *pW = pImage->width;
    *pH = pImage->height;
    return pImage->pixmap;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlImagePixmap --
 *
 * Results:
 *     Pixmap. Or zero.
 *
 * Side effects:
 *     May change the image storage to pixmap.
 *
 *---------------------------------------------------------------------------
 */
Pixmap
HtmlImagePixmap(pImage)
    HtmlImage2* pImage;
{
    if (!pImage->pImageServer->pTree->options.imagepixmapify ||
        !pImage->pImageName ||
        !getImageCompressed(pImage) ||
        pImage->width<=0 ||
        pImage->height<=0
    ) {
        return 0;
    }
    if (!pImage->isValid) {
        HtmlImageImage(pImage);
    }
    if (!pImage->pixmap && !HtmlImageAlphaChannel(pImage)) {
        Tk_Window win = pImage->pImageServer->pTree->tkwin;
        Tcl_Interp *interp = pImage->pImageServer->pTree->interp;

        Pixmap pix;
        int rc;
        Tcl_Obj *pGetData;

#if 0
printf("Pixmapifying - nData = %d\n", nData);
#endif

        pix = Tk_GetPixmap(Tk_Display(win), Tk_WindowId(win),
            pImage->width, pImage->height, Tk_Depth(win)
        );
        Tk_RedrawImage(
            pImage->image, 0, 0, pImage->width, pImage->height, pix, 0, 0
        );

        pImage->pixmap = pix;

        pGetData = Tcl_NewObj();
        Tcl_IncrRefCount(pGetData);
        Tcl_ListObjAppendElement(0, pGetData, Tcl_NewStringObj("image",-1));
        Tcl_ListObjAppendElement(0, pGetData, Tcl_NewStringObj("create",-1));
        Tcl_ListObjAppendElement(0, pGetData, Tcl_NewStringObj("photo",-1));
        Tcl_ListObjAppendElement(0, pGetData, pImage->pImageName);
        pImage->nIgnoreChange++;
        rc = Tcl_EvalObjEx(interp, pGetData, TCL_EVAL_GLOBAL|TCL_EVAL_DIRECT);
        pImage->nIgnoreChange--;
        Tcl_DecrRefCount(pGetData);
        assert(rc==TCL_OK);
    }
    return pImage->pixmap;
}

void 
HtmlImageFree(pImage)
    HtmlImage2 *pImage;
{
    if (!pImage) {
        return;
    }

    assert(pImage->nRef > 0);
    pImage->nRef--;
    if (
        pImage->nRef == 0 && 
        (pImage->pUnscaled || !pImage->pImageServer->isSuspendGC)
    ) {
        /* The reference count for this structure has reached zero.
         * Really delete it. The assert() says that an original image
         * cannot be deleted before all of it's scaled copies.
         */
        assert(pImage->pUnscaled || 0 == pImage->pNext);

        freeImageCompressed(pImage);
        freeTile(pImage);
        if (pImage->pixmap) {
            HtmlTree *pTree = pImage->pImageServer->pTree;
            Tk_FreePixmap(Tk_Display(pTree->tkwin), pImage->pixmap);
            pImage->pixmap = 0;
        }
        if (pImage->image) {
            Tk_FreeImage(pImage->image);
        }
        if (pImage->pImageName) {
            Tcl_Interp *interp = pImage->pImageServer->pTree->interp;
            Tcl_Obj *pEval;
            if (!pImage->pDelete) {
                pEval = Tcl_NewStringObj("image delete", -1);
                Tcl_IncrRefCount(pEval);
            } else {
                pEval = pImage->pDelete;
            }
            Tcl_ListObjAppendElement(interp, pEval, pImage->pImageName);
            Tcl_EvalObjEx(interp, pEval, TCL_EVAL_GLOBAL|TCL_EVAL_DIRECT);
            Tcl_DecrRefCount(pEval);
            Tcl_DecrRefCount(pImage->pImageName);
        }

        if (pImage->pUnscaled) {
            HtmlImage2 *pIter;
            for (
                pIter = pImage->pUnscaled; 
                pIter->pNext != pImage; 
                pIter = pIter->pNext
            ) {
                assert(pIter->pNext);
            }
            pIter->pNext = pIter->pNext->pNext;
            HtmlImageFree(pImage->pUnscaled);
        } else {
            const char *zKey = pImage->zUrl;
            Tcl_HashTable *paImage = &pImage->pImageServer->aImage;
            Tcl_HashEntry *pEntry = Tcl_FindHashEntry(paImage, zKey);
            assert(pEntry);
            Tcl_DeleteHashEntry(pEntry);
        }

        HtmlFree(pImage);
        Tcl_CancelIdleCall(asyncPixmapify, (ClientData)pImage);
    }
}

void 
HtmlImageRef(pImage)
    HtmlImage2 *pImage;
{
    if (pImage) {
        pImage->nRef++;
    }
}

const char *
HtmlImageUrl(pImage)
    HtmlImage2 *pImage;
{
    return pImage->zUrl;
}

void HtmlImageCheck(pImage)
    HtmlImage2 *pImage;
{
    if (pImage) {
        assert(pImage->isValid == 0 || pImage->isValid == 1);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlImageAlphaChannel --
 *
 * Results:
 *
 *     1 if there are one or more pixels in the image with an alpha
 *     alpha-channel value of other than 100%. Otherwise 0.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlImageAlphaChannel(pImage)
    HtmlImage2 *pImage;
{
    HtmlImage2 *p = (pImage->pUnscaled ? pImage->pUnscaled : pImage);
    if (p->eAlpha == ALPHA_CHANNEL_UNKNOWN) {
        HtmlTree *pTree= pImage->pImageServer->pTree;
        Tk_PhotoHandle photo;
        Tk_PhotoImageBlock block;
        int x, y;

        int w = p->width;
        int h = p->height;

        Tcl_Obj *pCompressed = getImageCompressed(pImage);
        unsigned char *zCompressed;
        int nCompressed;
        int i;
        assert(pCompressed);

        zCompressed = Tcl_GetByteArrayFromObj(pCompressed, &nCompressed);
        for(i = 0; 1 && i < 16 && i < (nCompressed-4); i++){
            if (zCompressed[i] == 'J' && 
                zCompressed[i+1] == 'F' && 
                zCompressed[i+2] == 'I' && 
                zCompressed[i+3] == 'F'
            ) {
                p->eAlpha = ALPHA_CHANNEL_FALSE;
                return 0;
            }
        }
 
        p->eAlpha = ALPHA_CHANNEL_FALSE;
        photo = Tk_FindPhoto(pTree->interp, Tcl_GetString(p->pImageName));
        if (!photo) return 0;
        Tk_PhotoGetImage(photo, &block);

        if (!block.pixelPtr) return 0;

        for (y = 0; y < h; y++) {
            unsigned char *z = &block.pixelPtr[block.pitch*y+block.offset[3]];
            for (x = 0; x < w; x++) {
                if (*z != 255) {
                    p->eAlpha = ALPHA_CHANNEL_TRUE;
                    return 1;
                }
		z += block.pixelSize;
            }
        }
    }

    return ((p->eAlpha == ALPHA_CHANNEL_TRUE) ? 1 : 0);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlImageTile --
 *
 * Results:
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
Tk_Image 
HtmlImageTile(pImage, pW, pH)
    HtmlImage2 *pImage;    /* Image object */
    int *pW;
    int *pH;
{
    HtmlTree *pTree = pImage->pImageServer->pTree;
    Tcl_Interp *interp = pTree->interp;

    Tcl_Obj *pTileName;             /* Name of tile image at the script level */
    Tk_PhotoHandle tilephoto;       /* Photo of tile */
    Tk_PhotoImageBlock tileblock;   /* Block of tile image */
    int iTileWidth;
    int iTileHeight;

    Tk_PhotoHandle origphoto;
    Tk_PhotoImageBlock origblock;

    int x;
    int y;

    /* The tile has already been generated. Return it. */
    if (pImage->pTileName) {
        goto return_tile;
    }

    /* The image is too big to bother with a tile. Return the original. */
    if (!tilesize(pImage, &iTileWidth, &iTileHeight)) {
        goto return_original;
    }

    /* Retrieve the block for the original image */
    origphoto = Tk_FindPhoto(interp, Tcl_GetString(pImage->pImageName));
    if (!origphoto) goto return_original;
    Tk_PhotoGetImage(origphoto, &origblock);
    if (!origblock.pixelPtr) goto return_original;

    /* Create the tile image. Surely there is a way to do this without
     * invoking a script, but I haven't found it yet.
     */
    Tcl_Eval(interp, "image create photo");
    pTileName = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(pTileName);
    tilephoto = Tk_FindPhoto(interp, Tcl_GetString(pTileName));
    Tk_PhotoGetImage(tilephoto, &tileblock);
    pImage->pTileName = pTileName;
    pImage->tile = Tk_GetImage(
            interp, pTree->tkwin, Tcl_GetString(pTileName), imageChanged, 0
    );

    /* Allocate a block to write the tile data into. */
    tileblock.pixelPtr = (unsigned char *)HtmlAlloc(
        "temp", iTileWidth * iTileHeight * 4
    );
    tileblock.width = iTileWidth;
    tileblock.height = iTileHeight;
    tileblock.pitch = iTileWidth * 4;
    tileblock.pixelSize = 4;
    tileblock.offset[0] = 0;
    tileblock.offset[1] = 1;
    tileblock.offset[2] = 2;
    tileblock.offset[3] = 3;

    for (x = 0; x < iTileWidth; x++) {
        for (y = 0; y < iTileHeight; y++) {
            unsigned char *zOrig;
            unsigned char *zScale;
            zOrig = &origblock.pixelPtr[
                 (x % pImage->width) *origblock.pixelSize + 
                 (y % pImage->height) * origblock.pitch
            ];
            zScale = &tileblock.pixelPtr[x * 4 + y * tileblock.pitch];
            zScale[0] = zOrig[origblock.offset[0]];
            zScale[1] = zOrig[origblock.offset[1]];
            zScale[2] = zOrig[origblock.offset[2]];
            zScale[3] = zOrig[origblock.offset[3]];
        }
    }

    photoputblock(interp,tilephoto,&tileblock,0,0,iTileWidth,iTileHeight,0);
    HtmlFree(tileblock.pixelPtr);
    pImage->iTileWidth = iTileWidth;
    pImage->iTileHeight = iTileHeight;

return_tile:
    *pW = pImage->iTileWidth;
    *pH = pImage->iTileHeight;
    return pImage->tile;

return_original:
    HtmlImageSize(pImage, pW, pH);
    return HtmlImageImage(pImage);
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlImageServerSuspendGC --
 *
 *     Put the image-server into a mode where it will not unload an
 *     unscaled (i.e. original) image, even if it's reference count
 *     drops to zero. The caller must ensure that HtmlImageServerDoGC()
 *     is called at some later point to garbage collect images 
 *     with zero ref-counts.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
void HtmlImageServerSuspendGC(pTree)
    HtmlTree *pTree;
{
    pTree->pImageServer->isSuspendGC = 1;
}

void HtmlImageServerDoGC(pTree)
    HtmlTree *pTree;
{
    if (pTree->pImageServer->isSuspendGC) {
        int nDelete;
        pTree->pImageServer->isSuspendGC = 0;
        do {
            int ii;
            HtmlImage2 *apDelete[32];
            Tcl_HashSearch srch;
            Tcl_HashEntry *pEntry;

            nDelete = 0;
            pEntry = Tcl_FirstHashEntry(&pTree->pImageServer->aImage, &srch);
            for ( ; nDelete < 32 && pEntry; pEntry = Tcl_NextHashEntry(&srch)) {
                HtmlImage2 *p = Tcl_GetHashValue(pEntry);
                if (p->nRef == 0) {
                    apDelete[nDelete++] = p;
                }
            }

            for (ii = 0; ii < nDelete; ii++) {
                HtmlImage2 *p = apDelete[ii];
                p->nRef = 1;
                HtmlImageFree(p);
            }
        } while (nDelete == 32);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlImageServerCount --
 *
 *     Return the number of images currently stored in the image-server.
 *     Only images returned by the -imagecmd script are counted, not 
 *     scaled or tiled copies thereof.
 *
 * Results:
 *     Number of images.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int HtmlImageServerCount(pTree)
    HtmlTree *pTree;
{
    int nImage = 0;
    Tcl_HashSearch srch;
    Tcl_HashEntry *pEntry;

    pEntry = Tcl_FirstHashEntry(&pTree->pImageServer->aImage, &srch);
    for ( ; pEntry; pEntry = Tcl_NextHashEntry(&srch)) {
        nImage++;
    }
   
    return nImage;
}

/*
 *---------------------------------------------------------------------------
 *
 * HtmlXImageToImage --
 *
 *     This is a utility procedure to convert from an XImage to a Tk 
 *     image that can be used at the script level (this function is called
 *     by the [widget image] command implementation).
 *
 * Results:
 *     A pointer to a new Tcl object with a ref-count of 1 containing the
 *     name of the new Tk image. It is the responsibility of the caller
 *     to call Tcl_DecrRefCount() on the returned value, and to eventually
 *     free the image via [image delete <image-name>] or equivalent C 
 *     calls, where <image-name> is the string contained in the 
 *     returned object.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#ifndef WIN32
#include <X11/Xutil.h>
Tcl_Obj *HtmlXImageToImage(pTree, pXImage, w, h)
    HtmlTree *pTree;
    XImage *pXImage;
    int w;
    int h;
{
    Tcl_Interp *interp = pTree->interp;

    Tcl_Obj *pImage;
    Tk_PhotoHandle photo;
    Tk_PhotoImageBlock block;
    int x;
    int y;
    unsigned long redmask, redshift;
    unsigned long greenmask, greenshift;
    unsigned long bluemask, blueshift;
    Visual *pVisual;

    Tcl_Eval(interp, "image create photo");
    pImage = Tcl_GetObjResult(interp);
    Tcl_IncrRefCount(pImage);

    block.pixelPtr = (unsigned char *)HtmlAlloc("temp", w * h * 4);
    block.width = w;
    block.height = h;
    block.pitch = w*4;
    block.pixelSize = 4;
    block.offset[0] = 0;
    block.offset[1] = 1;
    block.offset[2] = 2;
    block.offset[3] = 3;

    pVisual = Tk_Visual(pTree->tkwin);

    redmask = pVisual->red_mask;
    bluemask = pVisual->blue_mask;
    greenmask = pVisual->green_mask;
    for (redshift=0; !((redmask>>redshift)&0x000000001); redshift++);
    for (greenshift=0; !((greenmask>>greenshift)&0x00000001); greenshift++);
    for (blueshift=0; !((bluemask>>blueshift)&0x00000001); blueshift++);

    for (x=0; x<w; x++) {
        for (y=0; y<h; y++) {
            unsigned char *pOut;
            unsigned long pixel = XGetPixel(pXImage, x, y);

            pOut = &block.pixelPtr[x*block.pixelSize + y*block.pitch];
            pOut[0] = (pixel&redmask)>>redshift;
            pOut[1] = (pixel&greenmask)>>greenshift;
            pOut[2] = (pixel&bluemask)>>blueshift;
            pOut[3] = 0xFF;
        }
    }

    photo = Tk_FindPhoto(interp, Tcl_GetString(pImage));
    photoputblock(interp, photo, &block, 0, 0, w, h, 0);
    HtmlFree(block.pixelPtr);

    return pImage;
}
#else
Tcl_Obj *HtmlXImageToImage(pTree, pXImage, w, h)
    HtmlTree *pTree;
    XImage *pXImage;
    int w;
    int h;
{
    return 0;
}
#endif

/*
 *---------------------------------------------------------------------------
 *
 * HtmlImageServerReport --
 *
 *     For performance analysis. Returns a listing of the image server
 *     contents.
 *
 * Results:
 * 
 *     A list containing a single element for each HtmlImage2 structure
 *     managed by this image server. The format of each element is itself
 *     a list of the following form:
 *     
 *       { <url> <image name> <pixmapified> <width> <height> <alpha> <refs>}
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
int 
HtmlImageServerReport(clientData, interp, objc, objv)
    ClientData clientData;             /* The HTML widget data structure */
    Tcl_Interp *interp;                /* Current interpreter. */
    int objc;                          /* Number of arguments. */
    Tcl_Obj *CONST objv[];             /* Argument strings. */
{
    HtmlTree *pTree = (HtmlTree *)clientData;

    Tcl_HashSearch search;
    Tcl_HashEntry *pEntry;
    Tcl_Obj *pRet = Tcl_NewObj();
  
    for (
        pEntry = Tcl_FirstHashEntry(&pTree->pImageServer->aImage, &search); 
        pEntry; 
        pEntry = Tcl_NextHashEntry(&search)
    ) {
      HtmlImage2 *pImage = (HtmlImage2 *)Tcl_GetHashValue(pEntry);
      for( ; pImage; pImage = pImage->pNext){
        Tcl_Obj *p = Tcl_NewObj();
        const char *zUrl = "";
        if( !pImage->pUnscaled ){
          zUrl = pImage->zUrl;
        }
        Tcl_ListObjAppendElement(interp, p, Tcl_NewStringObj(zUrl, -1));
	if (pImage->pImageName) {
            Tcl_ListObjAppendElement(interp, p, pImage->pImageName);
        } else {
            Tcl_ListObjAppendElement(interp, p, Tcl_NewStringObj("", -1));
        }
        Tcl_ListObjAppendElement(interp, p, Tcl_NewStringObj(
            pImage->pixmap?"PIX":"", -1));
        Tcl_ListObjAppendElement(interp, p, Tcl_NewIntObj(pImage->width));
        Tcl_ListObjAppendElement(interp, p, Tcl_NewIntObj(pImage->height));
        Tcl_ListObjAppendElement(interp, p, Tcl_NewStringObj(
          pImage->eAlpha==ALPHA_CHANNEL_UNKNOWN?"unknown":
          pImage->eAlpha==ALPHA_CHANNEL_TRUE?"true":
          pImage->eAlpha==ALPHA_CHANNEL_FALSE?"false":"internal error!", -1));
        Tcl_ListObjAppendElement(interp, p, Tcl_NewIntObj(pImage->nRef));

        Tcl_ListObjAppendElement(interp, pRet, p);
      }
    }

    Tcl_SetObjResult(interp, pRet);
    return TCL_OK;
}
