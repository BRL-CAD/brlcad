/**
 * @file libwin/scrolbar.c
 *
 * Yori window scroll bar control
 *
 * Copyright (c) 2019-2024 Malcolm J. Smith
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "yoripch.h"
#include "yorilib.h"
#include "yoriwin.h"
#include "winpriv.h"

/**
 A structure describing the contents of a scroll bar control.
 */
typedef struct _YORI_WIN_CTRL_SCROLLBAR {

    /**
     A common header for all controls
     */
    YORI_WIN_CTRL Ctrl;

    /**
     The maximum value that the scrollbar will report when it has been
     scrolled to the end.
     */
    YORI_MAX_UNSIGNED_T MaximumValue;

    /**
     The current value of the position within the scrollbar.
     */
    YORI_MAX_UNSIGNED_T CurrentValue;

    /**
     The range of values that are visible at any one time.
     */
    YORI_MAX_UNSIGNED_T NumberVisible;

    /**
     A function to invoke when the scroll bar value is changed via any
     mechanism.
     */
     PYORI_WIN_NOTIFY ChangeCallback;

    /**
     Indicates if the scroll bar is in a vertical or horizontal orientation.
     */
    enum {
        YoriWinScrollbarHorizontal = 1,
        YoriWinScrollbarVertical = 2
    } Alignment;

} YORI_WIN_CTRL_SCROLLBAR, *PYORI_WIN_CTRL_SCROLLBAR;

/**
 Returns the height of the control for a vertical scroll bar, or the width of
 the control for a horizontal scroll bar.

 @param ScrollBar Pointer to the scroll bar control.

 @return The size of the control.
 */
WORD
YoriWinScrollBarGetClientSize(
    __in PYORI_WIN_CTRL_SCROLLBAR ScrollBar
    )
{
    WORD ClientSize;

    if (ScrollBar->Alignment == YoriWinScrollbarHorizontal) {
        ClientSize = (WORD)(ScrollBar->Ctrl.ClientRect.Right - ScrollBar->Ctrl.ClientRect.Left + 1);
    } else {
        ClientSize = (WORD)(ScrollBar->Ctrl.ClientRect.Bottom - ScrollBar->Ctrl.ClientRect.Top + 1);
    }

    return ClientSize;
}

/**
 A helper function to calculate how many entries are represented in each
 character cell.

 MSFIX This needs a bit of cleanup.  It's an attempt to use the same logic
 when rendering as when selecting, but breaking out pieces further would
 be good because rendering needs to know some of the details here.

 @param ScrollBar Pointer to the scroll bar control.

 @return The number of values that are represented by each character cell.
 */
YORI_MAX_UNSIGNED_T
YoriWinScrollBarValueCountPerCell(
    __in PYORI_WIN_CTRL_SCROLLBAR ScrollBar
    )
{
    WORD ClientSize;
    WORD NumberPositionCells;
    WORD NumberSelectedPositionCells;

    // MSFIX Remove duplication with the below

    ClientSize = YoriWinScrollBarGetClientSize(ScrollBar);
    NumberPositionCells = (WORD)(ClientSize - 2);
    if (ScrollBar->MaximumValue == 0) {
        NumberSelectedPositionCells = NumberPositionCells;
    } else {
        NumberSelectedPositionCells = (WORD)(ScrollBar->NumberVisible * NumberPositionCells / (ScrollBar->MaximumValue + ScrollBar->NumberVisible));
    }

    if (NumberSelectedPositionCells == 0) {
        NumberSelectedPositionCells = 1;
    } else if (NumberSelectedPositionCells > NumberPositionCells) {
        NumberSelectedPositionCells = NumberPositionCells;
    }

    return (ScrollBar->MaximumValue + ScrollBar->NumberVisible) / (NumberPositionCells + 1);
}

