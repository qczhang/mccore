//                              -*- Mode: C++ -*- 
// Residue.cc
// Copyright � 2003-04 Laboratoire de Biologie Informatique et Th�orique
// Author           : Patrick Gendron
// Created On       : Fri Mar 14 16:44:35 2003
// $Revision: 1.44 $
// $Id: Residue.cc,v 1.44 2004-08-17 18:30:37 thibaup Exp $
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

#include <typeinfo>

#include <algorithm>
#include <assert.h>
#include <math.h>
#include <set>

#include "Binstream.h"
#include "Exception.h"
#include "Messagestream.h"
#include "Pdbstream.h"
#include "PropertyType.h"
#include "ResId.h"
#include "Residue.h"
#include "ResidueTopology.h"
#include "ResidueFactoryMethod.h"
#include "Rmsd.h"

#define RAD_36  0.6283185
#define RAD_72  1.2566371
#define RAD_90  1.5707963
#define RAD_108 1.8849556
#define RAD_144 2.5132741
#define RAD_180 3.1415927
#define RAD_216 3.7699112
#define RAD_252 4.3982297
#define RAD_270 4.7123890
#define RAD_288 5.0265482
#define RAD_324 5.6548668
#define RAD_360 6.2831853

#define RAD(deg) (deg * M_PI / 180.0)
#define DEG(rad) (rad * 180.0 / M_PI)

namespace mccore {

  // Parameters taken for AMBER all_nuc94.in
  const float Residue::C_H_DIST_CYC = 1.08f;    // C-H distance for aromatic C
  const float Residue::C_H_DIST = 1.09f;    // C-H distance for SP3 C
  const float Residue::N_H_DIST = 1.01f;    // N-H distance for NH2 confo
  const float Residue::O_H_DIST = 0.96f;
  const float Residue::O_LP_DIST = 1.00f;
  const float Residue::N_LP_DIST = 1.00f;
  const float Residue::TAN19 = 0.354f;      // O2' H 
  const float Residue::TAN54 = 1.376f;
  const float Residue::TAN60 = 1.7320508f;  // For NH2-like conformations
  const float Residue::TAN70 = 2.7474774f;  // For CH3-like conformations
  const float Residue::TAN30 = 0.57735027f;
  
  const float Residue::s_cosf_amplitude = 1.3305;
  const float Residue::s_cosf_vshift = 2.0778;
  const float Residue::s_cosf_phase = 0.3041;
  const float Residue::s_cosf_2xphase = 2.0 * Residue::s_cosf_phase;

  const float Residue::s_2xpi = 2.0 * M_PI;
  const float Residue::s_4xpi = 4.0 * M_PI;

  float Residue::s_rib_minshift = 0.1;
  float Residue::s_rib_mindrop = 0.00001;
  float Residue::s_rib_shiftrate = 0.5;
  
  // LIFECYCLE ------------------------------------------------------------


  Residue::Residue ()
    : rib_dirty_ref (true),
      rib_built_valid (false),
      rib_built_count (0)
  {
    this->setType (0);
    rib_C1p = rib_C2p = rib_C3p = rib_C4p = rib_C5p = rib_O2p = rib_O3p = rib_O4p = rib_O5p = rib_P = 0;
  }

  

  Residue::Residue (const ResidueType *t, const ResId &i)
    : rib_dirty_ref (true),
      rib_built_valid (false),
      rib_built_count (0)
  {
    this->setType (t);
    this->setResId (i);
    rib_C1p = rib_C2p = rib_C3p = rib_C4p = rib_C5p = rib_O2p = rib_O3p = rib_O4p = rib_O5p = rib_P = 0;
  }


  Residue::Residue (const ResidueType *t, const ResId &i, const vector< Atom > &vec)
    : rib_dirty_ref (true),
      rib_built_valid (false),
      rib_built_count (0)
  {
    this->setType (t);
    this->setResId (i);
    vector< Atom >::const_iterator j;
    for (j=vec.begin (); j!=vec.end (); ++j) {
      insert (*j);
    }

    rib_C1p = rib_C2p = rib_C3p = rib_C4p = rib_C5p = rib_O2p = rib_O3p = rib_O4p = rib_O5p = rib_P = 0;
  }


  Residue::Residue (const Residue &other)
  {
    vector< Atom* >::const_iterator cit;

    type = other.type;
    resId = other.resId;
    for (cit = other.atomGlobal.begin (); cit != other.atomGlobal.end (); ++cit)
    {
      Atom* cl = (*cit)->clone ();
      atomGlobal.push_back (cl);
    }    
    atomIndex = other.atomIndex;

    rib_C1p = rib_C2p = rib_C3p = rib_C4p = rib_C5p = rib_O2p = rib_O3p = rib_O4p = rib_O5p = rib_P = 0;
    rib_dirty_ref = true;
    rib_built_valid = other.rib_built_valid;
    rib_built_count = other.rib_built_count;
  }

  
  Residue* 
  Residue::clone () const 
  { 
    return new Residue (*this); 
  }
  

  Residue::~Residue () 
  {
    vector< Atom* >::iterator it;
    for (it = atomGlobal.begin (); it != atomGlobal.end (); ++it)
      delete *it;
    atomGlobal.clear ();
    atomIndex.clear();
  }


  void
  Residue::setTheoretical ()
  {
    this->clear ();
    
    if (this->type->isA ())
      {
	this->insert (Atom ( 0.213f,  0.660f,  1.287f, AtomType::aN9));
	this->insert (Atom ( 0.250f,  2.016f,  1.509f, AtomType::aC4));
	this->insert (Atom ( 0.016f,  2.995f,  0.619f, AtomType::aN3));
	this->insert (Atom ( 0.142f,  4.189f,  1.194f, AtomType::aC2));
	this->insert (Atom ( 0.451f,  4.493f,  2.459f, AtomType::aN1));
	this->insert (Atom ( 0.681f,  3.485f,  3.329f, AtomType::aC6));
	this->insert (Atom ( 0.990f,  3.787f,  4.592f, AtomType::aN6));
	this->insert (Atom ( 0.579f,  2.170f,  2.844f, AtomType::aC5));
	this->insert (Atom ( 0.747f,  0.934f,  3.454f, AtomType::aN7));
	this->insert (Atom ( 0.520f,  0.074f,  2.491f, AtomType::aC8));
      }
    else if (this->type->isC ())
      {
	this->insert (Atom ( 0.212f,  0.668f,  1.294f, AtomType::aN1)); 
	this->insert (Atom ( 0.193f, -0.043f,  2.462f, AtomType::aC6));
	this->insert (Atom ( 0.374f,  2.055f,  1.315f, AtomType::aC2)); 
	this->insert (Atom ( 0.388f,  2.673f,  0.240f, AtomType::aO2)); 
	this->insert (Atom ( 0.511f,  2.687f,  2.504f, AtomType::aN3)); 
	this->insert (Atom ( 0.491f,  1.984f,  3.638f, AtomType::aC4)); 
	this->insert (Atom ( 0.631f,  2.649f,  4.788f, AtomType::aN4)); 
	this->insert (Atom ( 0.328f,  0.569f,  3.645f, AtomType::aC5)); 
      }
    else if (this->type->isG ())
      {
	this->insert (Atom ( 0.214f,  0.659f,  1.283f, AtomType::aN9)); 
	this->insert (Atom ( 0.254f,  2.014f,  1.509f, AtomType::aC4)); 
	this->insert (Atom ( 0.034f,  2.979f,  0.591f, AtomType::aN3)); 
	this->insert (Atom ( 0.142f,  4.190f,  1.110f, AtomType::aC2)); 
	this->insert (Atom (-0.047f,  5.269f,  0.336f, AtomType::aN2)); 
	this->insert (Atom ( 0.444f,  4.437f,  2.427f, AtomType::aN1)); 
	this->insert (Atom ( 0.676f,  3.459f,  3.389f, AtomType::aC6)); 
	this->insert (Atom ( 0.941f,  3.789f,  4.552f, AtomType::aO6)); 
	this->insert (Atom ( 0.562f,  2.154f,  2.846f, AtomType::aC5)); 
	this->insert (Atom ( 0.712f,  0.912f,  3.448f, AtomType::aN7)); 
	this->insert (Atom ( 0.498f,  0.057f,  2.485f, AtomType::aC8)); 
      }
    else if (this->type->isU ())
      {
	this->insert (Atom ( 0.212f,  0.676f,  1.281f, AtomType::aN1)); 
	this->insert (Atom ( 0.195f, -0.023f,  2.466f, AtomType::aC6));
	this->insert (Atom ( 0.370f,  2.048f,  1.265f, AtomType::aC2)); 
	this->insert (Atom ( 0.390f,  2.698f,  0.235f, AtomType::aO2)); 
	this->insert (Atom ( 0.505f,  2.629f,  2.502f, AtomType::aN3)); 
	this->insert (Atom ( 0.497f,  1.990f,  3.725f, AtomType::aC4)); 
	this->insert (Atom ( 0.629f,  2.653f,  4.755f, AtomType::aO4)); 
	this->insert (Atom ( 0.329f,  0.571f,  3.657f, AtomType::aC5)); 
      }
    else if (this->type->isT ())
      {
	this->insert (Atom ( 0.214f,  0.668f,  1.296f, AtomType::aN1)); 
	this->insert (Atom ( 0.171f, -0.052f,  2.470f, AtomType::aC6)); 
	this->insert (Atom ( 0.374f,  2.035f,  1.303f, AtomType::aC2)); 
	this->insert (Atom ( 0.416f,  2.705f,  0.284f, AtomType::aO2)); 
	this->insert (Atom ( 0.483f,  2.592f,  2.553f, AtomType::aN3)); 
	this->insert (Atom ( 0.449f,  1.933f,  3.767f, AtomType::aC4)); 
	this->insert (Atom ( 0.560f,  2.568f,  4.812f, AtomType::aO4)); 
	this->insert (Atom ( 0.279f,  0.500f,  3.685f, AtomType::aC5)); 
	this->insert (Atom ( 0.231f, -0.299f,  4.949f, AtomType::aC5M)); 
      }
    else if (this->type->isPhosphate ())
      {
	this->insert (Atom ( 4.691f,  0.327f, -2.444f, AtomType::aP));
	this->insert (Atom ( 5.034f,  1.678f, -1.932f, AtomType::aO1P));
	this->insert (Atom ( 4.718f,  0.068f, -3.906f, AtomType::aO2P));
	this->insert (Atom ( 3.246f, -0.057f, -1.895f, AtomType::aO5p));
	this->insert (Atom ( 5.662f, -0.712f, -1.734f, AtomType::aO3p));
      }
    else if (this->type->isRibose ())
      {
	this->insert (Atom ( 0.000f,  0.000f,  0.000f, AtomType::aC1p));                        
	this->insert (Atom (-0.694f, -0.627f, -1.210f, AtomType::aC2p));                
	this->insert (Atom ( 0.499f, -1.031f, -2.067f, AtomType::aC3p));                   
	this->insert (Atom ( 1.509f, -1.478f, -1.022f, AtomType::aC4p));                     
	this->insert (Atom ( 2.957f, -1.393f, -1.443f, AtomType::aC5p));
	this->insert (Atom ( 1.286f, -0.587f,  0.103f, AtomType::aO4p));
	if (this->type->isRNA ())
	  this->insert (Atom (-1.474f, -1.731f, -0.795f, AtomType::aO2p));
      }
    else
      {
	IntLibException ex ("", __FILE__, __LINE__);
	ex << "cannot create a theoretical residue for " << *this;
	throw ex;
      }

    this->finalize ();
    this->setReferential (HomogeneousTransfo::identity);

    // every atoms are present: inhibits warning
    int vlv = gOut.getVerboseLevel ();
    gOut.setVerboseLevel (2);
    this->addHydrogens ();
    this->addLonePairs ();
    gOut.setVerboseLevel (vlv);
    
  }

  void
  Residue::setFullTheoretical ()
  {
    if (this->type->isNucleicAcid ())
      {
	this->setTheoretical ();
      }
    else
      {
	IntLibException ex ("", __FILE__, __LINE__);
	ex << "cannot create a full theoretical residue for " << *this;
	throw ex;
      }
    
    // create a C3' endo anti ribose
    this->buildRibose (PropertyType::pC3p_endo, PropertyType::pAnti, true, true);

    // finish up 5' phosphate
    Residue *po4 = Residue::createPhosphate5p (*this);
    this->insert (*po4->find (AtomType::aO1P));
    this->insert (*po4->find (AtomType::aO2P));
    delete po4;
  }
  

  // OPERATORS -----------------------------------------------------------------


  Residue& 
  Residue::operator= (const Residue &other)
  {
    vector< Atom* >::const_iterator cit;

    if (this != &other) {
      type = other.type;
      resId = other.resId;
      clear ();
      
      for (cit = other.atomGlobal.begin (); cit != other.atomGlobal.end (); ++cit)
	atomGlobal.push_back ((*cit)->clone ());

      rib_built_valid = other.rib_built_valid;
      rib_built_count = other.rib_built_count;
      atomIndex = other.atomIndex;
    }

    return *this;
  }

  
  bool 
  Residue::operator== (const Residue &other) const
  { 
    return resId == other.resId 
      && *type == *other.getType (); 
  }
  

  bool 
  Residue::operator!= (const Residue &other) const 
  { 
    return ! operator== (other); 
  }
  
  
  bool 
  Residue::operator< (const Residue &other) const
  { 
    return resId < other.resId; 
  }


  // ACCESS --------------------------------------------------------------------


  const ResId& 
  Residue::getResId () const 
  { 
    return resId; 
  }


  void 
  Residue::setResId (const ResId& id) 
  { 
    resId = id; 
  }


  const ResidueType* 
  Residue::getType() const 
  { 
    return type; 
  }


  void 
  Residue::setType (const ResidueType* t) 
  { 
    type = t == 0 ? ResidueType::parseType ("undefined") : t; 
  }

  
  Residue::iterator 
  Residue::begin (AtomSet *atomset) 
  {
    return iterator (this, atomIndex.begin (), atomset);
  }
  
  
  Residue::iterator 
  Residue::end () 
  {
    return iterator (this, atomIndex.end ()); 
  }
  

