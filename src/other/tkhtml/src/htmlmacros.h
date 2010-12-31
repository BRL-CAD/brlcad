/*
 * htmlmacros.h --
 *
 *     This file contains functions that are implemented as macros to 
 *     speed things up. At present, this includes the following:
 *
 *         HtmlNodeIsText()
 *         HtmlNodeAsText()
 *         HtmlNodeAsElement()
 *         HtmlNodeComputedValues()
 *
 *--------------------------------------------------------------------------
 * Copyright (c) 2005 Dan Kennedy.
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
 *     * Neither the name of Eolas Technologies Inc. nor the names of its
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
#ifndef __HTMLMACROS_H__
#define __HTMLMACROS_H__

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeIsText --
 *
 *     Return non-zero if the node is a (possibly empty) text node, or zero
 *     otherwise.
 *
 * Results:
 *     Non-zero if the node is text, else zero.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#define HtmlNodeIsText(pNode) \
    ((pNode)->eTag == Html_Text)

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeIsWhitespace --
 *
 *     Return non-zero if the node is a text node that consists entirely
 *     of white-space.
 *
 * Results:
 *     Non-zero if the node is white-space, else zero.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#define HtmlNodeIsWhitespace(pNode) \
    (HtmlNodeIsText(pNode) && ((HtmlTextNode *)(pNode))->zText == 0)

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeAsElement --
 *
 *     This function is used as a safe cast from (HtmlNode*) to
 *     (HtmlElementNode*). If the node is actually a text node, NULL is
 *     returned. Otherwise, an (HtmlElementNode*) is returned.
 *
 * Results:
 *     Non-zero if the node is an element, else zero.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#define HtmlNodeAsElement(pNode) \
    (HtmlNodeIsText(pNode) ? 0 : (HtmlElementNode *)(pNode))

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeAsText --
 *
 *     This function is used as a safe cast from (HtmlNode *) to
 *     (HtmlTextNode *). If the node is actually a text node, NULL is
 *     returned. Otherwise, an (HtmlTextNode *) is returned.
 *
 * Results:
 *     Non-zero if the node is a text node, else zero.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#define HtmlNodeAsText(pNode) \
    (HtmlNodeIsText(pNode) ? (HtmlTextNode *)(pNode) : 0)

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeComputedValues --
 *
 *     Return a pointer to the HtmlComputedValues structure that contains
 *     computed property values for the argument node. If the argument is
 *     a text node, this will be the properties of the parent.
 *
 * Results:
 *     Pointer to HtmlComputedValues structure.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#define HtmlNodeComputedValues(pNode)                                   \
    (HtmlNodeIsText(pNode) ?                                            \
        ((HtmlElementNode *)((pNode)->pParent))->pPropertyValues :      \
        ((HtmlElementNode *)(pNode))->pPropertyValues                   \
    )

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeParent --
 *
 *     Get the parent of the current node.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#define HtmlNodeParent(pNode) \
    (((HtmlNode *)(pNode))->pParent)

/*
 *---------------------------------------------------------------------------
 *
 * HtmlNodeChild --
 *
 *     Return the Nth child of node pNode (numbered from 0). It is assumed 
 *     that pNode is an HtmlNodeElement with at least N children.
 *
 * Results:
 *     Pointer to child node.
 *
 * Side effects:
 *     None.
 *
 *---------------------------------------------------------------------------
 */
#define HtmlNodeChild(pNode, N) \
    (((HtmlElementNode *)pNode)->apChildren[N])

#endif
