//                              -*- Mode: C++ -*- 
// Molecule.h
// Copyright � 2003 Laboratoire de Biologie Informatique et Th�orique
//                  Universit� de Montr�al.
// Author           : Martin Larose
// Created On       : Mon Jul  7 15:59:36 2003
// $Revision: 1.1.4.4 $
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


#ifndef _mccore_Molecule_h_
#define _mccore_Molecule_h_

#include <iostream>
#include <list>
#include <map>

class Model;
class iPdbstream;
class oPdbstream;

using namespace std;



/**
 * @short Container for a collection of Models.
 *
 * This is a collection of mccore Models in a simple STL list.
 *
 * @author Martin Larose (<larosem@iro.umontreal.ca>)
 * @version $Id: Molecule.h,v 1.1.4.4 2003-11-21 15:55:14 larosem Exp $
 */
class Molecule : public list< Model* >
{
  /**
   * Properties for the molecule class.
   */
  map< const char*, char* > properties;
  
 public:
  
  // LIFECYCLE ------------------------------------------------------------
  
  /**
   * Initializes the object.
   */
  Molecule () { }
  
  /**
   * Initializes the object with the right's content.
   * @param right the object to copy.
   */
  Molecule (const Molecule &right);
  
  /**
   * Clones the molecule.
   * @return a copy of the molecule.
   */
  virtual Molecule* clone () const { return new Molecule (*this); }
  
  /**
   * Destroys the object.
   */
  virtual ~Molecule ();
  
  // OPERATORS ------------------------------------------------------------
  
  /**
   * Assigns the object with the right's content.
   * @param right the object to copy.
   * @return itself.
   */
  Molecule& operator= (const Molecule &right);
  
  // ACCESS ---------------------------------------------------------------
  
  /**
   * Gets the property value of the key.
   * @param key the key.
   * @return the value of the key.
   */
  const char* getProperty (const char *key) const;
  
  /**
   * Sets the key value pair.
   * @param key the key.
   * @param value the value.
   */
  void setProperty (const char *key, const char *value);
  
  /**
   * Gets the property map.
   * @return the property map.
   */
  map< const char*, char* >& getProperties () { return properties; }
  
  // METHODS --------------------------------------------------------------
  
  // I/O  -----------------------------------------------------------------
  
};

// NON-MEMBER FUNCTION -------------------------------------------------------

/**
 * Outputs the molecule to an output stream.
 * @param obs the output stream.
 * @param obj the molecule to output.
 * @return the output stream.
 */
ostream& operator<< (ostream &obs, const Molecule &obj);

/**
 * Inputs the molecule from a pdb stream.
 * @param ips the input pdb stream.
 * @param obj the molecule where to put the models.
 * @return the input pdb stream.
 */
iPdbstream& operator>> (iPdbstream &ips, Molecule &obj);

/**
 * Outputs the molecule to a pdb stream.
 * @param ops the output pdb stream.
 * @param obj the molecule to output.
 * @return the output pdb stream.
 */
oPdbstream& operator<< (oPdbstream &ops, const Molecule &obj);

#endif