  Residue::const_iterator 
  Residue::begin (AtomSet *atomset) const 
  {
    return const_iterator (this, atomIndex.begin (), atomset);
  }


  Residue::const_iterator 
  Residue::end () const 
  {
    return const_iterator (this, atomIndex.end ()); 
  }
  
  
  Residue::iterator 
  Residue::find (const AtomType *k) 
  {
    AtomMap::iterator it = atomIndex.find (k);
    if (it == atomIndex.end ())
      return end ();
    else
      return iterator (this, it);
  }


  Residue::const_iterator 
  Residue::find (const AtomType *k) const
  {
    AtomMap::const_iterator cit = atomIndex.find (k);
    if (cit == atomIndex.end ())
      return end ();
    else
      return const_iterator (this, cit);
  }

  
  Residue::iterator 
  Residue::safeFind (const AtomType *k) 
  {
    AtomMap::iterator it = atomIndex.find (k);
    if (it == atomIndex.end ())
    {
      NoSuchAtomException ex ("", __FILE__, __LINE__);
      ex << "residue " << *this << " is missing atom " << k;
      throw ex;
    }
    return iterator (this, it);
  }


  Residue::const_iterator 
  Residue::safeFind (const AtomType *k) const
  {
    AtomMap::const_iterator cit = atomIndex.find (k);
    if (cit == atomIndex.end ())
    {
      NoSuchAtomException ex ("", __FILE__, __LINE__);
      ex << "residue " << *this << " is missing atom " << k;
      throw ex;
    }
    return const_iterator (this, cit);
  }

  
  bool 
  Residue::contains (const AtomType *k) const 
  { 
    return find (k) != end (); 
  }
  

  bool
  Residue::validateRiboseBuilding () const
  {
    return this->rib_built_valid;
  }

  
  unsigned int
  Residue::getRiboseBuiltCount () const
  {
    return this->rib_built_count;
  }

  
  // METHODS -------------------------------------------------------------------

  const HomogeneousTransfo
  Residue::getReferential () const
  {
    return this->_compute_referential ();
  }


  void 
  Residue::setReferential (const HomogeneousTransfo& m) 
  {
    // Align the residue to the origin and transform.
    unsigned int i;
    HomogeneousTransfo t (m * this->_compute_referential ().invert ());
    for (i = 0; i < this->atomGlobal.size (); ++i)
      this->atomGlobal[i]->transform (t);
  }

//   const HomogeneousTransfo
//   Residue::getReferential () const
//   {
//     Vector3D *pivot[3];
//     HomogeneousTransfo t;

//     /* Set the pivots */
//     if ((this->_get (AtomType::aN9) != 0 || this->_get (AtomType::aN1) != 0) 
// 	&& this->_get (AtomType::aPSY) != 0
// 	&& this->_get (AtomType::aPSZ) != 0) {
//       pivot[0] = this->_get (AtomType::aN9) != 0 ? this->_get (AtomType::aN9) : this->_get (AtomType::aN1);
//       pivot[1] = this->_get (AtomType::aPSY);
//       pivot[2] = this->_get (AtomType::aPSZ);	   
//     } else if (type->isAminoAcid ()) {
//       pivot[0] = this->_get (AtomType::aCA);
//       pivot[1] = this->_get (AtomType::aN);
//       pivot[2] = this->_get (AtomType::aPSAZ);
//     } else if (type->isPhosphate ()) {
//       pivot[0] = this->_get (AtomType::aP);
//       pivot[1] = this->_get (AtomType::aO3p);
//       pivot[2] = this->_get (AtomType::aO5p);
//     } else if (size() >= 3) {
//       pivot[0] = (Atom*) atomGlobal[0];
//       pivot[1] = (Atom*) atomGlobal[1];
//       pivot[2] = (Atom*) atomGlobal[2];
//     } else {
//       pivot[0] = 0;
//       pivot[1] = 0;
//       pivot[2] = 0;
//       gOut (4) << "Residue " << *getType () << " " << getResId () 
// 	       << " has less than 3 atoms and cannot be moved: " << endl;
//     }
    
//     if (pivot[0] != 0 && pivot[1] != 0 && pivot[2] != 0) {
//       t = HomogeneousTransfo::align (*pivot[0], *pivot[1], *pivot[2]);
//     }
//     return t; 
//   }


//   void 
//   Residue::setReferential (const HomogeneousTransfo& m) 
//   {
//     Vector3D *pivot[3];
//     HomogeneousTransfo curr;

//     if ((this->_get (AtomType::aN9) != 0 || this->_get (AtomType::aN1) != 0) 
// 	&& this->_get (AtomType::aPSY) != 0
// 	&& this->_get (AtomType::aPSZ) != 0) {
//       pivot[0] = this->_get (AtomType::aN9) != 0 ? this->_get (AtomType::aN9) : this->_get (AtomType::aN1);
//       pivot[1] = this->_get (AtomType::aPSY);
//       pivot[2] = this->_get (AtomType::aPSZ);	   
//     } else if (type->isAminoAcid ()) {
//       pivot[0] = this->_get (AtomType::aCA);
//       pivot[1] = this->_get (AtomType::aN);
//       pivot[2] = this->_get (AtomType::aPSAZ);
//     } else if (type->isPhosphate ()) {
//       pivot[0] = this->_get (AtomType::aP);
//       pivot[1] = this->_get (AtomType::aO3p);
//       pivot[2] = this->_get (AtomType::aO5p);      
//     } else if (size() >= 3) {
//       pivot[0] = (Atom*) atomGlobal[0];
//       pivot[1] = (Atom*) atomGlobal[1];
//       pivot[2] = (Atom*) atomGlobal[2];
//     } else {
//       pivot[0] = 0;
//       pivot[1] = 0;
//       pivot[2] = 0;
//       gOut (4) << "Residue " << *getType () << " " << getResId () 
// 	       << " has less than 3 atoms and cannot be moved: " << endl;
//     }
//     curr = HomogeneousTransfo::align (*pivot[0], *pivot[1], *pivot[2]);

//     /* Align the residue to the origin and transform. */
//     unsigned int i;
//     HomogeneousTransfo inv = curr.invert ();
//     inv = m * inv;
//     for (i=0; i<atomGlobal.size (); ++i) {
//       atomGlobal[i]->transform (inv);
//     }
//   }


  void 
  Residue::insert (const Atom &atom)
  {
    int pos = size ();
    pair< AtomMap::iterator, bool > inserted =
      this->atomIndex.insert (make_pair (atom.getType (), pos));

    if (inserted.second)
      {
	this->atomGlobal.push_back (atom.clone ());
	this->rib_dirty_ref = true;
      }
    else
      {
	*this->atomGlobal[inserted.first->second] = atom;
      }

  }


  Residue::iterator 
  Residue::erase (const AtomType *aType)
  {
    AtomMap::iterator i = atomIndex.find (aType);
    
    if (i!=atomIndex.end ()) {
      vector< Atom* >::const_iterator cit;
      const AtomType* next;
      size_type index;

      rib_dirty_ref = true;
      
      delete atomGlobal[i->second];
      atomGlobal.erase (atomGlobal.begin () + i->second);
      next = ++i == atomIndex.end () ? 0 : i->first;
      atomIndex.clear ();
      for (cit = atomGlobal.begin (), index = 0;
	   cit != atomGlobal.end ();
	   ++cit, ++index)
	atomIndex[(*cit)->getType ()] = index;
      return find (next);
    } 
    return end ();
  }


  Residue::iterator 
  Residue::erase (const iterator pos)
  { 
    return erase (pos->getType ()); 
  }


  Residue::iterator 
  Residue::erase (const const_iterator pos)
  { 
    return erase (pos->getType ()); 
  }


  int 
  Residue::size () const 
  { 
    return atomIndex.size (); 
  }


  bool 
  Residue::empty () const 
  { 
    return size () == 0; 
  }


  void 
  Residue::clear () 
  {
    vector< Atom* >::iterator it;
    for (it = atomGlobal.begin (); it != atomGlobal.end (); ++it)
      delete *it;
    atomGlobal.clear ();
    atomIndex.clear();

    rib_C1p = rib_C2p = rib_C3p = rib_C4p = rib_C5p = rib_O2p = rib_O3p = rib_O4p = rib_O5p = rib_P = 0;
    rib_dirty_ref = true;
    rib_built_valid = false;
  }


  void Residue::validate () 
  {
    gOut (6) << "Validating " << resId << " " << *type << endl;

    if (!type) {
      gOut (6) << "Validate called on an empty residue" << endl;
      return;
    }

    if (!type->isNucleicAcid () && !type->isAminoAcid ()) {
      gOut (6) << "Validate called on a unknown residue: " << *type << endl;
      return;
    }

    
    /*
      O3' and O3P mismatches
        - if both O3P and O3' are present -> remove O3P atom
        - if O3P is present but O3' isn't -> rename O3P atom type to O3' 
    */
    Atom* O3_P = this->_get (AtomType::aO3P);
    if (0 != O3_P)
      {
	if (0 == this->_get (AtomType::aO3p))
	  {
	    O3_P->setType (AtomType::aO3p);
	    this->insert (*O3_P);
	    this->erase (AtomType::aO3P);
	    gOut (3) << "Renamed O3P to O3' in residue " << *this << endl;
	  }
	else
	  {
	    this->erase (AtomType::aO3P);
	    gOut (3) << "Removed O3P in residue " << *this << endl;
	  }
      }
    
    
    gOut (6) << "Fixed atom content" << endl;

    set< const AtomType* > actset;
    set< const AtomType* > diffset;    
    set< const AtomType* > oblset = ResidueTopology::getOblSet (type);
    AtomMap::iterator i;
    
    for (i=atomIndex.begin (); i!=atomIndex.end (); ++i) {
      actset.insert (i->first);
    }
    
    set_difference (oblset.begin (), oblset.end (),
 		    actset.begin (), actset.end (),
		    inserter (diffset, diffset.begin ()));
    
    if (!diffset.empty ()) {
      type = type->invalidate ();
    } else {
      // Add here the removal of non obligatory or optional atoms.
    }     

    gOut (6) << "Validated obligatory atoms and type is now " 
	     << (type->isUnknown ()? "invalid": "valid") << endl;
  }
  

  void Residue::removeOptionals () 
  {
    if (!type->isNucleicAcid () && !type->isAminoAcid ()) return;    
    set< const AtomType* > oblset = ResidueTopology::getOblSet (type);
    iterator i;
    const AtomType* t;

    i = begin ();
    while (i != end ()) {
      t = i->getType ();
      if (oblset.find (t) == oblset.end ()) i = erase (i);
      else ++i;
    }
  }


