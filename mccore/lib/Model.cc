//                              -*- Mode: C++ -*- 
// Model.cc
// Copyright � 2001 Laboratoire de Biologie Informatique et Th�orique.
//                  Universit� de Montr�al.
// Author           : Martin Larose <larosem@iro.umontreal.ca>
// Created On       : Wed Oct 10 15:34:08 2001
// Last Modified By : Martin Larose
// Last Modified On : Thu Oct 25 11:19:59 2001
// Update Count     : 2
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

#include <string.h>

#include "AbstractResidue.h"
#include "Binstream.h"
#ifndef HAVE_STRSEP
#include "McCore.h"
#endif
#include "Model.h"
#include "Pdbstream.h"
#include "CResidueFM.h"
#include "ResidueFM.h"
#include "ResidueType.h"



Model::model_iterator&
Model::model_iterator::operator= (const Model::model_iterator &right)
{
  if (this != &right)
    list< AbstractResidue* >::iterator::operator= (right);
  return *this;
}


  
Model::model_const_iterator&
Model::model_const_iterator::operator= (const Model::model_const_iterator &right)
{
  if (this != &right)
    list< AbstractResidue* >::const_iterator::operator= (right);
  return *this;
}



Model::Model (ResidueFactoryMethod *fm)
{
  residueFM = (fm == 0) ? new CResidueFM () : fm;
}


  
Model::Model (const Model &right)
{
  const_iterator cit;

  residueFM = right.residueFM->clone ();
  for (cit = right.begin (); cit != right.end (); ++cit)
    push_back (cit->clone ());
}



Model::~Model ()
{
  iterator it;

  delete residueFM;
  for (it = begin (); it != end (); ++it)
    delete &*it;
}



Model&
Model::operator= (const Model &right)
{
  if (this != &right)
    {
      iterator it;
      const_iterator cit;

      delete residueFM;
      residueFM = right.residueFM->clone ();
      for (it = begin (); it != end (); ++it)
	delete &*it;
      clear ();

      for (cit = right.begin (); cit != right.end (); ++cit)
	push_back (cit->clone ());
    }
  return *this;
}



AbstractResidue&
Model::operator[] (size_type nth)
{
  if (nth > size ())
    return *(end ());
  else
    {
      iterator it;
      
      for (it = begin (); nth > 0; --nth, ++it);
      return *it;
    }
}



const AbstractResidue&
Model::operator[] (size_type nth) const
{
  if (nth > size ())
    return *(end ());
  else
    {
      const_iterator cit;
      
      for (cit = begin (); nth > 0; --nth, ++cit);
      return *cit;
    }
}



void
Model::setResidueFM (ResidueFactoryMethod *fm)
{
  delete residueFM;
  residueFM = fm;
}



AbstractResidue::iterator
Model::find (const char *str)
{
  char *s = new char[strlen (str) + 1];
  char *p = s;
  char *argum;
  vector< char* > tok;
  Model::iterator mit;
  AbstractResidue::iterator it;
  
  strcpy (s, str);
  argum = strsep (&p, ":");
  while (argum)
    {
      if (strlen (argum) > 0) 
	tok.push_back (argum);
      argum = strsep (&p, ":");
    }

  if (tok.size () == 2
      && (mit = find (CResId (tok[0]))) != end ()
      && (it = mit->find (iPdbstream::GetAtomType (tok[1]))) != mit->end ())
    return it;
  else
    return AbstractResidue::iterator ();
}



AbstractResidue::const_iterator
Model::find (const char *str) const
{
  char *s = strdup (str);
  char *p = s;
  char *argum;
  vector< char* > tok;
  Model::const_iterator mit;
  AbstractResidue::const_iterator it;
  
  argum = strsep (&p, ":");
  while (argum)
    {
      if (strlen (argum) > 0) 
	tok.push_back (argum);
      argum = strsep (&p, ":");
    }

  if (tok.size () != 2
      || (mit = find (CResId (tok[0]))) == end ()
      || (it = mit->find (iPdbstream::GetAtomType (tok[1]))) == mit->end ())
    it = AbstractResidue::const_iterator ();
  delete[] s;
  return it;
}



