//
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2010 Alessandro Tasora
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file at the top level of the distribution
// and at http://projectchrono.org/license-chrono.txt.
//

#ifndef CHSOLVERMKL_H
#define CHSOLVERMKL_H

#include "chrono/core/ChMatrixDynamic.h"
#include "chrono/solver/ChSolver.h"
#include "chrono/solver/ChSystemDescriptor.h"
#include "chrono_mkl/ChMklEngine.h"
#include <core/ChTimer.h>

namespace chrono {

/// @addtogroup mkl_module
/// @{

/// Class that wraps the Intel MKL Pardiso parallel direct solver.
/// It can solve linear systems, but not VI and complementarity problems.
/// This class is usually set up by the end-user in its main program.
/// ::Solve(ChLcpSystemDescriptor&) and ::Factorize(ChLcpSystemDescriptor&) are instead called automatically during the integration step,
/// so they are not usually called by the end-user.


class ChApiMkl ChSolverMKL : public ChSolver {
    // Chrono RTTI, needed for serialization
    CH_RTTI(ChSolverMKL, ChSolver);

  private:
    size_t solver_call = 0;
    ChCSR3Matrix matCSR3;
    ChMatrixDynamic<double> rhs;
    ChMatrixDynamic<double> sol;
    ChMatrixDynamic<double> res;
    ChMklEngine mkl_engine;
    size_t n;
    size_t nnz;

    ChTimer<> timer_factorize;
    ChTimer<> timer_solve;
    ChTimer<> timer_buildmat;

    bool sparsity_pattern_lock;
    bool use_perm;
    bool use_rhs_sparsity;
    bool manual_factorization;

  public:
    ChSolverMKL();
    virtual ~ChSolverMKL() {}

    ChMklEngine& GetMklEngine() { return mkl_engine; }
    ChCSR3Matrix& GetMatrix() { return matCSR3; }

    /// If \a on_off is set to \c true then \c matCSR3 ChCSR3Matrix::Reset(int,int,int) function
    /// will keep the sparsity structure i.e. it is supposed that, during the next allocation,
    /// the matrix will have its nonzeros placed in the same position as the current allocation.
    void SetSparsityPatternLock(bool on_off) { sparsity_pattern_lock = on_off; }
    void UsePermutationVector(bool on_off) { use_perm = on_off; }
    void LeverageRhsSparsity(bool on_off) { use_rhs_sparsity = on_off; }
    void SetPreconditionedCGS(bool on_off, int L) { mkl_engine.SetPreconditionedCGS(on_off, L); }
    /// If \a on_off is set to \c true then ::Solve(ChSystemDescriptor&) call
    /// must be preceded by a ::Factorize(ChSystemDescriptor&) call.
    void SetManualFactorization(bool on_off) { manual_factorization = on_off; }
    void SetMatrixNNZ(size_t nnz_input) { nnz = nnz_input; }

    double GetTimingBuildMat() const { return timer_buildmat(); }
    double GetTimingFactorize() const { return timer_factorize(); }
    double GetTimingSolve() const { return timer_solve(); }

    /// Solve using the MKL Pardiso sparse direct solver.
    /// If ::manual_factorization is turned off (i.e. set to \c false) then
    /// it automatically calls ::Factorize(ChSystemDescriptor&) in order to perform analysis,
    /// reordering and factorization (MKL Pardiso phase 12).
    /// In any case a call to this function will end with a solve and refinement phase (Pardiso phase 33)
    virtual double Solve(ChSystemDescriptor& sysd) override;
    /// Performs a factorization of the system matrix.
    virtual double Factorize(ChSystemDescriptor& sysd) override;

    //
    // SERIALIZATION
    //

    virtual void ArchiveOUT(ChArchiveOut& marchive) override {
        // version number
        marchive.VersionWrite(1);
        // serialize parent class
        ChSolver::ArchiveOUT(marchive);
        // serialize all member data:
        marchive << CHNVP(sparsity_pattern_lock);
        marchive << CHNVP(use_perm);
        marchive << CHNVP(use_rhs_sparsity);
        marchive << CHNVP(manual_factorization);
    }

    /// Method to allow de serialization of transient data from archives.
    virtual void ArchiveIN(ChArchiveIn& marchive) override {
        // version number
        int version = marchive.VersionRead();
        // deserialize parent class
        ChSolver::ArchiveIN(marchive);
        // stream in all member data:
        marchive >> CHNVP(sparsity_pattern_lock);
        marchive >> CHNVP(use_perm);
        marchive >> CHNVP(use_rhs_sparsity);
        marchive >> CHNVP(manual_factorization);
    }
};

/// @} mkl_module

}  // end namespace chrono

#endif