  void Residue::addHydrogens () 
  { 
    Vector3D x, y, z, up, a, b, v;

    if (this->type->isA ()) 
    {
      try // H2
      {
	x = (*this->_safe_get (AtomType::aC2) - *this->_safe_get (AtomType::aN1)).normalize ();
	y = (*this->_safe_get (AtomType::aC2) - *this->_safe_get (AtomType::aN3)).normalize ();
	z = (x + y).normalize ();
	v = *this->_safe_get (AtomType::aC2) + z * Residue::C_H_DIST_CYC;
	this->insert (Atom (v, AtomType::aH2));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogen " << AtomType::aH2 << " in "
		 << *this << ": " << ex << endl;
      }
      try // H8
      {
	x = (*this->_safe_get (AtomType::aC8) - *this->_safe_get (AtomType::aN7)).normalize ();
	y = (*this->_safe_get (AtomType::aC8) - *this->_safe_get (AtomType::aN9)).normalize ();
	z = (x + y).normalize ();
	v = *this->_safe_get (AtomType::aC8) + z * Residue::C_H_DIST_CYC;	
	this->insert (Atom (v, AtomType::aH8));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogen " << AtomType::aH8 << " in "
		 << *this << ": " << ex << endl;
      }
      try // H61 and H62
      {
	x = (*this->_safe_get (AtomType::aC6) - *this->_safe_get (AtomType::aN1)).normalize ();
	y = (*this->_safe_get (AtomType::aC6) - *this->_safe_get (AtomType::aC5)).normalize ();
	z = (*this->_safe_get (AtomType::aN6) - *this->_safe_get (AtomType::aC6)).normalize ();  // axe N6-C6     
	up = x.cross (y).normalize ();

	a = (z + up.cross (z).normalize () * Residue::TAN60).normalize ();
	v = *this->_safe_get (AtomType::aN6) + a * Residue::N_H_DIST;
	this->insert (Atom (v, AtomType::a1H6));
	
	b = (z + z.cross (up).normalize () * Residue::TAN60).normalize ();	    
	v = *this->_safe_get (AtomType::aN6) + b * Residue::N_H_DIST;
	this->insert (Atom (v, AtomType::a2H6));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogens " << AtomType::a1H6 << " and "
		 << AtomType::a2H6 << " in " << *this << ": " << ex << endl;
      }
    }
    else if (this->type->isG ()) 
    {
      try // H1
      {
	x = (*this->_safe_get (AtomType::aN1) - *this->_safe_get (AtomType::aC2)).normalize ();
	y = (*this->_safe_get (AtomType::aN1) - *this->_safe_get (AtomType::aC6)).normalize ();
	z = (x + y).normalize ();
	v = *this->_safe_get (AtomType::aN1) + z * Residue::N_H_DIST;
	this->insert (Atom (v, AtomType::aH1));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogen " << AtomType::aH1 << " in "
		 << *this << ": " << ex << endl;
      }
      try // H8
      {
	x = (*this->_safe_get (AtomType::aC8) - *this->_safe_get (AtomType::aN7)).normalize ();
	y = (*this->_safe_get (AtomType::aC8) - *this->_safe_get (AtomType::aN9)).normalize ();
	z = (x + y).normalize ();
	v = *this->_safe_get (AtomType::aC8) + z * Residue::C_H_DIST_CYC;
	this->insert (Atom (v, AtomType::aH8));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogen " << AtomType::aH8 << " in "
		 << *this << ": " << ex << endl;
      }    
      try // 1H2 and 2H2
      {
	x = (*this->_safe_get (AtomType::aC2) - *this->_safe_get (AtomType::aN1)).normalize ();
	y = (*this->_safe_get (AtomType::aC2) - *this->_safe_get (AtomType::aN3)).normalize ();
	z = (*this->_safe_get (AtomType::aN2) - *this->_safe_get (AtomType::aC2)).normalize ();  // axe N2-C2	    
	up = x.cross (y).normalize ();
		
	b = (z + z.cross (up).normalize () * Residue::TAN60).normalize ();
	v = *this->_safe_get (AtomType::aN2) + b * Residue::N_H_DIST;
	this->insert (Atom (v, AtomType::a1H2));
	
	a = (z + up.cross (z).normalize () * Residue::TAN60).normalize ();
	v = *this->_safe_get (AtomType::aN2) + a * Residue::N_H_DIST;
	this->insert (Atom (v, AtomType::a2H2));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogens " << AtomType::a1H2 << " and "
		 << AtomType::a2H2 << " in " << *this << ": " << ex << endl;
      }
    }
    else if (this->type->isC ())
    {
      try // H5
      {
	x = (*this->_safe_get (AtomType::aC5) - *this->_safe_get (AtomType::aC4)).normalize ();
	y = (*this->_safe_get (AtomType::aC5) - *this->_safe_get (AtomType::aC6)).normalize ();
	z = (x + y).normalize ();
	v = *this->_safe_get (AtomType::aC5) + z * Residue::C_H_DIST; // Exceptionnal distance!
	this->insert (Atom (v, AtomType::aH5));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogen " << AtomType::aH5 << " in "
		 << *this << ": " << ex << endl;
      }          
      try // H6
      {
	x = (*this->_safe_get (AtomType::aC6) - *this->_safe_get (AtomType::aC5)).normalize ();
	y = (*this->_safe_get (AtomType::aC6) - *this->_safe_get (AtomType::aN1)).normalize ();
	z = (x + y).normalize ();
	v = *this->_safe_get (AtomType::aC6) + z * Residue::C_H_DIST_CYC;
	this->insert (Atom (v, AtomType::aH6));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogen " << AtomType::aH6 << " in "
		 << *this << ": " << ex << endl;
      }	    
      try // 1H4 and 2H4
      {
	x = (*this->_safe_get (AtomType::aC4) - *this->_safe_get (AtomType::aN3)).normalize ();
	y = (*this->_safe_get (AtomType::aC4) - *this->_safe_get (AtomType::aC5)).normalize ();
	z = (*this->_safe_get (AtomType::aN4) - *this->_safe_get (AtomType::aC4)).normalize ();	    
	up = x.cross (y).normalize ();

	b = (z + z.cross (up).normalize () * Residue::TAN60).normalize ();	    
	v = *this->_safe_get (AtomType::aN4) + b * Residue::N_H_DIST;
	this->insert (Atom (v, AtomType::a1H4));

	a = (z + up.cross (z).normalize () * Residue::TAN60).normalize ();
	v = *this->_safe_get (AtomType::aN4) + a * Residue::N_H_DIST;
	this->insert (Atom (v, AtomType::a2H4));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogens " << AtomType::a1H4 << " and "
		 << AtomType::a2H4 << " in " << *this << ": " << ex << endl;
      }	
    }
    else if (this->type->isU ()) 
    {
      try // H3
      {
	x = (*this->_safe_get (AtomType::aN3) - *this->_safe_get (AtomType::aC2)).normalize ();
	y = (*this->_safe_get (AtomType::aN3) - *this->_safe_get (AtomType::aC4)).normalize ();
	z = (x + y).normalize ();
	v = *this->_safe_get (AtomType::aN3) + z * Residue::C_H_DIST; // Exceptionnal distance!
	this->insert (Atom (v, AtomType::aH3));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogen " << AtomType::aH3 << " in "
		 << *this << ": " << ex << endl;
      }	 	    
      try // H5
      {
	x = (*this->_safe_get (AtomType::aC5) - *this->_safe_get (AtomType::aC4)).normalize ();
	y = (*this->_safe_get (AtomType::aC5) - *this->_safe_get (AtomType::aC6)).normalize ();
	z = (x + y).normalize ();
	v = *this->_safe_get (AtomType::aC5) + z * Residue::C_H_DIST; // Exceptionnal distance!	    
	this->insert (Atom (v, AtomType::aH5));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogen " << AtomType::aH5 << " in "
		 << *this << ": " << ex << endl;
      }		    
      try // H6
      {
	x = (*this->_safe_get (AtomType::aC6) - *this->_safe_get (AtomType::aC5)).normalize ();
	y = (*this->_safe_get (AtomType::aC6) - *this->_safe_get (AtomType::aN1)).normalize ();
	z = (x + y).normalize ();
	v = *this->_safe_get (AtomType::aC6) + z * Residue::C_H_DIST_CYC; 
	this->insert (Atom (v, AtomType::aH6));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogen " << AtomType::aH6 << " in "
		 << *this << ": " << ex << endl;
      }	
    }
    else if (this->type->isT ())
    {
      try // H3
      {
	x = (*this->_safe_get (AtomType::aN3) - *this->_safe_get (AtomType::aC2)).normalize ();
	y = (*this->_safe_get (AtomType::aN3) - *this->_safe_get (AtomType::aC4)).normalize ();
	z = (x + y).normalize ();
	v = *this->_safe_get (AtomType::aN3) + z * Residue::C_H_DIST; // Exceptionnal distance! 
	this->insert (Atom (v, AtomType::aH3));	    
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogen " << AtomType::aH3 << " in "
		 << *this << ": " << ex << endl;
      }		    
      try // H6
      {
	x = (*this->_safe_get (AtomType::aC6) - *this->_safe_get (AtomType::aC5)).normalize ();
	y = (*this->_safe_get (AtomType::aC6) - *this->_safe_get (AtomType::aN1)).normalize ();
	z = (x + y).normalize ();
	v = *this->_safe_get (AtomType::aC6) + z * Residue::C_H_DIST_CYC;
	this->insert (Atom (v, AtomType::aH6));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogen " << AtomType::aH6 << " in "
		 << *this << ": " << ex << endl;
      }	
      try // 1H5M, 2H5M, 3H5M (arbitrarily placed)
      {
	x = (*this->_safe_get (AtomType::aC5M) - *this->_safe_get (AtomType::aC5)).normalize ();
	y = (*this->_safe_get (AtomType::aC5) - *this->_safe_get (AtomType::aC4)).normalize ();
	up = x.cross (y).normalize ();
	z = x.cross (up);
	
	v = *this->_safe_get (AtomType::aC5M) + (x + z * Residue::TAN70).normalize () * Residue::C_H_DIST;
	this->insert (Atom (v, AtomType::a1H5M));
	
	a = (up - z*Residue::TAN30).normalize ();
	v = *this->_safe_get (AtomType::aC5M) + (x + a * Residue::TAN70).normalize () * Residue::C_H_DIST;
	this->insert (Atom (v, AtomType::a2H5M));
	    
	b = (-up - z*Residue::TAN30).normalize ();
	v = *this->_safe_get (AtomType::aC5M) + (x + b * Residue::TAN70).normalize () * Residue::C_H_DIST;
	this->insert (Atom (v, AtomType::a3H5M));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogens " << AtomType::a1H5M << ", "
		 << AtomType::a2H5M << " and " << AtomType::a3H5M << " in "
		 << *this << ": " << ex << endl;
      }	      
    }

    this->_add_ribose_hydrogens ();
  }


  void
  Residue::_add_ribose_hydrogens ()
  {
    Vector3D x, y, z, up, v;
	  
    const Vector3D *r1;
    const Vector3D *r2;
    const Vector3D *r3;
    const Vector3D *r4;
	    
    if (0 == this->_get (AtomType::aH1p))
    {
      try
      {
	r1 = this->_safe_get (AtomType::aC1p);
	r2 = this->_safe_get (AtomType::aC2p);
	r3 = (type->isPurine ()) ? this->_safe_get (AtomType::aN9) : this->_safe_get (AtomType::aN1);
	r4 = this->_safe_get (AtomType::aO4p);
	  
	x = (*r1 - *r2).normalize ();
	y = (*r1 - *r3).normalize ();
	z = (*r1 - *r4).normalize ();
	
	v = *r1 + (x + y + z).normalize () * Residue::C_H_DIST;
	this->insert (Atom (v, AtomType::aH1p));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogen " << AtomType::aH1p << " in "
		 << *this << ": " << ex << endl;
      }
    }
    if (0 == this->_get (AtomType::aH3p))
    {
      try
      {
	r1 = this->_safe_get (AtomType::aC3p);
	r2 = this->_safe_get (AtomType::aC2p);
	r3 = this->_safe_get (AtomType::aO3p);
	r4 = this->_safe_get (AtomType::aC4p);
		    
	x = (*r1 - *r2).normalize ();
	y = (*r1 - *r3).normalize ();
	z = (*r1 - *r4).normalize ();
		    		    
	v = *r1 + (x + y + z).normalize () * Residue::C_H_DIST;
	this->insert (Atom (v, AtomType::aH3p));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogen " << AtomType::aH3p << " in "
		 << *this << ": " << ex << endl;
      }
    }
    if (0 == this->_get (AtomType::aH4p))
    {
      try
      {
	r1 = this->_safe_get (AtomType::aC4p);
	r2 = this->_safe_get (AtomType::aC3p);
	r3 = this->_safe_get (AtomType::aO4p);
	r4 = this->_safe_get (AtomType::aC5p);
		    
	x = (*r1 - *r2).normalize ();
	y = (*r1 - *r3).normalize ();
	z = (*r1 - *r4).normalize ();
	  
	v = *r1 + (x + y + z).normalize () * Residue::C_H_DIST;
	this->insert (Atom (v, AtomType::aH4p));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogen " << AtomType::aH4p << " in "
		 << *this << ": " << ex << endl;
      }
    }
    if (0 == this->_get (AtomType::a1H5p) || 0 == this->_get (AtomType::a1H5p))
    {
      try
      {
	r1 = this->_safe_get (AtomType::aC5p);
	r2 = this->_safe_get (AtomType::aC4p);
	r3 = this->_safe_get (AtomType::aO5p);
		    
	x = (*r1 - *r2).normalize ();
	y = (*r1 - *r3).normalize ();
	z = (x + y).normalize ();
	up = x.cross (y).normalize ();

	v = *r1 + (up * Residue::TAN54 + z).normalize () * Residue::C_H_DIST;
	this->insert (Atom (v, AtomType::a1H5p));
	
	v = *r1 + (-up * Residue::TAN54 + z).normalize () * Residue::C_H_DIST;
	this->insert (Atom (v, AtomType::a2H5p));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogens " << AtomType::a1H5p << " and "
		 << AtomType::a2H5p << " in " << *this << ": " << ex << endl;
      }	
    }

    // check for O2'
    //   yes -> RNA ribose: H2p and HO2p
    //   no  -> DNA ribose: 1H2p and 2H2p
    
    if (0 == this->_get (AtomType::aO2p))
    {
      if (0 == this->_get (AtomType::a1H2p) || 0 == this->_get (AtomType::a2H2p))
      {
	try
	{
	  r1 = this->_safe_get (AtomType::aC2p);
	  r2 = this->_safe_get (AtomType::aC1p);
	  r3 = this->_safe_get (AtomType::aC3p);
			
	  x = (*r1 - *r2).normalize ();
	  y = (*r1 - *r3).normalize ();
	  z = (x + y).normalize ();
	  up = x.cross (y).normalize ();
			
	  v = *r1 + (up * Residue::TAN54 + z).normalize () * Residue::C_H_DIST;
	  this->insert (Atom (v, AtomType::a1H2p));
	  
	  v = *r1 + (-up * Residue::TAN54 + z).normalize () * Residue::C_H_DIST;
	  this->insert (Atom (v, AtomType::a2H2p));
	}
	catch (NoSuchAtomException& ex)
	{
	  gOut (3) << "Failed to add hydrogens " << AtomType::a1H2p << " and "
		   << AtomType::a2H2p << " in " << *this << ": " << ex << endl;
	}	
      }
    }
    else
    {
      if (0 == this->_get (AtomType::aH2p))
      {
	try
	{
	  r1 = this->_safe_get (AtomType::aC2p);
	  r2 = this->_safe_get (AtomType::aC1p);
	  r3 = this->_safe_get (AtomType::aC3p);
	  r4 = this->_safe_get (AtomType::aO2p);
			
	  x = (*r1 - *r2).normalize ();
	  y = (*r1 - *r3).normalize ();
	  z = (*r1 - *r4).normalize ();
			
	  v = *r1 + (x + y + z).normalize () * Residue::C_H_DIST;
	  this->insert (Atom (v, AtomType::aH2p));
	}
	catch (NoSuchAtomException& ex)
	{
	  gOut (3) << "Failed to add hydrogen " << AtomType::aH2p << " in "
		   << *this << ": " << ex << endl;
	}
      }
      if (0 == this->_get (AtomType::aHO2p))
      {
	try
	{
	  r1 = this->_safe_get (AtomType::aO2p);
	  r2 = this->_safe_get (AtomType::aC2p);
	  r3 = this->_safe_get (AtomType::aC1p);
			
	  x = (*r2 - *r3).normalize ();
	  y = (*r1 - *r2).normalize ();
	  z = x.cross (y).cross (y).normalize ();
	  
	  v = *r1 + (y * Residue::TAN19 - z).normalize () * Residue::O_H_DIST;
	  this->insert (Atom (v, AtomType::aHO2p));
	}
	catch (NoSuchAtomException& ex)
	{
	  gOut (3) << "Failed to add hydrogen " << AtomType::aHO2p << " in "
		   << *this << ": " << ex << endl;
	}
      }
    }
    
  }


