//                              -*- Mode: C++ -*- 
// sockstream.cc
// Copyright � 2002 Laboratoire de Biologie Informatique et Th�orique.
// Author           : Patrick Gendron
// Created On       : Wed Jan 30 19:57:48 2002
// Last Modified By : Patrick Gendron
// Last Modified On : Mon Feb 25 11:37:17 2002
// Update Count     : 1
// Status           : Unknown.
// 

#include <iostream.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <signal.h>

#include "CException.h"
#include "sockstream.h"

#if defined(__sgi) || defined (__sun)
#include <strings.h>
#define MSG_NOSIGNAL 0
#endif


#ifndef HAVE_ISFDTYPE
int
isfdtype (int fd, int fdtype)
{
  struct stat buf;

  if (fstat (fd, &buf) < 0)
    return -1;
  if ((int)(buf.st_mode & S_IFMT) == fdtype)
    return 1;
  else
    return 0;
}
#endif




sockstreambuf::sockstreambuf ()
{
  // Creating socket ---
  if ((socket_id = ::socket (AF_INET, SOCK_STREAM, 0)) < 0) 
    {
      CFatalSocketException exc ("socket creation failed", __FILE__, __LINE__);
      exc << ": " << strerror (errno);
      throw exc;
    }
  
  setg (buffer+putback_size, 
	buffer+putback_size, 
	buffer+putback_size);
}


sockstreambuf::sockstreambuf (int s)
{
  socket_id = s;

  setg (buffer+putback_size,
	buffer+putback_size,
	buffer+putback_size);
}



sockstreambuf::sockstreambuf (const char* host, int port) 
{ 
  if ((socket_id = ::socket (AF_INET, SOCK_STREAM, 0)) < 0) 
    {
      CFatalSocketException exc ("socket creation failed", __FILE__, __LINE__);
      exc << ": " << strerror (errno);
      throw exc;
    }
  
  setg (buffer+putback_size,     // beginning of putback area
	buffer+putback_size,     // read position
	buffer+putback_size);    // end position
  
  open (host, port); 
}


sockstreambuf::~sockstreambuf () 
{ 
  close (); 
}


sockstreambuf* 
sockstreambuf::open (const char* host, int port)
{
  sockaddr_in sin;
  hostent *hp;

  // Getting host info ---
  hp = gethostbyname (host);

  if (hp == NULL)
    {
      CSocketException exc ("unknown host: ");
      exc << host;
      throw exc;
    }

  // Setting server parameters ---
  bzero ((char*)&sin, sizeof (sin));
  bcopy (hp->h_addr, (char*)&sin.sin_addr, hp->h_length);
  sin.sin_family = hp->h_addrtype;
  sin.sin_port = htons (port);
  
  if (::connect (socket_id, (sockaddr*)&sin, sizeof (sin)) < 0)
    {
      CConnectionException exc ("connection to ");
      exc << host << " via port #" << port
          << " failed:\n\t" << strerror (errno);
      throw exc;
    }

  return this;
}


sockstreambuf* 
sockstreambuf::close ()
{
  return sys_close () ? this : 0;
}


bool 
sockstreambuf::is_open () const
{
  return isfdtype (socket_id, S_IFSOCK) > 0;
}


streamsize
sockstreambuf::xsgetn (char* s, streamsize n)
{
  // More things to do if the read is incomplete
  streamsize r = sys_read (s, n);
  
  if (r != n)
    {
      CSocketException exc ("Warning: incomplete read (");
      
      exc << r << "/" << n << " bytes):\n\t" << strerror (errno);
      throw exc;
    }
  return r;
}


streamsize 
sockstreambuf::xsputn (const char* s, streamsize n)
{
  // More things to do if the write is incomplete
  streamsize w = sys_write (s, n);

  if (w !=n )
    {
      CSocketException exc ("Warning: incomplete write (");      
      exc << w << "/" << n << " bytes):\n\t" << strerror (errno);
      throw exc;
    }
  return w;
}



int     
sockstreambuf::underflow ()
{
  // is read position before end of buffer?
  if (gptr () && (gptr() < egptr())) {
    return *gptr();
  }
  
  /* process size of putback area
   * - use number of characters read
   * - but at most size of putback area
   */
  int num_pb;
  num_pb = gptr() - eback();
  if (num_pb > putback_size) {
    num_pb = putback_size;
  }
  
  /* copy up to putback_size characters previously read into
   * the putback area
   */
  memcpy (buffer+(putback_size-num_pb), gptr()-num_pb,
	  num_pb);
  
  // read at most buf_size new characters
  int num;
  num = sys_read (buffer+putback_size, buf_size);

  if (num <= 0) {
    // ERROR or EOF
    return EOF;
  }
  
  // reset buffer pointers
  setg (buffer+(putback_size-num_pb),   // beginning of putback area
	buffer+putback_size,            // read position
	buffer+putback_size+num);       // end of buffer
  
  // return next character
  return *(unsigned char *)gptr();
}




streamsize
sockstreambuf::sys_read (char* buf, streamsize size)
{
  int nleft;
  int nread;
  char* ptr;
  
  ptr = buf;
  nleft = size;

  while (nleft > 0)
    {
#if defined (__sgi) || defined (__sun) || defined (__FreeBSD__)
      signal (SIGPIPE, SIG_IGN);
      if ((nread = ::recv (socket_id, ptr, nleft, 0)) < 0)
#else
      if ((nread = ::recv (socket_id, ptr, nleft, MSG_NOSIGNAL)) < 0)
#endif
        {
          if (errno == EINTR)   
            nread = 0;
          else
            {
#if defined (__sgi) || defined (__sun) || defined (__FreeBSD__)
              signal (SIGPIPE, SIG_DFL);
#endif
              return -1;
            }
        }
      else if (nread == 0)
        break;
      nleft -= nread;
      ptr += nread;
    }
#if defined (__sgi) || defined (__sun) || defined (__FreeBSD__)
  signal (SIGPIPE, SIG_DFL);
#endif
  return (size - nleft);
}



streamsize 
sockstreambuf::sys_write (const char *buf, streamsize size)
{
  int nleft;
  int nsent;
  const char* ptr;

  ptr = buf;
  nleft = size;
  while (nleft > 0)
    {
#if defined (__sgi) || defined (__sun) || defined (__FreeBSD__)
      signal (SIGPIPE, SIG_IGN);
      if ((nsent = ::send (socket_id, ptr, nleft, 0)) < 0)
#else
      if ((nsent = ::send (socket_id, ptr, nleft, MSG_NOSIGNAL)) < 0)
#endif
        {
          if (errno == EINTR)
            nsent = 0;
          else
            {
#if defined (__sgi) || defined (__sun) || defined (__FreeBSD__)
              signal (SIGPIPE, SIG_DFL);
#endif
              return -1;
            }
        }
      nleft -= nsent;
      ptr += nsent;
    }
#if defined (__sgi) || defined (__sun) || defined (__FreeBSD__)
  signal (SIGPIPE, SIG_DFL);
#endif
  return size;
}


int
sockstreambuf::sys_close ()
{
  return ::close (socket_id);
}



void 
sockstreambase::open (const char* host, int port)
{
  clear ();
  if (!rdbuf ()->open (host, port))
    setstate (ios::badbit);
}


void 
sockstreambase::close ()
{
  if (!rdbuf ()->close ())
    setstate (ios::failbit);
}

