//                              -*- Mode: C++ -*- 
// RnamlReader.h
// Copyright � 2003 Laboratoire de Biologie Informatique et Th�orique
//                  Universit� de Montr�al.
// Author           : Martin Larose
// Created On       : Tue Jul 15 12:56:11 2003
// $Revision: 1.1.2.4 $
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


#ifndef _mccore_RnamlReader_h_
#define _mccore_RnamlReader_h_

#include <vector>

namespace rnaml {
  class Atom;
  class Base;
  class InputStream;
  class Model;
  class Molecule;
  class Object;
  class Rnaml;
}

class AbstractResidue;
class CAtom;
class Model;
class Molecule;
class ResidueFactoryMethod;

using namespace std;



/**
 * @short Reader for mccore objects from rnaml.
 *
 * This class reads mccore objets from a rnaml stream.  It returns a
 * molecule.
 *
 * @author Martin Larose (<a href="mailto:larosem@iro.umontreal.ca">larosem@iro.umontreal.ca</a>)
 * @version $Id: RnamlReader.h,v 1.1.2.4 2003-11-21 15:55:43 larosem Exp $
 */
class RnamlReader
{
  
 private:
    
  /**
   * The input stream.
   */
  rnaml::InputStream *is;

  /**
   * The residue factory method.  It is set to the default residue fm of the
   * model.
   */
  ResidueFactoryMethod *residueFM;
  
  /**
   * The rnaml object.
   */
  rnaml::Rnaml *rnaml;
  
  /**
   * The vector of Rnaml children.
   */
  vector< rnaml::Object* > children;
  
  /**
   * The iterator over the children vector.
   */
  vector< rnaml::Object* >::iterator it;
    
 public:
  
  // LIFECYCLE ------------------------------------------------------------
  
  /**
   * Initializes the reader with a file name.
   * @param name the file name.
   * @param fm the residue factory method optionnal parameter.
   */
  RnamlReader (const char *name, ResidueFactoryMethod *fm = 0);

  /**
   * Inintializes the reader with a input stream.
   * @param is the input stream.
   * @param fm the residue factory method optionnal parameter.
   */
  RnamlReader (rnaml::InputStream *is, ResidueFactoryMethod *fm = 0);
  
  /**
   * Destroys the object.
   */
  virtual ~RnamlReader ();
  
  // OPERATORS ------------------------------------------------------------
  
  // ACCESS ---------------------------------------------------------------
  
  // METHODS --------------------------------------------------------------
  
 private:
  
  /**
   * Converts the rnaml Atom to a mccore CAtom.
   * @param base the rnaml Atom to convert.
   * @return the mccore CAtom.
   */
  CAtom* toMccore (const rnaml::Atom &atom);

  /**
   * Converts the rnaml Base to a mccore AbstractResidue.
   * @param base the rnaml Base to convert.
   * @return the mccore AbstractResidue.
   */
  AbstractResidue* toMccore (const rnaml::Base &base);
  
  /**
   * Converts the rnaml Model to a mccore Model.
   * @param model the rnaml Model to convert.
   * @return the mccore Model.
   */
  Model* toMccore (const rnaml::Model &model);
  
  /**
   * Converts the rnaml Molecule to a mccore Molecule.
   * @param mol the rnaml Molecule to convert.
   * @return the mccore Molecule.
   */
  Molecule* toMccore (const rnaml::Molecule &molecule);
  
 public:
  
  // I/O  -----------------------------------------------------------------
  
  /**
   * Closes the stream.
   */
  void close ();
  
  /**
   * Reads a molecule from the rnaml input stream.
   * @return the mccore Molecule or 0 if there is no more molecule in the
   * file or a problem occurs.
   */
  Molecule* read ();
  
};

#endif
