//                              -*- Mode: C++ -*- 
// CHBond.h
// Copyright � 2000 Laboratoire de Biologie Informatique et Th�orique.
// Author           : S�bastien Lemieux <lemieuxs@iro.umontreal.ca>
// Created On       : 
// Last Modified By : Martin Larose
// Last Modified On : Tue Oct 24 11:14:42 2000
// Update Count     : 1
// Status           : Ok.
// 


#ifndef _CHBond_h
#define _CHBond_h


class CResidue;
class t_Atom;
class ostream;



/**
 * @short Describes the H-bond between to atoms.
 *
 * @author S�bastien Lemieux <lemieuxs@iro.umontreal.ca>
 */
class CHBond
{
  /**
   * The left residue in the bond.
   */
  const CResidue *mResidueA;

  /**
   * The right residue in the bond.
   */
  const CResidue *mResidueB;

  /**
   * The donor atom type implicated in the bond.
   */
  t_Atom *mDonor;

  /**
   * The acceptor type implicated in the bond.
   */
  t_Atom *mAcceptor;

  /**
   * The hydrogen type implicated in the bond.
   */
  t_Atom *mHydro;

  /**
   * The lone pair type implicated in the bond.
   */
  t_Atom *mLonePair;

  /**
   * The penality associated with the bond.  It is calculated once and then
   * cached in this variable.  Initial value -1.
   */
  mutable float cache_penality;

public:

  // LIFECYCLE ------------------------------------------------------------

  /**
   * Initializes the object.  Every pointer is set to 0 and the
   * cache_penalty is set to -1.
   */
  CHBond ();

  /**
   * Initializes the object with the right's content.
   * @param right the object to copy.
   */
  CHBond (const CHBond &right);

  /**
   * Initializes the object.
   * @param nResidueA the left residue.
   * @param nResidueB the right residue.
   * @param nDonor the donor atom type.
   * @param nHydro the hydrogen type.
   * @param nAcceptor the acceptor atom type.
   * @param nLonePair the lonepair implicated.
   */
  CHBond (const CResidue *nResidueA, const CResidue *nResidueB,
	  t_Atom *nDonor, t_Atom *nHydro, 
	  t_Atom *nAcceptor, t_Atom *nLonePair);

  /**
   * Destructs the object.  Nothing to do.
   */
  ~CHBond () { }

  // OPERATORS ------------------------------------------------------------

  /**
   * Assigns the right's content to the object.
   * @param right the object to copy.
   * @return itself.
   */
  const CHBond& operator= (const CHBond &right);

  /**
   * Calculates the penalty for the actual H-bond.  The result is cached
   * into cache_penalty.
   */
  operator float () const;
  
  // ACCESS ---------------------------------------------------------------

  /**
   * Sets the CHBond slots.
   * @param nResidueA the left residue.
   * @param nResidueB the right residue.
   * @param nDonor the donor atom type.
   * @param nHydro the hydrogen type.
   * @param nAcceptor the acceptor atom type.
   * @param nLonePair the lonepair implicated.
   */
  void SetHBond (const CResidue *nResidueA, const CResidue *nResidueB,
		 t_Atom *nDonor, t_Atom *nHydro,
		 t_Atom *nAcceptor, t_Atom *nLonePair);

  // METHODS --------------------------------------------------------------

  // I/O  -----------------------------------------------------------------

  /**
   * Outputs the H-bond in Amber restraint format.
   * @param os the output stream.
   */
  void DumpAmberRestraint (ostream &os);

  /**
   * Outputs the H-bond in human readable format.
   * @param os the output stream.
   * @param theBond the H-bond to display.
   * @return the used output stream.
   */
  friend ostream &operator<< (ostream &os, const CHBond &theBond);
};

#endif