/**
 Draw the scroll bar with its current state applied.

 @param ScrollBar Pointer to the scoll bar to draw.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOLEAN
YoriWinScrollBarPaint(
    __in PYORI_WIN_CTRL_SCROLLBAR ScrollBar
    )
{
    WORD WindowAttributes;
    WORD ClientSize;
    WORD NumberPositionCells;
    WORD FirstSelectedPositionCell;
    WORD NumberSelectedPositionCells;
    WORD Index;
    YORI_MAX_UNSIGNED_T ValueCountPerCell;
    CONST TCHAR* ScrollChars;
    PYORI_WIN_WINDOW_MANAGER_HANDLE WinMgrHandle;

    WindowAttributes = ScrollBar->Ctrl.DefaultAttributes;

    WinMgrHandle = YoriWinGetWindowManagerHandle(YoriWinGetTopLevelWindow(&ScrollBar->Ctrl));
    ScrollChars = YoriWinGetDrawingCharacters(WinMgrHandle, YoriWinCharsScrollBar);

    ClientSize = YoriWinScrollBarGetClientSize(ScrollBar);
    NumberPositionCells = (WORD)(ClientSize - 2);
    if (ScrollBar->MaximumValue == 0) {
        NumberSelectedPositionCells = NumberPositionCells;
    } else {
        NumberSelectedPositionCells = (WORD)(ScrollBar->NumberVisible * NumberPositionCells / (ScrollBar->MaximumValue + ScrollBar->NumberVisible));
    }
    if (NumberSelectedPositionCells == 0) {
        NumberSelectedPositionCells = 1;
    } else if (NumberSelectedPositionCells > NumberPositionCells) {
        NumberSelectedPositionCells = NumberPositionCells;
    }

    ValueCountPerCell = YoriWinScrollBarValueCountPerCell(ScrollBar);

    if (ValueCountPerCell == 0) {
        FirstSelectedPositionCell = 0;
    } else {
        FirstSelectedPositionCell = (WORD)(ScrollBar->CurrentValue / ValueCountPerCell);
    }

    if (FirstSelectedPositionCell + NumberSelectedPositionCells > NumberPositionCells) {

        FirstSelectedPositionCell = (WORD)(NumberPositionCells - NumberSelectedPositionCells);
    }

    if (ScrollBar->Alignment == YoriWinScrollbarHorizontal) {
        YoriWinSetControlClientCell(&ScrollBar->Ctrl, 0, 0, ScrollChars[1], WindowAttributes);
        for (Index = 0; Index < NumberPositionCells; Index++) {
            if (Index >= FirstSelectedPositionCell && Index < FirstSelectedPositionCell + NumberSelectedPositionCells) {
                YoriWinSetControlClientCell(&ScrollBar->Ctrl, (WORD)(1 + Index), 0, ScrollChars[2], WindowAttributes);
            } else {
                YoriWinSetControlClientCell(&ScrollBar->Ctrl, (WORD)(1 + Index), 0, ScrollChars[3], WindowAttributes);
            }
        }
        YoriWinSetControlClientCell(&ScrollBar->Ctrl, (WORD)(1 + Index), 0, ScrollChars[5], WindowAttributes);
    } else {
        YoriWinSetControlClientCell(&ScrollBar->Ctrl, 0, 0, ScrollChars[0], WindowAttributes);
        for (Index = 0; Index < NumberPositionCells; Index++) {
            if (Index >= FirstSelectedPositionCell && Index < FirstSelectedPositionCell + NumberSelectedPositionCells) {
                YoriWinSetControlClientCell(&ScrollBar->Ctrl, 0, (WORD)(1 + Index), ScrollChars[2], WindowAttributes);
            } else {
                YoriWinSetControlClientCell(&ScrollBar->Ctrl, 0, (WORD)(1 + Index), ScrollChars[3], WindowAttributes);
            }
        }
        YoriWinSetControlClientCell(&ScrollBar->Ctrl, 0, (WORD)(1 + Index), ScrollChars[4], WindowAttributes);
    }

    return TRUE;
}

/**
 Return TRUE if the specified location contains the up or left arrow,
 indicating the value should be decreased.  Returns FALSE for any other
 cell.

 @param ScrollBar Pointer to the scroll bar control.

 @param Location Specifies the location to test.

 @return TRUE to indicate the up or left arrow resides at Location.
 */
