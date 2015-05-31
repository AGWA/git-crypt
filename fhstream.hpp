/*
 * Copyright (C) 2012, 2015 Andrew Ayer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 */

#ifndef GIT_CRYPT_FHSTREAM_HPP
#define GIT_CRYPT_FHSTREAM_HPP

#include <ostream>
#include <istream>
#include <streambuf>

/*
 * ofhstream
 */
class ofhbuf : public std::streambuf {
	enum { default_buffer_size = 8192 };

	void*			handle;
	size_t			(*write_fun)(void*, const void*, size_t);
	char*			buffer;
	size_t			buffer_size;

	inline void reset_buffer ()
	{
		std::streambuf::setp(buffer, buffer + buffer_size - 1);
	}
	static inline bool is_eof (int_type ch) { return traits_type::eq_int_type(ch, traits_type::eof()); }

	// Disallow copy
#if __cplusplus >= 201103L /* C++11 */
	ofhbuf (const ofhbuf&) = delete;
	ofhbuf& operator= (const ofhbuf&) = delete;
#else
	ofhbuf (const ofhbuf&);
	ofhbuf& operator= (const ofhbuf&);
#endif

protected:
	virtual int_type	overflow (int_type ch =traits_type::eof());
	virtual int		sync ();
	virtual std::streamsize	xsputn (const char*, std::streamsize);
	virtual	std::streambuf*	setbuf (char*, std::streamsize);

public:
	ofhbuf (void*, size_t (*)(void*, const void*, size_t));
	~ofhbuf (); // WARNING: calls sync() and ignores exceptions
};

class ofhstream : public std::ostream {
	mutable ofhbuf	buf;
public:
	ofhstream (void* handle, size_t (*write_fun)(void*, const void*, size_t))
	: std::ostream(0), buf(handle, write_fun)
	{
		std::ostream::rdbuf(&buf);
	}

	ofhbuf*	rdbuf () const { return &buf; }
};


/*
 * ifhstream
 */
class ifhbuf : public std::streambuf {
	enum {
		default_buffer_size = 8192,
		putback_size = 4
	};

	void*			handle;
	size_t			(*read_fun)(void*, void*, size_t);
	char*			buffer;
	size_t			buffer_size;

	inline void reset_buffer (size_t nputback, size_t nread)
	{
		std::streambuf::setg(buffer + (putback_size - nputback), buffer + putback_size, buffer + putback_size + nread);
	}
	// Disallow copy
#if __cplusplus >= 201103L /* C++11 */
	ifhbuf (const ifhbuf&) = delete;
	ifhbuf& operator= (const ifhbuf&) = delete;
#else
	ifhbuf (const ifhbuf&);
	ifhbuf& operator= (const ifhbuf&);
#endif

protected:
	virtual int_type	underflow ();
	virtual std::streamsize	xsgetn (char*, std::streamsize);
	virtual	std::streambuf*	setbuf (char*, std::streamsize);

public:
	ifhbuf (void*, size_t (*)(void*, void*, size_t));
	~ifhbuf (); // Can't fail
};

class ifhstream : public std::istream {
	mutable ifhbuf	buf;
public:
	explicit ifhstream (void* handle, size_t (*read_fun)(void*, void*, size_t))
	: std::istream(0), buf(handle, read_fun)
	{
		std::istream::rdbuf(&buf);
	}

	ifhbuf*	rdbuf () const { return &buf; }
};

#endif
