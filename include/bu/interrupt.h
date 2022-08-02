/*                    I N T E R R U P T . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

#ifndef BU_INTERRUPT_H
#define BU_INTERRUPT_H

#include "common.h"

#include "bu/defines.h"


__BEGIN_DECLS

/** @addtogroup bu_env
 *
 * @brief Routines for managing signals.
 *
 * These routines provide a common means to temporarily disable and
 * buffer signal processing during critical write operations.
 *
 */
/** @{ */


/**
 * Defer signal processing and interrupts before critical sections.
 *
 * Signal processing for a variety of signals that would otherwise
 * disrupt the logic of an application are put on hold until
 * bu_interrupt_restore() is called.
 *
 * If an interrupt signal is received while suspended, it will be
 * raised when/if interrupts are restored.
 *
 * Returns 0 on success.
 * Returns non-zero on error (with perror set if signal() failure).
 */
BU_EXPORT extern int bu_interrupt_suspend(void);


/**
 * Resume signal processing and interrupts after critical sections.
 *
 * If a signal was raised since bu_interrupt_suspend() was called,
 * the previously installed signal handler will be immediately called
 * albeit only once even if multiple signals were received.
 *
 * Returns 0 on success.
 * Returns non-zero on error (with perror set if signal() failure).
 */
BU_EXPORT extern int bu_interrupt_restore(void);


/** @} */

__END_DECLS

#endif  /* BU_INTERRUPT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
