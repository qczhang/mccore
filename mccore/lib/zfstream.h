/* This is part of mcsym library, providing -*- C++ -*- zipped input/output.
Copyright (C) 1993 Free Software Foundation

This library is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this library; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

As a special exception, if you link this file compiled with a GNU compiler
to produce an executable, this does not cause the resulting executable to be
covered by the GNU General Public License.  This exception does not however
invalidate any other reasons why the executable file might be covered by the
GNU General Public License.

Written by Martin Larose <larosem@acm.org>
*/


#ifndef _zfstream_h_
#define _zfstream_h_


#include <iostream.h>

#include "zfstreambase.h"



/**
 * @short Compressed input file stream.
 *
 * These streams behaves like regular fstream except that we are able to
 * sets it's compression ratio on open operation.  It uses zlib.
 *
 * @author Martin Larose <larosem@iro.umontreal.ca>
 */
class izfstream : public zfstreambase, public istream
{
  
public:

  // LIFECYCLE ------------------------------------------------------------

  /**
   * Initializes the stream.
   */
  izfstream () : zfstreambase () { }

  /**
   * Initializes the objet with a file descriptor.
   * @param fd the input file descriptor.
   */
  izfstream (int fd) : zfstreambase (fd) { }
  
  /**
   * Initializes the stream with file name and parameters.
   * @param name the path and file name to open.
   * @param mode the open mode (default ios::in).
   * @param prot the protection (default 0644).
   */
  izfstream(const char *name, int mode = ios::in, int prot = 0664)
    : zfstreambase(name, mode, Z_BEST_SPEED, prot) { }

  // OPERATORS ------------------------------------------------------------

  // ACCESS ---------------------------------------------------------------

  // METHODS --------------------------------------------------------------

  /**
   * Opens the stream with file name and parameters.
   * @param name the path and file name to open.
   * @param mode the open mode (default ios::in).
   * @param prot the protection (default 0644).
   */
  void open(const char *name, int mode=ios::in, int prot = 0664)
    { zfstreambase::open(name, mode, Z_BEST_SPEED, prot); }

  /**
   * Closes the stream.  It calls zfstreambase close () method to empty the
   * buffer and put the compression trailer at the end of the file.
   */
  void close () { zfstreambase::close (); }

  // I/O ------------------------------------------------------------------
};



/**
 * @short Compressed output file stream.
 *
 * These streams behaves like regular fstream except that we are able to
 * sets it's compression ratio on open operation.  It uses zlib.
 *
 * @author Martin Larose <larosem@iro.umontreal.ca>
 */
class ozfstream : public zfstreambase, public ostream
{
  
public:
  
  // LIFECYCLE ------------------------------------------------------------

  /**
   * Initializes the stream.
   */
  ozfstream () : zfstreambase () { }

  /**
   * Initializes the objet with a file descriptor.
   * @param fd the input file descriptor.
   */
  ozfstream (int fd) : zfstreambase (fd) { }

  /**
   * Initializes the stream with file name and parameters.
   * @param name the path and file name to open.
   * @param level the compression level for output.
   * @param mode the open mode (default ios::out).
   * @param prot the protection (default 0644).
   */
  ozfstream (const char *name, int level = Z_BEST_SPEED,
	     int mode = ios::out, int prot = 0664)
    : zfstreambase (name, mode, level, prot) { }

  // OPERATORS ------------------------------------------------------------

  // ACCESS ---------------------------------------------------------------

  // METHODS --------------------------------------------------------------

  /**
   * Opens the stream with file name and parameters.
   * @param name the path and file name to open.
   * @param level the compression level for output.
   * @param mode the open mode (default ios::out).
   * @param prot the protection (default 0644).
   */
  void open (const char *name, int level = Z_BEST_SPEED,
	     int mode = ios::out, int prot = 0664)
    { zfstreambase::open (name, mode, level, prot); }

  /**
   * Closes the stream.  It calls zfstreambase close () method to empty the
   * buffer and put the compression trailer at the end of the file.
   */
  void close () { zfstreambase::close (); }

  // I/O ------------------------------------------------------------------
};



/**
 * @short General compressed file stream.
 *
 * These streams behaves like regular fstream except that we are able to
 * sets it's compression ratio on open operation.  It uses zlib.
 *
 * @author Martin Larose <larosem@iro.umontreal.ca>
 */
class zfstream : public zfstreambase, public iostream
{
  
public:
  
  // LIFECYCLE ------------------------------------------------------------

  /**
   * Initializes the stream.
   */
  zfstream () : zfstreambase () { }

  /**
   * Initializes the objet with a file descriptor.
   * @param fd the input file descriptor.
   */
  zfstream (int fd) : zfstreambase (fd) { }

  /**
   * Initializes the stream with file name and parameters.
   * @param name the path and file name to open.
   * @param level the compression level for output.
   * @param mode the open mode (default ios::in).
   * @param prot the protection (default 0644).
   */
  zfstream (const char *name, int level = Z_BEST_SPEED,
	    int mode = ios::in, int prot = 0664)
    : zfstreambase(name, mode, level, prot) { }

  // OPERATORS ------------------------------------------------------------

  // ACCESS ---------------------------------------------------------------

  // METHODS --------------------------------------------------------------

  /**
   * Opens the stream with file name and parameters.
   * @param name the path and file name to open.
   * @param level the compression level for output.
   * @param mode the open mode (default ios::in).
   * @param prot the protection (default 0644).
   */
  void open (const char *name, int level = Z_BEST_SPEED,
	     int mode = ios::in, int prot = 0664)
    { zfstreambase::open (name, mode, level, prot); }

  /**
   * Closes the stream.  It calls zfstreambase close () method to empty the
   * buffer and put the compression trailer at the end of the file.
   */
  void close () { zfstreambase::close (); }

  // I/O ------------------------------------------------------------------
};


#endif
