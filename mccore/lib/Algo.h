//                              -*- Mode: C++ -*- 
// Algo.h
// Copyright � 2001-03 Laboratoire de Biologie Informatique et Th�orique.
//                     Universit� de Montr�al.
// Author           : Sebastien Lemieux <lemieuxs@iro.umontreal.ca>
// Created On       : Wed Feb 14 15:33:58 2001
// $Revision $
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


#ifndef _Algo_h_
#define _Algo_h_

#include <algorithm>
#include <map>
#include <math.h>
#include <utility>
#include <values.h>
#include <vector>

#include "Residue.h"
#include "ResidueType.h"
#include "Atom.h"

using namespace std;



namespace mccore {
  /**
   * @short A class for lone algorithms in the mccore library
   * 
   * Long This is a container for algorithms that are not part of any
   * objects.  Note: const_iterator are now used as much as possible so that
   * the functions may be called with const_iterator and iterator types.
   *
   * @author Sebastien Lemieux
   * @version $Id: Algo.h,v 1.18 2003-12-23 14:46:12 larosem Exp $
   */
  class Algo
  {
  public:
    
    /**
     * Using the Axis Aligned Bounding Box for collision detection, this
     * method calculates the possible contacts between residues.
     * @param begin an iterator on a collection of Residue.
     * @param end an iterator on a collection of Residue.
     * @param cutoff on the minimum distance for a contact (default = 5.0 Angstroms).
     * @return a vector of pair of iterators on residues in contact.
     */
    template< class iter_type >
    static vector< pair< iter_type, iter_type > > 
    extractContacts (iter_type begin, iter_type end, float cutoff = 5.0) 
    {
      vector< pair< iter_type, iter_type > > result;
      vector< ResidueRange< iter_type > > X_range;
      vector< ResidueRange< iter_type > > Y_range;
      vector< ResidueRange< iter_type > > Z_range;
      iter_type i;
      
      for (i = begin; i != end; ++i) 
	{
	  Residue::const_iterator j;
	  
	  float minX, minY, minZ, maxX, maxY, maxZ;
	  minX = minY = minZ = HUGE;
	  maxX = maxY = maxZ = -HUGE;
	  
	  for (j = i->begin (new AtomSetNot (new AtomSetPSE ())); j != i->end (); ++j)
	    {
	      minX = min (minX, j->getX ());
	      minY = min (minY, j->getY ());
	      minZ = min (minZ, j->getZ ());
	      maxX = max (maxX, j->getX ());
	      maxY = max (maxY, j->getY ());
	      maxZ = max (maxZ, j->getZ ());
	    }
	  X_range.push_back (ResidueRange< iter_type > (i, minX, maxX));
	Y_range.push_back (ResidueRange< iter_type > (i, minY, maxY));
	Z_range.push_back (ResidueRange< iter_type > (i, minZ, maxZ));
	}
      sort (X_range.begin (), X_range.end ());
      sort (Y_range.begin (), Y_range.end ());
      sort (Z_range.begin (), Z_range.end ());
      
      map< pair< iter_type, iter_type >, int > contact;
      
      ExtractContact_OneDim (X_range, contact, cutoff);
      ExtractContact_OneDim (Y_range, contact, cutoff);
      
      typename map< pair< iter_type, iter_type >, int >::iterator cont_i;
      
      for (cont_i = contact.begin (); cont_i != contact.end (); ++cont_i)
	{
	  typename map< pair< iter_type, iter_type >, int >::iterator tmp = cont_i;
	  
	  tmp++;
	  if (cont_i->second < 2)
	    contact.erase (cont_i);
	  // For an unknown reason, when the map is empty, 
	  // cont_i-- does not points to contact.begin (), so this test is added:
	  if (contact.size () == 0)
	    break;
	  cont_i = tmp;
	  cont_i--;
	}
      
      ExtractContact_OneDim (Z_range, contact, cutoff);
      
      for (cont_i = contact.begin (); cont_i != contact.end (); ++cont_i)
	if (cont_i->second == 3) {
	  result.push_back (cont_i->first);
	}
      return result;
    }
    
    
  private:
    
    template< class iter_type >
    class ResidueRange
    {
      iter_type res;
      float lower;
      float upper;
      
    public:
      
      ResidueRange (iter_type r, float l, float u)
	: res (r), lower (l), upper (u)
      {}
      
      /**
       * Imposes a total ordering on the ResidueRange objects.  They are sorted
       * by lower and upper bounds.
       * @param obj the ResidueRange to compare.
       * @return true if this range is less than obj
       */
      bool operator< (const ResidueRange &obj) const { 
	if (lower < obj.lower)
	  return true;
	else if (lower == obj.lower)
	  return upper < obj.upper;
	else return false;
      }
      
      /**
       * Determines if the bounds of the residue overlap that of this residue.
       */
      bool overlap (const ResidueRange &r) {
	if (lower < r.lower)
	  return upper > r.lower;
        else
	  return lower <= r.upper;
      }
      
      float lowerBound () { return lower; }
      float upperBound () { return upper; }
      
      iter_type getResidue () { return res; }
    
      void output (ostream &out)
      {
	out << res->getResId () << " : " << lower << "-" << upper;
      }
  };
    
  /**
   * Builds a map of contacts in one dimension given that range elements are sorted.     
   */
  template< class iter_type >
  static void ExtractContact_OneDim (vector< ResidueRange< iter_type > > &range, 
				     map< pair< iter_type, iter_type >, int > &contact,
				     float cutoff)
    {
      typename vector< ResidueRange< iter_type > >::iterator i;
      
      for (i = range.begin (); i != range.end (); ++i)
	{
	  typename vector< ResidueRange< iter_type > >::iterator j;
	  
	  for (j = i; j != range.end (); ++j)
	    if (i != j)
	      {
		if (j->lowerBound () - cutoff <= i->upperBound ())
		  {
		    if (i->getResidue () < j->getResidue ())
		      ++contact[make_pair (i->getResidue (), j->getResidue ())];
		    else
		      ++contact[make_pair (j->getResidue (), i->getResidue ())];
		  }
		else
		  break;
	      }
	}
    }
  };
}

#endif