  void
  Residue::addHO3p ()
  {
    if (0 == this->_get (AtomType::aHO3p))
    {
      try
      {
	const Vector3D *r1 = this->_safe_get (AtomType::aO3p);
	const Vector3D *r2 = this->_safe_get (AtomType::aC3p);
	const Vector3D *r3 = this->_safe_get (AtomType::aC4p);

	Vector3D x = (*r2 - *r3).normalize ();
	Vector3D y = (*r1 - *r2).normalize ();
	Vector3D z = x.cross (y).cross (y).normalize ();
		    
	Vector3D v = *r1 + (y*Residue::TAN19+z).normalize () * Residue::O_H_DIST;
	this->insert (Atom (v, AtomType::aHO3p));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add hydrogen " << AtomType::aHO3p << " in "
		 << *this << ": " << ex << endl;
      }
    }
  }

  
  void 
  Residue::addLonePairs () 
  {
    Vector3D x, y, z, up, a, b, v;
    
    if (this->type->isA ()) 
    {	
      try // LP1
      {
	x = (*this->_safe_get (AtomType::aN1) - *this->_safe_get (AtomType::aC2)).normalize ();
	y = (*this->_safe_get (AtomType::aN1) - *this->_safe_get (AtomType::aC6)).normalize ();
	z = (x + y).normalize ();
	v = *this->_safe_get (AtomType::aN1) + z * Residue::N_LP_DIST;
	this->insert (Atom (v, AtomType::aLP1));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add lone pair " << AtomType::aLP1 << " in "
		 << *this << ": " << ex << endl;
      }
      try // LP3
      {
	x = (*this->_safe_get (AtomType::aN3) - *this->_safe_get (AtomType::aC2)).normalize ();
	y = (*this->_safe_get (AtomType::aN3) - *this->_safe_get (AtomType::aC4)).normalize ();
	z = (x + y).normalize ();
	v =*this->_safe_get (AtomType::aN3) + z * Residue::N_LP_DIST;
	this->insert (Atom (v, AtomType::aLP3));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add lone pair " << AtomType::aLP3 << " in "
		 << *this << ": " << ex << endl;
      }      
      try // LP7
      {
	x = (*this->_safe_get (AtomType::aN7) - *this->_safe_get (AtomType::aC5)).normalize ();
	y = (*this->_safe_get (AtomType::aN7) - *this->_safe_get (AtomType::aC8)).normalize ();
	z = (x + y).normalize ();
	v = *this->_safe_get (AtomType::aN7) + z * Residue::N_LP_DIST;
	this->insert (Atom (v, AtomType::aLP7));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add lone pair " << AtomType::aLP7 << " in "
		 << *this << ": " << ex << endl;
      }  	
    } 
    else if (this->type->isG ()) 
    {	
      try // LP3
      {
	x = (*this->_safe_get (AtomType::aN3) - *this->_safe_get (AtomType::aC2)).normalize ();
	y = (*this->_safe_get (AtomType::aN3) - *this->_safe_get (AtomType::aC4)).normalize ();
	z = (x + y).normalize ();
	v = *this->_safe_get (AtomType::aN3) + z * Residue::N_LP_DIST;
	this->insert (Atom (v, AtomType::aLP3));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add lone pair " << AtomType::aLP3 << " in "
		 << *this << ": " << ex << endl;
      }	
      try // LP7
      {
	x = (*this->_safe_get (AtomType::aN7) - *this->_safe_get (AtomType::aC5)).normalize ();
	y = (*this->_safe_get (AtomType::aN7) - *this->_safe_get (AtomType::aC8)).normalize ();
	z = (x + y).normalize ();
	v = *this->_safe_get (AtomType::aN7) + z * Residue::N_LP_DIST;
	this->insert (Atom (v, AtomType::aLP7));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add lone pair " << AtomType::aLP7 << " in "
		 << *this << ": " << ex << endl;
      }	
      try // 1LP6 and 2LP6
      {
	x = (*this->_safe_get (AtomType::aC6) - *this->_safe_get (AtomType::aN1)).normalize ();
	y = (*this->_safe_get (AtomType::aC6) - *this->_safe_get (AtomType::aC5)).normalize ();
	z = (*this->_safe_get (AtomType::aO6) - *this->_safe_get (AtomType::aC6)).normalize ();	
	up = x.cross (y).normalize ();
	
	b = (z + z.cross (up).normalize () * Residue::TAN60).normalize ();	
	v = *this->_safe_get (AtomType::aO6) + b * Residue::O_LP_DIST;
	this->insert (Atom (v, AtomType::a1LP6));

	a = (z + up.cross (z).normalize () * Residue::TAN60).normalize ();
	v = *this->_safe_get (AtomType::aO6) + a * Residue::O_LP_DIST;
	this->insert (Atom (v, AtomType::a2LP6));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add lone pairs " << AtomType::a1LP6 << " and "
		 << AtomType::a2LP6 << " in " << *this << ": " << ex << endl;
      }	
    } 
    else if (this->type->isC ()) 
    {
      try // LP3
      {
	x = (*this->_safe_get (AtomType::aN3) - *this->_safe_get (AtomType::aC2)).normalize ();
	y = (*this->_safe_get (AtomType::aN3) - *this->_safe_get (AtomType::aC4)).normalize ();
	z = (x + y).normalize ();
	v = *this->_safe_get (AtomType::aN3) + z * Residue::N_LP_DIST;
	this->insert (Atom (v, AtomType::aLP3));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add lone pair " << AtomType::aLP3 << " in "
		 << *this << ": " << ex << endl;
      }			
      try // 1LP2 and 2LP2
      {
	x = (*this->_safe_get (AtomType::aC2) - *this->_safe_get (AtomType::aN1)).normalize ();
	y = (*this->_safe_get (AtomType::aC2) - *this->_safe_get (AtomType::aN3)).normalize ();
	z = (*this->_safe_get (AtomType::aO2) - *this->_safe_get (AtomType::aC2)).normalize ();
	up = x.cross (y).normalize ();

	a = (z + up.cross (z).normalize () * Residue::TAN60).normalize ();
	v = *this->_safe_get (AtomType::aO2) + a * Residue::O_LP_DIST;
	this->insert (Atom (v, AtomType::a1LP2));

	b = (z + z.cross (up).normalize () * Residue::TAN60).normalize ();	
	v = *this->_safe_get (AtomType::aO2) + b * Residue::O_LP_DIST;
	this->insert (Atom (v, AtomType::a2LP2));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add lone pairs " << AtomType::a1LP2 << " and "
		 << AtomType::a2LP2 << " in " << *this << ": " << ex << endl;
      }	      
    } 
    else if (this->type->isU () || this->type->isT ()) 
    {	    
      try // 1LP2 and 2LP2
      {
	x = (*this->_safe_get (AtomType::aC2) - *this->_safe_get (AtomType::aN1)).normalize ();
	y = (*this->_safe_get (AtomType::aC2) - *this->_safe_get (AtomType::aN3)).normalize ();
	z = (*this->_safe_get (AtomType::aO2) - *this->_safe_get (AtomType::aC2)).normalize ();
	up = x.cross (y).normalize ();

	a = (z + up.cross (z).normalize () * Residue::TAN60).normalize ();
	v = *this->_safe_get (AtomType::aO2) + a * Residue::O_LP_DIST;
	this->insert (Atom (v, AtomType::a1LP2));
	
	b = (z + z.cross (up).normalize () * Residue::TAN60).normalize ();
	v = *this->_safe_get (AtomType::aO2) + b * Residue::O_LP_DIST;
	this->insert (Atom (v, AtomType::a2LP2));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add lone pairs " << AtomType::a1LP2 << " and "
		 << AtomType::a2LP2 << " in " << *this << ": " << ex << endl;
      }	        
      try // 1LP4 and 2LP4
      {
	x = (*this->_safe_get (AtomType::aC4) - *this->_safe_get (AtomType::aN3)).normalize ();
	y = (*this->_safe_get (AtomType::aC4) - *this->_safe_get (AtomType::aC5)).normalize ();
	z = (*this->_safe_get (AtomType::aO4) - *this->_safe_get (AtomType::aC4)).normalize ();	
	up = x.cross (y).normalize ();
	
	b = (z + z.cross (up).normalize () * Residue::TAN60).normalize ();
	v = *this->_safe_get (AtomType::aO4) + b * Residue::O_LP_DIST;
	this->insert (Atom (v, AtomType::a1LP4));
	
	a = (z + up.cross (z).normalize () * Residue::TAN60).normalize ();
	v = *this->_safe_get (AtomType::aO4) + a * Residue::O_LP_DIST;
	this->insert (Atom (v, AtomType::a2LP4));
      }
      catch (NoSuchAtomException& ex)
      {
	gOut (3) << "Failed to add lone pairs " << AtomType::a1LP4 << " and "
		 << AtomType::a2LP4 << " in " << *this << ": " << ex << endl;
      }	       
    }      
  }

  
  void
  Residue::setupHLP ()
  {
    this->removeOptionals ();
    this->addHydrogens ();
    this->addLonePairs ();
  }
  
  
  float
  Residue::getRho () const
  {
    Atom* c1p = this->_safe_get (AtomType::aC1p);
    Atom* c2p = this->_safe_get (AtomType::aC2p);
    Atom* c3p = this->_safe_get (AtomType::aC3p);
    Atom* c4p = this->_safe_get (AtomType::aC4p);
    Atom* o4p = this->_safe_get (AtomType::aO4p);
    
    double nu0 = o4p->torsionAngle (*c4p, *c1p, *c2p);
    double nu1 = c1p->torsionAngle (*o4p, *c2p, *c3p);
    double nu2 = c2p->torsionAngle (*c1p, *c3p, *c4p);
    double nu3 = c3p->torsionAngle (*c2p, *c4p, *o4p);
    double nu4 = c4p->torsionAngle (*c3p, *o4p, *c1p);
    double rho = atan2 (nu4 + nu1 - nu3 - nu0, nu2 * 3.07768354);

    return rho > 0 ? rho : s_2xpi + rho;
  }

  
  const PropertyType* 
  Residue::getPucker () const
  {
    float rho;
    try
      {
	rho = this->getRho ();
      }
    catch (IntLibException& ex)
      {
	gOut (3) << "Failed to compute pseudorotation: " << ex << endl;
	return PropertyType::parseType ("undefined");
      }
    return Residue::getPuckerType (rho);
  }


  float
  Residue::getChi () const
  {
    Atom *c24, *n19, *c1p, *o4p;
    
    if (this->type->isPyrimidine ())
      {
	c24 = this->_safe_get (AtomType::aC2);
	n19 = this->_safe_get (AtomType::aN1);
      }
    else if (this->type->isPurine ())
      {
	c24 = this->_safe_get (AtomType::aC4);
	n19 = this->_safe_get (AtomType::aN9);
      }
    else
      {
	IntLibException ex ("", __FILE__, __LINE__);
	ex << "cannot evaluate glycosyl torsion for " << *this;
	throw ex;
      }

    c1p = this->_safe_get (AtomType::aC1p);
    o4p = this->_safe_get (AtomType::aO4p);
    
    return c1p->torsionAngle (*o4p, *n19, *c24);
  }

  
  const PropertyType* 
  Residue::getGlycosyl () const
  {
    float chi;
    try
      {
	chi = this->getChi ();
      }
    catch (IntLibException& ex)
      {
	gOut (3) << "Failed to compute glycosyl torsion: " << ex << endl;
	return PropertyType::parseType ("undefined");
      }
    return Residue::getGlycosylType (chi);
  }

  
  void 
  Residue::finalize ()
  {
    Vector3D *v1, *v2, *v3;
    Vector3D a, b, y, z;

    try
    {
      if (this->type->isPurine ())
      {
	// fetch needed atoms
	v1 = this->_safe_get (AtomType::aN9);
	v2 = this->_safe_get (AtomType::aC8);
	v3 = this->_safe_get (AtomType::aC4);

	// compute and insert pseudo-atoms
	a = (*v2-*v1).normalize();
	b = (*v3-*v1).normalize();
	y = *v1 + (a + b).normalize();
	z = *v1 + (b.cross(a)).normalize();
	this->insert (Atom (y, AtomType::aPSY));
	this->insert (Atom (z, AtomType::aPSZ));
      }
      else if (this->type->isPyrimidine ())
      {
	// fetch needed atoms
	v1 = this->_safe_get (AtomType::aN1);
	v2 = this->_safe_get (AtomType::aC6);
	v3 = this->_safe_get (AtomType::aC2);

	// compute and insert pseudo-atoms
	a = (*v2-*v1).normalize();
	b = (*v3-*v1).normalize();
	y = *v1 + (a + b).normalize();
	z = *v1 + (b.cross(a)).normalize();
	this->insert (Atom (y, AtomType::aPSY));
	this->insert (Atom (z, AtomType::aPSZ));
      }
      else if (this->type->isPhosphate ())
      {
	// no pseudo-atoms needed
      }
      else if (this->type->isRibose ())
      {
	// no pseudo-atoms needed
      }
      else if (this->type->isAminoAcid ())
      {
	// fetch needed atoms
	v1 = this->_safe_get (AtomType::aCA);
	v2 = this->_safe_get (AtomType::aN);
	v3 = this->_safe_get (AtomType::aC);

	// compute and insert pseudo-atoms
	a = (*v2 - *v1).normalize ();
	b = (*v3 - *v1).normalize ();
	z = *v1 + (b.cross (a)).normalize ();
	this->insert (Atom (z, AtomType::aPSAZ));
      }
      else
      {
	gOut (3) << "Unknown pseudo-atoms for residue type " << *this << endl;
      }
    }
    catch (NoSuchAtomException& ex)
    {
      gOut (3) << "Unknown pseudo-atoms for residue "<< *this << ": " << ex << endl;
    }
    
  }


