/* atomic-install -- high-level copying helper
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#pragma once
#ifndef _ATOMIC_INSTALL_COPY_HXX
#define _ATOMIC_INSTALL_COPY_HXX

void ai_mv(const char *source, const char *dest);
void ai_cp_l(const char *source, const char *dest);
void ai_cp_a(const char *source, const char *dest);

#endif /*_ATOMIC_INSTALL_COPY_HXX*/
