/*
 * Copyright (c) 1993-2013, 2015 Paul Mattes.
 * Copyright (c) 1990, Jeff Sparkes.
 * Copyright (c) 1989, Georgia Tech Research Corporation (GTRC), Atlanta,
 *  GA 30332.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes, Jeff Sparkes, GTRC nor their
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES, JEFF SPARKES AND GTRC "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PAUL MATTES, JEFF SPARKES OR
 * GTRC BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	xio.c
 *		Low-level I/O setup functions and exit code.
 */

#include "globals.h"

#include "actions.h"
#include "telnet.h"
#include "utils.h"
#include "xio.h"

/* Globals. */
int x3270_exit_code = 0;

/* Statics. */
static ioid_t ns_read_id;
static ioid_t ns_exception_id;
static bool reading = false;
static bool excepting = false;

/*
 * Called to set up input on a new network connection.
 */
void
x_add_input(iosrc_t iosrc)
{
    ns_exception_id = AddExcept(iosrc, net_exception);
    excepting = true;
    ns_read_id = AddInput(iosrc, net_input);
    reading = true;
}

/*
 * Called when an exception is received to disable further exceptions.
 */
void
x_except_off(void)
{
    if (excepting) {
	RemoveInput(ns_exception_id);
	excepting = false;
    }
}

/*
 * Called when exception processing is complete to re-enable exceptions.
 * This includes removing and restoring reading, so the exceptions are always
 * processed first.
 */
void
x_except_on(iosrc_t iosrc)
{
    if (excepting) {
	return;
    }
    if (reading) {
	RemoveInput(ns_read_id);
    }
    ns_exception_id = AddExcept(iosrc, net_exception);
    excepting = true;
    if (reading) {
	ns_read_id = AddInput(iosrc, net_input);
    }
}

/*
 * Called to disable input on a closing network connection.
 */
void
x_remove_input(void)
{
    if (reading) {
	RemoveInput(ns_read_id);
	reading = false;
    }
    if (excepting) {
	RemoveInput(ns_exception_id);
	excepting = false;
    }
}

/*
 * Application exit, with cleanup.
 */
void
x3270_exit(int n)
{
    static bool already_exiting = false;

    /* Handle unintentional recursion. */
    if (already_exiting) {
	return;
    }
    already_exiting = true;

    /* Set the exit code. */
    x3270_exit_code = n;

    /* Flush any pending output (mostly for Windows). */
    fflush(stdout);
    fflush(stderr);

    /* Tell everyone else who's interested. */
    st_changed(ST_EXITING, true);

#if !defined(_WIN32) /*[*/
    exit(n);
#else /*][*/
    /*
     * On Windows, call ExitProcess() instead of the POSIXish exit().
     * Apparently calling exit() in a ConsoleCtrlHandler is a bad thing on
     * XP, and causes a hang.
     */
    ExitProcess(n);
#endif /*]*/
}

static bool
Quit_action(ia_t ia, unsigned argc, const char **argv)
{
    action_debug("Quit", ia, argc, argv);
    if (check_argc("Quit", argc, 0, 0) < 0) {
	return false;
    }

    /*
     * We allow Quit() to succeed if invoked from anything besides a keymap, and
     * from a keymap if we're not connected.
     *
     * This test is imperfect. Someone could put a Source() in a keymap and
     * read in a file that includes a Quit(). If we are connected, it will
     * fail.
     */
    if (ia != IA_KEYMAP || !CONNECTED) {
	x3270_exit(0);
    }
    return false;
}

/*
 * X I/O module registration.
 */
void
xio_register(void)
{
    static action_table_t xio_actions[] = {
	{ "Quit",		Quit_action,	ACTION_KE },
	{ "Exit",		Quit_action,	ACTION_KE }
    };

    /* Register our actions. */
    register_actions(xio_actions, ARRAY_SIZE(xio_actions));
}
