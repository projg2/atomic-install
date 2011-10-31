/* atomic-install -- I/O helper functions
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

/* ai_mv()
 * @param source: current file path
 * @param dest: new complete file path
 *
 * Move file from @source to @dest as atomically as possible. Preserve
 * permissions, extended attributes, mtime.
 *
 * @return: 0 on success, errno value on failure.
 */
int ai_mv(const char *source, const char *dest);

/* ai_cp_l()
 * @param source: current file path
 * @param dest: new complete file path
 *
 * Create a copy of file @source as @dest as atomically as possible. Preserve
 * permissions, extended attributes, mtime. The resulting file may be a hardlink
 * to the source file.
 *
 * @return: 0 on success, errno value on failure.
 */
int ai_cp_l(const char *source, const char *dest);

/* ai_cp()
 * @param source: current file path
 * @param dest: new complete file path
 *
 * Copy the contents and attributes of @source file to @dest. Preserve
 * permissions, extended attributes, mtime. Try to copy as fast as possible
 * but always create a new file.
 *
 * @return: 0 on success, errno value on failure.
 */
int ai_cp(const char *source, const char *dest);
