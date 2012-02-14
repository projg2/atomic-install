/* atomic-install -- high-level copying helper
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#pragma once
#ifndef _ATOMIC_INSTALL_EXCEPTIONS_HXX
#define _ATOMIC_INSTALL_EXCEPTIONS_HXX 1

#include <stdexcept>
#include <ostream>
#include <string>

namespace atomic_install
{
	class io_error;
};

std::ostream& operator<<(std::ostream& os, const atomic_install::io_error& err);
bool operator==(const atomic_install::io_error& err, int errno_val);
bool operator!=(const atomic_install::io_error& err, int errno_val);

namespace atomic_install
{
	class io_error: virtual std::exception
	{
		const char* _func;
		int _err_num;
		std::string _source, _dest;

	public:
		io_error(const char* func, int err_num,
				const char* fn1 = NULL, const char* fn2 = NULL);
		~io_error() throw();

		void set_paths(const char* fn1, const char* fn2 = NULL);

		virtual const char* what() const throw();

		friend std::ostream& ::operator<<(std::ostream& os, const io_error& err);
		friend bool ::operator==(const io_error& err, int errno_val);
		friend bool ::operator!=(const io_error& err, int errno_val);
	};
};

#endif /*_ATOMIC_INSTALL_EXCEPTIONS_HXX*/
