/* atomic-install -- high-level copying helper
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#pragma once
#ifndef _ATOMIC_INSTALL_COPY_HXX
#define _ATOMIC_INSTALL_COPY_HXX

namespace atomic_install
{
	class local_fd
	{
		int _fd;

	public:
		local_fd(int fd) throw();
		~local_fd() throw();

		operator int() const throw();
	};

	void mv(const char *source, const char *dest);
	void cp_l(const char *source, const char *dest);
	void cp_a(const char *source, const char *dest);
};

#endif /*_ATOMIC_INSTALL_COPY_HXX*/