  float 
  Residue::distance (const Residue &r) const
  {
    if (getType ()->isAminoAcid ()) {
      float deltaPseudoPhi =
	find (AtomType::aN)->torsionAngle (*find (AtomType::aH),
					   *find (AtomType::aCA),
					   *find (AtomType::aC))
	- r.find (AtomType::aN)->torsionAngle (*r.find (AtomType::aH),
					       *r.find (AtomType::aCA),
					       *r.find (AtomType::aC));
      
      float deltaPseudoPsi =
	find (AtomType::aCA)->torsionAngle (*find (AtomType::aN),
					    *find (AtomType::aC),
					    *find (AtomType::aO))
	- r.find (AtomType::aCA)->torsionAngle (*r.find (AtomType::aN),
						*r.find (AtomType::aC),
						*r.find (AtomType::aO));
      
      return (min ((float) abs (deltaPseudoPhi), (float)(2 * M_PI - abs (deltaPseudoPhi)))
	      + min ((float) abs (deltaPseudoPsi), (float)(2 * M_PI - abs (deltaPseudoPsi))));
    } else if (getType ()->isNucleicAcid ()) {
      // nucleic acid
      Residue *tmpRef = clone ();
      Residue *tmpRes = r.clone ();
      AtomSet *as;
      float result;
      
      tmpRes->setReferential (tmpRef->getReferential ());

      // This supposes that the atoms are in the same order in the two 
      // residues, which is the case since we iterate on sorted residues 
      // by definition of a residue iterator
      as = new AtomSetAnd (new AtomSetBackbone (),
			   new AtomSetNot (new AtomSetOr (new AtomSetHydrogen (), 
							  new AtomSetAtom (AtomType::aO2p))));
      result = Rmsd::rmsd (tmpRef->begin (as->clone ()), 
			   tmpRef->end (),
			   tmpRes->begin (as),
			   tmpRes->end ());
      delete tmpRef;
      delete tmpRes;
      return result;
    }
    gOut (2) << "Distance metric is not defined for residues " 
	     << *getType () << " and " << *r.getType () << endl;
    return MAXFLOAT;
  }
  

  void Residue::atomCopy (const Residue& other) 
  {
    const Residue *resp = dynamic_cast< const Residue* > (&other);
    
    if (this != &other && resp) {
      if (type != resp->type) {
	LibException exc ("Invalid residue type ");
	
	exc << *resp->type << ".";
	throw exc;
      }
      
      unsigned int i;
      for (i=0; i<atomGlobal.size (); ++i) {
	*atomGlobal[i] = *resp->atomGlobal[i];
      }
    }
  }

  
  void
  Residue::buildRibose (const PropertyType* pucker, const PropertyType* glycosyl,
			bool build5p, bool build3p)
  {
    float p0 = Residue::getMinRho (pucker), p1 = Residue::getMaxRho (pucker);
    float g0 = Residue::getMinChi (glycosyl), g1 = Residue::getMaxChi (glycosyl);
    this->buildRibose (p0 + (p1 - p0) / 2.0, g0 + (g1 - g0) / 2.0, 1, M_PI, build5p, build3p);
  }

  
  void
  Residue::buildRibose (float rho, float chi, float gamma, float beta,
			bool build5p, bool build3p)
  {
    HomogeneousTransfo tfo;
    Atom a1, a2;
    this->_build_ribose_preprocess (0, 0,
				    build5p, build3p,
				    a1, a2,
				    tfo);
    this->_build_ribose (rho, chi, gamma, beta, build5p, build3p);
    this->_build_ribose_postprocess (tfo, build5p, build3p);
    this->rib_built_valid = true;
  }

  
  float
  Residue::buildRiboseByCCM4D (const Residue* po4_5p,
			       const Residue* po4_3p,
			       const PropertyType* pucker,
			       const PropertyType* glycosyl)
  {
    return this->buildRiboseByCCM4D
      (po4_5p, po4_3p, s_rib_minshift, s_rib_mindrop, s_rib_shiftrate, pucker, glycosyl);
  }

  float
  Residue::buildRiboseByCCM4D (const Residue* po4_5p,
			       const Residue* po4_3p,
			       float minshift,
			       float mindrop,
			       float shiftrate,
			       const PropertyType* pucker,
			       const PropertyType* glycosyl)
  {
    HomogeneousTransfo tfo;
    Atom anchor_O5p, anchor_O3p;
    bool build5p = po4_5p == 0;
    bool build3p = po4_3p == 0;

    if (build5p && build3p)
      {
	IntLibException ex ("", __FILE__, __LINE__);
	ex << "needs at least one phosphate to build ribose for " << *this;
	throw ex;
      }
    
    this->_build_ribose_preprocess (po4_5p, po4_3p,
				    build5p, build3p,
				    anchor_O5p, anchor_O3p,
				    tfo);
    
    // [0] -> rho
    // [1] -> chi
    // [3] -> gamma
    // [4] -> beta
    
    int i;
    float x[4], new_x[4];
    float p_min[4], p_max[4], p_range[4], p_shift[4];
    float eval_x, eval_new_x;
    bool not_shifted;

    if (pucker)
      {
	p_min[0] = Residue::getMinRho (pucker);
	p_max[0] = Residue::getMaxRho (pucker);
      }
    else
      {
	p_min[0] = 0.0;
	p_max[0] = s_2xpi;
      }

    if (glycosyl)
      {
	p_min[1] = Residue::getMinChi (pucker);
	p_max[1] = Residue::getMaxChi (pucker);
      }
    else
      {
	p_min[1] = 0.0;
	p_max[1] = s_2xpi;
      }
    
    p_min[2] = p_min[3] = 0.0;
    p_max[2] = p_max[3] = 360.0;

    for (i = 0; i < 4; ++i)
      {
	p_range[i] = p_max[i] - p_min[i];
	p_shift[i] = 0.25 * p_range[i];
	new_x[i] = x[i] = p_min[i] + 0.5 * p_range[i];
      }

    rib_built_count = 0;
    this->_build_ribose (x[0], x[1], x[2], x[3], build5p, build3p);
    eval_x = this->_evaluate_ribose (anchor_O5p, anchor_O3p, build5p, build3p);

    while (p_shift[0] > minshift ||
	   p_shift[1] > minshift ||
	   p_shift[2] > minshift ||
	   p_shift[3] > minshift)
      {
	not_shifted = true;

	for (i = 0; i < 4; ++i)
	  {
	    new_x[i] = x[i] + p_shift[i];
	    if (new_x[i] > p_max[i])
	      new_x[i] = p_max[i];

	    this->_build_ribose (x[0], x[1], x[2], x[3], build5p, build3p);
	    eval_new_x = this->_evaluate_ribose (anchor_O5p, anchor_O3p, build5p, build3p);
	    
	    if (eval_new_x < eval_x - mindrop)
	      {
		x[i] = new_x[i];
		eval_x = eval_new_x;
		not_shifted = false;
	      }
	    else
	      {
		new_x[i] = x[i] - p_shift[i];
		if (new_x[i] < p_min[i])
		  new_x[i] = p_min[i];

		this->_build_ribose (x[0], x[1], x[2], x[3], build5p, build3p);
		eval_new_x = this->_evaluate_ribose (anchor_O5p, anchor_O3p, build5p, build3p);

		if (eval_new_x < eval_x - mindrop)
		  {
		    x[i] = new_x[i];
		    eval_x = eval_new_x;
		    not_shifted = false;
		  }
		else
		  new_x[i] = x[i];
	      }
	  }
	if (not_shifted)
	  {
	    for (i = 0; i < 4; ++i)
	      p_shift[i] *= shiftrate;
	  }
      }

    this->_build_ribose (x[0], x[1], x[2], x[3], build5p, build3p);
    eval_x = this->_evaluate_ribose (anchor_O5p, anchor_O3p, build5p, build3p);

    this->_build_ribose_postprocess (tfo, build5p, build3p);
    this->rib_built_valid = true;
    
    return sqrt (eval_x / 2.0);
  }

  
  float
  Residue::buildRiboseByCCM2D (const Residue* po4_5p,
			       const Residue* po4_3p,
			       const PropertyType* pucker,
			       const PropertyType* glycosyl)
  {
    return this->buildRiboseByCCM2D
      (po4_5p, po4_3p, s_rib_minshift, s_rib_mindrop, s_rib_shiftrate, pucker, glycosyl);
  }


  
  float
  Residue::buildRiboseByCCM2D (const Residue* po4_5p,
			       const Residue* po4_3p,
			       float minshift,
			       float mindrop,
			       float shiftrate,
			       const PropertyType* pucker,
			       const PropertyType* glycosyl)
  {
    HomogeneousTransfo tfo;
    Atom anchor_O5p, anchor_O3p;
    bool build5p = po4_5p == 0;
    bool build3p = po4_3p == 0;

    if (build5p && build3p)
      {
	IntLibException ex ("", __FILE__, __LINE__);
	ex << "needs at least one phosphate to build ribose for " << *this;
	throw ex;
      }
    
    this->_build_ribose_preprocess (po4_5p, po4_3p,
				    build5p, build3p,
				    anchor_O5p, anchor_O3p,
				    tfo);

    // [0] -> rho
    // [1] -> chi
    
    int i;
    float x[2], new_x[2];
    float p_min[2], p_max[2], p_range[2], p_shift[2];
    float eval_x, eval_new_x;
    float def_gamma = RAD(55.0);
    float def_beta = M_PI;
    bool not_shifted;

    if (pucker)
      {
	p_min[0] = Residue::getMinRho (pucker);
	p_max[0] = Residue::getMaxRho (pucker);
      }
    else
      {
	p_min[0] = 0.0;
	p_max[0] = s_2xpi;
      }

    if (glycosyl)
      {
	p_min[1] = Residue::getMinChi (pucker);
	p_max[1] = Residue::getMaxChi (pucker);
      }
    else
      {
	p_min[1] = 0.0;
	p_max[1] = s_2xpi;
      }
    
    for (i = 0; i < 2; ++i)
      {
	p_range[i] = p_max[i] - p_min[i];
	p_shift[i] = 0.25 * p_range[i];
	new_x[i] = x[i] = p_min[i] + 0.5 * p_range[i];
      }
    
    rib_built_count = 0;
    this->_build_ribose (x[0], x[1], def_gamma, def_beta, build5p, build3p);
    eval_x = this->_evaluate_ribose (anchor_O5p, anchor_O3p, build5p, build3p);

    while (p_shift[0] > minshift || p_shift[1] > minshift)
      {
	not_shifted = true;

	for (i = 0; i < 2; ++i)
	  {
	    new_x[i] = x[i] + p_shift[i];
	    if (new_x[i] > p_max[i])
	      new_x[i] = p_max[i];

	    this->_build_ribose (new_x[0], new_x[1], def_gamma, def_beta, build5p, build3p);
	    eval_new_x = this->_evaluate_ribose (anchor_O5p, anchor_O3p, build5p, build3p);
	    
	    if (eval_new_x < eval_x - mindrop)
	      {
		x[i] = new_x[i];
		eval_x = eval_new_x;
		not_shifted = false;
	      }
	    else
	      {
		new_x[i] = x[i] - p_shift[i];
		if (new_x[i] < p_min[i])
		  new_x[i] = p_min[i];
		
		this->_build_ribose (new_x[0], new_x[1], def_gamma, def_beta, build5p, build3p);
		eval_new_x = this->_evaluate_ribose (anchor_O5p, anchor_O3p, build5p, build3p);		

		if (eval_new_x < eval_x - mindrop)
		  {
		    x[i] = new_x[i];
		    eval_x = eval_new_x;
		    not_shifted = false;
		  }
		else
		  new_x[i] = x[i];
	      }
	  }
	if (not_shifted)
	  {
	    p_shift[0] *= shiftrate;
	    p_shift[1] *= shiftrate;
	  }
      }

    this->_build_ribose (x[0], x[1], def_gamma, def_beta, build5p, build3p);
    eval_x = this->_evaluate_ribose (anchor_O5p, anchor_O3p, build5p, build3p);

    this->_build_ribose_postprocess (tfo, build5p, build3p);
    this->rib_built_valid = true;
    
    return sqrt (eval_x / 2.0);
  }

  
  float
  Residue::buildRiboseByEstimation (const Residue* po4_5p, const Residue* po4_3p)
  {
    HomogeneousTransfo tfo;
    Atom anchor_O5p, anchor_O3p;
    bool build5p = po4_5p == 0;

    float erho1, erho2;
    float x, z, xz_len;
    float anchor_yrot, built_yrot;
    float value1 = 0, value2 = 0, final_value;
    HomogeneousTransfo rotation;
    
    if (po4_3p == 0)
      {
	IntLibException ex ("", __FILE__, __LINE__);
	ex << "3' phosphate is mandatory to estimate ribose for " << *this;
	throw ex;
      }

    /** preprocessing **/
    
    this->_build_ribose_preprocess (po4_5p, po4_3p,
				    build5p, false,
				    anchor_O5p, anchor_O3p,
				    tfo);
    this->rib_built_valid = true;
    this->rib_built_count = 0;
    
    // dummy O3'
    // O3' must be built for estimation but 3' branch is always constrained
    Atom dummy_O3p;
    this->rib_O3p = &dummy_O3p;

    
    /** pseudorotation estimation  **/
    
    // O3' XZ plane projection vector length
    x = anchor_O3p.getX ();
    z = anchor_O3p.getZ ();
    xz_len = sqrt (x*x + z*z);

    // rho estimation with respect to O3'
    erho1 = (xz_len - Residue::s_cosf_vshift) / Residue::s_cosf_amplitude;
    
    // +/- 0.2 tolerance on cos amplitude
    if (erho1 < -1.2 || erho1 > 1.2)
      {
	this->rib_built_valid = false;
	return MAXFLOAT;
      }
    if (erho1 < -1)
      erho1 = -1;
    else if (erho1 > 1)
      erho1 = 1;

    erho1 = acos (erho1) - Residue::s_cosf_phase;

    // get mirror rho
    if  (erho1 > 0)
      erho2 = Residue::s_2xpi - 2 * Residue::s_cosf_phase - erho1;
    else
      {
	erho1 = Residue::s_2xpi + erho1;
	erho2 = Residue::s_4xpi - Residue::s_cosf_phase - erho1;
      }

    /** glycosyl torsion estimation **/

    // Y rotation from X axis to anchored O3'
    anchor_yrot = z < 0 ? acos (x / xz_len) : Residue::s_2xpi - acos (x / xz_len);
    
    // build with first rho. Must build O3'! 
    this->_build_ribose (erho1, 0, 1, M_PI, build5p, true);
  
    // Y rotation from X axis to built O3'
    x = this->rib_O3p->getX ();
    z = this->rib_O3p->getZ ();
    xz_len = sqrt (x*x + z*z);
    built_yrot = z < 0 ? acos (x / xz_len) : Residue::s_2xpi - acos (x / xz_len);

    // compute estimated chi and apply Y rotation
    this->_transform_ribose (rotation.rotate (0.0, anchor_yrot - built_yrot, 0.0), build5p, false);
    value1 = this->_evaluate_ribose (anchor_O5p, anchor_O3p, build5p, false);

    // back up this ribose
    vector< Atom > saved_rib;
    saved_rib.push_back (*this->rib_C1p);
    saved_rib.push_back (*this->rib_C2p);
    saved_rib.push_back (*this->rib_C3p);
    saved_rib.push_back (*this->rib_C4p);
    saved_rib.push_back (*this->rib_C5p);
    if (this->rib_O2p)
      saved_rib.push_back (*this->rib_O2p);
    saved_rib.push_back (*this->rib_O4p);
    if (build5p)
      {
	saved_rib.push_back (*this->rib_O5p);
	saved_rib.push_back (*this->rib_P);
      }
    
    // build with second rho. Must build O3'!
    this->_build_ribose (erho2, 0, 1, M_PI, build5p, true);
    
    // Y rotation from X axis to built O3'
    x = this->rib_O3p->getX ();
    z = this->rib_O3p->getZ ();
    xz_len = sqrt (x*x + z*z);
    built_yrot = z < 0 ? acos (x / xz_len) : Residue::s_2xpi - acos (x / xz_len);
  
    // compute estimated chi and apply Y rotation
    this->_transform_ribose (rotation.rotate (0.0, anchor_yrot - built_yrot, 0.0), build5p, false);
    value2 = this->_evaluate_ribose (anchor_O5p, anchor_O3p, build5p, false);
    
    if (value1 < value2)
      {
	// set as first estimation: retrieve back upped ribose.
	int i = -1;
	*this->rib_C1p = saved_rib[++i];
	*this->rib_C2p = saved_rib[++i];
	*this->rib_C3p = saved_rib[++i];
	*this->rib_C4p = saved_rib[++i];
	*this->rib_C5p = saved_rib[++i];
	if (this->rib_O2p)
	  *this->rib_O2p = saved_rib[++i];
	*this->rib_O4p = saved_rib[++i];
	if (build5p)
	  {
	    *this->rib_O5p = saved_rib[++i];
	    *this->rib_P = saved_rib[++i];
	  }
	final_value = value1;
      }
    
    // set as second estimation: keep current ribose.
    final_value = value2;
    this->_build_ribose_postprocess (tfo, build5p, false);
    this->rib_O3p = 0;
    
    return sqrt (final_value / 2.0);
  }


