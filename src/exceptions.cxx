/* atomic-install
 * (c) 2012 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include "exceptions.hxx"

#include <cassert>
#include <cstring>

namespace ai = atomic_install;

ai::io_error::io_error(const char* func, int err_num,
		const char* fn1, const char* fn2)
	: _func(func), _err_num(err_num)
{
	if (fn1)
		_source = fn1;
	if (fn2)
		_dest = fn2;
}

ai::io_error::~io_error() throw()
{
}

void ai::io_error::set_paths(const char* fn1, const char* fn2)
{
	assert(fn1);

	_source = fn1;
	if (fn2)
		_dest = fn2;
	else
		_dest.erase();
}

const char* ai::io_error::what() const throw()
{
	return "I/O error occured.";
}

std::ostream& operator<<(std::ostream& os, const ai::io_error& err)
{
	if (err._dest.empty())
	{
		return os << "I/O error in function " << err._func
				<< ":\n	error: " << strerror(err._err_num)
				<< "\n	path: " << err._source;
	}
	else
	{
		return os << "I/O error in function " << err._func
				<< ":\n	error: " << strerror(err._err_num)
				<< "\n	source: " << err._source
				<< "\n	dest: " << err._dest;
	}
}

bool operator==(const ai::io_error& err, int errno_val)
{
	return err._err_num == errno_val;
}

bool operator!=(const ai::io_error& err, int errno_val)
{
	return err._err_num != errno_val;
}
