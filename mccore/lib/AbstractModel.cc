//                              -*- Mode: C++ -*- 
// AbstractModel.cc
// Copyright � 2004 Laboratoire de Biologie Informatique et Th�orique
//                  Universit� de Montr�al.
// Author           : Martin Larose <larosem@iro.umontreal.ca>
// Created On       : Thu Dec  9 16:46:03 2004
// $Revision: 1.2 $
// $Id: AbstractModel.cc,v 1.2 2005-01-03 22:48:44 larosem Exp $
// 
// This file is part of mccore.
// 
// mccore is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// 
// mccore is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public
// License along with mccore; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <algorithm>
#include <string.h>

#include "AbstractModel.h"
#include "Binstream.h"
#include "ExtendedResidue.h"
#include "Pdbstream.h"
#include "Residue.h"
#include "ResidueFactoryMethod.h"



namespace mccore
{

  AbstractModel::AbstractModel (const ResidueFactoryMethod *fm)
    : residueFM (0 == fm ? new ExtendedResidueFM () : fm->clone ())
  { }

  
  AbstractModel::AbstractModel (const AbstractModel &right)
    : residueFM (right.residueFM->clone ())
  { }


  AbstractModel::~AbstractModel ()
  {
    delete this->residueFM;
  }


  AbstractModel&
  AbstractModel::operator= (const AbstractModel &right)
  {
    delete residueFM;
    residueFM = right.residueFM->clone ();
    return *this;
  }

  
  void
  AbstractModel::setResidueFM (const ResidueFactoryMethod *fm)
  {
    delete this->residueFM;
    this->residueFM = 0 == fm ? new ExtendedResidueFM () : fm->clone ();
  }


  AbstractModel::iterator
  AbstractModel::find (const ResId &id)
  {
    iterator it;

    for (it = begin (); it != end (); ++it)
      if (id == it->getResId ())
	break;
    return it;
  }


  AbstractModel::const_iterator
  AbstractModel::find (const ResId &id) const
  {
    const_iterator it;

    for (it = begin (); it != end (); ++it)
      if (id == it->getResId ())
	break;
    return it;
  }
  
  
  void
  AbstractModel::validate ()
  {
    iterator it = begin ();
    
    while (it != end ())
      {
	it->validate ();
	if (it->getType ()->isNucleicAcid ()
	    || it->getType ()->isAminoAcid ())
	  ++it;
	else
	  {
	    it = erase (it);
	  }
      }
  }

  
  void
  AbstractModel::addHLP (bool overwrite)
  {
    iterator it;
    
    for (it = begin (); it != end (); ++it)
      {
	it->addHydrogens (overwrite);
	it->addLonePairs (overwrite);
      }
  }

  
  void
  AbstractModel::removeOptionals ()
  {
    iterator it;

    for (it = begin (); it != end (); ++it)
      it->removeOptionals ();
  }


  void
  AbstractModel::removeAminoAcid ()
  {
    iterator modelIt = begin ();

    while (modelIt != end ())
      {
	if (modelIt->getType ()->isAminoAcid ())
	  {
	    modelIt = erase (modelIt);
	  }
	else
	  ++modelIt;
      }
  }


  void
  AbstractModel::removeNucleicAcid ()
  {
    iterator modelIt;

    modelIt = begin ();

    while (modelIt != end ())
      {
	if (modelIt->getType ()->isNucleicAcid ())
	  {
	    modelIt = erase (modelIt);
	  }
	else
	  ++modelIt;
      }
  }


  void
  AbstractModel::removeWater ()
  {
    const ResidueType* water = ResidueType::parseType ("HOH");
    iterator modelIt;

    modelIt = begin ();
    while (modelIt != end ())
      {
	if (modelIt->getType () == water)
	  {
	    modelIt = erase (modelIt);
	  }
	else
	  ++modelIt;
      }
  }


  void
  AbstractModel::keepAminoAcid ()
  {
    iterator modelIt;

    for (modelIt = begin (); modelIt != end ();)
      {
	if (modelIt->getType ()->isAminoAcid ())
	  ++modelIt;
	else
	  {
	    modelIt = erase (modelIt);
	  }
      }
  }


  void
  AbstractModel::keepNucleicAcid ()
  {
    iterator modelIt;

    for (modelIt = begin (); modelIt != end ();)
      {
	if (modelIt->getType ()->isNucleicAcid ())
	  ++modelIt;
	else
	  {
	    modelIt = erase (modelIt);
	  }
      }
  }


  oPdbstream&
  AbstractModel::output (oPdbstream &ops) const
  {
    const_iterator cit;
    char chainId;

    cit = begin ();
    if (end () != cit)
      {
	chainId = cit->getResId ().getChainId ();
	for (; end () != cit; ++cit)
	  {
	    if (cit->getResId ().getChainId () != chainId)
	      {
		ops.ter ();
		chainId = cit->getResId ().getChainId ();
	      }
	    ops << *cit;
	  }
	ops.ter ();
      }
    return ops;
  }
  
  
  iPdbstream&
  operator>> (iPdbstream &ips, AbstractModel &obj)
  {
    return obj.input (ips);
  }


  oPdbstream&
  operator<< (oPdbstream &ops, const AbstractModel &obj)
  {
    return obj.output (ops);
  }


  iBinstream&
  operator>> (iBinstream &ibs, AbstractModel &obj)
  {
    return obj.input (ibs);
  }


  oBinstream&
  operator<< (oBinstream &obs, const AbstractModel &obj)
  {
    return obj.output (obs);
  }


  bool
  operator< (const AbstractModel::iterator &left, const AbstractModel::iterator &right)
  {
    return *left < *right;
  }


  bool
  operator< (const AbstractModel::const_iterator &left, const AbstractModel::const_iterator &right)
  {
    return *left < *right;
  }
  
}



namespace std
{
  
  ostream&
  operator<< (ostream &os, const mccore::AbstractModel &obj)
  {
    return obj.output (os);
  }

}

