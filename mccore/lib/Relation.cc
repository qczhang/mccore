//                              -*- Mode: C++ -*- 
// Relation.cc
// Copyright � 2003-06 Laboratoire de Biologie Informatique et Th�orique
//                     Universit� de Montr�al
// Author           : Patrick Gendron
// Created On       : Fri Apr  4 14:47:53 2003
// $Revision: 1.57 $
// $Id: Relation.cc,v 1.57 2006-05-15 18:10:21 thibaup Exp $
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
#include <cmath>
#include <iterator>
#include <limits>
#include <string>

#include "Relation.h"

#include "Atom.h"
#include "AtomSet.h"
#include "AtomType.h"
#include "Binstream.h"
#include "Exception.h"
#include "ExtendedResidue.h"
#include "MaximumFlowGraph.h"
#include "PairingPattern.h"
#include "PropertyType.h"
#include "ResidueType.h"
#include "Messagestream.h"

namespace mccore
{
  // GLOBAL CONSTANT ---------------------------------------------------------

  /**
   * Pairing annotation cutoffs.
   */
  const float PAIRING_CUTOFF = 0.8;
  const float TWO_BONDS_CUTOFF = 1.5;
  const float THREE_BONDS_CUTOFF = 2.1;
  //const float HBOND_DIST_MAX = 4;
  const float HBOND_DIST_MAX = 1.7;

  /**
   * Other annotation cutoffs, respectively:
   *
   * - O3'-P squared bond length cutoff between adjacent nucleotides (Angstroms square).
   * - nitrogen base center squared distance cutoff for a stacking (Angstroms square).
   * - nitrogen base plane tilt cutoff for a stacking (radian).
   * - nitrogen base plane overlap cutoff for a stacking (radian).
   */
  const float gc_adjacency_distance_cutoff_square = 4.00;  // 2.0 Ang
  const float gc_stack_distance_cutoff_square     = 20.25; // 4.5 Ang
  const float gc_stack_tilt_cutoff                = 0.61;  // 35 deg
  const float gc_stack_overlap_cutoff             = 0.61;  // 35 deg

  // STATIC MEMBER  ---------------------------------------------------------

  vector< pair< Vector3D, const PropertyType* > > Relation::faces_A;
  vector< pair< Vector3D, const PropertyType* > > Relation::faces_C;
  vector< pair< Vector3D, const PropertyType* > > Relation::faces_G;
  vector< pair< Vector3D, const PropertyType* > > Relation::faces_U;
  vector< pair< Vector3D, const PropertyType* > > Relation::faces_T;

  bool Relation::face_init = false;




  ostream&
  HBondFlow::write (ostream &os) const
  {
    return os << hbond << " " << flow;
  }


  iBinstream&
  HBondFlow::read (iBinstream &is, const map< ResId, const Residue* > &resMap)
  {
    hbond.read (is, resMap);
    return is >> flow;
  }


  oBinstream&
  HBondFlow::write (oBinstream &os) const
  {
    hbond.write (os);
    return os << flow;
  }


  Relation::Relation ()
    : ref (0),
      res (0),
      refFace (PropertyType::pNull),
      resFace (PropertyType::pNull),
      type_aspb (0),
      sum_flow (0)
  {
    
  }
  
  
  Relation::Relation (const Residue *rA, const Residue *rB)
    : ref (rA),
      res (rB),
      refFace (PropertyType::pNull),
      resFace (PropertyType::pNull),
      type_aspb (0),
      sum_flow (0)
  {
    reset (rA, rB);  
  }

  
  Relation::Relation (const Relation &other)
    : ref (other.ref),
      res (other.res),
      tfo (other.tfo),
      po4_tfo (other.po4_tfo),
      refFace (other.refFace),
      resFace (other.resFace), 
      labels (other.labels),
      type_aspb (other.type_aspb),
      hbonds (other.hbonds),
      sum_flow (other.sum_flow),
      pairedFaces (other.pairedFaces)
  { }


  Relation& 
  Relation::operator= (const Relation &other)
  {
    if (this != &other)
      {
	ref = other.ref;
	res = other.res;
	tfo = other.tfo;
	po4_tfo = other.po4_tfo;
	refFace = other.refFace;
	resFace = other.resFace;
	type_aspb = other.type_aspb;
	labels = other.labels;
	hbonds = other.hbonds;
	sum_flow = other.sum_flow;
	pairedFaces = other.pairedFaces;
      }
    return *this;
  }

  
  // METHODS --------------------------------------------------------------


  bool
  Relation::is (const PropertyType* t) const
  {
    set< const PropertyType* >::const_iterator it;
    
    for (it = labels.begin (); it != labels.end (); ++it)
      if ((*it)->is (t))
	return true;
    return false;
  }

  bool
  Relation::has (const PropertyType* t) const
  {
    return labels.find (t) != labels.end ();
  }


  void
  Relation::reset (const Residue* org, const Residue* dest)
  {
    ref = org;
    res = dest;
    tfo = ref->getReferential ().invert () * res->getReferential ();
    po4_tfo.setIdentity ();
    refFace = resFace = PropertyType::pNull;
    labels.clear ();
    type_aspb = 0;
    hbonds.clear ();
    sum_flow = 0.0;
  }


  bool
  Relation::isAdjacent () const
  {
    return type_aspb & Relation::adjacent_mask;
  }

  
  bool
  Relation::isStacking () const
  {
    return type_aspb & Relation::stacking_mask;
  }

  
  bool
  Relation::isPairing () const
  {
    return type_aspb & Relation::pairing_mask;
  }


  void
  Relation::reassignResiduePointers (const set< const Residue*, less_deref< Residue> > &resSet) throw (NoSuchElementException)
  {
    set< const Residue* >::iterator resIt;
    vector< HBondFlow >::iterator hbfIt;
    set< const Residue*, less_deref< Residue > > newSet;
    
    if (resSet.end () == (resIt = resSet.find (ref)))
      {
	throw NoSuchElementException ("Residue not found", __FILE__, __LINE__);
      }
    ref = *resIt;
    newSet.insert (ref);
    if (resSet.end () == (resIt = resSet.find (res)))
      {
	throw NoSuchElementException ("Residue not found", __FILE__, __LINE__);
      }
    res = *resIt;
    newSet.insert (res);
    
    for (hbfIt = hbonds.begin (); hbonds.end () != hbfIt; ++hbfIt)
      {
	hbfIt->hbond.reassignResiduePointers (newSet);
      }
  }


