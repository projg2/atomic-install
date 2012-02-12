/* atomic-install -- high-level copying helper
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#pragma once
#ifndef _ATOMIC_INSTALL_COPY_H
#define _ATOMIC_INSTALL_COPY_H

/**
 * SECTION: copy
 * @short_description: Utility functions to copy and move files
 * @include: atomic-install/copy.h
 *
 * libai-copy provides a few convenience functions to copy and move files,
 * preserving their ownership, permissions, mtimes and extended attributes.
 */

/**
 * ai_mv
 * @source: current file path
 * @dest: new complete file path
 *
 * Move file from @source to @dest as atomically as possible. Preserve
 * permissions, extended attributes, mtime.
 *
 * Returns: 0 on success, errno value on failure.
 */
int ai_mv(const char *source, const char *dest);

/**
 * ai_cp_l
 * @source: current file path
 * @dest: new complete file path
 *
 * Create a copy of file @source as @dest as atomically as possible. Preserve
 * permissions, extended attributes, mtime. The resulting file may be a hardlink
 * to the source file.
 *
 * Returns: 0 on success, errno value on failure.
 */
int ai_cp_l(const char *source, const char *dest);

/**
 * ai_cp_a
 * @source: current file path
 * @dest: new complete file path
 *
 * Copy the contents and attributes of @source file to @dest. Preserve
 * permissions, extended attributes, mtime. Try to copy as fast as possible
 * but always create a new file.
 *
 * If @source is a directory, then a new directory will be created at @dest,
 * and permissions and extended attributes will be copied from @source.
 * The destination directory must not exist.
 *
 * Returns: 0 on success, errno value on failure.
 */
int ai_cp_a(const char *source, const char *dest);

#endif /*_ATOMIC_INSTALL_COPY_H*/