  Residue*
  Residue::createPhosphate5p (const Residue& reference,
			      const ResidueFactoryMethod* fm)
  {
    Vector3D phos_v, oxy_v, u, v;
    Residue::const_iterator phos_it, oxy_it;
    
    Vector3D rib_phos_v = *reference._safe_get (AtomType::aP);
    Vector3D rib_oxy_v  = *reference._safe_get (AtomType::aO5p);

    ExtendedResidueFM def_fm;
    if (fm == 0) fm = &def_fm;
    Residue* po4 = fm->createResidue ();
    
    po4->setType (ResidueType::rPhosphate);
    po4->setResId (ResId ("p0"));
    po4->setTheoretical ();
    po4->setReferential (HomogeneousTransfo::identity);
      
    phos_it = po4->find (AtomType::aP);
    oxy_it = po4->find (AtomType::aO5p);
    phos_v = *phos_it;
  
    po4->transform (HomogeneousTransfo::translation (rib_phos_v - phos_v));
    
    oxy_v = *oxy_it;

    u = oxy_v - rib_phos_v;
    v = rib_oxy_v - rib_phos_v;

    po4->transform
      (HomogeneousTransfo::translation (rib_phos_v) *
       
       HomogeneousTransfo::rotation
       ((u.cross (v)).normalize (), rib_phos_v.angle (oxy_v, rib_oxy_v)) *
       
       HomogeneousTransfo::translation (rib_phos_v * -1));

    return po4;
  }


  const PropertyType*
  Residue::getPuckerType (float rho)
  {
    // adjust value between [0,360]
    while (rho < 0.0)
      rho += Residue::s_2xpi;
    while (rho > s_2xpi)
      rho -= Residue::s_2xpi;

    if (rho < RAD_36)
      return PropertyType::pC3p_endo;
    else if (rho < RAD_72)
      return PropertyType::pC4p_exo;
    else if (rho < RAD_108)
      return PropertyType::pO4p_endo;
    else if (rho < RAD_144)
      return PropertyType::pC1p_exo;
    else if (rho < RAD_180)
      return PropertyType::pC2p_endo;
    else if (rho < RAD_216)
      return PropertyType::pC3p_exo;
    else if (rho < RAD_252)
      return PropertyType::pC4p_endo;
    else if (rho < RAD_288)
      return PropertyType::pO4p_exo;
    else if (rho < RAD_324)
      return PropertyType::pC1p_endo;
    else 
      return PropertyType::pC2p_exo;   
  }
  

  const PropertyType*
  Residue::getGlycosylType (float chi)
  {
    // adjust value between [-90,270]
    while (chi < -RAD_90)
      chi += Residue::s_2xpi;
    while (chi > RAD_270)
      chi -= Residue::s_2xpi;

    if (chi < RAD_90)
      return PropertyType::pSyn;
    else 
      return PropertyType::pAnti;
  }
  

  float
  Residue::getMinRho (const PropertyType* pucker)
  {
    if (pucker == PropertyType::pC3p_endo)
      return 0.0;
    else if (pucker == PropertyType::pC4p_exo)
      return RAD_36;
    else if (pucker == PropertyType::pO4p_endo)
      return RAD_72;
    else if (pucker == PropertyType::pC1p_exo)
      return RAD_108;
    else if (pucker == PropertyType::pC2p_endo)
      return RAD_144;
    else if (pucker == PropertyType::pC3p_exo)
      return RAD_180;
    else if (pucker == PropertyType::pC4p_endo)
      return RAD_216;
    else if (pucker == PropertyType::pO4p_exo)
      return RAD_252;
    else if (pucker == PropertyType::pC1p_endo)
      return RAD_288;
    else if (pucker == PropertyType::pC2p_exo)
      return RAD_324;
    else
      {
	IntLibException ex ("", __FILE__, __LINE__);
	ex << "unknown pucker type " << pucker;
	throw ex;
      }
  }

  
  float
  Residue::getMaxRho (const PropertyType* pucker)
  {
    if (pucker == PropertyType::pC3p_endo)
      return RAD_36;
    else if (pucker == PropertyType::pC4p_exo)
      return RAD_72;
    else if (pucker == PropertyType::pO4p_endo)
      return RAD_108;
    else if (pucker == PropertyType::pC1p_exo)
      return RAD_144;
    else if (pucker == PropertyType::pC2p_endo)
      return RAD_180;
    else if (pucker == PropertyType::pC3p_exo)
      return RAD_216;
    else if (pucker == PropertyType::pC4p_endo)
      return RAD_252;
    else if (pucker == PropertyType::pO4p_exo)
      return RAD_288;
    else if (pucker == PropertyType::pC1p_endo)
      return RAD_324;
    else if (pucker == PropertyType::pC2p_exo)
      return RAD_360;
    else
      {
	IntLibException ex ("", __FILE__, __LINE__);
	ex << "unknown pucker type " << pucker;
	throw ex;
      }
  }

  
  float
  Residue::getMinChi (const PropertyType* glycosyl)
  {
    if (glycosyl == PropertyType::pSyn)
      return -RAD_90;
    else if (glycosyl == PropertyType::pAnti)
      return RAD_90;
    else
      {
	IntLibException ex ("", __FILE__, __LINE__);
	ex << "unknown glycosyl torsion type " << glycosyl;
	throw ex;
      }
  }

  
  float
  Residue::getMaxChi (const PropertyType* glycosyl)
  {
    if (glycosyl == PropertyType::pSyn)
      return RAD_90;
    else if (glycosyl == PropertyType::pAnti)
      return RAD_270;
    else
      {
	IntLibException ex ("", __FILE__, __LINE__);
	ex << "unknown glycosyl torsion type " << glycosyl;
	throw ex;
      }
  }
    
  
  // PRIVATE METHODS -----------------------------------------------------------
  
  
  Atom& 
  Residue::_get (size_type pos) const 
  {
    return *atomGlobal[pos];
  }


  Atom* 
  Residue::_get (const AtomType* aType) const 
  {
    AtomMap::const_iterator it = atomIndex.find (aType);
    if (it == atomIndex.end ())
      return 0;
    else
      return atomGlobal[it->second];
  }


  Atom* 
  Residue::_safe_get (const AtomType* aType) const 
  {
    AtomMap::const_iterator it = atomIndex.find (aType);
    if (it == atomIndex.end ())
      {
	NoSuchAtomException ex ("", __FILE__, __LINE__);
	ex << "residue " << *this << " is missing atom " << aType;
	throw ex;
      }
    return atomGlobal[it->second];
  }

  
  Atom*
  Residue::_get_or_create (const AtomType *aType)
  {
    size_type pos = size ();
    pair< AtomMap::iterator, bool > inserted =
      atomIndex.insert (make_pair (aType, pos));

    if (inserted.second)
      {
	atomGlobal.push_back (new Atom (0.0, 0.0, 0.0, aType));
	rib_dirty_ref = true;
	return atomGlobal[pos];
      }
    else
      {
	return atomGlobal[inserted.first->second];
      }
  }

  
  void
  Residue::_insert_local (const Vector3D& coord, AtomMap::iterator posit)
  {
    // no local atom container
    atomGlobal[posit->second]->set (coord.getX (), coord.getY (), coord.getZ ());
    atomGlobal[posit->second]->transform (getReferential ());
  }

  
  HomogeneousTransfo
  Residue::_compute_referential () const
  {
    Vector3D *pivot[3];

    try
    {
      // fetch pivot types
      if (this->type->isPurine ())
      {
	pivot[0] = this->_safe_get (AtomType::aN9);
	pivot[1] = this->_safe_get (AtomType::aPSY);
	pivot[2] = this->_safe_get (AtomType::aPSZ);
      }
      else if (this->type->isPyrimidine ())
      {
	pivot[0] = this->_safe_get (AtomType::aN1);
	pivot[1] = this->_safe_get (AtomType::aPSY);
	pivot[2] = this->_safe_get (AtomType::aPSZ);
      }
      else if (this->type->isPhosphate ())
      {
	pivot[0] = this->_safe_get (AtomType::aP);
	pivot[1] = this->_safe_get (AtomType::aO3p);
	pivot[2] = this->_safe_get (AtomType::aO5p);
      }
      else if (this->type->isRibose ())
      {
	pivot[0] = this->_safe_get (AtomType::aC1p);
	pivot[1] = this->_safe_get (AtomType::aC2p);
	pivot[2] = this->_safe_get (AtomType::aO4p);
      }
      else if (this->type->isAminoAcid ())
      {
	pivot[0] = this->_safe_get (AtomType::aCA);
	pivot[1] = this->_safe_get (AtomType::aN);
	pivot[2] = this->_safe_get (AtomType::aPSAZ);
      }
      else if (this->size () >= 3)
      {
	pivot[0] = (Atom*)this->atomGlobal[0];
	pivot[1] = (Atom*)this->atomGlobal[1];
	pivot[2] = (Atom*)this->atomGlobal[2];
	gOut (3) << "default referential with first three atoms for residue type " << *this << endl;	
      }
      else
      {
	gOut (3) << "no referential for residue type " << *this << endl;
	return HomogeneousTransfo::identity;
      }
    }
    catch (NoSuchAtomException& ex)
    {
      gOut (3) << "no referential for residue " << *this << ": " << ex << endl;
      return HomogeneousTransfo::identity;
    }

    return HomogeneousTransfo::align (*pivot[0], *pivot[1], *pivot[2]);
  }
  
  
  void
  Residue::_build_ribose_preprocess (const Residue* po4_5p,
				     const Residue* po4_3p,
				     bool build5p,
				     bool build3p,
				     Atom& o5p,
				     Atom& o3p,
				     HomogeneousTransfo& referential)
  {
    // set reference to ribose's atoms
    
    if (this->rib_dirty_ref)
      {
	// check residue type (DNA doesn't have a O2')
	if (this->type->isRNA ())
	  {
	    this->rib_O2p = _get_or_create (AtomType::aO2p);
	    this->rib_O2p->set (0.0, 0.0, 0.0);
	  }
	else if (this->type->isDNA ())
	  {
	    this->rib_O2p = 0;
	  }
	else
	  {
	    IntLibException ex ("", __FILE__, __LINE__);
	    ex << "cannot build ribose on residue " << *this;
	    throw ex;
	  }

	this->rib_C1p = _get_or_create (AtomType::aC1p);
	this->rib_C2p = _get_or_create (AtomType::aC2p);
	this->rib_C3p = _get_or_create (AtomType::aC3p);
	this->rib_C4p = _get_or_create (AtomType::aC4p);
	this->rib_C5p = _get_or_create (AtomType::aC5p);
	this->rib_O4p = _get_or_create (AtomType::aO4p);

	if (build5p)
	  {
	    this->rib_O5p = _get_or_create (AtomType::aO5p);
	    this->rib_P   = _get_or_create (AtomType::aP);
	  }
	
	if (build3p)
	  this->rib_O3p = _get_or_create (AtomType::aO3p);
	
	this->rib_dirty_ref = false;
      }

    // save referential and align anchor atoms
    
    referential = getReferential ();
    HomogeneousTransfo inv (referential.invert ());
    
    // fetch phosphates' anchor atoms
    
    if (po4_5p)
      {
	o5p = *this->_safe_get (AtomType::aO5p);
	o5p.transform (inv);
      }
    
    if (po4_3p)
      {
	o3p = *this->_safe_get (AtomType::aO3p);
	o3p.transform (inv);
      }

  }


