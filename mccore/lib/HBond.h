//                              -*- Mode: C++ -*- 
// HBond.h
// Copyright � 2000-03 Laboratoire de Biologie Informatique et Th�orique.
//                     Universit� de Montr�al.
// Author           : S�bastien Lemieux <lemieuxs@iro.umontreal.ca>
// Created On       : 
// Last Modified By : Patrick Gendron
// Last Modified On : Wed Apr  9 11:23:49 2003
// Update Count     : 30
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


#ifndef _HBond_h
#define _HBond_h

#include <iostream>
using namespace std;

namespace mccore {

  class AtomType;
  class BasicResidue;
  class Atom;

  /**
   * @short Hydrogen bonds.
   *
   * A class to represent hydrogen bonds.  Hydrogen bonds are a
   * specialization of the more general concept of a chemical bond
   * between any two atoms.
   *
   * @author Patrick Gendron (<a href="mailto:gendrop@iro.umontreal.ca">gendrop@iro.umontreal.ca</a>)
   * @version $Id: HBond.h,v 1.2 2003-04-11 01:33:56 gendrop Exp $
   */
  class HBond
    {
    protected:
      /**
       * The donor atom type implicated in the bond.
       */
      const AtomType *donor;

      /**
       * The hydrogen type implicated in the bond (connected to the donor atom).
       */
      const AtomType *hydrogen;
      
      /**
       * The acceptor type implicated in the bond.
       */
      const AtomType *acceptor;
      
      /**
       * The lone pair type implicated in the bond (connected to the acceptor).
       */
      const AtomType *lonepair;

      /**
       * The value of the last evaluation.
       */
      float value;
      
      /**
       * The residue on which we find the donor group
       */
      const BasicResidue *resD;
      
      /**
       * The residue on which we find the acceptor group
       */
      const BasicResidue *resA;
      
      /**
       * Gaussian parameters for Pairing pattern detection. (see
       * S. Lemieux and F. Major (2002) RNA canonical and non-canonical
       * base pairing types: a recognition method and complete
       * repertoire, NAR 30(19):4250-4263.
       */
      static const int sNbGauss;
      static const float sProbH[7];
      static const float sWeight[7];
      static const float sMean[7][3];
      static const float sCovarInv[7][3][3];
      static const float sCovarDet[7];

    public:
      
      // LIFECYCLE -------------------------------------------------------------
      
      /**
       * Initializes the object.  Every pointer is set to 0.
       */
      HBond ();
      
      /**
       * Initializes the object.
       * @param d the donor atom type.
       * @param h the hydrogen type.
       * @param a the acceptor atom type.
       * @param l the lonepair implicated.
       */
      HBond (const AtomType *d, const AtomType *h, 
	     const AtomType *a, const AtomType *l);

      /**
       * Initializes the object with the other's content.
       * @param other the object to copy.
       */
      HBond (const HBond &other);
      
      /**
       * Destroys the object.  Nothing to do.
       */
      virtual ~HBond () { }
      
      // OPERATORS -------------------------------------------------------------
      
      /**
       * Assigns the other's content to the object.
       * @param other the object to copy.
       * @return itself.
       */
      const HBond& operator= (const HBond &other);
      
      /**
       * Tests the equality between two HBond objects.
       * @param other the object to test.
       * @return the truth value.
       */
      virtual bool operator== (const HBond &other) const;
      
      // ACCESS ----------------------------------------------------------------

      /**
       * Returns the donor atom.
       * @param r a residue into which to take the atom.  If none is
       * supplied and the HBond was evaluated on a pair of residues,
       * returns the atom from this donor.
       * @return the donor atom. 
       */
      const Atom& getDonor () const;
      const Atom& getDonor (const BasicResidue& r) const;

      /**
       * Returns the hydrogen atom.
       * @param r a residue into which to take the atom.  If none is
       * supplied and the HBond was evaluated on a pair of residues,
       * returns the atom from this donor.
       * @return the hydrogen atom. 
       */
      const Atom& getHydrogen () const;
      const Atom& getHydrogen (const BasicResidue& r) const;

      /**
       * Returns the acceptor atom.
       * @param r a residue into which to take the atom.  If none is
       * supplied and the HBond was evaluated on a pair of residues,
       * returns the atom from this donor.
       * @return the acceptor atom. 
       */
      const Atom& getAcceptor () const;
      const Atom& getAcceptor (const BasicResidue& r) const;
      
      /**
       * Returns the lonepair atom.
       * @param r a residue into which to take the atom.  If none is
       * supplied and the HBond was evaluated on a pair of residues,
       * returns the atom from this donor.
       * @return the lonepair atom. 
       */
      const Atom& getLonePair () const;
      const Atom& getLonePair (const BasicResidue& r) const;
      
      
      /**
       * Returns the donor residue, if one is set for this HBond.
       */
      const BasicResidue* getDonorResidue () const {
	return resD;
      }

      /**
       * Returns the donor residue, if one is set for this HBond.
       */
      const BasicResidue* getAcceptorResidue () const {
	return resA;
      }

      float getValue () { return value; }

      // METHODS ---------------------------------------------------------------

      /**
       * Evaluates the possibility that the given hydrogen bond forms
       * given the spatial coordinates of the residues (based on
       * Sebastien's observation that the Hydrogen-LP distance is a good
       * criteria).
       * @param ra the residue that contains the donor atom.
       * @param rb the residue that contains the acceptor atom.
       * @return a score between 0 and 1, where 0 is low probability and 1 is high probability.
       */
      float eval (const BasicResidue &ra, const BasicResidue &rb);

      /**
       * Evaluates the possibility that the given hydrogen bond forms
       * given the spatial coordinates of the residues (based on a
       * Gaussian mixture model which should be used only for RNA/DNA
       * base pairs).
       * @param ra the residue that contains the donor atom.
       * @param rb the residue that contains the acceptor atom.
       * @return a score between 0 and 1, where 0 is low probability and 1 is high probability.      
       */
      float evalStatistically (const BasicResidue &ra, const BasicResidue &rb);
      
      // I/O -------------------------------------------------------------------

      /**
       * Outputs the H-bond in Amber restraint format.
       * @param os the output stream.
       */
      void writeAmberRestraint (ostream &os) const;
      
      /**
       * Outputs the H-bond in human readable format.
       * @param os the output stream.
       * @param theBond the H-bond to display.
       * @return the used output stream.
       */
      ostream &output (ostream &os) const;
    };
 
  ostream &operator<< (ostream &os, const HBond &theBond); 
}

#endif