BOOLEAN
YoriWinScrollBarIsLocationSmallerButton(
    __in PYORI_WIN_CTRL_SCROLLBAR ScrollBar,
    __in PCOORD Location
    )
{
    if (ScrollBar->Alignment == YoriWinScrollbarHorizontal) {
        if (Location->X == 0) {
            return TRUE;
        }
    } else {
        if (Location->Y == 0) {
            return TRUE;
        }
    }

    return FALSE;
}

/**
 Return TRUE if the specified location contains the down or right arrow,
 indicating the value should be increased.  Returns FALSE for any other
 cell.

 @param ScrollBar Pointer to the scroll bar control.

 @param Location Specifies the location to test.

 @return TRUE to indicate the down or right arrow resides at Location.
 */
BOOLEAN
YoriWinScrollBarIsLocationLargerButton(
    __in PYORI_WIN_CTRL_SCROLLBAR ScrollBar,
    __in PCOORD Location
    )
{
    if (ScrollBar->Alignment == YoriWinScrollbarHorizontal) {
        if (Location->X == ScrollBar->Ctrl.ClientRect.Right) {
            return TRUE;
        }
    } else {
        if (Location->Y == ScrollBar->Ctrl.ClientRect.Bottom) {
            return TRUE;
        }
    }

    return FALSE;
}


/**
 Process input events for a scroll bar control.

 @param Ctrl Pointer to the scroll bar control.

 @param Event Pointer to the input event.

 @return TRUE to indicate that the event was processed and no further
         processing should occur.  FALSE to indicate that regular processing
         should continue (although this does not imply that no processing
         has already occurred.)
 */