  void
  Residue::_transform_ribose (const HomogeneousTransfo& tfo,
			      bool build5p, bool build3p)
  {
    this->rib_C1p->transform (tfo);
    this->rib_C2p->transform (tfo);
    this->rib_C3p->transform (tfo);
    this->rib_C4p->transform (tfo);
    this->rib_C5p->transform (tfo);
    if (this->rib_O2p)
      this->rib_O2p->transform (tfo);
    this->rib_O4p->transform (tfo);

    if (build5p)
      {
	this->rib_O5p->transform (tfo);
	this->rib_P->transform (tfo);
      }

    if (build3p)
      this->rib_O3p->transform (tfo);
  }

  
  void
  Residue::_build_ribose (float rho, float chi, float gamma, float beta,
			  bool build5p, bool build3p)
  {
    // use explicit method for now...
    //_build_ribose_explicitly (rho, chi, gamma, beta, build5p, build3p);

    ++this->rib_built_count;
    
    // nu0 = 37.68 * cos (rho + 3 * 144)
    // nu0 = 37.68 * cos (rho + 4 * 144)
    float nu0 = 0.6576400621514634 * cos (rho + 7.5398223686155035);
    float nu1 = 0.6576400621514634 * cos (rho + 10.053096491487338);
    
    float cos_chi_0 = cos (chi);
    float cos_chi_1 = cos (2.067167966062084 - chi);  // 118.440 - chi
    float cos_nu0_1 = cos (2.1176952479073194 - nu0); // 121.335 - nu0 
    float cos_nu1_1 = cos (2.1146409217163296 - nu1); // 121.160 - nu1
    float cos_nu1_2 = cos (4.2034160639181035 - nu1); // 240.838 - nu1

    float sin_chi_0 = sin (chi);
    float sin_chi_1 = sin (2.067167966062084 - chi);  // 118.440 - chi
    float sin_nu0_1 = sin (2.1176952479073194 - nu0); // 121.335 - nu0 
    float sin_nu1_1 = sin (2.1146409217163296 - nu1); // 121.160 - nu1
    float sin_nu1_2 = sin (4.2034160639181035 - nu1); // 240.838 - nu1
    
 
    // C1' (only a translation is needed)
    HomogeneousTransfo tfo
      (1.0, 0.0, 0.0, 0.0,
       0.0, 1.0, 0.0, -1.465,
       0.0, 0.0, 1.0, 0.0,
       0.0, 0.0, 0.0, 1.0);

    this->rib_C1p->set (0.0, 0.0, 0.0);
    this->rib_C1p->transform (tfo);

    
    // O4' with respect to chi (from C1')
    tfo.set
      (0.3178011539917053*cos_chi_0,
       -0.9481573848900511*cos_chi_0,
       -sin_chi_0,
       1.3435390143892023*cos_chi_0,
       
       0.9481573848900511,
       0.3178011539917053,
       0.0,
       -1.9153242352062465,
       
       0.3178011539917053*sin_chi_0,
       -0.9481573848900511*sin_chi_0,
       cos_chi_0,
       1.3435390143892023*sin_chi_0,
      
       0.0, 0.0 ,0.0 ,1.0);
    
    this->rib_O4p->set (0.0, 0.0, 0.0);
    this->rib_O4p->transform (tfo);
    
    
    // C2' with respect to chi (from C1')
    tfo.set
      (0.37505965617503256*cos_chi_1,
       -0.9270006765422916*cos_chi_1,
       sin_chi_1,
       1.4173840344331636*cos_chi_1,

       0.9270006765422916,
       0.37505965617503256,
       0.0,
       -2.038466214291625,
      
       -0.37505965617503256*sin_chi_1,
       0.9270006765422916*sin_chi_1,
       cos_chi_1,
       -1.4173840344331636*sin_chi_1,
 
       0.0, 0.0 ,0.0 ,1.0);

    this->rib_C2p->set (0.0, 0.0, 0.0);
    this->rib_C2p->transform (tfo);

    
    // C3' with respect to nu1 (from C2')
    tfo.set
      (cos_chi_1*(-0.9085646439620313 + 0.07442844670182767*cos_nu1_2) - 0.1984442887322796*sin_chi_1*sin_nu1_2,
       cos_chi_1*(-0.18395798991077703 - 0.3676005332037575*cos_nu1_2) + 0.9801121692283693*sin_chi_1*sin_nu1_2,
       cos_nu1_2*sin_chi_1 + 0.37505965617503256*cos_chi_1*sin_nu1_2,
       cos_chi_1*(1.697552053067277 + 0.5598556120693227*cos_nu1_2) - 1.4927108337348063*sin_chi_1*sin_nu1_2,
       
       0.3676005332037575 + 0.18395798991077703*cos_nu1_2,
       0.07442844670182767 - 0.9085646439620313*cos_nu1_2,
       0.9270006765422916*sin_nu1_2,
       -2.1518207386185084 + 1.3837439527541735*cos_nu1_2,
      
       (0.9085646439620313 - 0.07442844670182767*cos_nu1_2)*sin_chi_1 - 0.1984442887322796*cos_chi_1*sin_nu1_2,
       (0.18395798991077703 + 0.3676005332037575*cos_nu1_2)*sin_chi_1 + 0.9801121692283693*cos_chi_1*sin_nu1_2,
       cos_chi_1*cos_nu1_2 - 0.37505965617503256*sin_chi_1*sin_nu1_2,
       (-1.697552053067277 - 0.5598556120693227*cos_nu1_2)*sin_chi_1 - 1.4927108337348063*cos_chi_1*sin_nu1_2,

       0.0, 0.0, 0.0, 1.0);
    
    this->rib_C3p->set (0.0, 0.0, 0.0);
    this->rib_C3p->transform (tfo);

    if (this->rib_O2p)
      {
	// O2' with respect to nu1 (from C2')
	tfo.set
	  (cos_chi_1*(-0.8723723497509978 + 0.12684981914014418*cos_nu1_1) - 0.3382123804884682*sin_chi_1*sin_nu1_1,
	   cos_chi_1*(-0.31352310552778895 - 0.3529573190546505*cos_nu1_1) + 0.9410698091450621*sin_chi_1*sin_nu1_1,
	   cos_nu1_1*sin_chi_1 + 0.37505965617503256*cos_chi_1*sin_nu1_1,
	   cos_chi_1*(1.8607057056494571 + 0.4990816491432758*cos_nu1_1) - 1.3306727101311178*sin_chi_1*sin_nu1_1,
	   
	   0.3529573190546505 + 0.31352310552778895*cos_nu1_1,
	   0.12684981914014418 - 0.8723723497509978*cos_nu1_1,
	   0.9270006765422916*sin_nu1_1,
	   -2.217831858555789 + 1.2335345025479107*cos_nu1_1,
	   
	   (0.8723723497509978 - 0.12684981914014418*cos_nu1_1)*sin_chi_1 - 0.3382123804884682*cos_chi_1*sin_nu1_1,
	   (0.31352310552778895 + 0.3529573190546505*cos_nu1_1)*sin_chi_1 + 0.9410698091450621*cos_chi_1*sin_nu1_1,
	   cos_chi_1*cos_nu1_1 - 0.37505965617503256*sin_chi_1*sin_nu1_1,
	   (-1.8607057056494571 - 0.4990816491432758*cos_nu1_1)*sin_chi_1 - 1.3306727101311178*cos_chi_1*sin_nu1_1,

	   0.0, 0.0, 0.0, 1.0);

	this->rib_O2p->set (0.0, 0.0, 0.0);
	this->rib_O2p->transform (tfo);
      }


    // C4' with respect to nu0 (from O4')
    tfo.set
      (cos_chi_0*(-0.8926622497199935 + 0.10712926213198759*cos_nu0_1) + 0.3370952584230821*sin_chi_0*sin_nu0_1,
       cos_chi_0*(-0.3196193586852655 - 0.2992004255904651*cos_nu0_1) - 0.941470544812038*sin_chi_0*sin_nu0_1,
       -(cos_nu0_1*sin_chi_0) + 0.3178011539917053*cos_chi_0*sin_nu0_1,
       cos_chi_0*(1.8076263232002079 + 0.43443901795735534*cos_nu0_1) + 1.3670152310670791*sin_chi_0*sin_nu0_1,
       
       0.2992004255904651 + 0.3196193586852655*cos_nu0_1,
       0.10712926213198759 - 0.8926622497199935*cos_nu0_1,
       0.9481573848900511*sin_nu0_1,
       -2.0708759238218923 + 1.2961455865934306*cos_nu0_1,
      
       (-0.8926622497199935 + 0.10712926213198759*cos_nu0_1)*sin_chi_0 - 0.3370952584230821*cos_chi_0*sin_nu0_1,
       (-0.3196193586852655 - 0.2992004255904651*cos_nu0_1)*sin_chi_0 + 0.941470544812038*cos_chi_0*sin_nu0_1,
       cos_chi_0*cos_nu0_1 + 0.3178011539917053*sin_chi_0*sin_nu0_1,
       (1.8076263232002079 + 0.43443901795735534*cos_nu0_1)*sin_chi_0 - 1.3670152310670791*cos_chi_0*sin_nu0_1,

       0.0, 0.0, 0.0, 1.0);
    
    this->rib_C4p->set (0.0, 0.0, 0.0);
    this->rib_C4p->transform (tfo);
    
    // C5' => align first for 5' branch
    HomogeneousTransfo branch5p =
      HomogeneousTransfo::align (*rib_C4p, *rib_C3p, *rib_O4p);

    tfo.set
      (0.3716846792351733,
       -0.7832599952069786,
       0.4983515617816504,
       1.1827225927625378,
       
       0.9034400745204099,
       0.4287143941490142,
       0.0,
       -0.6473587351650114,
       
       -0.21365048788243526,
       0.45023077211337686,
       0.8669750405114266,
       -0.6798484658911991,

       0.0, 0.0, 0.0, 1.0);
    
    this->rib_C5p->set (0.0, 0.0, 0.0);
    this->rib_C5p->transform (branch5p * tfo);

    if (build5p)
      {
	// O5' from C5' according to gamma => align first for 5' branch
	float cos_gamma = cos (gamma);
	float sin_gamma = sin (gamma);

	tfo.set
	  (-0.7387794924982604 + 0.12347144128320701*cos_gamma + 0.16554942680320905*sin_gamma,
	   -0.26019431499486884 - 0.35057710131382114*cos_gamma - 0.4700507062172538*sin_gamma,
	   0.4983515617816504*cos_gamma - 0.3716846792351733*sin_gamma,
	   1.5574024063551488 + 0.5048310258919024*cos_gamma + 0.6768730169528454*sin_gamma,
	   
	   0.40436815932672815 + 0.3001174230360551*cos_gamma,
	   0.14241637361367576 - 0.8521346728303227*cos_gamma,
	   -0.9034400745204099*sin_gamma,
	   -0.8524383131687046 + 1.2270739288756647*cos_gamma,
	   
	   0.42466264505328705 - 0.07097342221365434*cos_gamma + 0.28800395547318697*sin_gamma,
	   0.14956398648790234 + 0.2015175037352462*cos_gamma - 0.8177404493490513*sin_gamma,
	   0.8669750405114266*cos_gamma + 0.21365048788243526*sin_gamma,
	   -0.8952206064337784 - 0.29018520537875453*cos_gamma + 1.1775462470626339*sin_gamma,
	   
	   0.0, 0.0, 0.0, 1.0);

	this->rib_O5p->set (0.0, 0.0, 0.0);
	this->rib_O5p->transform (branch5p * tfo);
	
	// P5' from O5' according to beta => align first for 5' branch
	float cos_beta = cos (beta);
	float sin_beta = sin (beta);

	tfo.set
	  (-0.22318427905141008 + cos_gamma*(-0.3007110190328383 + 0.25617779354834425*sin_beta) + cos_beta*(-0.379769854899928 + 0.06347053730720459*cos_gamma + 0.08510074038901261*sin_gamma) - 0.4031906999458186*sin_gamma - 0.1910646385491005*sin_beta*sin_gamma,
	   -0.13375297806012146 + cos_gamma*(-0.18021428078216548 - 0.427466042186876*sin_beta) + cos_beta*(0.6336955071998497 - 0.10590886509866484*cos_gamma - 0.14200167851160403*sin_gamma) - 0.24162972890879952*sin_gamma + 0.3188162553482103*sin_beta*sin_gamma,
	   cos_beta*(0.4983515617816504*cos_gamma - 0.3716846792351733*sin_gamma) + sin_beta*(0.7387794924982604 - 0.12347144128320701*cos_gamma - 0.16554942680320905*sin_gamma),
	   1.7704709004049224 + cos_gamma*(0.791912375177892 + 0.6809534052036935*sin_beta) + cos_beta*(-1.0094769429693606 + 0.1687128221021731*cos_gamma + 0.22620867386898522*sin_gamma) + 1.061789175104563*sin_gamma - 0.507874294769699*sin_beta*sin_gamma,

	   0.1221590705035631 - cos_gamma*0.7309270481720375 + cos_beta*(0.20786559285012113 + 0.15427546562496294*cos_gamma) - 0.4644136843202968*sin_beta*sin_gamma,
	   0.07320918635646471 - cos_gamma*0.4380401247490384 + cos_beta*(-0.3468508376613121 - 0.25742872472978484*cos_gamma) + 0.7749347702003643*sin_beta*sin_gamma,
	   -0.9034400745204099*cos_beta*sin_gamma + sin_beta*(-0.40436815932672815 - 0.3001174230360551*cos_gamma),
	   -0.9690605470345528 + cos_gamma*1.9248718476008828 + cos_beta*(0.5525333843944701 + 0.41008395849454726*cos_gamma) - 1.2344710889291803*sin_beta*sin_gamma,
	   
	   0.12829000701654686 + cos_gamma*(0.17285365665378855 + 0.44566882091364896*sin_beta) + cos_beta*(0.21829798029171227 - 0.036483912357508086*cos_gamma + 0.14804853341393348*sin_gamma) - 0.7014250585864771*sin_gamma + 0.10982710755550963*sin_beta*sin_gamma,
	   0.07688341923879162 + cos_gamma*(0.10359014283752652 - 0.7436565221493238*sin_beta) + cos_beta*(-0.36425863600498654 + 0.06087816357124541*cos_gamma - 0.24703827663377365*sin_gamma) - 0.42035976221392257*sin_gamma - 0.18326084529542347*sin_beta*sin_gamma,
	   sin_beta*(-0.42466264505328705 + 0.07097342221365434*cos_gamma - 0.28800395547318697*sin_gamma) + cos_beta*(0.8669750405114266*cos_gamma + 0.21365048788243526*sin_gamma),
	   -1.0176958932811735 + cos_gamma*(-0.45520430291893427 + 1.1846448397838727*sin_beta) + cos_beta*(0.5802640071559435 - 0.09697891456899393*cos_gamma + 0.3935319746776014*sin_gamma) + 1.8471793482694125*sin_gamma + 0.2919345265556096*sin_beta*sin_gamma,

	   0.0, 0.0, 0.0, 1.0);

	this->rib_P->set (0.0, 0.0, 0.0);
	this->rib_P->transform (branch5p * tfo);
      }

    if (build3p)
      { // O3' => align first for 3' branch
	HomogeneousTransfo branch3p =
	  HomogeneousTransfo::align (*rib_C3p, *rib_C4p, *rib_C2p);

	tfo.set
	  (0.32094659913896034,
	   -0.8016121226249803,
	   0.5043919957352705,
	   1.147106947476347,
	   
	   0.9283561384876282,
	   0.3716919156133742,
	   0.0,
	   -0.5318911312427386,
	   
	   -0.18747842711489557,
	   0.46825540544486394,
	   0.8634747909685556,
	   -0.6700734851916004,

	   0.0, 0.0, 0.0, 1.0);

	this->rib_O3p->set (0.0, 0.0, 0.0);
	this->rib_O3p->transform (branch3p * tfo);
      }
  }

  
  void
  Residue::_build_ribose_explicitly (float rho, float chi, float gamma, float beta,
				     bool build5p, bool build3p)
  {
    ++rib_built_count;

    float nu0 = RAD(37.68) * cos (rho + 3 * RAD(144));
    float nu1 = RAD(37.68) * cos (rho + 4 * RAD(144));

    HomogeneousTransfo c1p, c2p, o4p, c5p, branch5p;

    // procedure from local referential:
    // fixed -Y translation for bond length
    // fixed +Z rotation for bond angle
    // variable Y rotation to respect torsion parameter (if any) 
    
    // C1' (only a translation is needed)
    c1p = HomogeneousTransfo::translation (Vector3D (0.0, -1.465, 0.0));
    rib_C1p->set (0.0, 0.0, 0.0);
    rib_C1p->transform (c1p);
    
    // O4' with respect to chi (from C1')
    o4p =
      c1p * 
      HomogeneousTransfo::rotationY (-chi) *
      HomogeneousTransfo::rotationZ (RAD(71.47)) *
      HomogeneousTransfo::translation (Vector3D (0.0, -1.417, 0.0));
    rib_O4p->set (0.0, 0.0, 0.0);
    rib_O4p->transform (o4p);


    
    // C2' with respect to chi (from C1')
    c2p =
      c1p *
      HomogeneousTransfo::rotationY (RAD(118.44) - chi) *
      HomogeneousTransfo::rotationZ (RAD(67.972)) *
      HomogeneousTransfo::translation (Vector3D (0.0, -1.529, 0.0));
    rib_C2p->set (0.0, 0.0, 0.0);
    rib_C2p->transform (c2p);
      
    // C3' with respect to nu1 (from C2')
    rib_C3p->set (0.0, 0.0, 0.0);
    rib_C3p->transform
      ( c2p *
        HomogeneousTransfo::rotationY (RAD(240.838) - nu1) *
        HomogeneousTransfo::rotationZ (RAD(78.554)) *
        HomogeneousTransfo::translation (Vector3D (0.0, -1.523, 0.0)) );

    if (rib_O2p)
      {
	// O2' with respect to nu1 (from C2')
	rib_O2p->set (0.0, 0.0, 0.0);
	rib_O2p->transform
	  ( c2p *
	    HomogeneousTransfo::rotationY (RAD(121.160) - nu1) *
	    HomogeneousTransfo::rotationZ (RAD(70.232)) *
	    HomogeneousTransfo::translation (Vector3D (0.0, -1.414, 0.0)) );
      }

    // C4' with respect to nu0 (from O4')
    rib_C4p->set (0.0, 0.0, 0.0);
    rib_C4p->transform
      ( o4p *
        HomogeneousTransfo::rotationY (RAD(121.335) - nu0) *
        HomogeneousTransfo::rotationZ (RAD(70.3)) *
        HomogeneousTransfo::translation (Vector3D (0.0, -1.452, 0.0)) );

    // C5' aligned on furanose for 5' branch
    branch5p =
      HomogeneousTransfo::align (*rib_C4p, *rib_C3p, *rib_O4p);
    c5p =
      HomogeneousTransfo::rotationY (RAD(29.891)) *
      HomogeneousTransfo::rotationZ (RAD(64.614)) *
      HomogeneousTransfo::translation (Vector3D (0.0, -1.510, 0.0));
    rib_C5p->set (0.0, 0.0, 0.0);
    rib_C5p->transform (branch5p * c5p);

    if (build5p)
      {
	// O5' from C5' according to gamma (aligned on furanose for 5' branch)
	HomogeneousTransfo o5p =
	  c5p *
	  HomogeneousTransfo::rotationY (-gamma) *
	  HomogeneousTransfo::rotationZ (RAD(70.598)) *
	  HomogeneousTransfo::translation (Vector3D (0.0, -1.440, 0.0));
	rib_O5p->set (0.0, 0.0, 0.0);
	rib_O5p->transform (branch5p * o5p);

	// P from O5' according to beta (aligned on furanose for 5' branch)
	rib_P->set (0.0, 0.0, 0.0);
	rib_P->transform
	  ( branch5p *
	    o5p *
	    HomogeneousTransfo::rotationY (-beta) *
	    HomogeneousTransfo::rotationZ (RAD(59.066)) *
	    HomogeneousTransfo::translation (Vector3D (0.0, -1.593, 0.0)) );
      }

    if (build3p)
      {
	// O3' aligned on furanose for 3' branch
	rib_O3p->set (0.0, 0.0, 0.0);
	rib_O3p->transform
	  (HomogeneousTransfo::align (*rib_C3p, *rib_C4p, *rib_C2p) *
	   HomogeneousTransfo::rotationY (RAD(30.291)) *
	   HomogeneousTransfo::rotationZ (RAD(68.18)) *
	   HomogeneousTransfo::translation (Vector3D (0.0, -1.431, 0.0)));
      }
  }

  
  float
  Residue::_evaluate_ribose (const Atom& o5p, const Atom& o3p,
			     bool build5p, bool build3p) const
  {
    return
      (build5p ? 2.0736 : this->rib_C5p->squareDistance (o5p)) +
      (build3p ? 2.047761 : this->rib_C3p->squareDistance (o3p));
  }
  
