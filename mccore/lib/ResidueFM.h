//                              -*- Mode: C++ -*- 
// ResidueFM.h
// Copyright � 2001-03 Laboratoire de Biologie Informatique et Th�orique.
//                  Universit� de Montr�al.
// Author           : Martin Larose <larosem@iro.umontreal.ca>
// Created On       : Fri Oct 12 14:46:02 2001
// $Revision: 1.1.12.1 $
// $Id: ResidueFM.h,v 1.1.12.1 2003-12-09 21:54:26 larosem Exp $
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


#ifndef _ResidueFM_h_
#define _ResidueFM_h_

#include <vector>

#include "CAtom.h"
#include "ResidueFactoryMethod.h"

class AbstractResidue;
class t_Residue;
class CResId;

using namespace std;



/**
 * @short ResidueFactoryMethod implementation for Residue class.
 *
 * This is the residue factory method implementation for the Residue class.
 *
 * @author Martin Larose <larosem@iro.umontreal.ca>
 */
class ResidueFM : public ResidueFactoryMethod
{

public:

  // LIFECYCLE ------------------------------------------------------------

  /**
   * Initializes the object.
   */
  ResidueFM () { }

  /**
   * Clones the object.
   * @return the copy of the object.
   */
  virtual ResidueFactoryMethod* clone () const { return new ResidueFM (); }
  
  /**
   * Destroys the object.
   */
  virtual ~ResidueFM () { }

  // OPERATORS ------------------------------------------------------------

  // ACCESS ---------------------------------------------------------------

  // METHODS --------------------------------------------------------------

  /**
   * Creates a new residue of Residue type.
   * @return the newly created empty residue.
   */
  virtual AbstractResidue* createResidue () const;

  /**
   * Creates the residue.
   * @param type the residue type.
   * @param vec the atom container.
   * @param nId the residue id.
   * @return the newly created residue.
   */
  virtual AbstractResidue* createResidue (t_Residue *type, const vector< CAtom > &vec, const CResId &nId) const;

  // I/O  -----------------------------------------------------------------
  
};

#endif