  bool
  Relation::annotate (unsigned char aspb) 
  {
    if (0 != (aspb & Relation::adjacent_mask))
      areAdjacent ();

    if (0 != (aspb & Relation::stacking_mask))
      areStacked ();

    if (0 != (aspb & Relation::pairing_mask))
      arePaired ();

    if (0 != (aspb & Relation::backbone_mask))
      areHBonded ();

    return ! empty ();
  }
  
  
  void
  Relation::areAdjacent ()
  {
    Residue::const_iterator up, down;
    const PropertyType * adj_type = PropertyType::pNull;
    
    if (ref->end () != (down = ref->find (AtomType::aO3p)) &&
	res->end () != (up = res->find (AtomType::aP)) &&
	down->squareDistance (*up) <= gc_adjacency_distance_cutoff_square)
      adj_type = PropertyType::pAdjacent5p;
    else if (res->end () != (down = res->find (AtomType::aO3p)) &&
	     ref->end () != (up = ref->find (AtomType::aP)) &&
	     down->squareDistance (*up) <= gc_adjacency_distance_cutoff_square)
      adj_type = PropertyType::pAdjacent3p;
    else if (ref->end () != (down = ref->find (AtomType::aC)) &&
	     res->end () != (up = res->find (AtomType::aN)) &&
	     down->squareDistance (*up) <= gc_adjacency_distance_cutoff_square)
      adj_type = PropertyType::pAdjacent5p;
    else if (res->end () != (down = res->find (AtomType::aC)) &&
	     ref->end () != (up = ref->find (AtomType::aN)) &&
	     down->squareDistance (*up) <= gc_adjacency_distance_cutoff_square)
      adj_type = PropertyType::pAdjacent3p;

    if (adj_type != PropertyType::pNull)
      {
	labels.insert (adj_type);
	type_aspb |= Relation::adjacent_mask;
      }
    
    /*
      Compute relative transfo to place phosphate form ref base.
      Only in adjacent nucleic acids.
    */
    
    po4_tfo.setIdentity ();
    
    if (ref->getType ()->isNucleicAcid () &&
	res->getType ()->isNucleicAcid () &&
	adj_type->isAdjacent ())
      try
	{
	  // fetch phosphate residue depending on adjacency direction
	  ResId rid;
	  Residue pRes (ResidueType::rPhosphate, rid);
	  
	  pRes.setType (ResidueType::rPhosphate);

	  if (adj_type->is (PropertyType::pAdjacent5p))
	    {
	      pRes.insert (*ref->safeFind (AtomType::aO3p));
	      pRes.insert (*res->safeFind (AtomType::aP));
	      pRes.insert (*res->safeFind (AtomType::aO1P));
	      pRes.insert (*res->safeFind (AtomType::aO2P));
	      pRes.insert (*res->safeFind (AtomType::aO5p));
	    }
	  else if (adj_type->is (PropertyType::pAdjacent3p))
	    {
	      pRes.insert (*res->safeFind (AtomType::aO3p));
	      pRes.insert (*ref->safeFind (AtomType::aP));
	      pRes.insert (*ref->safeFind (AtomType::aO1P));
	      pRes.insert (*ref->safeFind (AtomType::aO2P));
	      pRes.insert (*ref->safeFind (AtomType::aO5p));
	    }
	  else
	    {
	      IntLibException ex ("", __FILE__, __LINE__);
	      ex << "adjacent type \"" << adj_type << "\" not handled";
	      throw ex;
	    }

	  pRes.finalize ();
	  po4_tfo = ref->getReferential ().invert () * pRes.getReferential ();
	}
      catch (IntLibException& ex)
	{
	  gOut (3) << "unable to compute phosphate transfo in adjacent relation: " << ex << endl;
	  po4_tfo.setIdentity ();
	}
  }