Model::iterator
Model::find (const CResId &id)
{
  iterator it;

  for (it = begin (); it != end (); ++it)
    if (id == *it)
      break;
  return it;
}



Model::const_iterator
Model::find (const CResId &id) const
{
  const_iterator it;

  for (it = begin (); it != end (); ++it)
    if (id == *it)
      break;
  return it;
}



void
Model::validate ()
{
  iterator it = begin ();

  while (it != end ())
    {
      it->validate ();
      if (it->getType ()->is_NucleicAcid ()
	  || it->getType ()->is_AminoAcid ())
	++it;
      else
	{
	  delete &*it;
	  it = erase (it);
	}
    }
}



void
Model::removeOptionals ()
{
  iterator it;

  for (it = begin (); it != end (); ++it)
    it->removeOptionals ();
}



void
Model::removeAminoAcid ()
{
  iterator modelIt = begin ();

  while (modelIt != end ())
    {
      if (modelIt->getType ()->is_AminoAcid ())
	{
	  delete &*modelIt;
	  modelIt = erase (modelIt);
	}
      else
	++modelIt;
    }
}



void
Model::removeNucleicAcid ()
{
  iterator modelIt;

  for (modelIt = begin (); modelIt != end ();)
    {
      if (modelIt->getType ()->is_NucleicAcid ())
	{
	  delete &*modelIt;
	  modelIt = erase (modelIt);
	}
      else
	++modelIt;
    }
}



void
Model::keepAminoAcid ()
{
  iterator modelIt;

  for (modelIt = begin (); modelIt != end ();)
    {
      if (modelIt->getType ()->is_AminoAcid ())
	++modelIt;
      else
	{
	  delete &*modelIt;
	  modelIt = erase (modelIt);
	}
    }
}



void
Model::keepNucleicAcid ()
{
  iterator modelIt;

  for (modelIt = begin (); modelIt != end ();)
    {
      if (modelIt->getType ()->is_NucleicAcid ())
	++modelIt;
      else
	{
	  modelIt = erase (modelIt);
	  delete &*modelIt;
	}
    }
}



oBinstream&
Model::write (oBinstream &obs) const
{
  const_iterator cit;
  
  obs << size ();
  for (cit = begin (); cit != end (); ++cit)
    obs << *cit;
  return obs;
}



oPdbstream&
Model::write (oPdbstream &ops) const
{
  const_iterator cit;

  if ((cit = begin ()) != end ())
    {
      char last_chain_id = ((CResId)*cit).GetChainId ();

      while (cit != end ())
	{
	  if (((CResId)*cit).GetChainId () != last_chain_id)
	    {
	      ops.TER ();
	      last_chain_id = ((CResId)*cit).GetChainId ();
	    }
	  ops << *cit;
	  ++cit;
	}
      ops.TER ();
    }
  return ops;
}



bool
operator< (const Model::iterator &left, const Model::iterator &right)
{
  return *left < *right;
}



iPdbstream&
operator>> (iPdbstream &ips, Model &obj)
{
  obj.clear ();
  while (! (ips.eof () || ips.eop ()))
    {
      AbstractResidue *res = obj.getResidueFM ()->createResidue ();

      ips >> *res;
      if (res->size () != 0)
	obj.push_back (res);
      else
	delete res;
      if (ips.eom ())
	break;
    }
  return ips;
}



oPdbstream&
operator<< (oPdbstream &ops, const Model &obj)
{
  return obj.write (ops);
}



iBinstream&
operator>> (iBinstream &ibs, Model &obj)
{
  Model::size_type sz;

  ibs >> sz;
  for (; sz > 0; --sz)
    {
      AbstractResidue *res = obj.getResidueFM ()->createResidue ();

      ibs >> *res;
      obj.push_back (res);
    }
  return ibs;
}



oBinstream&
operator<< (oBinstream &obs, const Model &obj)
{
  return obj.write (obs);
}



ostream&
operator<< (ostream &os, const Model &model)
{
  Model::const_iterator cit;

  for (cit = model.begin (); cit != model.end (); ++cit)
    os << *cit;
  return os;
}
