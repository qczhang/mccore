//                              -*- Mode: C++ -*- 
// Atom.cc
// Copyright � 2003 Laboratoire de Biologie Informatique et Th�orique
//                  Universit� de Montr�al.
// Author           : Patrick Gendron
// Created On       : Mon Mar 10 14:00:09 2003


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "Atom.h"
#include "Binstream.h"
#include "AtomType.h"

namespace mccore {


  // LIFECYCLE ------------------------------------------------------------
  
  
  Atom::Atom () : type (0) 
  {}


  Atom::Atom (float x, float y, float z, const AtomType *aType) 
    : Vector3D (x, y, z), 
      type (aType) 
  {}


  Atom::Atom (Vector3D aPoint, const AtomType *aType)
    : Vector3D (aPoint.getX(), aPoint.getY(), aPoint.getZ()),
      type (aType) 
  {}
  

  Atom::Atom (const Atom &other) 
    : Vector3D (other),
      type (other.type) 
  {
  }
  

  Atom* 
  Atom::clone () const 
  { 
    return new Atom (*this); 
  }
  

  // OPERATORS ------------------------------------------------------------


  Atom& 
  Atom::operator= (const Atom &other)
  {
    if (&other != this) {
      Vector3D::operator= (other);
      type = other.type;
    }
    return *this;
  }


  bool 
  Atom::operator== (const Atom &other) const
  { 
    return *type == *other.getType (); 
  }
  
  
  bool 
  Atom::operator!= (const Atom &other) const 
  { 
    return ! operator== (other); 
  }
  
  bool 
  Atom::operator< (const Atom &other) const
  { 
    return *type < *other.type; 
  }
  

// METHODS --------------------------------------------------------------

  Vector3D 
  Atom::getColor () const 
  {
    return AtomType::getColor (getType ());
  }
  
  // I/O -----------------------------------------------------------------------

  ostream&
  operator<< (ostream &os, const Atom& atom)
  {
  return os << (const Vector3D&) atom 
	    << ' ' << *atom.getType ();
}



iBinstream&
operator>> (iBinstream &ibs, Atom &atom)
{
  const AtomType *type;

  ibs >> (Vector3D&)atom;
  ibs >> type;
  
  atom.setType (type);
  return ibs;
}



oBinstream&
operator<< (oBinstream &obs, const Atom &atom)
{
  
  return obs << (const Vector3D&) atom << atom.getType ();
}

}