  void
  Relation::areHBonded ()
  {
    Residue::const_iterator i;
    Residue::const_iterator j;
    Residue::const_iterator k;
    Residue::const_iterator l;
    //     vector< Residue::const_iterator > ref_at;
    //     vector< Residue::const_iterator > refn_at;
    //     vector< Residue::const_iterator > res_at;
    //     vector< Residue::const_iterator > resn_at;
    AtomSetOr as (new AtomSetSideChain (),
		  new AtomSetOr (new AtomSetAtom (AtomType::aO2p),
				 new AtomSetOr (new AtomSetAtom (AtomType::aO2P),
						new AtomSetAtom (AtomType::aO1P))));

    if (ref->getType ()->isNucleicAcid ()
	&& res->getType ()->isNucleicAcid ())
      {
	for (i = ref->begin (as); ref->end () != i; ++i)
	  {
	    if (i->getType ()->isNitrogen () || i->getType ()->isOxygen ())
	      {
		for (j = res->begin (as); res->end () != j; ++j)
		  {
		    if (((i->getType ()->isNitrogen ()
			  && j->getType ()->isBackbone ())
			 || (j->getType ()->isNitrogen ()
			     && i->getType ()->isBackbone ()))
			&& i->distance (*j) > HBOND_DIST_MAX
			&& i->distance (*j) < 3.2)
		      {
			const PropertyType *refface;
			const PropertyType *resface;
			const AtomType *refType;
			const AtomType *resType;
		    
			labels.insert (PropertyType::pPairing);
			type_aspb |= Relation::pairing_mask;
			refType = i->getType ();
			resType = j->getType ();
			refface = (refType->isNitrogen ()
				   ? getFace (ref, *ref->safeFind (refType))
				   : (AtomType::aO2p == refType
				      ? PropertyType::pRibose
				      : PropertyType::pPhosphate));
			resface = (resType->isNitrogen ()
				   ? getFace (res, *res->safeFind (resType))
				   : (AtomType::aO2p == resType
				      ? PropertyType::pRibose
				      : PropertyType::pPhosphate));
			pairedFaces.push_back (make_pair (refface, resface));
		      }
		  }
	      }
	  }
      }

    // TODO: This is experimental and is a tentative to identify as
    // precisely as possible the presence of H-bonds on unindentified
    // residues, between amino-nucleic acids, or when hydrogens are
    // missing and their position is unknown (modified bases).
    
    //     if (ref->getType ()->isNucleicAcid () && res->getType ()->isNucleicAcid ())
    //       {
    // 	AtomSet* hl = new AtomSetOr (new AtomSetHydrogen (), new AtomSetLP ());
    // 	AtomSet* da = new AtomSetNot (hl->clone ());

    // 	for (i=ref->begin (hl->clone ()); i!=ref->end (); ++i) {
    // 	  for (j=ref->begin (da->clone ()); j!=ref->end (); ++j) {
    // 	    if (i->distance (*j) < HBOND_DIST_MAX) {
    // 	      ref_at.push_back (i);
    // 	      refn_at.push_back (j);	  
    // 	    }
    // 	  }
    // 	}
    // 	for (i=res->begin (hl->clone ()); i!=res->end (); ++i) {
    // 	  for (j=res->begin (da->clone ()); j!=res->end (); ++j) {
    // 	    if (i->distance (*j) < HBOND_DIST_MAX) {
    // 	      res_at.push_back (i);
    // 	      resn_at.push_back (j);
    // 	    }
    // 	  }
    // 	}    
	
    // 	delete hl; 
    // 	delete da;

	
    // 	for (x=0; x<(int)ref_at.size (); ++x) {
    // 	  i = ref_at[x];
    // 	  j = refn_at[x];
    // 	  for (y=0; y<(int)res_at.size (); ++y) {
    // 	    k = res_at[y];
    // 	    l = resn_at[y];
	    
    // 	    if (i->getType ()->isHydrogen () && k->getType ()->isLonePair ()) {
    // 	      HBond h (j->getType (), i->getType (), l->getType (), k->getType ());
    // 	      h.eval (*ref, *res);
    // 	      if (h.getValue () > 0.01) {
    // // 		cout << h << endl;
    // 	      }
    // 	    } else if (k->getType ()->isHydrogen () && i->getType ()->isLonePair ()) {
    // 	      HBond h (l->getType (), k->getType (), j->getType (), i->getType ());
    // 	      h.eval (*res, *ref);
    // 	      if (h.getValue () > 0.01) {
    // // 		cout << h << endl;
    // 	      }	
    // 	    } 
    // 	  }
    // 	}
	      
    //       }

    //     if (ref->getType ()->isAminoAcid () && res->getType ()->isAminoAcid ())
    //       {	
    // 	for (i=ref->begin (); i!=ref->end (); ++i) {
    // 	  if (i->getType ()->isNitrogen () 
    // 	      || i->getType () == AtomType::aOG || i->getType () == AtomType::aOH) {
    // 	    for (j=res->begin (); j!=res->end (); ++j) {
    // 	      if (j->getType ()->isOxygen ()) {
		
    // 		if (i->distance (*j) > HBOND_DIST_MAX && i->distance (*j) < 3.5) {
    // 		  bool reject = false;
    // 		  if (i->getType () == AtomType::aN) {  // N donor
    // 		    Vector3D u, v, w;
    // 		    u = (*res->find (AtomType::aCA) - *i).normalize ();
    // 		    w = (*j - *i).normalize ();
    // 		    float theta = v.angle (u, w) * 180 / M_PI;
    // 		    float theta_ideal = 115;
    // 		    float theta_diff = fabs (theta - theta_ideal);
    // 		    if (theta_diff > 40) reject = true;
    // 		  }

    // 		  if (j->getType () == AtomType::aO) { // O acceptor
    // 		    Vector3D u, v, w;
    // 		    u = (*ref->find (AtomType::aC) - *j).normalize ();
    // 		    w = (*i - *j).normalize ();
    // 		    float theta = v.angle (u, w) * 180 / M_PI;
    // 		    float theta_ideal = 106;
    // 		    float theta_diff = fabs (theta - theta_ideal);
    // 		    if (theta_diff > 40) reject = true;
    // 		  }		  
		  
    // 		  if (!reject) {
    // // 		    cout << *ref << " " << *res << " : " <<  *i << " -> " << *j << " \t " << i->distance (*j) << endl;
    // 		    labels.insert (PropertyType::parseType ("amino-pair"));
    // 		  }
    // 		}
    // 	      }
    // 	    }
    // 	  }
    // 	}
	
    // 	for (i=res->begin (); i!=res->end (); ++i) {
    // 	  if (i->getType ()->isNitrogen () 
    // 	      || i->getType () == AtomType::aOG || i->getType () == AtomType::aOH) {
    // 	    for (j=ref->begin (); j!=ref->end (); ++j) {
    // 	      if (j->getType ()->isOxygen ()) {
		
    // 		if (i->distance (*j) > HBOND_DIST_MAX && i->distance (*j) < 3.5) {
    // 		   bool reject = false;

    // 		  if (i->getType () == AtomType::aN) {  // N donor
    // 		    Vector3D u, v, w;
    // 		    u = (*res->find (AtomType::aCA) - *i).normalize ();
    // 		    w = (*j - *i).normalize ();
    // 		    float theta = v.angle (u, w) * 180 / M_PI;
    // 		    float theta_ideal = 115;
    // 		    float theta_diff = fabs (theta - theta_ideal);
    // // 		    cout << theta << endl;
    // // 		    cout << theta_diff << endl;
    // 		    if (theta_diff > 40) reject = true;
    // 		  }


    // 		  if (j->getType () == AtomType::aO) { // O acceptor
    // 		    Vector3D u, v, w;
    // 		    u = (*ref->find (AtomType::aC) - *j).normalize ();
    // 		    w = (*i - *j).normalize ();
    // 		    float theta = v.angle (u, w) * 180 / M_PI;
    // 		    float theta_ideal = 106;
    // 		    float theta_diff = fabs (theta - theta_ideal);
    // // 		    cout << theta << endl;
    // // 		    cout << theta_diff << endl;
    // 		    if (theta_diff > 40) reject = true;
    // 		  }

    // 		  if (!reject) {
    // //  		    cout << *ref << " " << *res << " : " <<  *j << " <- " << *i << " \t " << i->distance (*j) << endl;		    
    // 		    ts.insert (PropertyType::parseType ("amino-pair"));
    // 		  }
    // 		}
    // 	      }
    // 	    }
    // 	  }
    // 	}
    //       }
  }
  

  void
  Relation::arePaired ()
  {
    typedef MaximumFlowGraph< unsigned int, HBond > HBondFlowGraph;
    typedef map< Residue::const_iterator, unsigned int > AtomToInt;

    try
      {
	Residue::const_iterator i;
	Residue::const_iterator j;
	Residue::const_iterator k;
	Residue::const_iterator l;
	AtomToInt atomToInt;
	unsigned int node;
	HBondFlowGraph graph;
	vector< Residue::const_iterator > ref_at;
	vector< Residue::const_iterator > refn_at;
	vector< Residue::const_iterator > res_at;
	vector< Residue::const_iterator > resn_at;
	vector< Residue::const_iterator >::size_type x;
	vector< Residue::const_iterator >::size_type y;
	AtomSetAnd da (new AtomSetSideChain (),
		       new AtomSetNot (new AtomSetOr (new AtomSetAtom (AtomType::a2H5M),
						      new AtomSetAtom (AtomType::a3H5M))));

	node = 0;
	graph.insert (node++, 1); // Source
	graph.insert (node++, 1); // Sink

	for (i = ref->begin (da); i != ref->end (); ++i)
	  {
	    if ((i->getType ()->isCarbon ()
		 || i->getType ()->isNitrogen ()
		 || i->getType ()->isOxygen ()))
	      {
		for (j = ref->begin (da); j != ref->end (); ++j)
		  {
		    if ((j->getType ()->isHydrogen ()
			 || j->getType ()->isLonePair ())
			&& i->distance (*j) < HBOND_DIST_MAX)
		      {
			ref_at.push_back (j);
			refn_at.push_back (i);	  
		      }
		  }
	      }
	  }
      
	for (i = res->begin (da); i != res->end (); ++i)
	  {
	    if ((i->getType ()->isCarbon ()
		 || i->getType ()->isNitrogen ()
		 || i->getType ()->isOxygen ()))
	      {
		for (j = res->begin (da); j != res->end (); ++j)
		  {
		    if ((j->getType ()->isHydrogen ()
			 || j->getType ()->isLonePair ())
			&& i->distance (*j) < HBOND_DIST_MAX)
		      {
			res_at.push_back (j);
			resn_at.push_back (i);	  
		      }
		  }
	      }
	  }
	
	for (x = 0; x < ref_at.size (); ++x)
	  {
	    i = ref_at[x];
	    j = refn_at[x];
	    for (y = 0; y < res_at.size (); ++y)
	      {
		k = res_at[y];
		l = resn_at[y];
	      
		if (i->getType ()->isHydrogen () && k->getType ()->isLonePair ())
		  {
		    HBond h (j->getType (), i->getType (), l->getType (), k->getType ());
		  
		    h.evalStatistically (ref, res);
#ifdef DEBUG
		    gOut (4) << h << endl;
#endif
		    if (h.getValue () > 0.01)
		      {
			HBond fake (1);
			pair< AtomToInt::iterator, bool > iIt = atomToInt.insert (make_pair (i, node));

			if (iIt.second)
			  {
			    graph.insert (node, 1);
			    graph.internalConnect (0, node, fake, 0);
			    ++node;
			  }
			pair< AtomToInt::iterator, bool > kIt = atomToInt.insert (make_pair (k, node));
			if (kIt.second)
			  {
			    graph.insert (node, 1);
			    graph.internalConnect (node, 1, fake, 0);
			    ++node;
			  }
			graph.internalConnect (iIt.first->second, kIt.first->second, h, 0);
		      }
		  }
		else if (k->getType ()->isHydrogen () && i->getType ()->isLonePair ())
		  {
		    HBond h (l->getType (), k->getType (), j->getType (), i->getType ());
		  
		    h.evalStatistically (res, ref);
#ifdef DEBUG
		    gOut (4) << h << endl;
#endif
		    if (h.getValue () > 0.01)
		      {
			HBond fake (1);
			pair< AtomToInt::iterator, bool > kIt = atomToInt.insert (make_pair (k, node));
			
			if (kIt.second)
			  {
			    graph.insert (node, 1);
			    graph.internalConnect (0, node, fake, 0);
			    ++node;
			  }
			pair< AtomToInt::iterator, bool > iIt = atomToInt.insert (make_pair (i, node));
			if (iIt.second)
			  {
			    graph.insert (node, 1);
			    graph.internalConnect (node, 1, fake, 0);
			    ++node;
			  }
			graph.internalConnect (kIt.first->second, iIt.first->second, h, 0);
		      }
		  }
	      }
	  }

#ifdef DEBUG
	gOut (4) << graph << endl;
#endif
	
	if (graph.size () >= 3)
	  {
	    HBondFlowGraph::size_type label;

	    graph.preFlowPush (0, 1);
	    
#ifdef DEBUG
	    gOut (4) << graph << endl;
#endif

	    for (label = 0; label < graph.edgeSize (); ++label)
	      {
		HBond &hbond = graph.internalGetEdge (label);

		if (0 != hbond.getDonorType ())
		  {
		    float flow;

		    flow = graph.internalGetEdgeWeight (label);
		    sum_flow += flow;
		    hbonds.push_back (HBondFlow (hbond, flow));
		  }
	      }

	    gOut (4) << "Pairing annotation sum flow = " << sum_flow << endl;
	    
	    if (sum_flow >= PAIRING_CUTOFF)
	      {
		addPairingLabels ();
	      }

	  }
	else
	  {
#ifdef DEBUG
	    gOut (4) << "MaximumFlowGraph.size () = " << graph.size ()
		     << endl << hbonds << endl;
#endif
	    hbonds.clear ();
	  }
      }
    catch (IntLibException& ex)
      {
	gOut (3) << "An error occured during pairing annotation: " << ex << endl;
      }
  }


  void
  Relation::addPairingLabels ()
  {
    Vector3D pa;
    Vector3D pb;
    Vector3D pc;
    Vector3D pd;
    list< HBondFlow > hbf (hbonds.begin (), hbonds.end ());
    vector< HBondFlow >::iterator hbIt;
    float rad;
    unsigned int size_hint;
    const PropertyType *pp;
    const PropertyType *bpo;

    type_aspb |= Relation::pairing_mask;
    labels.insert (PropertyType::pPairing);
    if (sum_flow < TWO_BONDS_CUTOFF)
      {
	labels.insert (PropertyType::pOneHbond);
      }
		
    // -- parallel/antiparallel orientation
    bpo = (Relation::_pyrimidine_ring_normal (*ref,
					      Relation::_pyrimidine_ring_center (*ref)).dot (Relation::_pyrimidine_ring_normal (*res,
																Relation::_pyrimidine_ring_center (*res))) > 0
	   ? PropertyType::pParallel
	   : PropertyType::pAntiparallel);
    labels.insert (bpo);

    // Compute contact points and visual contact points
    for (hbIt = hbonds.begin (); hbonds.end () != hbIt; ++hbIt)
      {
	HBondFlow &fl = *hbIt;
		    
	if (fl.hbond.getDonorResidue () == ref)
	  {
	    pa = pa + (fl.hbond.getHydrogen () * fl.flow);
	    pb = pb + (fl.hbond.getLonePair () * fl.flow);
	  }
	else
	  {
	    pa = pa + (fl.hbond.getLonePair () * fl.flow);
	    pb = pb + (fl.hbond.getHydrogen () * fl.flow);
	  }
      }
			
    pa = pa / sum_flow;
    pb = pb / sum_flow;

    // Compute pairing according to LW+ and Saenger/Gautheret nomenclatures.
    refFace = getFace (ref, pa);
    resFace = getFace (res, pb);
    if (PropertyType::pNull != refFace && PropertyType::pNull != resFace)
      {
	pairedFaces.push_back (make_pair (refFace, resFace));
	if (sum_flow >= PAIRING_CUTOFF && sum_flow < TWO_BONDS_CUTOFF)
	  {
	    size_hint = 1;
	  }
	else if (sum_flow < THREE_BONDS_CUTOFF)
	  {
	    size_hint = 2;
	  }
	else
	  {
	    size_hint = 3;
	  }
	hbf.sort ();
	while (hbf.size () != size_hint)
	  {
	    hbf.pop_front ();
	  }
	if (0 != (pp = translatePairing (ref, res, bpo, hbf, sum_flow, size_hint)))
	  {
	    labels.insert (pp);
	  }
      }

    // -- cis/trans orientation
    Vector3D refpyr = Relation::_pyrimidine_ring_center (*ref);
    Vector3D respyr = Relation::_pyrimidine_ring_center (*res);
    
    pc = *ref->safeFind (AtomType::aC1p); 
    pc = pc - *ref->safeFind (AtomType::aPSY);
    pc = pc + refpyr;
    pd = *res->safeFind (AtomType::aC1p); 
    pd = pd - *res->safeFind (AtomType::aPSY);
    pd = pd + respyr;
    rad = fabs (refpyr.torsionAngle (pc, respyr, pd));
    labels.insert (rad < M_PI / 2 ? PropertyType::pCis : PropertyType::pTrans);
  }
  
  
  Vector3D
  Relation::_pyrimidine_ring_center (const Residue& res)
  {
    return (*(res.safeFind (AtomType::aN1)) + *(res.safeFind (AtomType::aC2)) +
	    *(res.safeFind (AtomType::aN3)) + *(res.safeFind (AtomType::aC4)) +
	    *(res.safeFind (AtomType::aC5)) + *(res.safeFind (AtomType::aC6))) / 6.0;
  }


  Vector3D
  Relation::_imidazole_ring_center (const Residue& res)
  {
    return (*(res.safeFind (AtomType::aC4)) + *(res.safeFind (AtomType::aC5)) +
	    *(res.safeFind (AtomType::aN7)) + *(res.safeFind (AtomType::aC8)) +
	    *(res.safeFind (AtomType::aN9))) / 5.0;
  }

  
  Vector3D
  Relation::_pyrimidine_ring_normal (const Residue& res, const Vector3D& center)
  {
    Vector3D r1 = (((*(res.safeFind (AtomType::aN1)) - center) * 1) +
		   ((*(res.safeFind (AtomType::aC2)) - center) * 0.5) +
		   ((*(res.safeFind (AtomType::aN3)) - center) * -0.5) +
		   ((*(res.safeFind (AtomType::aC4)) - center) * -1) +
		   ((*(res.safeFind (AtomType::aC5)) - center) * -0.5) +
		   ((*(res.safeFind (AtomType::aC6)) - center) * 0.5));
    
    Vector3D r2 = (((*(res.safeFind (AtomType::aC2)) - center) * 0.8660254) +
		   ((*(res.safeFind (AtomType::aN3)) - center) * 0.8660254) +
		   ((*(res.safeFind (AtomType::aC5)) - center) * -0.8660254) +
		   ((*(res.safeFind (AtomType::aC6)) - center) * -0.8660254));

    if (res.getType ()->isPurine ())
      return -(r1.cross (r2).normalize ());

    return r1.cross (r2).normalize ();
  }

  
  Vector3D
  Relation::_imidazole_ring_normal (const Residue& res, const Vector3D& center)
  {
    Vector3D r1 = (((*(res.safeFind (AtomType::aC4)) - center) * 1) +
		   ((*(res.safeFind (AtomType::aC5)) - center) * 0.30901699) +
		   ((*(res.safeFind (AtomType::aN7)) - center) * -0.80901699) +
		   ((*(res.safeFind (AtomType::aC8)) - center) * -0.80901699) +
		   ((*(res.safeFind (AtomType::aN9)) - center) * 0.30901699));

    Vector3D r2 = (((*(res.safeFind (AtomType::aC5)) - center) * 0.95105652) +
		   ((*(res.safeFind (AtomType::aN7)) - center) * 0.58778525) +
		   ((*(res.safeFind (AtomType::aC8)) - center) * -0.58778525) +
		   ((*(res.safeFind (AtomType::aN9)) - center) * -0.95105652));

    return r1.cross (r2).normalize ();
  }
  
  
  const PropertyType*
  Relation::_ring_stacking (const Vector3D& centerA, const Vector3D& normalA,
			    const Vector3D& centerB, const Vector3D& normalB)
  {
    float theta1, theta2;

    /*
      msb - b1b0 - lsb
      b0: up (0) / down (1)
      b1: straight (0) / reverse (1)
    */
    unsigned int annotation = 0; 
    
    // - check ring center distance
    
    if (centerA.squareDistance (centerB) > gc_stack_distance_cutoff_square)
      return PropertyType::pNull;

    // - check ring normal angle (symetrical)    

    theta1 = acos (normalA.dot (normalB));

    if (theta1 > gc_stack_tilt_cutoff)
      {
	if ((float)M_PI - theta1 < gc_stack_tilt_cutoff)
	  annotation = 2;
	else
	  return PropertyType::pNull;
      }

    // - check ring overlap
    //   -> not symetrical, thus we say stack if any direction is satisfying.
    
    Vector3D vAB = (centerB - centerA).normalize ();
    theta1 = acos (normalA.dot (vAB));

    if (theta1 > gc_stack_overlap_cutoff)
      {
	if ((float)M_PI - theta1 < gc_stack_overlap_cutoff)
	  annotation |= 1;
	else
	  {
	    // try from other base point of view
	    theta2 = acos (normalB.dot (vAB));

	    if (theta2 < gc_stack_overlap_cutoff ||
		(float)M_PI - theta2 < gc_stack_overlap_cutoff)
	      {
		if (theta1 > M_PI_2)
		  annotation |= 1;
	      }
	    else
	      return PropertyType::pNull;
	  }
      }

    type_aspb |= Relation::stacking_mask;
    
    switch (annotation)
      {
      case 0:
	return PropertyType::pUpward;
      case 1:
	return PropertyType::pDownward;
      case 2:
	return PropertyType::pInward;
      case 3:
	return PropertyType::pOutward;
      default:
	FatalIntLibException ex ("", __FILE__, __LINE__);
	ex << "invalid stacking annotation value " << annotation;
	throw ex;
      }

    return PropertyType::pNull;
  }
  
  
  void
  Relation::areStacked ()
  {
    if (ref->getType ()->isNucleicAcid () && res->getType ()->isNucleicAcid ())
      {
	try
	  {
	    Vector3D pyrCA, pyrCB, imidCA, imidCB, pyrNA, pyrNB, imidNA, imidNB;
	    const PropertyType* stacking;
	    unsigned char rtypes;

	    /*
	      Compute necessary geometry based on type family:

	      00 (0) => Pur / Pur: imid / imid, imid / -pyr, -pyr / imid, -pyr / -pyr
	      01 (1) => Pur / Pyr:              imid /  pyr,              -pyr /  pyr
	      10 (2) => Pyr / Pur:                            pyr / imid,  pyr / -pyr
	      11 (3) => Pyr / Pyr:                                         pyr /  pyr
	    */
      
	    pyrCA = Relation::_pyrimidine_ring_center (*ref);
	    pyrNA = Relation::_pyrimidine_ring_normal (*ref, pyrCA);
      
	    pyrCB = Relation::_pyrimidine_ring_center (*res);
	    pyrNB = Relation::_pyrimidine_ring_normal (*res, pyrCB);
      
	    if (ref->getType ()->isPurine ())
	      {
		rtypes = 0;
		//pyrNA = -pyrNA;
		imidCA = Relation::_imidazole_ring_center (*ref);
		imidNA = Relation::_imidazole_ring_normal (*ref, imidCA);
	      }
	    else if (ref->getType ()->isPyrimidine ())
	      {
		rtypes = 2;
	      }
	    else
	      {
		IntLibException ex ("", __FILE__, __LINE__);
		ex << "Type \"" << ref->getType () << "\" not handled for residue"
		   << ref->getResId ();
		throw ex;
	      }
      
	    if (res->getType ()->isPurine ())
	      {
		//pyrNB = -pyrNB;
		imidCB = Relation::_imidazole_ring_center (*res);
		imidNB = Relation::_imidazole_ring_normal (*res, imidCB);
	      }
	    else if (res->getType ()->isPyrimidine ())
	      {
		rtypes |= 1;
	      }
	    else
	      {
		IntLibException ex ("", __FILE__, __LINE__);
		ex << "Type \"" << res->getType () << "\" not handled for residue"
		   << res->getResId ();
		throw ex;
	      }

	    // pyrimidine / pyrimidine
	    stacking = Relation::_ring_stacking (pyrCA, pyrNA, pyrCB, pyrNB);

	    // imidazole / pyrimidine
	    if (PropertyType::pNull == stacking && (1 == rtypes || 0 == rtypes))
	      stacking = Relation::_ring_stacking (imidCA, imidNA, pyrCB, pyrNB);

	    // pyrimidine / imidazole
	    if (PropertyType::pNull == stacking && (2 == rtypes || 0 == rtypes))
	      stacking = Relation::_ring_stacking (pyrCA, pyrNA, imidCB, imidNB);

	    // imidazole / imidazole
	    if (PropertyType::pNull == stacking && 0 == rtypes)
	      stacking = Relation::_ring_stacking (imidCA, imidNA, imidCB, imidNB);

	    if (PropertyType::pNull != stacking)
	      labels.insert (stacking);
	  }
	catch (IntLibException& ex)
	  {
	    gOut (3) << "An error occured during stacking annotation: " << ex << endl;
	  }
      }
  }
  

  //   Relation
  //   Relation::invert () const
  //   {
  //     Relation inv;
  //     inv.ref = res;
  //     inv.res = ref;
  //     inv.tfo = tfo.invert ();
  //     inv.po4_tfo = inv.tfo * po4_tfo;
  //     inv.refFace = resFace;
  //     inv.resFace = refFace;
  //     for (set< const PropertyType* >::const_iterator i=labels.begin (); i!=labels.end (); ++i) {
  //       if (*i == PropertyType::pDIR_5p) inv.labels.insert (PropertyType::pDIR_3p);
  //       else if (*i == PropertyType::pDIR_3p) inv.labels.insert (PropertyType::pDIR_5p);
  //       else inv.labels.insert (*i);
  //     }
  //     return inv;
  //   }
  

  Relation&
  Relation::invert ()
  {
    const Residue* rt;
    const PropertyType* pt;
    set< const PropertyType* > lt;
    set< const PropertyType* >::const_iterator it;
    vector< pair< const PropertyType*, const PropertyType* > >::iterator pfit;

    // -- invert residues and faces
    rt = ref;
    ref = res;
    res = rt;
    pt = refFace;
    refFace = resFace;
    resFace = pt;

    // -- invert transfos
    this->tfo = this->tfo.invert ();
    if (this->isAdjacent ())
      this->po4_tfo = this->tfo * this->po4_tfo;

    // -- invert labels
    for (it = labels.begin (); it != labels.end (); ++it) 
      lt.insert (PropertyType::invert (*it));
    labels = lt;
    for (pfit = pairedFaces.begin (); pairedFaces.end () != pfit; ++pfit)
      {
	swap (pfit->first, pfit->second);
      }
    return *this;
  }


  // I/O -----------------------------------------------------------------------


  ostream&
  Relation::write (ostream &os) const
  {
    if (ref != 0 && res != 0)
      {
	os << "{" 
	   << ref->getResId () << ref->getType () << " -> " 
	   << res->getResId () << res->getType () << ": ";
	copy (labels.begin (), labels.end (), ostream_iterator< const PropertyType* > (os, " "));
	if (is (PropertyType::pPairing))
	  {
	    vector< pair< const PropertyType*, const PropertyType* > >::const_iterator it;

	    for (it = pairedFaces.begin (); pairedFaces.end () != it; ++it)
	      {
		os << *it->first << "/" << *it->second << ' ';
	      }
	  }
	os << "}";

      }
    return os;
  }

  
  // STATIC METHODS ------------------------------------------------------------


  set< const PropertyType* > 
  Relation::areAdjacent (const Residue* ra, const Residue *rb)
  {
    Relation rel (ra, rb);

    rel.areAdjacent ();
    return rel.getLabels ();
  }
  

  set< const PropertyType* >
  Relation::arePaired (const Residue *ra, const Residue *rb, const PropertyType *pta, const PropertyType *ptb)
  {
    Relation rel (ra, rb);

    rel.arePaired ();
    pta = rel.getRefFace ();
    ptb = rel.getResFace ();
    return rel.getLabels ();
  }

  
  set< const PropertyType* >
  Relation::areStacked (const Residue *ra, const Residue *rb)
  {
    Relation rel (ra, rb);

    rel.areStacked ();
    return rel.getLabels ();
  }
  

  set< const PropertyType* > 
  Relation::areHBonded (const Residue* ra, const Residue *rb)
  {
    Relation rel (ra, rb);
    
    rel.areHBonded ();
    return rel.getLabels ();
  }
    
  
  const PropertyType* 
  Relation::getFace (const Residue *r, const Vector3D &p)
  {

    if (!Relation::face_init)
      Relation::init ();

    HomogeneousTransfo t;
    t = r->getReferential ();
    t = t.invert ();
    Vector3D pp = t * p;
    vector< pair< Vector3D, const PropertyType* > > *faces = 0;

    if (r->getType ()->isA ())
      {
	faces = &faces_A;
      }
    else if (r->getType ()->isC ())
      {
	faces = &faces_C;
      }
    else if (r->getType ()->isG ())
      {
	faces = &faces_G;
      }
    else if (r->getType ()->isU ())
      {
	faces = &faces_U;
      }
    else if (r->getType ()->isT ())
      {
	faces = &faces_T;
      }

    if (0 != faces)
      {
	int face_index = 0;
	unsigned int x;
	float dist = numeric_limits< float >::max ();
    
	for (x = 0; x < faces->size (); ++x)
	  {
	    float tmp = pp.distance ((*faces)[x].first);
	    if (tmp < dist)
	      {
		face_index = x;
		dist = tmp;
	      }
	  }

	return (*faces)[face_index].second;
      }
    else
      {
	return PropertyType::pNull;
      }
  }
  
  
  void 
  Relation::init () 
  {
    try
      {
	Vector3D ta, tb;
	ExtendedResidue A (ResidueType::rRA, ResId ('A', 1));
	ExtendedResidue C (ResidueType::rRC, ResId ('C', 1));
	ExtendedResidue G (ResidueType::rRG, ResId ('G', 1));
	ExtendedResidue U (ResidueType::rRU, ResId ('U', 1));
	ExtendedResidue T (ResidueType::rDT, ResId ('T', 1));

	A.setTheoretical ();
	ta = *A.safeFind (AtomType::aH8);	
	Relation::faces_A.push_back (make_pair (ta, PropertyType::parseType ("C8")));
	ta = (*A.safeFind (AtomType::aH8) + *A.safeFind (AtomType::aLP7)) / 2;	
	Relation::faces_A.push_back (make_pair (ta, PropertyType::parseType ("Hh")));
	ta = (*A.safeFind (AtomType::a2H6) + *A.safeFind (AtomType::aLP7)) / 2;
	Relation::faces_A.push_back (make_pair (ta, PropertyType::parseType ("Hh")));
	ta = *A.safeFind (AtomType::a2H6);
	Relation::faces_A.push_back (make_pair (ta, PropertyType::parseType ("Hw")));
	ta = (*A.safeFind (AtomType::a1H6) + *A.safeFind (AtomType::a2H6)) / 2;
	Relation::faces_A.push_back (make_pair (ta, PropertyType::parseType ("Bh")));
	ta = *A.safeFind (AtomType::a1H6);
	Relation::faces_A.push_back (make_pair (ta, PropertyType::parseType ("Wh")));
	ta = (*A.safeFind (AtomType::aLP1) + *A.safeFind (AtomType::a1H6)) / 2;
	Relation::faces_A.push_back (make_pair (ta, PropertyType::parseType ("Ww")));
	ta = (*A.safeFind (AtomType::aLP1) + *A.safeFind (AtomType::aH2)) / 2;
	Relation::faces_A.push_back (make_pair (ta, PropertyType::parseType ("Ww")));
	ta = *A.safeFind (AtomType::aH2);
	Relation::faces_A.push_back (make_pair (ta, PropertyType::parseType ("Bs")));
	ta = (*A.safeFind (AtomType::aH2) + *A.safeFind (AtomType::aLP3)) / 2;
	Relation::faces_A.push_back (make_pair (ta, PropertyType::parseType ("Ss")));
	ta = *A.safeFind (AtomType::aLP3);
	Relation::faces_A.push_back (make_pair (ta, PropertyType::parseType ("Ss")));

	C.setTheoretical ();
	ta = *C.safeFind (AtomType::aH6);
	Relation::faces_C.push_back (make_pair (ta, PropertyType::parseType ("Hh")));
	ta = (*C.safeFind (AtomType::a1H4) + *C.safeFind (AtomType::aH5)) / 2;
	Relation::faces_C.push_back (make_pair (ta, PropertyType::parseType ("Hh")));
	ta = *C.safeFind (AtomType::a1H4);
	Relation::faces_C.push_back (make_pair (ta, PropertyType::parseType ("Hw")));
	ta = (*C.safeFind (AtomType::a1H4) + *C.safeFind (AtomType::a2H4)) / 2;
	Relation::faces_C.push_back (make_pair (ta, PropertyType::parseType ("Bh")));
	ta = *C.safeFind (AtomType::a2H4);
	Relation::faces_C.push_back (make_pair (ta, PropertyType::parseType ("Wh")));
	ta = (*C.safeFind (AtomType::a2H4) + *C.safeFind (AtomType::aLP3)) / 2;
	Relation::faces_C.push_back (make_pair (ta, PropertyType::parseType ("Ww")));
	ta = (*C.safeFind (AtomType::aLP3) + *C.safeFind (AtomType::a2LP2)) / 2;
	Relation::faces_C.push_back (make_pair (ta, PropertyType::parseType ("Ww")));
	ta = *C.safeFind (AtomType::a2LP2);
	Relation::faces_C.push_back (make_pair (ta, PropertyType::parseType ("Ws")));
	ta = (*C.safeFind (AtomType::a2LP2) + *C.safeFind (AtomType::a1LP2)) / 2;
	Relation::faces_C.push_back (make_pair (ta, PropertyType::parseType ("Bs")));
	ta = *C.safeFind (AtomType::a1LP2);
	Relation::faces_C.push_back (make_pair (ta, PropertyType::parseType ("Ss")));

	G.setTheoretical ();
	ta = *G.safeFind (AtomType::aH8);
	Relation::faces_G.push_back (make_pair (ta, PropertyType::parseType ("C8")));
	ta = (*G.safeFind (AtomType::aH8) + *G.safeFind (AtomType::aLP7)) / 2;
	Relation::faces_G.push_back (make_pair (ta, PropertyType::parseType ("Hh")));
	ta = (*G.safeFind (AtomType::a1LP6) + *G.safeFind (AtomType::aLP7)) / 2;
	Relation::faces_G.push_back (make_pair (ta, PropertyType::parseType ("Hh")));
	ta = *G.safeFind (AtomType::a1LP6);
	Relation::faces_G.push_back (make_pair (ta, PropertyType::parseType ("Hw")));
	ta = (*G.safeFind (AtomType::a1LP6) + *G.safeFind (AtomType::a2LP6)) / 2;
	Relation::faces_G.push_back (make_pair (ta, PropertyType::parseType ("Bh")));
	ta = *G.safeFind (AtomType::a2LP6);
	Relation::faces_G.push_back (make_pair (ta, PropertyType::parseType ("Wh")));
	ta = (*G.safeFind (AtomType::a2LP6) + *G.safeFind (AtomType::aH1)) / 2;
	Relation::faces_G.push_back (make_pair (ta, PropertyType::parseType ("Ww")));
	ta = (*G.safeFind (AtomType::aH1) + *G.safeFind (AtomType::a2H2)) / 2;
	Relation::faces_G.push_back (make_pair (ta, PropertyType::parseType ("Ww")));
	ta = *G.safeFind (AtomType::a2H2);
	Relation::faces_G.push_back (make_pair (ta, PropertyType::parseType ("Ws")));
	ta = (*G.safeFind (AtomType::a2H2) + *G.safeFind (AtomType::a1H2)) / 2;
	Relation::faces_G.push_back (make_pair (ta, PropertyType::parseType ("Bs")));
	ta = *G.safeFind (AtomType::a1H2);
	Relation::faces_G.push_back (make_pair (ta, PropertyType::parseType ("Sw")));
	ta = (*G.safeFind (AtomType::a1H2) + *G.safeFind (AtomType::aLP3)) / 2;
	Relation::faces_G.push_back (make_pair (ta, PropertyType::parseType ("Ss")));
    
	U.setTheoretical ();
	ta = *U.safeFind (AtomType::aH6);
	Relation::faces_U.push_back (make_pair (ta, PropertyType::parseType ("Hh")));
	ta = (*U.safeFind (AtomType::a1LP4) + *U.safeFind (AtomType::aH5)) / 2;
	Relation::faces_U.push_back (make_pair (ta, PropertyType::parseType ("Hh")));
	ta = *U.safeFind (AtomType::a1LP4);
	Relation::faces_U.push_back (make_pair (ta, PropertyType::parseType ("Hw")));
	ta = (*U.safeFind (AtomType::a1LP4) + *U.safeFind (AtomType::a2LP4)) / 2;
	Relation::faces_U.push_back (make_pair (ta, PropertyType::parseType ("Bh")));
	ta = *U.safeFind (AtomType::a2LP4);
	Relation::faces_U.push_back (make_pair (ta, PropertyType::parseType ("Wh")));
	ta = (*U.safeFind (AtomType::a2LP4) + *U.safeFind (AtomType::aH3)) / 2;
	Relation::faces_U.push_back (make_pair (ta, PropertyType::parseType ("Ww")));
	ta = *U.safeFind (AtomType::aH3);
	Relation::faces_U.push_back (make_pair (ta, PropertyType::parseType ("Ww")));
	ta = (*U.safeFind (AtomType::a2LP2) + *U.safeFind (AtomType::aH3)) / 2;
	Relation::faces_U.push_back (make_pair (ta, PropertyType::parseType ("Ws")));
	ta = *U.safeFind (AtomType::a2LP2);
	Relation::faces_U.push_back (make_pair (ta, PropertyType::parseType ("Ws")));
	ta = (*U.safeFind (AtomType::a2LP2) + *U.safeFind (AtomType::a1LP2)) / 2;
	Relation::faces_U.push_back (make_pair (ta, PropertyType::parseType ("Bs")));
	ta = *U.safeFind (AtomType::a1LP2);
	Relation::faces_U.push_back (make_pair (ta, PropertyType::parseType ("Ss")));
    
	T.setTheoretical ();
	ta = *T.safeFind (AtomType::aH6);
	Relation::faces_T.push_back (make_pair (ta, PropertyType::parseType ("Hh")));
	ta = (*T.safeFind (AtomType::a1LP4) + *T.safeFind (AtomType::aC5M)) / 2;
	Relation::faces_T.push_back (make_pair (ta, PropertyType::parseType ("Hh")));
	ta = *T.safeFind (AtomType::a1LP4);
	Relation::faces_T.push_back (make_pair (ta, PropertyType::parseType ("Hw")));
	ta = (*T.safeFind (AtomType::a1LP4) + *T.safeFind (AtomType::a2LP4)) / 2;
	Relation::faces_T.push_back (make_pair (ta, PropertyType::parseType ("Bh")));
	ta = *T.safeFind (AtomType::a2LP4);
	Relation::faces_T.push_back (make_pair (ta, PropertyType::parseType ("Wh")));
	ta = (*T.safeFind (AtomType::a2LP4) + *T.safeFind (AtomType::aH3)) / 2;
	Relation::faces_T.push_back (make_pair (ta, PropertyType::parseType ("Ww")));
	ta = *T.safeFind (AtomType::aH3);
	Relation::faces_T.push_back (make_pair (ta, PropertyType::parseType ("Ww")));
	ta = (*T.safeFind (AtomType::a2LP2) + *T.safeFind (AtomType::aH3)) / 2;
	Relation::faces_T.push_back (make_pair (ta, PropertyType::parseType ("Ws")));
	ta = *T.safeFind (AtomType::a2LP2);
	Relation::faces_T.push_back (make_pair (ta, PropertyType::parseType ("Ws")));
	ta = (*T.safeFind (AtomType::a2LP2) + *T.safeFind (AtomType::a1LP2)) / 2;
	Relation::faces_T.push_back (make_pair (ta, PropertyType::parseType ("Bs")));
	ta = *T.safeFind (AtomType::a1LP2);
	Relation::faces_T.push_back (make_pair (ta, PropertyType::parseType ("Ss")));
 
	Relation::face_init = true;
      }
    catch (IntLibException& ex)
      {
	FatalIntLibException fex ("", __FILE__, __LINE__);
	fex << "failed to initialize faces in relation: " << ex.what ();
	throw fex;
      }
    
  }


