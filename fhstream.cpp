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

#include <cstring>
#include <algorithm> // for std::min

#include "fhstream.hpp"

/*
 * ofhstream
 */

ofhbuf::ofhbuf (void* arg_handle, size_t (*arg_write_fun)(void*, const void*, size_t))
: handle(arg_handle),
  write_fun(arg_write_fun),
  buffer(new char[default_buffer_size]),
  buffer_size(default_buffer_size)
{
	reset_buffer();
}

ofhbuf::~ofhbuf ()
{
	if (handle) {
		try {
			sync();
		} catch (...) {
			// Ignore exception since we're in the destructor.
			// To catch write errors, call sync() explicitly.
		}
	}
	delete[] buffer;
}

ofhbuf::int_type	ofhbuf::overflow (ofhbuf::int_type c)
{
	const char*	p = pbase();
	std::streamsize	bytes_to_write = pptr() - p;

	if (!is_eof(c)) {
	      *pptr() = c;
	      ++bytes_to_write;
	}

	while (bytes_to_write > 0) {
		const size_t	bytes_written = write_fun(handle, p, bytes_to_write);
		bytes_to_write -= bytes_written;
		p += bytes_written;
	}

	reset_buffer();

	return traits_type::to_int_type(0);
}

int		ofhbuf::sync ()
{
	return !is_eof(overflow(traits_type::eof())) ? 0 : -1;
}

std::streamsize	ofhbuf::xsputn (const char* s, std::streamsize n)
{
	// Use heuristic to decide whether to write directly or just use buffer
	// Write directly only if n >= MIN(4096, available buffer capacity)
	// (this is similar to what basic_filebuf does)

	if (n < std::min<std::streamsize>(4096, epptr() - pptr())) {
		// Not worth it to do a direct write
		return std::streambuf::xsputn(s, n);
	}

	// Before we can do a direct write of this string, we need to flush
	// out the current contents of the buffer.
	if (pbase() != pptr()) {
		overflow(traits_type::eof()); // throws an exception or it succeeds
	}

	// Now we can go ahead and write out the string.
	size_t		bytes_to_write = n;

	while (bytes_to_write > 0) {
		const size_t	bytes_written = write_fun(handle, s, bytes_to_write);
		bytes_to_write -= bytes_written;
		s += bytes_written;
	}

	return n; // Return the total bytes written
}

std::streambuf*	ofhbuf::setbuf (char* s, std::streamsize n)
{
	if (s == 0 && n == 0) {
		// Switch to unbuffered
		// This won't take effect until the next overflow or sync
		// (We defer it taking effect so that write errors can be properly reported)
		// To cause it to take effect as soon as possible, we artificially reduce the
		// size of the buffer so it has no space left.  This will trigger an overflow
		// on the next put.
		std::streambuf::setp(pbase(), pptr());
		std::streambuf::pbump(pptr() - pbase());
		buffer_size = 1;
	}
	return this;
}



/*
 * ifhstream
 */

ifhbuf::ifhbuf (void* arg_handle, size_t (*arg_read_fun)(void*, void*, size_t))
: handle(arg_handle),
  read_fun(arg_read_fun),
  buffer(new char[default_buffer_size + putback_size]),
  buffer_size(default_buffer_size)
{
	reset_buffer(0, 0);
}

ifhbuf::~ifhbuf ()
{
	delete[] buffer;
}

ifhbuf::int_type	ifhbuf::underflow ()
{
	if (gptr() >= egptr()) { // A true underflow (no bytes in buffer left to read)

		// Move the putback_size most-recently-read characters into the putback area
		size_t		nputback = std::min<size_t>(gptr() - eback(), putback_size);
		std::memmove(buffer + (putback_size - nputback), gptr() - nputback, nputback);

		// Now read new characters from the file descriptor
		const size_t	nread = read_fun(handle, buffer + putback_size, buffer_size);
		if (nread == 0) {
			// EOF
			return traits_type::eof();
		}

		// Reset the buffer
		reset_buffer(nputback, nread);
	}

	// Return the next character
	return traits_type::to_int_type(*gptr());
}

std::streamsize	ifhbuf::xsgetn (char* s, std::streamsize n)
{
	// Use heuristic to decide whether to read directly
	// Read directly only if n >= bytes_available + 4096

	std::streamsize	bytes_available = egptr() - gptr();

	if (n < bytes_available + 4096) {
		// Not worth it to do a direct read
		return std::streambuf::xsgetn(s, n);
	}

	std::streamsize	total_bytes_read = 0;

	// First, copy out the bytes currently in the buffer
	std::memcpy(s, gptr(), bytes_available);

	s += bytes_available;
	n -= bytes_available;
	total_bytes_read += bytes_available;

	// Now do the direct read
	while (n > 0) {
		const size_t	bytes_read = read_fun(handle, s, n);
		if (bytes_read == 0) {
			// EOF
			break;
		}

		s += bytes_read;
		n -= bytes_read;
		total_bytes_read += bytes_read;
	}

	// Fill up the putback area with the most recently read characters
	size_t		nputback = std::min<size_t>(total_bytes_read, putback_size);
	std::memcpy(buffer + (putback_size - nputback), s - nputback, nputback);

	// Reset the buffer with no bytes available for reading, but with some putback characters
	reset_buffer(nputback, 0);

	// Return the total number of bytes read
	return total_bytes_read;
}

std::streambuf*	ifhbuf::setbuf (char* s, std::streamsize n)
{
	if (s == 0 && n == 0) {
		// Switch to unbuffered
		// This won't take effect until the next underflow (we don't want to
		// lose what's currently in the buffer!)
		buffer_size = 1;
	}
	return this;
}
