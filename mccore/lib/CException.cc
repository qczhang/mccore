//                              -*- Mode: C++ -*- 
// CException.cc
// Copyright � 1999, 2000-01 Laboratoire de Biologie Informatique et Th�orique.
//                           Universit� de Montr�al.
// Author           : Martin Larose <larosem@iro.umontreal.ca>
// Created On       : 
// Last Modified By : Martin Larose
// Last Modified On : Thu Aug 23 15:09:34 2001
// Update Count     : 11
// Status           : Ok.
// 
//  This file is part of mccore.
//  
//  mccore is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//  
//  mccore is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//  
//  You should have received a copy of the GNU Lesser General Public
//  License along with mccore; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <iostream.h>
#include <stdio.h>
#include <string.h>

#include "CException.h"



CException::CException ()
{
  mMessage = new char[1];
  mMessage[0] = '\0';
}



CException::CException (const char *message)
{
  mMessage = new char[strlen (message) + 1];
  strcpy (mMessage, message);
}



CException::CException (const CException &right)
{
  mMessage = new char[strlen (right.mMessage) + 1];
  strcpy (mMessage, right.mMessage);
}



const CException&
CException::operator= (const CException &right)
{
  if (this != &right)
    {
      delete[] mMessage;
      mMessage = new char[strlen (right.mMessage) + 1];
      strcpy (mMessage, right.mMessage);
    }
  return *this;
}



CException&
CException::operator<< (const char *message)
{
  char *tmp;

  tmp = new char[strlen (mMessage) + strlen (message) + 1];
  strcpy (tmp, mMessage);
  strcat (tmp, message);
  delete[] mMessage;
  mMessage = tmp;
  return *this;
}



CException&
CException::operator<< (int integer)
{
  char str[256];
  char *tmp;

  sprintf (str, "%d", integer);
  tmp = new char[strlen (mMessage) + strlen (str) + 1];
  strcpy (tmp, mMessage);
  strcat (tmp, str);
  delete[] mMessage;
  mMessage = tmp;
  return *this;
}



CException&
CException::operator<< (unsigned int integer)
{
  char str[256];
  char *tmp;

  sprintf (str, "%u", integer);
  tmp = new char[strlen (mMessage) + strlen (str) + 1];
  strcpy (tmp, mMessage);
  strcat (tmp, str);
  delete[] mMessage;
  mMessage = tmp;
  return *this;
}



CException&
CException::operator<< (float float_val)
{
  char str[256];
  char *tmp;

  sprintf (str, "%f", float_val);
  tmp = new char[strlen (mMessage) + strlen (str) + 1];
  strcpy (tmp, mMessage);
  strcat (tmp, str);
  delete[] mMessage;
  mMessage = tmp;
  return *this;
}



CException&
CException::operator<< (char theChar)
{
  char a[2];
  char *tmp;

  a[0] = theChar;
  a[1] = '\0';
  tmp = new char[strlen (mMessage) + 2];
  strcpy (tmp, mMessage);
  strcat (tmp, a);
  delete[] mMessage;
  mMessage = tmp;
  return *this;
}



ostream&
operator<< (ostream& os, const CException &exc) 
{
  return os << exc.GetMessage ();
}



const CInterruptException&
CInterruptException::operator= (const CInterruptException &right)
{
  if (this != &right)
    CException::operator= (right);
  return *this;
}	



const CLibException&
CLibException::operator= (const CLibException &right)
{
  if (this != &right)
    CException::operator= (right);
  return *this;
}



const CFatalLibException&
CFatalLibException::operator= (const CFatalLibException &right)
{
  if (this != &right)
    CLibException::operator= (right);
  return *this;
}



CIntLibException::CIntLibException ()
  : CException (), mLine (0)
{
  mFile = new char[1];
  mFile[0] = '\0';
}



CIntLibException::CIntLibException (const char *theMessage,
				    const char *file, int line)
  : CException (theMessage), mLine (line)
{
  if (file)
    {
      mFile = new char[strlen (file) + 1];
      strcpy (mFile, file);
    }
  else
    {
      mFile = new char[1];
      mFile[0] = '\0';
    }
}



CIntLibException::CIntLibException (const CIntLibException &right)
  : CException (right), mLine (right.mLine)
{
  mFile = new char[strlen (right.mFile) + 1];
  strcpy (mFile, right.mFile);
}



const CIntLibException&
CIntLibException::operator= (const CIntLibException &right)
{
  if (this != &right)
    {
      CException::operator= (right);
      mFile = new char[strlen (right.mFile) + 1];
      strcpy (mFile, right.mFile);
      mLine = right.mLine;
    }
  return *this;
}



ostream&
operator<< (ostream &os, const CIntLibException &exc)
{
  return os << exc.GetFileName () << ":" << exc.GetLine () << ": "
	    << (CException&) exc << endl << endl
	    << "Please send a bug report to 'bug-mcsym@iro.umontreal.ca'.";
}



const CFatalIntLibException&
CFatalIntLibException::operator= (const CFatalIntLibException &right)
{
  if (this != &right)
    CIntLibException::operator= (right);
  return *this;
}
