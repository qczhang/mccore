//                              -*- Mode: C++ -*- 
// ResidueFactoryMethod.cc
// Copyright � 2003 Laboratoire de Biologie Informatique et Th�orique
//                  Universit� de Montr�al.
// Author           : Patrick Gendron
// Created On       : Thu Mar 20 16:21:52 2003
// Last Modified By : Patrick Gendron
// Last Modified On : Mon Mar 24 17:59:08 2003
// Update Count     : 7
// Status           : Unknown.
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


#include "ResidueFactoryMethod.h"
#include "BasicResidue.h"
#include "Residue.h"

namespace mccore {


  BasicResidue* 
  BasicResidueFM::createResidue () const
  {
    return new BasicResidue ();
  }


  BasicResidue* 
  ResidueFM::createResidue () const
  {
    return new Residue ();
  }

}