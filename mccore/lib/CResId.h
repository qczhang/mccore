//                              -*- Mode: C++ -*- 
// CResId.h
// Copyright � 2000-01 Laboratoire de Biologie Informatique et Th�orique.
//                     Universit� de Montr�al.
// Author           : Patrick Gendron <gendrop@iro.umontreal.ca>
// Created On       : Thu Sep 28 15:55:29 2000
// Last Modified By : Philippe Thibault
// Last Modified On : Wed Mar 19 08:30:47 2003
// Update Count     : 9
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


#ifndef _CResId_h_
#define _CResId_h_


class ostream;
class iBinstream;
class oBinstream;



/**
 * @short Identification structure for residues.
 *
 * @author Patrick Gendron <gendrop@iro.umontreal.ca>
 */
class CResId
{
  /**
   * The residue number.
   */
  int no;
  
  /**
   * The residue chain id.
   */
  char chain;

  /**
   * The insertion code.
   */
  char iCode;

  /**
   * The id representation.
   */
  mutable char *mRep;
  
public:

  // LIFECYCLE ------------------------------------------------------------

  /**
   * Initializes the structure.
   * @param n the residue number (default = -1).
   * @param c the residue chain id (default = ' ').
   */
  CResId (int n = -1, char c = ' ', char ic = ' ')
    : no (n), chain (c), iCode (ic), mRep (0) { }

  /**
   * Initializes the structure with a text representation.
   * @param str the text representation.
   */
  CResId (const char *str);
  
  /**
   * Initializes the object with the right's content.
   * @param right the object to copy.
   */
  CResId (const CResId &right)
    : no (right.no), chain (right.chain), iCode (right.iCode), mRep (0)
  { }

  /**
   * Destructs the object.
   */
  ~CResId () { if (mRep) delete[] mRep; }

  // OPERATORS ------------------------------------------------------------

  /**
   * Assigns the object with the right's content.
   * @param right the object to copy.
   * @return itself.
   */
  CResId& operator= (const CResId &right);
  
  /**
   * Tests the equality between ids.
   * @param right the object to test.
   * @return the truth value.
   */
  bool operator== (const CResId &right) const
  {
    return (chain == right.chain
	    && no == right.no
	    && iCode == right.iCode);
  }
  
  /**
   * Tests the difference between ids.
   * @param right the object to test.
   * @return the truth value.
   */
  bool operator!= (const CResId &right) const { return !operator== (right); }
  
  /**
   * Tests the partial order over ids.
   * @param right the object to test.
   * @return the truth value.
   */
  bool operator< (const CResId &right) const
  {
    return (chain < right.chain
	    || (chain == right.chain && no < right.no)
	    || (chain == right.chain
		&& no == right.no
		&& iCode < right.iCode));
  }

  /**
   * Calculate an id equal to this with its residue number incremented.
   * @param offset the increment.
   * @return the increment id.
   */
  CResId operator+ (int offset) const;

  /**
   * Calculate an id equal to this with its residue number decremented.
   * @param offset the decrement.
   * @return the increment id.
   */
  CResId operator- (int offset) const;
  
  /**
   * Converts the residue id to a string representation.
   */
  operator const char* () const;
  
  // ACCESS ---------------------------------------------------------------

  /**
   * Gets the residue number.
   * @return the residue number.
   */
  int GetResNo () const { return no; }

  /**
   * Sets the residue number.
   * @param resno the new residue number.
   */
  void SetResNo (int resno)
  {
    if (mRep)
      {
	delete[] mRep;
	mRep = 0;
	}
    no = resno;
  }

  /**
   * Gets the chain id.
   * @return the chain id.
   */
  char GetChainId () const { return chain; }

  /**
   * Sets the chain id.
   * @param chainid the new chain id.
   */
  void SetChainId (char chainid)
  {
    if (mRep)
      {
	delete[] mRep;
	mRep = 0;
      }
    chain = chainid;
  }

  /**
   * Gets the residue insertion code.
   * @return the insertion code.
   */
  char getInsertionCode () const { return iCode; }

  /**
   * Sets the insertion code.  It invalidates the residue id representation.
   * @param ic the insertion code.
   */
  void setInsertionCode (char ic)
  {
    if (mRep)
      {
	delete[] mRep;
	mRep = 0;
      }
    iCode = ic;
  }
  
  // METHODS --------------------------------------------------------------

  // I/O  -----------------------------------------------------------------
};



/**
 * Outputs the residue id to the stream.
 * @param os the output stream.
 * @param obj the residue id.
 * @return the used output stream.
 */
ostream& operator<< (ostream &os, const CResId &obj);



/**
 * Inputs the residue id from the binary stream.
 * @param ibs the input binary stream.
 * @param obj the id to put data in.
 * @return the used input binary stream.
 */
iBinstream& operator>> (iBinstream &ibs, CResId &obj);



/**
 * Outputs the residue id to the binary stream.
 * @param obs the output binary stream.
 * @param obj the residue id.
 * @return the used output binary stream.
 */
oBinstream& operator<< (oBinstream &obs, const CResId &obj);

#endif