  void
  Residue::_build_ribose_postprocess (const HomogeneousTransfo& referential,
				      bool build5p, bool build3p)
  {
    // place built ribose's atoms back in referential
    this->_transform_ribose (referential, build5p, build3p);
    this->_add_ribose_hydrogens ();
  }
  
  // I/O -----------------------------------------------------------------------


  ostream&
  Residue::output (ostream &os) const 
  {
    os << this->resId << this->type;
//     AtomMap::const_iterator cit;
//     for (cit=atomIndex.begin (); cit!=atomIndex.end (); ++cit) {
//       os << endl << *(atomGlobal[cit->second]);
//     }
    return os;
  }
  
  
  iBinstream& 
  Residue::input (iBinstream &ibs)
  {
    clear ();

    int i, s;
    Atom a;

    ibs >> type;
    ibs >> resId;
    ibs >> s;
    
    for (i=0; i<s; ++i) {
      ibs >> a;
      insert (a);
    }

    // Finalize
    finalize ();
    
    return ibs;
  }


  oBinstream& 
  Residue::output (oBinstream &obs) const
  {
    const_iterator cit;

    obs << type;
    obs << resId;
    obs << size ();

    for (cit = begin (); cit != end (); ++cit) {
      obs << *cit;
    }
    return obs;
  }


  ostream&
  operator<< (ostream &os, const Residue &r)
  {
    return r.output (os);
  }

  // ostream&
//   operator<< (ostream &os, const Residue *r)
//   {
//     return r->output (os);
//   }

  Exception&
  operator<< (Exception& ex, const Residue &r)
  {
    ex << '<' << r.getResId () << ':' << r.getType () << '>';
    return ex;
  }
  
  
  iBinstream& operator>> (iBinstream &ibs, Residue &res)
  {
    return res.input (ibs);
  }


  oBinstream& operator<< (oBinstream &obs, const Residue &res)
  {
    return res.output (obs);
  }


  iPdbstream& 
  operator>> (iPdbstream &ips, Residue &res)
  {
    ips.read (res);
    return ips;
  }



  oPdbstream&
  operator<< (oPdbstream &ops, const Residue &res)
  {
    ops.write (res);
    return ops;
  }

  

  // ITERATORS -----------------------------------------------------------------


  Residue::ResidueIterator::ResidueIterator ()
    : res (0),
      filter (new AtomSetAll ())
  {
  }
  


  Residue::ResidueIterator::ResidueIterator (Residue *r,
					     AtomMap::iterator p,
					     AtomSet *f)
    : res (r),
      pos (p),
      filter (f)
  {
    AtomMap::iterator last = res->atomIndex.end ();

    if (filter == 0)
      filter = new AtomSetAll ();
    while (pos != last && ! (*filter) (res->_get (pos->second)))
      ++pos;
  }
  


  Residue::ResidueIterator::ResidueIterator (const ResidueIterator &right)
    : res (right.res),
      pos (right.pos),
      filter (right.filter->clone ())
  {
  }



  Residue::ResidueIterator::~ResidueIterator ()
  {
    delete filter;
  }
  


  Residue::ResidueIterator&
  Residue::ResidueIterator::operator= (const ResidueIterator &right)
  {
    if (this != &right)
      {
	res = right.res;
	pos = right.pos;
	delete filter;
	filter = right.filter->clone ();
      }
    return *this;
  }



  Residue::ResidueIterator&
  Residue::ResidueIterator::operator+= (difference_type k)
  {
    AtomMap::iterator last = res->atomIndex.end ();

    while (k > 0 && pos != last)
      if (++pos != last && (*filter) (res->_get (pos->second)))
	--k;
    return *this;
  }


  
  Residue::iterator&
  Residue::ResidueIterator::operator++ ()
  {
    AtomMap::iterator last = res->atomIndex.end ();

    while (pos != last)
      if (++pos == last || (*filter) (res->_get (pos->second)))
	break;
    return *this;
  }



  Residue::iterator
  Residue::ResidueIterator::operator++ (int ign)
  {
    ResidueIterator ret = *this;
    AtomMap::iterator last = res->atomIndex.end ();

    while (pos != last)
      if (++pos == last || (*filter) (res->_get (pos->second)))
	break;
    return ret;
  }



  Residue::ResidueConstIterator::ResidueConstIterator ()
    : res (0),
      filter (new AtomSetAll ())
  { }



  Residue::ResidueConstIterator::ResidueConstIterator (const Residue *r,
							    AtomMap::const_iterator p,
							    AtomSet *f)
    : res (r),
      pos (p),
      filter (f)
  {
    AtomMap::const_iterator last = res->atomIndex.end ();

    if (filter == 0)
      filter = new AtomSetAll ();
    while (pos != last && ! (*filter) (res->_get (pos->second)))
      ++pos;
  }



  Residue::ResidueConstIterator::ResidueConstIterator (const Residue::const_iterator &right)
    : res (right.res),
      pos (right.pos),
      filter (right.filter->clone ())
  { }



  Residue::ResidueConstIterator::ResidueConstIterator (const Residue::iterator &right)
    : res (((ResidueConstIterator&) right).res),
      pos (((ResidueConstIterator&) right).pos),
      filter (((ResidueConstIterator&) right).filter->clone ())
  { }



  Residue::ResidueConstIterator::~ResidueConstIterator ()
  {
    delete filter;
  }



  Residue::const_iterator&
  Residue::ResidueConstIterator::operator= (const Residue::const_iterator &right)
  {
    if (this != &right)
      {
	res = right.res;
	pos = right.pos;
	delete filter;
	filter = right.filter->clone ();
      }
    return *this;
  }



  Residue::const_iterator&
  Residue::ResidueConstIterator::operator= (const Residue::iterator &right)
  {
    res = ((ResidueConstIterator&) right).res;
    pos = ((ResidueConstIterator&) right).pos;
    delete filter;
    filter = ((ResidueConstIterator&) right).filter->clone ();
    return *this;
  }



  Residue::const_iterator&
  Residue::ResidueConstIterator::operator+= (difference_type k)
  {
    AtomMap::const_iterator last = res->atomIndex.end ();

    while (k > 0 && pos != last)
      if (++pos != last && (*filter) (res->_get (pos->second)))
	--k;
    return *this;
  }



  Residue::const_iterator&
  Residue::ResidueConstIterator::operator++ ()
  {
    AtomMap::const_iterator last = res->atomIndex.end ();

    while (pos != last)
      if (++pos == last || (*filter) (res->_get (pos->second)))
	break;
    return *this;
  }



  Residue::const_iterator
  Residue::ResidueConstIterator::operator++ (int ign)
  {
    ResidueConstIterator ret = *this;
    AtomMap::const_iterator last = res->atomIndex.end ();
  
    while (pos != last)
      if (++pos == last || (*filter) (res->_get (pos->second)))
	break;
    return ret;
  }


}