BOOLEAN
YoriWinScrollBarEventHandler(
    __in PYORI_WIN_CTRL Ctrl,
    __in PYORI_WIN_EVENT Event
    )
{
    PYORI_WIN_CTRL_SCROLLBAR ScrollBar;
    ScrollBar = CONTAINING_RECORD(Ctrl, YORI_WIN_CTRL_SCROLLBAR, Ctrl);
    switch(Event->EventType) {
        case YoriWinEventKeyDown:
            // MSFIX Generically a scrollbar should handle up/down etc,
            // but right now it's not being used as a top level control
            // and this is handled by list, so it's not functionally
            // reachable
            break;
        case YoriWinEventParentDestroyed:
            YoriWinDestroyControl(Ctrl);
            YoriLibDereference(ScrollBar);
            break;
        case YoriWinEventMouseDownInClient:
        case YoriWinEventMouseDownInNonClient:
            if (YoriWinScrollBarIsLocationSmallerButton(ScrollBar, &Event->MouseDown.Location)) {
                if (ScrollBar->CurrentValue > 0) {
                    ScrollBar->CurrentValue--;
                    if (ScrollBar->ChangeCallback != NULL) {
                        ScrollBar->ChangeCallback(Ctrl);
                    }
                    YoriWinScrollBarPaint(ScrollBar);
                }
            } else if (YoriWinScrollBarIsLocationLargerButton(ScrollBar, &Event->MouseDown.Location)) {
                if (ScrollBar->CurrentValue < ScrollBar->MaximumValue) {
                    ScrollBar->CurrentValue++;
                    if (ScrollBar->ChangeCallback != NULL) {
                        ScrollBar->ChangeCallback(Ctrl);
                    }
                    YoriWinScrollBarPaint(ScrollBar);
                }
            } else {
                WORD ClientSize;
                WORD NumberPositionCells;
                WORD ClickedPositionCell;
                YORI_MAX_UNSIGNED_T ValueCountPerCell;

                ClientSize = YoriWinScrollBarGetClientSize(ScrollBar);
                NumberPositionCells = (WORD)(ClientSize - 2);
                if (ScrollBar->Alignment == YoriWinScrollbarHorizontal) {
                    ClickedPositionCell = (WORD)(Event->MouseDown.Location.X - 1);
                } else {
                    ClickedPositionCell = (WORD)(Event->MouseDown.Location.Y - 1);
                }
                ASSERT(ClickedPositionCell <= NumberPositionCells);
                ValueCountPerCell = YoriWinScrollBarValueCountPerCell(ScrollBar);

                ScrollBar->CurrentValue = ValueCountPerCell * ClickedPositionCell;
                if (ScrollBar->CurrentValue > ScrollBar->MaximumValue) {
                    ScrollBar->CurrentValue = ScrollBar->MaximumValue;
                }

                if (ScrollBar->ChangeCallback != NULL) {
                    ScrollBar->ChangeCallback(Ctrl);
                }
                YoriWinScrollBarPaint(ScrollBar);
            }
            break;
        case YoriWinEventMouseUpInClient:
        case YoriWinEventMouseUpInNonClient:
            break;
        case YoriWinEventMouseUpOutsideWindow:
            break;
        case YoriWinEventMouseDoubleClickInClient:
        case YoriWinEventMouseDoubleClickInNonClient:
            if (YoriWinScrollBarIsLocationSmallerButton(ScrollBar, &Event->MouseDown.Location)) {
                if (ScrollBar->CurrentValue > 0) {
                    ScrollBar->CurrentValue--;
                    if (ScrollBar->CurrentValue > 0) {
                        ScrollBar->CurrentValue--;
                    }
                    if (ScrollBar->ChangeCallback != NULL) {
                        ScrollBar->ChangeCallback(Ctrl);
                    }
                    YoriWinScrollBarPaint(ScrollBar);
                }
            } else if (YoriWinScrollBarIsLocationLargerButton(ScrollBar, &Event->MouseDown.Location)) {
                if (ScrollBar->CurrentValue < ScrollBar->MaximumValue) {
                    ScrollBar->CurrentValue++;
                    if (ScrollBar->CurrentValue < ScrollBar->MaximumValue) {
                        ScrollBar->CurrentValue++;
                    }
                    if (ScrollBar->ChangeCallback != NULL) {
                        ScrollBar->ChangeCallback(Ctrl);
                    }
                    YoriWinScrollBarPaint(ScrollBar);
                }
            }
            break;
    }

    return FALSE;
}

/**
 Return the current value of the scroll bar.

 @param Ctrl Pointer to a scroll bar control.

 @return The currently selected value.
 */
YORI_MAX_UNSIGNED_T
YoriWinScrollBarGetPosition(
    __in PYORI_WIN_CTRL Ctrl
    )
{
    PYORI_WIN_CTRL_SCROLLBAR ScrollBar;
    ScrollBar = CONTAINING_RECORD(Ctrl, YORI_WIN_CTRL_SCROLLBAR, Ctrl);
    return ScrollBar->CurrentValue;
}

/**
 Set the current values for the scroll bar and let it re-render.

 @param Ctrl Pointer to the scroll bar control.

 @param CurrentValue Specifies the top of the range of visible values.

 @param NumberVisible Specifies the length of the range of visible values.

 @param MaximumValue Specifies the largest value which could be generated by
        a fully advanced scroll bar.  Note this means the amount visible on
        the display is effectively MaximumValue + NumberVisible.
 */
VOID
YoriWinScrollBarSetPosition(
    __in PYORI_WIN_CTRL Ctrl,
    __in YORI_MAX_UNSIGNED_T CurrentValue,
    __in YORI_MAX_UNSIGNED_T NumberVisible,
    __in YORI_MAX_UNSIGNED_T MaximumValue
    )
{
    PYORI_WIN_CTRL_SCROLLBAR ScrollBar;
    ScrollBar = CONTAINING_RECORD(Ctrl, YORI_WIN_CTRL_SCROLLBAR, Ctrl);

    ScrollBar->CurrentValue = CurrentValue;
    ScrollBar->MaximumValue = MaximumValue;
    ScrollBar->NumberVisible = NumberVisible;
    YoriWinScrollBarPaint(ScrollBar);
}

