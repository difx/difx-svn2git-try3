/***************************************************************************
 *   Copyright (C) 2011 by Jan Wagner                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/
//===========================================================================
// SVN properties (DO NOT CHANGE)
//
// $Id: $
// $HeadURL: $
// $LastChangedRevision: $
// $Author: $
// $LastChangedDate: $
//
//============================================================================

#ifndef _ANALYZER_H
#define _ANALYZER_H

#include "Covariance.h"

/**
 * Base class for computing matrix decompositions of a 3D data cube or single 2D matrix.
 * The base class provides only the generic interface implementation and allocation functions,
 * it does not perform an actual matrix decomposition.
 */
class Decomposition {

   private:
      Decomposition();

   public:

      /**
       * C'stor for batch decomposition. Allocates sufficient internal space
       * to compute and store decomposition results of multiple covariance
       * matrices in Rxx data cube.
       *
       * @param[in] Rxx Reference to covariance class
       */
      Decomposition(Covariance& Rxx) { /*derived should call: cstor_alloc(Rxx.N_ant, Rxx.N_chan, numMat, numVec);*/ }

      /**
       * C'stor for decomposition. Allocates sufficient internal space
       * to compute and store decomposition results of a single
       * covariance matrix Rxx.
       *
       * @param[in] Rxx Reference to covariance class
       */
      Decomposition(arma::Mat<arma::cx_double>& Rxx) { /*derived should call: cstor_alloc(Rxx.n_cols, 1, numMat, numVec);*/ }

      /**
       * D'stor to free internal allocations.
       */
      ~Decomposition() { }

  protected:
       /**
        * Allocate output matrices or output cubes.
        * @param[in]  Nant   Dimension of 2D square matrix.
        * @param[in]  Nchan  Number of 2D slices in 3D cube.
        * @param[in]  NdecoM  Number of matrices to store decomposition (1 for Eig, 2 for QR, 2 for SVD, etc)
        * @param[in]  NdecoV  Number of vectors to store decomposition (1 for Eig, 0 for QR, 1 for SVD, etc)
        *
        * If Nchan<=1, only the _single_out_matrices[] is allocated.
        * Otherwise, only the _batch_out_cubes[] is allocated.
        */
       void cstor_alloc(const int Nant, const int Nchan, const int NdecoM, const int NdecoV);

   public:

      /**
       * Perform batch decomposition of all covariances in the argument class.
       * @param[in]  allRxx  The covariance class containing one or more matrices.
       * @return 0 on success.
       */
      int decompose(Covariance& cov);

      /**
       * Perform single decomposition of given covariance matrix class.
       * @param[in]  Rxx  The covariance matrix to decompose.
       * @return 0 on success.
       */
      int decompose(arma::Mat<arma::cx_double>& Rxx) {
         return do_decomposition(0, Rxx);
      }

   private:
      /**
       * Decompose covariance matrix and store results into output array
       * specified by index 'sliceNr'. Dummy template function only.
       * @param[in]  sliceNr   Index into output cube (0=single matrix, 1..N+1=cube storage)
       * @param[in]  Rxx       Matrix to decompose
       * @return 0 on success
       */
      virtual int do_decomposition(int sliceNr, arma::Mat<arma::cx_double>& Rxx);

   protected:
      // Storage when processing several decompositions
      arma::Mat<double>           _batch_out_vectors;       // Nant x Nchannels
      arma::Cube<arma::cx_double> _batch_out_matrices[3];  // Nant x Nant x Nchannels
      // Storage when processing only one decomposition
      arma::Col<double>           _single_out_vector;       // Nant x 1
      arma::Mat<arma::cx_double>  _single_out_matrices[3]; // Nant x Nant x 1
};

#endif // _ANALYZER_H
