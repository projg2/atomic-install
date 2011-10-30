/* atomic-install -- I/O helper functions
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

/* mv()
 * @param source: current file path
 * @param dest: new complete file path
 *
 * Move file from @source to @dest as atomically as possible. Preserve
 * permissions, extended attributes, mtime.
 *
 * @return: 0 on success, errno value on failure.
 */
int mv(const char *source, const char *dest);

/* cp()
 * @param source: current file path
 * @param dest: new complete file path
 *
 * Create a copy of file @source as @dest as atomically as possible. Preserve
 * permissions, extended attributes, mtime. The resulting file may be a hardlink
 * to the source file.
 *
 * @return: 0 on success, errno value on failure.
 */
int cp(const char *source, const char *dest);