/**
 Set the size and location of a scroll bar control, and redraw the contents.

 @param CtrlHandle Pointer to the scroll bar to resize or reposition.

 @param CtrlRect Specifies the new size and position of the scroll bar.

 @return TRUE to indicate success, FALSE to indicate failure.
 */
BOOLEAN
YoriWinScrollBarReposition(
    __in PYORI_WIN_CTRL_HANDLE CtrlHandle,
    __in PSMALL_RECT CtrlRect
    )
{
    PYORI_WIN_CTRL Ctrl = (PYORI_WIN_CTRL)CtrlHandle;
    PYORI_WIN_CTRL_SCROLLBAR ScrollBar;

    Ctrl = (PYORI_WIN_CTRL)CtrlHandle;
    ScrollBar = CONTAINING_RECORD(Ctrl, YORI_WIN_CTRL_SCROLLBAR, Ctrl);

    if (ScrollBar->Alignment == YoriWinScrollbarHorizontal) {
        if (CtrlRect->Top != CtrlRect->Bottom) {
            return FALSE;
        }
    } else {
        if (CtrlRect->Left != CtrlRect->Right) {
            return FALSE;
        }
    }

    if (!YoriWinControlReposition(Ctrl, CtrlRect)) {
        return FALSE;
    }

    YoriWinScrollBarPaint(ScrollBar);
    return TRUE;
}

/**
 Create a scroll bar control and add it to a parent.  This is destroyed when
 the parent is destroyed.

 @param Parent Pointer to the parent window or control.

 @param Size Specifies the location and size of the scroll bar.

 @param Style Specifies style flags for the scroll bar.

 @param ChangeCallback A function to invoke when the scroll bar value is
        changed.

 @return Pointer to the newly created control or NULL on failure.
 */
PYORI_WIN_CTRL
YoriWinScrollBarCreate(
    __in PYORI_WIN_CTRL Parent,
    __in PSMALL_RECT Size,
    __in DWORD Style,
    __in_opt PYORI_WIN_NOTIFY ChangeCallback
    )
{
    PYORI_WIN_CTRL_SCROLLBAR ScrollBar;
    WORD CellCount;

    UNREFERENCED_PARAMETER(Style);

    if (Size->Left == Size->Right) {
        CellCount = (WORD)(Size->Bottom - Size->Top);
    } else if (Size->Top == Size->Bottom) {
        CellCount = (WORD)(Size->Right - Size->Left);
    } else {
        return NULL;
    }

    //
    //  This control requires space for two arrows plus some cell to render
    //  position
    //

    if (CellCount < 3) {
        return NULL;
    }

    ScrollBar = YoriLibReferencedMalloc(sizeof(YORI_WIN_CTRL_SCROLLBAR));
    if (ScrollBar == NULL) {
        return NULL;
    }

    ZeroMemory(ScrollBar, sizeof(YORI_WIN_CTRL_SCROLLBAR));

    if (Size->Left == Size->Right) {
        ScrollBar->Alignment = YoriWinScrollbarVertical;
    } else if (Size->Top == Size->Bottom) {
        ScrollBar->Alignment = YoriWinScrollbarHorizontal;
    }

    ScrollBar->Ctrl.NotifyEventFn = YoriWinScrollBarEventHandler;
    if (!YoriWinCreateControl(Parent, Size, TRUE, FALSE, &ScrollBar->Ctrl)) {
        YoriLibDereference(ScrollBar);
        return NULL;
    }
    if (Parent->Parent != NULL) {
        ScrollBar->Ctrl.RelativeToParentClient = FALSE;
    }
    ScrollBar->ChangeCallback = ChangeCallback;

    ScrollBar->MaximumValue = 1;
    ScrollBar->NumberVisible = 1;
    YoriWinScrollBarPaint(ScrollBar);

    return &ScrollBar->Ctrl;
}


// vim:sw=4:ts=4:et:
