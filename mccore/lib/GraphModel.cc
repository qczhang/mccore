//                              -*- Mode: C++ -*- 
// GraphModel.cc
// Copyright © 2004 Laboratoire de Biologie Informatique et Théorique
//                  Université de Montréal.
// Author           : Martin Larose <larosem@iro.umontreal.ca>
// Created On       : Thu Dec  9 19:34:11 2004
// $Revision: 1.1.2.5 $
// $Id: GraphModel.cc,v 1.1.2.5 2004-12-29 21:11:39 larosem Exp $
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
#include <set>
#include <vector>

#include "Binstream.h"
#include "GraphModel.h"
#include "Messagestream.h"
#include "Pdbstream.h"
#include "Relation.h"
#include "Residue.h"
#include "ResidueFactoryMethod.h"

using namespace std;



namespace mccore
{

  GraphModel::GraphModel (const AbstractModel &right)
    : AbstractModel (right)
  {
    const GraphModel *model;
    
    if (0 == (model = dynamic_cast< const GraphModel* > (&right)))
      {
	annotated = false;
	AbstractModel::insert (right.begin (), right.end ());
      }
    else
      {
	annotated = model->annotated;
	deepCopy (*model);
      }
  }
  

  GraphModel::~GraphModel ()
  {
    vector< Residue* >::iterator resIt;
    vector< Relation* >::iterator relIt;

    for (resIt = vertices.begin (); vertices.end () != resIt; ++resIt)
      {
	delete *resIt;
      }
    for (relIt = edges.begin (); edges.end () != relIt; ++relIt)
      {
	delete *relIt;
      }
  }


  void
  GraphModel::deepCopy (const GraphModel &right)
  {
    vector< Residue* >::const_iterator resIt;
    vector< Relation* >::const_iterator relIt;
    set< const Residue*, less_deref< Residue > > resSet;

    for (resIt = right.vertices.begin (); right.vertices.end () != resIt; ++resIt)
      {
	Residue *res = (*resIt)->clone ();
	
	graphsuper::insert (res, 0);
	resSet.insert (res);	
      }
    for (relIt = right.edges.begin (); right.edges.end () != relIt; ++relIt)
      {
	Relation *rel;

	rel = (*relIt)->clone ();
	rel->reassignResiduePointers (resSet);
	graphsuper::connect (const_cast< Residue* > (rel->getRef ()), const_cast < Residue* > (rel->getRes ()), rel, 0);
      }
  }
  
  
  GraphModel&
  GraphModel::operator= (const AbstractModel &right)
  {
    if (this != &right)
      {
	const GraphModel *model;
	
	clear ();
	AbstractModel::operator= (right);
	if (0 == (model = dynamic_cast< const GraphModel* > (&right)))
	  {
	    AbstractModel::insert (right.begin (), right.end ());
	  }
	else
	  {
	    annotated = model->annotated;
	    deepCopy (*model);
	  }
      }
    return *this;
  }
    

  GraphModel::iterator
  GraphModel::erase (AbstractModel::iterator pos) 
  {
    Residue *res = &*pos;
    iterator ret (graphsuper::erase (&*pos));

    delete res;
    return ret;
  }

  
  void
  GraphModel::sort ()
  {
    vector< Residue* > orig = vertices;
    vector< int > origWeights = vertexWeights;
    graphsuper::size_type vIndex;
    graphsuper::size_type *corresp;
    EV2ELabel origEdgeMap = ev2elabel;
    EV2ELabel::iterator evIt;

    std::sort (vertices.begin (), vertices.end (), less_deref< Residue > ());
    corresp = new graphsuper::size_type[size ()];
    for (vIndex = 0; vIndex < size () - 1; ++vIndex)
      {
	graphsuper::size_type origIndex;

	origIndex = v2vlabel.find (&orig[vIndex])->second;
	corresp[origIndex] = vIndex;
	vertexWeights[vIndex] = origWeights[origIndex];
      }	
    rebuildV2VLabel ();

    ev2elabel.clear ();
    for (evIt = origEdgeMap.begin (); origEdgeMap.end () != evIt; ++evIt)
      {
	const EndVertices &endVertices = evIt->first;
	EndVertices ev (corresp[endVertices.getHeadLabel ()],
			corresp[endVertices.getTailLabel ()]);

	ev2elabel.insert (make_pair (ev, evIt->second));
      }
    delete[] corresp;
  }
  
    
  void
  GraphModel::clear ()
  {
    graphsuper::iterator vIt;
    edge_iterator eIt;
    
    for (vIt = graphsuper::begin (); graphsuper::end () != vIt; ++vIt)
      {
	delete *vIt;
      }
    for (eIt = edge_begin (); edge_end () != eIt; ++eIt)
      {
	delete *eIt;
      }
    graphsuper::clear ();
    annotated = false;
  }


  void
  GraphModel::annotate ()
  {
    if (! annotated)
      {
	edge_iterator eIt;
	vector< pair< AbstractModel::iterator, AbstractModel::iterator > > contacts;
	vector< pair< AbstractModel::iterator, AbstractModel::iterator > >::iterator l;

	for (eIt = edge_begin (); edge_end () != eIt; ++eIt)
	  {
	    delete *eIt;
	  }
	edges.clear ();
	ev2elabel.clear ();
	edgeWeights.clear ();

	removeWater ();
	addHLP ();
	
	contacts = Algo::extractContacts (begin (), end (), 5.0);
	gErr (3) << "Found " << contacts.size () << " possible contacts " << endl;
  
	for (l = contacts.begin (); contacts.end () != l; ++l)
	  {
	    Residue *i = &*l->first;
	    Residue *j = &*l->second;
	    Relation *rel = new Relation (i, j);

	    if (rel->annotate ())
	      {
		Relation invert = rel->invert ();
	    
		connect (i, j, rel, 0);
		connect (j, i, invert.clone (), 0);
	      }
	    else
	      {
		delete rel;
	      }
	  }
	annotated = true;
      }
  }
    

  ostream&
  GraphModel::output (ostream &os) const
  {
    const_iterator vit;
    vector< int >::const_iterator vwit;
    edge_const_iterator eit;
    vector< int >::const_iterator ewit;
    EV2ELabel::const_iterator evit;
    label counter;
    label linecounter;

    os << "[GraphModel]" << endl
       << "[Vertices]" << endl;
    for (vit = vertices.begin (), vwit = vertexWeights.begin (), counter = 0; vertices.end () != vit; ++vit, ++vwit, ++counter)
      {
	os << setw (5) << counter << "  " << *vit << "  " << *vwit << endl;
      }
    os << "[Edges]" << endl;
    for (eit = edges.begin (), ewit = edgeWeights.begin (), counter = 0; edges.end () != eit; ++eit, ++ewit, ++counter)
      {
	os << setw (5) << counter << "  " << **eit << "  " << *ewit << endl;
      }
    os << "[Adjacency matrix]" << endl;
    os << "     ";
    for (counter = 0; vertices.size () > counter; ++counter)
      {
	os << setw (5) << counter;
      }
    linecounter = 0;
    if (! empty ())
      {
	os << endl << setw (5) << linecounter;
      }
    for (counter = 0, evit = ev2elabel.begin (); ev2elabel.end () != evit; ++evit, ++counter)
      {
	label evline;
	label evcolumn;

	evline = evit->first.getHeadLabel ();
	evcolumn = evit->first.getTailLabel ();
	while (linecounter < evline)
	  {
	    while (size () > counter)
	      {
		os << setw (5) << '.';
		++counter;
	      }
	    counter = 0;
	    ++linecounter;
	    os << endl << setw (5) << linecounter;
	  }
	while (counter < evcolumn)
	  {
	    os << setw (5) << '.';
	    ++counter;
	  }
	os << setw (5) << evit->second;
      }
    while (linecounter < size ())
      {
	while (counter < size ())
	  {
	    os << setw (5) << '.';
	    ++counter;
	  }
	counter = 0;
	++linecounter;
	if (linecounter < size ())
	  {
	    os << endl << setw (5) << linecounter;
	  }
      }
    os << endl;      
    return os;
  }


  iPdbstream&
  GraphModel::input (iPdbstream &ips)
  {
//     int currNb = ips.getModelNb ();

    clear ();
    if (! ips)
      {
	return ips;
      }
    while (! ips.eof ())
      {
	Residue *res = getResidueFM ()->createResidue ();
	
	ips >> *res;
 	if (0 != res->size ())
	  {
	    graphsuper::insert (res, 0); 
	  }
	else
	  {
	    delete res;
	  }
      }    
    return ips;
  }
  

  oBinstream&
  GraphModel::output (oBinstream &obs) const
  {
    graphsuper::const_iterator cit;
    
    obs << size ();
    for (cit = graphsuper::begin (); cit != graphsuper::end (); ++cit)
      obs << **cit;
    return obs;
  }
  
  
  iBinstream&
  GraphModel::input (iBinstream &ibs)
  {
    GraphModel::size_type sz;

    clear ();
    ibs >> sz;
    for (; sz > 0; --sz)
      {
	Residue *res = getResidueFM ()->createResidue ();
	
	ibs >> *res;
	graphsuper::insert (res, 0); 
      }
    return ibs;
  }
  
}
