// SPDX-License-Identifier: GPL-2.0-or-later
// SPDX-FileCopyrightText: (c) 2026 Evgeny Ugreninov
/*
 * of_eh_frames.c -- register .eh_frame with libgcc's unwinder.
 *
 * Bare-metal libgcc_eh has no dl_iterate_phdr, so _Unwind_Find_FDE only
 * sees frames registered via __register_frame_info. app.ld PROVIDEs
 * __EH_FRAME_BEGIN__ / __FRAME_END__ around the KEEP'd .eh_frame section.
 * Runs at constructor priority 101, before any C++ static initializer.
 */
extern char __EH_FRAME_BEGIN__[];
extern char __FRAME_END__[];
void __register_frame_info(const void *, void *);
void __deregister_frame_info(const void *);

static void *of_eh_object[16];

__attribute__((constructor(101)))
static void of_eh_register(void) {
	if (__EH_FRAME_BEGIN__ != __FRAME_END__)
		__register_frame_info(__EH_FRAME_BEGIN__, of_eh_object);
}

__attribute__((destructor(101)))
static void of_eh_deregister(void) {
	if (__EH_FRAME_BEGIN__ != __FRAME_END__)
		__deregister_frame_info(__EH_FRAME_BEGIN__);
}
