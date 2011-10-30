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

/* clonefile()
 * @param source: current file path
 * @param dest: new complete file path
 *
 * Manually copy the contents and attributes of @source to @dest.
 *
 * @return: 0 on success, errno value on failure.
 */
int clonefile(const char *source, const char *dest);