  const PropertyType* 
  Relation::translatePairing (const Residue *ra, const Residue *rb, const PropertyType *bpo, list< HBondFlow > &hbf, float total_flow, unsigned int size_hint)
  {
    list< PairingPattern >::const_iterator i;
    const PropertyType *type;
    const PropertyType *best_type = 0;
    unsigned int best_size = 0;

    for (i = PairingPattern::patternList ().begin ();
	 i != PairingPattern::patternList().end ();
	 ++i)
      {
	if (size_hint >= (*i).size ()
	    && (type = (*i).evaluate (ra, rb, bpo, hbf)) != 0)
	  {
	    if ((*i).size () > best_size)
	      {
		best_size = (*i).size ();
		best_type = type;
	      }
	  }
      }
    return best_type;
  }


  iBinstream&
  Relation::read (iBinstream &is, const map< ResId, const Residue* > &resMap) throw (NoSuchElementException)
  {
    ResId id;
    map<ResId, const Residue* >::const_iterator rmIt;
    mccore::bin_ui64 qty = 0;
    
    is >> id;
    if (resMap.end () == (rmIt = resMap.find (id)))
      {
	NoSuchElementException e ("cannot find residue id ", __FILE__, __LINE__);

	e << id;
	throw e;
      }
    else
      {
	ref = rmIt->second;
      }
    is >> id;
    if (resMap.end () == (rmIt = resMap.find (id)))
      {
	NoSuchElementException e ("cannot find residue id ", __FILE__, __LINE__);

	e << id;
	throw e;
      }
    else
      {
	res = rmIt->second;
      }
    is >> tfo >> po4_tfo;
    is >> refFace >> resFace;
    labels.clear ();
    is >> qty;
    while (0 < qty)
      {
	if (!is.good ())
	  {
	    FatalIntLibException ex ("", __FILE__, __LINE__);
	    ex << "read failure, " << (unsigned)qty << " to go.";
	    throw ex;
	  }

	const PropertyType *prop;
	
	is >> prop;
	labels.insert (prop);
	--qty;
      }
    is >> type_aspb;
    hbonds.clear ();
    is >> qty;
    while (0 < qty)
      {
	if (!is.good ())
	  {
	    FatalIntLibException ex ("", __FILE__, __LINE__);
	    ex << "read failure, " << (unsigned)qty << " to go.";
	    throw ex;
	  }

	hbonds.push_back (HBondFlow ());
	HBondFlow &hf = hbonds.back ();
	hf.read (is, resMap);
	--qty;
      }
    is >> sum_flow;
    is >> qty;
    while (0 < qty)
      {
	const PropertyType *ref;
	const PropertyType *res;
	
	if (!is.good ())
	  {
	    FatalIntLibException ex ("", __FILE__, __LINE__);
	    ex << "read failure, " << (unsigned)qty << " to go.";
	    throw ex;
	  }
	is >> ref >> res;
	pairedFaces.push_back (make_pair (ref, res));
	--qty;
      }
    return is;
  }
    

  oBinstream&
  Relation::write (oBinstream &os) const
  {
    set< const PropertyType* >::const_iterator propsIt;
    vector< HBondFlow >::const_iterator hfsIt;
    vector< pair< const PropertyType*, const PropertyType* > >::const_iterator pfit;
    
    os << ref->getResId ();
    os << res->getResId ();
    os << tfo << po4_tfo;
    os << refFace << resFace;
    os << (mccore::bin_ui64) labels.size ();
    for (propsIt = labels.begin (); labels.end () != propsIt; ++propsIt)
      {
	os << *propsIt;
      }
    os << type_aspb;
    os << (mccore::bin_ui64) hbonds.size ();
    for (hfsIt = hbonds.begin (); hbonds.end () != hfsIt; ++hfsIt)
      {
	hfsIt->write (os);
      }
    os << sum_flow;
    os << (mccore::bin_ui64) pairedFaces.size ();
    for (pfit = pairedFaces.begin (); pairedFaces.end () != pfit; ++pfit)
      {
	os << pfit->first << pfit->second;
      }
    return os;
  }

}


namespace std
{
  
  ostream& operator<< (ostream &os, const mccore::HBondFlow &hbf)
  {
    return hbf.write (os);
  }

  
  ostream& operator<< (ostream &os, const mccore::Relation &r)
  {
    return r.write (os);
  }

  
  ostream& operator<< (ostream &os, const mccore::Relation *r)
  {
    return r->write (os);
  }
  
}
