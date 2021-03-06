// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2014 projectchrono.org
// All right reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Alessandro Tasora, Radu Serban, Antonio Recuero
// =============================================================================
// ANCF gradient-deficient cable element.
// =============================================================================

#ifndef CHELEMENTCABLEANCF_H
#define CHELEMENTCABLEANCF_H

//#define BEAM_VERBOSE
#include "chrono/core/ChVector.h"
#include "chrono/core/ChQuadrature.h"
#include "chrono_fea/ChElementBeam.h"
#include "chrono_fea/ChBeamSection.h"
#include "chrono_fea/ChNodeFEAxyzD.h"
#include <cmath>

namespace chrono {
namespace fea {

/// @addtogroup fea_elements
/// @{

/// Simple beam element with two nodes and ANCF gradient-deficient formulation.
/// For this 'basic' implementation, constant section and constant material are assumed along the beam coordinate.
/// Torsional stiffness is impossible because of the formulation.
/// Based on the formulation in:
///  "Analysis of Thin Beams and Cables Using the Absolute Nodal Co-ordinate Formulation"
///  J.GERSTMAYR, A.SHABANA
///  Nonlinear Dynamics (2006) 45: 109�130
///  DOI: 10.1007/s11071-006-1856-1
/// and in:
/// "On the Validation and Applications of a Parallel Flexible Multi-body
///  Dynamics Implementation"
///  D. MELANZ
class ChElementCableANCF : public ChElementBeam, public ChLoadableU, public ChLoadableUVW {
  protected:
    std::vector<std::shared_ptr<ChNodeFEAxyzD> > nodes;

    std::shared_ptr<ChBeamSectionCable> section;
    ChMatrixNM<double, 12, 1> m_GenForceVec0;
    ChMatrixNM<double, 12, 12> m_JacobianMatrix;  ///< Jacobian matrix (Kfactor*[K] + Rfactor*[R])
    ChMatrixNM<double, 12, 12> m_MassMatrix;      ///< mass matrix

  public:
    ChElementCableANCF() {
        nodes.resize(2);
        m_use_damping = false;  ///< flag to add internal damping and its Jacobian
        m_alpha = 0.0;          ///< scaling factor for internal damping

        // this->StiffnessMatrix.Resize(this->GetNdofs(), this->GetNdofs());
        // this->MassMatrix.Resize(this->GetNdofs(), this->GetNdofs());
    }

    bool m_use_damping;  ///< Boolean indicating whether internal damping is added
    double m_alpha;      ///< Scaling factor for internal damping

    virtual ~ChElementCableANCF() {}

    virtual int GetNnodes() override { return 2; }
    virtual int GetNdofs() override { return 2 * 6; }
    virtual int GetNodeNdofs(int n) override { return 6; }

    virtual std::shared_ptr<ChNodeFEAbase> GetNodeN(int n) override { return nodes[n]; }

    virtual void SetNodes(std::shared_ptr<ChNodeFEAxyzD> nodeA, std::shared_ptr<ChNodeFEAxyzD> nodeB) {
        assert(nodeA);
        assert(nodeB);

        nodes[0] = nodeA;
        nodes[1] = nodeB;
        std::vector<ChVariables*> mvars;
        mvars.push_back(&nodes[0]->Variables());
        mvars.push_back(&nodes[0]->Variables_D());
        mvars.push_back(&nodes[1]->Variables());
        mvars.push_back(&nodes[1]->Variables_D());
        Kmatr.SetVariables(mvars);
    }

    //
    // FEM functions
    //

    /// Set the section & material of beam element .
    /// It is a shared property, so it can be shared between other beams.
    void SetSection(std::shared_ptr<ChBeamSectionCable> my_material) { section = my_material; }
    /// Get the section & material of the element
    std::shared_ptr<ChBeamSectionCable> GetSection() { return section; }

    /// Get the first node (beginning)
    std::shared_ptr<ChNodeFEAxyzD> GetNodeA() { return nodes[0]; }

    /// Get the second node (ending)
    std::shared_ptr<ChNodeFEAxyzD> GetNodeB() { return nodes[1]; }

    /// Fills the N shape function matrix with the
    /// values of shape functions at abscyssa 'xi'.
    /// Note, xi=0 at node1, xi=+1 at node2.
    /// NOTE! actually N should be a 3row, 12 column sparse matrix,
    /// as  N = [s1*eye(3) s2*eye(3) s3*eye(3) s4*eye(3)]; ,
    /// but to avoid wasting zero and repeated elements, here
    /// it stores only the s1 s2 s3 s4 values in a 1 row, 4 columns matrix!
    virtual void ShapeFunctions(ChMatrix<>& N, double xi) {
        double l = this->GetRestLength();

        N(0) = 1 - 3 * pow(xi, 2) + 2 * pow(xi, 3);
        N(1) = l * (xi - 2 * pow(xi, 2) + pow(xi, 3));
        N(2) = 3 * pow(xi, 2) - 2 * pow(xi, 3);
        N(3) = l * (-pow(xi, 2) + pow(xi, 3));
    };

    /// Fills the N shape function derivative matrix with the
    /// values of shape function derivatives at abscyssa 'xi'.
    /// Note, xi=0 at node1, xi=+1 at node2.
    /// NOTE! to avoid wasting zero and repeated elements, here
    /// it stores only the four values in a 1 row, 4 columns matrix!
    virtual void ShapeFunctionsDerivatives(ChMatrix<>& Nd, double xi) {
        double l = this->GetRestLength();

        Nd(0) = (6.0 * pow(xi, 2.0) - 6.0 * xi) / l;
        Nd(1) = 1.0 - 4.0 * xi + 3.0 * pow(xi, 2.0);
        Nd(2) = -(6.0 * pow(xi, 2.0) - 6.0 * xi) / l;
        Nd(3) = -2.0 * xi + 3.0 * pow(xi, 2.0);
    };

    virtual void ShapeFunctionsDerivatives2(ChMatrix<>& Ndd, double xi) {
        double l = this->GetRestLength();
        Ndd(0) = (12 * xi - 6) / pow(l, 2);
        Ndd(1) = (-4 + 6 * xi) / l;
        Ndd(2) = (6 - 12 * xi) / pow(l, 2);
        Ndd(3) = (-2 + 6 * xi) / l;
    };

    virtual void Update() override {
        // parent class update:
        ChElementGeneric::Update();
    };

    /// Fills the D vector (column matrix) with the current
    /// field values at the nodes of the element, with proper ordering.
    /// If the D vector has not the size of this->GetNdofs(), it will be resized.
    ///  {x_a y_a z_a Dx_a Dx_a Dx_a x_b y_b z_b Dx_b Dy_b Dz_b}
    virtual void GetStateBlock(ChMatrixDynamic<>& mD) override {
        mD.Reset(12, 1);

        mD.PasteVector(this->nodes[0]->GetPos(), 0, 0);
        mD.PasteVector(this->nodes[0]->GetD(), 3, 0);
        mD.PasteVector(this->nodes[1]->GetPos(), 6, 0);
        mD.PasteVector(this->nodes[1]->GetD(), 9, 0);
    }

    /// Computes the STIFFNESS MATRIX of the element:
    /// K = integral( .... ),
    /// Note: in this 'basic' implementation, constant section and
    /// constant material are assumed
    virtual void ComputeInternalJacobians(double Kfactor, double Rfactor) {
        assert(section);
        bool use_numerical_differentiation = true;  // Only option tested for now

        // Option: compute the stiffness matrix by doing a numerical differentiation
        // of the internal forces. This fixes a problem with the code by D.Melanz, that
        // produces a rank deficient matrix for straight beams.
        if (use_numerical_differentiation) {
            double diff = 1e-8;
            ChMatrixDynamic<> Kcolumn(12, 1);
            ChMatrixDynamic<> F0(12, 1);
            ChMatrixDynamic<> F1(12, 1);

            this->ComputeInternalForces(F0);

            // Create local copies of the nodal coordinates and use the implementation version
            // of the function for calculating the internal forces.  With this, the calculation
            // of the Jacobian with finite differences is thread safe (otherwise, there would
            // be race conditions when adjacent elements attempt to perturb a common node).
            ChVector<> pos[2] = {this->nodes[0]->pos, this->nodes[1]->pos};
            ChVector<> D[2] = {this->nodes[0]->D, this->nodes[1]->D};
            ChVector<> pos_dt[2] = {this->nodes[0]->pos_dt, this->nodes[1]->pos_dt};
            ChVector<> D_dt[2] = {this->nodes[0]->D_dt, this->nodes[1]->D_dt};

            // Add part of the Jacobian stemming from elastic forces
            for (int inode = 0; inode < 2; ++inode) {
                pos[inode].x() += diff;
                this->ComputeInternalForces_Impl(pos[0], D[0], pos[1], D[1], pos_dt[0], D_dt[0], pos_dt[1], D_dt[1],
                                                 F1);
                Kcolumn = (F0 - F1) * (1.0 / diff) * Kfactor;
                this->m_JacobianMatrix.PasteClippedMatrix(Kcolumn, 0, 0, 12, 1, 0, 0 + inode * 6);
                pos[inode].x() -= diff;

                pos[inode].y() += diff;
                this->ComputeInternalForces_Impl(pos[0], D[0], pos[1], D[1], pos_dt[0], D_dt[0], pos_dt[1], D_dt[1],
                                                 F1);
                Kcolumn = (F0 - F1) * (1.0 / diff) * Kfactor;
                this->m_JacobianMatrix.PasteClippedMatrix(Kcolumn, 0, 0, 12, 1, 0, 1 + inode * 6);
                pos[inode].y() -= diff;

                pos[inode].z() += diff;
                this->ComputeInternalForces_Impl(pos[0], D[0], pos[1], D[1], pos_dt[0], D_dt[0], pos_dt[1], D_dt[1],
                                                 F1);
                Kcolumn = (F0 - F1) * (1.0 / diff) * Kfactor;
                this->m_JacobianMatrix.PasteClippedMatrix(Kcolumn, 0, 0, 12, 1, 0, 2 + inode * 6);
                pos[inode].z() -= diff;

                D[inode].x() += diff;
                this->ComputeInternalForces_Impl(pos[0], D[0], pos[1], D[1], pos_dt[0], D_dt[0], pos_dt[1], D_dt[1],
                                                 F1);
                Kcolumn = (F0 - F1) * (1.0 / diff) * Kfactor;
                this->m_JacobianMatrix.PasteClippedMatrix(Kcolumn, 0, 0, 12, 1, 0, 3 + inode * 6);
                D[inode].x() -= diff;

                D[inode].y() += diff;
                this->ComputeInternalForces_Impl(pos[0], D[0], pos[1], D[1], pos_dt[0], D_dt[0], pos_dt[1], D_dt[1],
                                                 F1);
                Kcolumn = (F0 - F1) * (1.0 / diff) * Kfactor;
                this->m_JacobianMatrix.PasteClippedMatrix(Kcolumn, 0, 0, 12, 1, 0, 4 + inode * 6);
                D[inode].y() -= diff;

                D[inode].z() += diff;
                this->ComputeInternalForces_Impl(pos[0], D[0], pos[1], D[1], pos_dt[0], D_dt[0], pos_dt[1], D_dt[1],
                                                 F1);
                Kcolumn = (F0 - F1) * (1.0 / diff) * Kfactor;
                this->m_JacobianMatrix.PasteClippedMatrix(Kcolumn, 0, 0, 12, 1, 0, 5 + inode * 6);
                D[inode].z() -= diff;
            }

            // Add part of the Jacobian stemming from internal damping forces, if selected by user.
            if (m_use_damping) {
                for (int inode = 0; inode < 2; ++inode) {
                    pos_dt[inode].x() += diff;
                    this->ComputeInternalForces_Impl(pos[0], D[0], pos[1], D[1], pos_dt[0], D_dt[0], pos_dt[1], D_dt[1],
                                                     F1);
                    Kcolumn = (F0 - F1) * (1.0 / diff) * Rfactor;
                    this->m_JacobianMatrix.PasteClippedMatrix(Kcolumn, 0, 0, 12, 1, 0, 0 + inode * 6);
                    pos_dt[inode].x() -= diff;

                    pos_dt[inode].y() += diff;
                    this->ComputeInternalForces_Impl(pos[0], D[0], pos[1], D[1], pos_dt[0], D_dt[0], pos_dt[1], D_dt[1],
                                                     F1);
                    Kcolumn = (F0 - F1) * (1.0 / diff) * Rfactor;
                    this->m_JacobianMatrix.PasteClippedMatrix(Kcolumn, 0, 0, 12, 1, 0, 1 + inode * 6);
                    pos_dt[inode].y() -= diff;

                    pos_dt[inode].z() += diff;
                    this->ComputeInternalForces_Impl(pos[0], D[0], pos[1], D[1], pos_dt[0], D_dt[0], pos_dt[1], D_dt[1],
                                                     F1);
                    Kcolumn = (F0 - F1) * (1.0 / diff) * Rfactor;
                    this->m_JacobianMatrix.PasteClippedMatrix(Kcolumn, 0, 0, 12, 1, 0, 2 + inode * 6);
                    pos_dt[inode].z() -= diff;

                    D_dt[inode].x() += diff;
                    this->ComputeInternalForces_Impl(pos[0], D[0], pos[1], D[1], pos_dt[0], D_dt[0], pos_dt[1], D_dt[1],
                                                     F1);
                    Kcolumn = (F0 - F1) * (1.0 / diff) * Rfactor;
                    this->m_JacobianMatrix.PasteClippedMatrix(Kcolumn, 0, 0, 12, 1, 0, 3 + inode * 6);
                    D_dt[inode].x() -= diff;

                    D_dt[inode].y() += diff;
                    this->ComputeInternalForces_Impl(pos[0], D[0], pos[1], D[1], pos_dt[0], D_dt[0], pos_dt[1], D_dt[1],
                                                     F1);
                    Kcolumn = (F0 - F1) * (1.0 / diff) * Rfactor;
                    this->m_JacobianMatrix.PasteClippedMatrix(Kcolumn, 0, 0, 12, 1, 0, 4 + inode * 6);
                    D_dt[inode].y() -= diff;

                    D_dt[inode].z() += diff;
                    this->ComputeInternalForces_Impl(pos[0], D[0], pos[1], D[1], pos_dt[0], D_dt[0], pos_dt[1], D_dt[1],
                                                     F1);
                    Kcolumn = (F0 - F1) * (1.0 / diff) * Rfactor;
                    this->m_JacobianMatrix.PasteClippedMatrix(Kcolumn, 0, 0, 12, 1, 0, 5 + inode * 6);
                    D_dt[inode].z() -= diff;
                }
            }
        }

        else {
            // Option B: use the code in D.Melanz thesis. These formulas, however,
            // produce a rank deficient matrix for straight beams.
            //// TODO: Test it and include internal damping from strain rates.
            double Area = section->Area;
            double E = section->E;
            double I = section->I;

            double l = this->length;

            ChVector<> pA = this->nodes[0]->GetPos();
            ChVector<> dA = this->nodes[0]->GetD();
            ChVector<> pB = this->nodes[1]->GetPos();
            ChVector<> dB = this->nodes[1]->GetD();

            // this matrix will be used in both MyStiffnessAxial and MyStiffnessCurv integrators
            ChMatrixNM<double, 4, 3> d;
            d(0, 0) = pA.x();
            d(0, 1) = pA.y();
            d(0, 2) = pA.z();
            d(1, 0) = dA.x();
            d(1, 1) = dA.y();
            d(1, 2) = dA.z();
            d(2, 0) = pB.x();
            d(2, 1) = pB.y();
            d(2, 2) = pB.z();
            d(3, 0) = dB.x();
            d(3, 1) = dB.y();
            d(3, 2) = dB.z();

            /// 1)
            /// Integrate   ((strainD'*strainD)+(strain*Sd'*Sd))

            class MyStiffnessAxial : public ChIntegrable1D<ChMatrixNM<double, 12, 12> > {
              public:
                ChElementCableANCF* element;
                ChMatrixNM<double, 4, 3>* d;
                ChMatrixNM<double, 3, 12> Sd;
                ChMatrixNM<double, 1, 4> N;
                ChMatrixNM<double, 1, 4> Nd;
                ChMatrixNM<double, 1, 12> strainD;
                ChMatrixNM<double, 1, 1> strain;
                ChMatrixNM<double, 1, 3> Nd_d;
                ChMatrixNM<double, 12, 12> temp;

                /// Evaluate ((strainD'*strainD)+(strain*Sd'*Sd)) at point x
                virtual void Evaluate(ChMatrixNM<double, 12, 12>& result, const double x) {
                    element->ShapeFunctionsDerivatives(Nd, x);

                    // Sd=[Nd1*eye(3) Nd2*eye(3) Nd3*eye(3) Nd4*eye(3)]
                    ChMatrix33<> Sdi;
                    Sdi.FillDiag(Nd(0));
                    Sd.PasteMatrix(Sdi, 0, 0);
                    Sdi.FillDiag(Nd(1));
                    Sd.PasteMatrix(Sdi, 0, 3);
                    Sdi.FillDiag(Nd(2));
                    Sd.PasteMatrix(Sdi, 0, 6);
                    Sdi.FillDiag(Nd(3));
                    Sd.PasteMatrix(Sdi, 0, 9);

                    Nd_d = Nd * (*d);

                    strainD = Nd_d * Sd;

                    // strain = (Nd*(d*d')*Nd'-1)*0.5;

                    strain.MatrMultiplyT(Nd_d, Nd_d);
                    strain(0, 0) += -1;
                    strain(0, 0) *= 0.5;  // strain

                    // result:  ((strainD'*strainD)+(strain*Sd'*Sd))

                    result.MatrTMultiply(strainD, strainD);  //(strainD'*strainD)

                    temp.MatrTMultiply(Sd, Sd);  //(strain*Sd'*Sd)
                    temp *= strain(0, 0);
                    result += temp;
                }
            };

            MyStiffnessAxial myformulaAx;
            myformulaAx.d = &d;
            myformulaAx.element = this;

            ChMatrixNM<double, 12, 12> Kaxial;
            ChQuadrature::Integrate1D<ChMatrixNM<double, 12, 12> >(Kaxial,       // result of integration will go there
                                                                   myformulaAx,  // formula to integrate
                                                                   0,            // start of x
                                                                   1,            // end of x
                                                                   5             // order of integration
                                                                   );
            Kaxial *= E * Area * length;

            this->m_JacobianMatrix = Kaxial;

            /// 2)
            /// Integrate   (k_e'*k_e)

            class MyStiffnessCurv : public ChIntegrable1D<ChMatrixNM<double, 12, 12> > {
              public:
                ChElementCableANCF* element;
                ChMatrixNM<double, 4, 3>* d;
                ChMatrixNM<double, 3, 12> Sd;
                ChMatrixNM<double, 3, 12> Sdd;
                ChMatrixNM<double, 1, 4> Nd;
                ChMatrixNM<double, 1, 4> Ndd;

                ChMatrixNM<double, 1, 3> r_x;
                ChMatrixNM<double, 1, 3> r_xx;

                ChMatrixNM<double, 1, 12> g_e;
                ChMatrixNM<double, 1, 12> f_e;
                ChMatrixNM<double, 1, 12> k_e;
                ChMatrixNM<double, 3, 12> fe1;

                /// Evaluate  at point x
                virtual void Evaluate(ChMatrixNM<double, 12, 12>& result, const double x) {
                    element->ShapeFunctionsDerivatives(Nd, x);
                    element->ShapeFunctionsDerivatives2(Ndd, x);

                    // Sd=[Nd1*eye(3) Nd2*eye(3) Nd3*eye(3) Nd4*eye(3)]
                    // Sdd=[Ndd1*eye(3) Ndd2*eye(3) Ndd3*eye(3) Ndd4*eye(3)]
                    ChMatrix33<> Sdi;
                    Sdi.FillDiag(Nd(0));
                    Sd.PasteMatrix(Sdi, 0, 0);
                    Sdi.FillDiag(Nd(1));
                    Sd.PasteMatrix(Sdi, 0, 3);
                    Sdi.FillDiag(Nd(2));
                    Sd.PasteMatrix(Sdi, 0, 6);
                    Sdi.FillDiag(Nd(3));
                    Sd.PasteMatrix(Sdi, 0, 9);

                    Sdi.FillDiag(Ndd(0));
                    Sdd.PasteMatrix(Sdi, 0, 0);
                    Sdi.FillDiag(Ndd(1));
                    Sdd.PasteMatrix(Sdi, 0, 3);
                    Sdi.FillDiag(Ndd(2));
                    Sdd.PasteMatrix(Sdi, 0, 6);
                    Sdi.FillDiag(Ndd(3));
                    Sdd.PasteMatrix(Sdi, 0, 9);

                    r_x.MatrMultiply(Nd, *d);    // r_x=d'*Nd';  (transposed)
                    r_xx.MatrMultiply(Ndd, *d);  // r_xx=d'*Ndd';  (transposed)

                    // if (r_xx.Length()==0)
                    //     {r_xx(0)=0; r_xx(1)=1; r_xx(2)=0;}
                    ChVector<> vr_x(r_x(0), r_x(1), r_x(2));
                    ChVector<> vr_xx(r_xx(0), r_xx(1), r_xx(2));
                    ChVector<> vf1 = Vcross(vr_x, vr_xx);
                    double f = vf1.Length();
                    double g1 = vr_x.Length();
                    double g = pow(g1, 3);
                    double k = f / g;

                    g_e = (Nd * (*d)) * Sd;
                    g_e *= (3 * g1);

                    // do:  fe1=cross(Sd,r_xxrep)+cross(r_xrep,Sdd);
                    for (int col = 0; col < 12; ++col) {
                        ChVector<> Sd_i = Sd.ClipVector(0, col);
                        fe1.PasteVector(Vcross(Sd_i, vr_xx), 0, col);
                        ChVector<> Sdd_i = Sdd.ClipVector(0, col);
                        fe1.PasteSumVector(Vcross(vr_x, Sdd_i), 0, col);
                    }
                    ChMatrixNM<double, 3, 1> f1;
                    f1.PasteVector(vf1, 0, 0);

                    if (f == 0)
                        f_e.MatrTMultiply(f1, fe1);
                    else {
                        f_e.MatrTMultiply(f1, fe1);
                        f_e *= (1 / f);
                    }

                    k_e = (f_e * g - g_e * f) * (1 / (pow(g, 2)));

                    // result:  k_e'*k_e
                    result.MatrTMultiply(k_e, k_e);
                }
            };

            MyStiffnessCurv myformulaCurv;
            myformulaCurv.d = &d;
            myformulaCurv.element = this;

            ChMatrixNM<double, 12, 12> Kcurv;
            ChQuadrature::Integrate1D<ChMatrixNM<double, 12, 12> >(Kcurv,  // result of integration will go there
                                                                   myformulaCurv,  // formula to integrate
                                                                   0,              // start of x
                                                                   1,              // end of x
                                                                   3               // order of integration
                                                                   );
            Kcurv *= E * I * length;  // note Iyy should be the same value (circular section assumption)

            this->m_JacobianMatrix += Kcurv;
        }

        //***DEBUG***
        /*
        GetLog() << "Stiffness matr file dump. L=" << this->length << " A=" << this->section->Area << " E=" <<
        this->section->E << " I=" << this->section->Izz << "\n";
        GetLog() << this->StiffnessMatrix;
        ChStreamOutAsciiFile mdump("dump_stiff.txt");
        this->StiffnessMatrix.StreamOUTdenseMatlabFormat(mdump);
        */
    }

    /// Computes the MASS MATRIX of the element
    /// Note: in this 'basic' implementation, constant section and
    /// constant material are assumed
    virtual void ComputeMassMatrix() {
        assert(section);

        double Area = section->Area;
        double rho = section->density;

        /// Integrate  Area*rho*(S'*S)
        /// where S=[N1*eye(3) N2*eye(3) N3*eye(3) N4*eye(3)]

        class MyMass : public ChIntegrable1D<ChMatrixNM<double, 12, 12> > {
          public:
            ChElementCableANCF* element;
            ChMatrixNM<double, 3, 12> S;
            ChMatrixNM<double, 1, 4> N;

            /// Evaluate the S'*S  at point x
            virtual void Evaluate(ChMatrixNM<double, 12, 12>& result, const double x) {
                element->ShapeFunctions(N, x);
                // S=[N1*eye(3) N2*eye(3) N3*eye(3) N4*eye(3)]
                ChMatrix33<> Si;
                Si.FillDiag(N(0));
                S.PasteMatrix(Si, 0, 0);
                Si.FillDiag(N(1));
                S.PasteMatrix(Si, 0, 3);
                Si.FillDiag(N(2));
                S.PasteMatrix(Si, 0, 6);
                Si.FillDiag(N(3));
                S.PasteMatrix(Si, 0, 9);
                // perform  r = S'*S
                result.MatrTMultiply(S, S);
            }
        };

        MyMass myformula;
        myformula.element = this;

        ChQuadrature::Integrate1D<ChMatrixNM<double, 12, 12> >(
            this->m_MassMatrix,  // result of integration will go there
            myformula,           // formula to integrate
            0,                   // start of x
            1,                   // end of x
            4                    // order of integration
            );

        this->m_MassMatrix *= (rho * Area * this->length);
    }

    /// Setup. Precompute mass and matrices that do not change during the
    /// simulation, ex. the mass matrix in ANCF is constant

    virtual void SetupInitial(ChSystem* system) override {
        assert(section);

        // Compute rest length, mass:
        this->length = (nodes[1]->GetX0() - nodes[0]->GetX0()).Length();
        this->mass = this->length * this->section->Area * this->section->density;

        // Here we calculate the internal forces in the initial configuration
        // Contribution of initial configuration in elastic forces is automatically subtracted
        ChMatrixDynamic<> FVector0(12, 1);
        FVector0.FillElem(0.0);
        this->m_GenForceVec0.FillElem(0.0);
        ComputeInternalForces(FVector0);
        this->m_GenForceVec0 = FVector0;

        // Compute mass matrix
        ComputeMassMatrix();
    }

    /// Sets M as the global mass matrix.
    virtual void ComputeMmatrixGlobal(ChMatrix<>& M) override { M = m_MassMatrix; }

    /// Sets H as the global stiffness matrix K, scaled  by Kfactor. Optionally, also
    /// superimposes global damping matrix R, scaled by Rfactor, and global mass matrix M multiplied by Mfactor.
    virtual void ComputeKRMmatricesGlobal(ChMatrix<>& H, double Kfactor, double Rfactor = 0, double Mfactor = 0) override {
        assert((H.GetRows() == 12) && (H.GetColumns() == 12));
        assert(section);

        // Calculate the linear combination Kfactor*[K] + Rfactor*[R]
        ComputeInternalJacobians(Kfactor, Rfactor);

        // Load Jac + Mfactor*[M] into H
        for (int i = 0; i < 12; i++)
            for (int j = 0; j < 12; j++)
                H(i, j) = m_JacobianMatrix(i, j) + Mfactor * m_MassMatrix(i, j);
    }

    /// Computes the internal forces and set values in the Fi vector.
    /// (ex. the actual position of nodes is not in relaxed reference position).
    virtual void ComputeInternalForces(ChMatrixDynamic<>& Fi) override {
        ComputeInternalForces_Impl(this->nodes[0]->GetPos(), this->nodes[0]->GetD(), this->nodes[1]->GetPos(),
                                   this->nodes[1]->GetD(), this->nodes[0]->GetPos_dt(), this->nodes[0]->GetD_dt(),
                                   this->nodes[1]->GetPos_dt(), this->nodes[1]->GetD_dt(), Fi);
    }

    /// Worker function for computing the internal forces.
    /// This function takes the nodal coordinates as arguments and is therefore thread-safe.
    /// (Typically invoked by ComputeInternalForces. Used explicitly in the FD Jacobian approximation)
    void ComputeInternalForces_Impl(const ChVector<>& pA,
                                    const ChVector<>& dA,
                                    const ChVector<>& pB,
                                    const ChVector<>& dB,
                                    const ChVector<>& pA_dt,
                                    const ChVector<>& dA_dt,
                                    const ChVector<>& pB_dt,
                                    const ChVector<>& dB_dt,
                                    ChMatrixDynamic<>& Fi) {
        assert((Fi.GetRows() == 12) && (Fi.GetColumns() == 1));
        assert(section);

        double Area = section->Area;
        double E = section->E;
        double I = section->I;

        double l = this->length;

        // this matrix will be used in both MyForcesAxial and MyForcesCurv integrators
        ChMatrixNM<double, 4, 3> d;
        d(0, 0) = pA.x();
        d(0, 1) = pA.y();
        d(0, 2) = pA.z();
        d(1, 0) = dA.x();
        d(1, 1) = dA.y();
        d(1, 2) = dA.z();
        d(2, 0) = pB.x();
        d(2, 1) = pB.y();
        d(2, 2) = pB.z();
        d(3, 0) = dB.x();
        d(3, 1) = dB.y();
        d(3, 2) = dB.z();

        // this matrix will be used in both MyForcesAxial and MyForcesCurv integrators
        ChMatrixNM<double, 12, 1> vel_vector;
        vel_vector(0, 0) = pA_dt.x();
        vel_vector(1, 0) = pA_dt.y();
        vel_vector(2, 0) = pA_dt.z();
        vel_vector(3, 0) = dA_dt.x();
        vel_vector(4, 0) = dA_dt.y();
        vel_vector(5, 0) = dA_dt.z();
        vel_vector(6, 0) = pB_dt.x();
        vel_vector(7, 0) = pB_dt.y();
        vel_vector(8, 0) = pB_dt.z();
        vel_vector(9, 0) = dB_dt.x();
        vel_vector(10, 0) = dB_dt.y();
        vel_vector(11, 0) = dB_dt.z();
        /// 1)
        /// Integrate   (strainD'*strain)

        class MyForcesAxial : public ChIntegrable1D<ChMatrixNM<double, 12, 1> > {
          public:
            ChElementCableANCF* element;
            ChMatrixNM<double, 4, 3>* d;      // this is an external matrix, use pointer
            ChMatrixNM<double, 12, 1>* d_dt;  // this is an external matrix, use pointer
            ChMatrixNM<double, 3, 12> Sd;
            ChMatrixNM<double, 1, 4> N;
            ChMatrixNM<double, 1, 4> Nd;
            ChMatrixNM<double, 1, 12> strainD;
            ChMatrixNM<double, 1, 1> strain;
            ChMatrixNM<double, 1, 3> Nd_d;
            ChMatrixNM<double, 12, 12> temp;

            /// Evaluate (strainD'*strain)  at point x
            virtual void Evaluate(ChMatrixNM<double, 12, 1>& result, const double x) {
                element->ShapeFunctionsDerivatives(Nd, x);

                // Sd=[Nd1*eye(3) Nd2*eye(3) Nd3*eye(3) Nd4*eye(3)]
                ChMatrix33<> Sdi;
                Sdi.FillDiag(Nd(0));
                Sd.PasteMatrix(Sdi, 0, 0);
                Sdi.FillDiag(Nd(1));
                Sd.PasteMatrix(Sdi, 0, 3);
                Sdi.FillDiag(Nd(2));
                Sd.PasteMatrix(Sdi, 0, 6);
                Sdi.FillDiag(Nd(3));
                Sd.PasteMatrix(Sdi, 0, 9);

                Nd_d = Nd * (*d);

                strainD = Nd_d * Sd;

                // strain = (Nd*(d*d')*Nd'-1)*0.5;

                strain.MatrMultiplyT(Nd_d, Nd_d);
                strain(0, 0) += -1;
                strain(0, 0) *= 0.5;

                // Add damping forces if selected
                if (element->m_use_damping)
                    strain(0, 0) += (element->m_alpha) * (strainD * (*d_dt))(0, 0);

                result.MatrTMultiply(strainD, strain);
                // result:  strainD'*strain
            }
        };

        MyForcesAxial myformulaAx;
        myformulaAx.d = &d;
        myformulaAx.d_dt = &vel_vector;
        myformulaAx.element = this;

        ChMatrixNM<double, 12, 1> Faxial;
        ChQuadrature::Integrate1D<ChMatrixNM<double, 12, 1> >(Faxial,       // result of integration will go there
                                                              myformulaAx,  // formula to integrate
                                                              0,            // start of x
                                                              1,            // end of x
                                                              5             // order of integration
                                                              );
        Faxial *= -E * Area * length;

        Fi = Faxial;

        /// 2)
        /// Integrate   (k_e'*k_e)

        class MyForcesCurv : public ChIntegrable1D<ChMatrixNM<double, 12, 1> > {
          public:
            ChElementCableANCF* element;
            ChMatrixNM<double, 4, 3>* d;      // this is an external matrix, use pointer
            ChMatrixNM<double, 12, 1>* d_dt;  // this is an external matrix, use pointer
            ChMatrixNM<double, 3, 12> Sd;
            ChMatrixNM<double, 3, 12> Sdd;
            ChMatrixNM<double, 1, 4> Nd;
            ChMatrixNM<double, 1, 4> Ndd;
            ChMatrixNM<double, 1, 3> r_x;
            ChMatrixNM<double, 1, 3> r_xx;
            ChMatrixNM<double, 1, 12> g_e;
            ChMatrixNM<double, 1, 12> f_e;
            ChMatrixNM<double, 1, 12> k_e;
            ChMatrixNM<double, 3, 12> fe1;

            /// Evaluate  at point x
            virtual void Evaluate(ChMatrixNM<double, 12, 1>& result, const double x) {
                element->ShapeFunctionsDerivatives(Nd, x);
                element->ShapeFunctionsDerivatives2(Ndd, x);

                // Sd=[Nd1*eye(3) Nd2*eye(3) Nd3*eye(3) Nd4*eye(3)]
                // Sdd=[Ndd1*eye(3) Ndd2*eye(3) Ndd3*eye(3) Ndd4*eye(3)]
                ChMatrix33<> Sdi;
                Sdi.FillDiag(Nd(0));
                Sd.PasteMatrix(Sdi, 0, 0);
                Sdi.FillDiag(Nd(1));
                Sd.PasteMatrix(Sdi, 0, 3);
                Sdi.FillDiag(Nd(2));
                Sd.PasteMatrix(Sdi, 0, 6);
                Sdi.FillDiag(Nd(3));
                Sd.PasteMatrix(Sdi, 0, 9);

                Sdi.FillDiag(Ndd(0));
                Sdd.PasteMatrix(Sdi, 0, 0);
                Sdi.FillDiag(Ndd(1));
                Sdd.PasteMatrix(Sdi, 0, 3);
                Sdi.FillDiag(Ndd(2));
                Sdd.PasteMatrix(Sdi, 0, 6);
                Sdi.FillDiag(Ndd(3));
                Sdd.PasteMatrix(Sdi, 0, 9);

                r_x.MatrMultiply(Nd, (*d));    // r_x=d'*Nd';  (transposed)
                r_xx.MatrMultiply(Ndd, (*d));  // r_xx=d'*Ndd';  (transposed)

                // if (r_xx.Length()==0)
                //     {r_xx(0)=0; r_xx(1)=1; r_xx(2)=0;}
                ChVector<> vr_x(r_x(0), r_x(1), r_x(2));
                ChVector<> vr_xx(r_xx(0), r_xx(1), r_xx(2));
                ChVector<> vf1 = Vcross(vr_x, vr_xx);
                double f = vf1.Length();
                double g1 = vr_x.Length();
                double g = pow(g1, 3);
                double k = f / g;
                g_e = (Nd * (*d)) * Sd;
                g_e *= (3 * g1);

                // do:  fe1=cross(Sd,r_xxrep)+cross(r_xrep,Sdd);
                for (int col = 0; col < 12; ++col) {
                    ChVector<> Sd_i = Sd.ClipVector(0, col);
                    fe1.PasteVector(Vcross(Sd_i, vr_xx), 0, col);
                    ChVector<> Sdd_i = Sdd.ClipVector(0, col);
                    fe1.PasteSumVector(Vcross(vr_x, Sdd_i), 0, col);
                }
                ChMatrixNM<double, 3, 1> f1;
                f1.PasteVector(vf1, 0, 0);

                if (f == 0)
                    f_e.MatrTMultiply(f1, fe1);
                else {
                    f_e.MatrTMultiply(f1, fe1);
                    f_e *= (1 / f);
                }

                k_e = (f_e * g - g_e * f) * (1 / (pow(g, 2)));

                // result:  k_e'*k
                result.CopyFromMatrixT(k_e);

                // Add damping if selected by user: curvature rate
                if (element->m_use_damping)
                    k += (element->m_alpha) * (k_e * (*d_dt))(0, 0);

                result *= k;
            }
        };

        MyForcesCurv myformulaCurv;
        myformulaCurv.d = &d;
        myformulaCurv.d_dt = &vel_vector;
        myformulaCurv.element = this;

        ChMatrixNM<double, 12, 1> Fcurv;
        ChQuadrature::Integrate1D<ChMatrixNM<double, 12, 1> >(Fcurv,          // result of integration will go there
                                                              myformulaCurv,  // formula to integrate
                                                              0,              // start of x
                                                              1,              // end of x
                                                              3               // order of integration
                                                              );
        Fcurv *= -E * I * length;  // note Iyy should be the same value (circular section assumption)

        Fi += Fcurv;

        // Substract contribution of initial configuration
        Fi -= this->m_GenForceVec0;
    }

    //
    // Beam-specific functions
    //

    /// Gets the xyz displacement of a point on the beam line,
    /// and the rotation RxRyRz of section plane, at abscyssa 'eta'.
    /// Note, eta=-1 at node1, eta=+1 at node2.
    /// Note, 'displ' is the displ.state of 2 nodes, ex. get it as GetStateBlock()
    /// Results are not corotated.
    virtual void EvaluateSectionDisplacement(const double eta,
                                             const ChMatrix<>& displ,
                                             ChVector<>& u_displ,
                                             ChVector<>& u_rotaz) override {
        ChMatrixNM<double, 1, 4> N;

        double xi = (eta + 1.0) * 0.5;

        this->ShapeFunctions(N, xi);  // because ShapeFunctions() works in 0..1 range

        u_displ = VNULL;  //(not needed in ANCF? )
        u_rotaz = VNULL;  //(not needed in ANCF? )
    }

    /// Gets the absolute xyz position of a point on the beam line,
    /// and the absolute rotation of section plane, at abscyssa 'eta'.
    /// Note, eta=-1 at node1, eta=+1 at node2.
    /// Note, 'displ' is the displ.state of 2 nodes, ex. get it as GetStateBlock()
    /// Results are corotated (expressed in world reference)
    virtual void EvaluateSectionFrame(const double eta,
                                      const ChMatrix<>& displ,
                                      ChVector<>& point,
                                      ChQuaternion<>& rot) override {
        ChVector<> u_displ;
        ChVector<> u_rotaz;

        ChMatrixNM<double, 1, 4> N;

        double xi = (eta + 1.0) * 0.5;  // because ShapeFunctions() works in 0..1 range

        this->ShapeFunctions(N, xi);

        ChVector<> pA = this->nodes[0]->GetPos();
        ChVector<> dA = this->nodes[0]->GetD();
        ChVector<> pB = this->nodes[1]->GetPos();
        ChVector<> dB = this->nodes[1]->GetD();

        point.x() = N(0) * pA.x() + N(1) * dA.x() + N(2) * pB.x() + N(3) * dB.x();
        point.y() = N(0) * pA.y() + N(1) * dA.y() + N(2) * pB.y() + N(3) * dB.y();
        point.z() = N(0) * pA.z() + N(1) * dA.z() + N(2) * pB.z() + N(3) * dB.z();

        this->ShapeFunctionsDerivatives(N, xi);

        ChVector<> Dx;

        Dx.x() = N(0) * pA.x() + N(1) * dA.x() + N(2) * pB.x() + N(3) * dB.x();
        Dx.y() = N(0) * pA.y() + N(1) * dA.y() + N(2) * pB.y() + N(3) * dB.y();
        Dx.z() = N(0) * pA.z() + N(1) * dA.z() + N(2) * pB.z() + N(3) * dB.z();

        // This element has no torsional dof, so once we have the Dx direction
        // of the line, we must compute the Dy and Dz directions by using a
        // Gram-Schmidt orthonormalization , where we propose a guess direction
        // VECT_Y for the vertical:
        ChMatrix33<> msect;
        Dx.Normalize();
        msect.Set_A_Xdir(Dx, VECT_Y);

        rot = msect.Get_A_quaternion();
    }

    /// Gets the force (traction x, shear y, shear z) and the
    /// torque (torsion on x, bending on y, on bending on z) at a section along
    /// the beam line, at abscyssa 'eta'.
    /// Note, eta=-1 at node1, eta=+1 at node2.
    /// Note, 'displ' is the displ.state of 2 nodes, ex. get it as GetStateBlock().
    /// Results are not corotated, and are expressed in the reference system of beam.
    /// This is not mandatory for the element to work, but it can be useful for plotting,
    /// showing results, etc.
    virtual void EvaluateSectionForceTorque(const double eta,
                                            const ChMatrix<>& displ,
                                            ChVector<>& Fforce,
                                            ChVector<>& Mtorque) override {
        assert(section);

        ChMatrixNM<double, 1, 4> N;
        ChMatrixNM<double, 1, 4> Nd;
        ChMatrixNM<double, 1, 4> Ndd;
        // double xi = (eta*2 - 1.0);
        double xi = (eta + 1.0) / 2.0;

        /* To be completed*/
    }
    /// Gets the axial and bending strain of the ANCF element
    /// torque (torsion on x, bending on y, on bending on z) at a section along
    /// the beam line, at abscyssa 'eta'.
    /// Note, eta=-1 at node1, eta=+1 at node2.
    /// Note, 'displ' is the displ.state of 2 nodes, ex. get it as GetStateBlock().
    /// Results are not corotated, and are expressed in the reference system of beam.
    /// This is not mandatory for the element to work, but it can be useful for plotting,
    /// showing results, etc.
    virtual void EvaluateSectionStrain(const double eta, const ChMatrix<>& displ, ChVector<>& StrainV) override {
        assert(section);

        ChMatrixNM<double, 1, 4> N;
        ChMatrixNM<double, 1, 4> Nd;
        ChMatrixNM<double, 1, 4> Ndd;
        // double xi = (eta*2 - 1.0);
        double xi = (eta + 1.0) / 2.0;

        this->ShapeFunctions(N, xi);  // Evaluate shape functions
        this->ShapeFunctionsDerivatives(Nd, xi);
        this->ShapeFunctionsDerivatives2(Ndd, xi);
        ChMatrixDynamic<> mD(GetNdofs(), 1);
        ChMatrix33<> Sdi;
        ChMatrixNM<double, 3, 12> Sd;
        ChMatrixNM<double, 3, 12> Sdd;
        ChMatrixNM<double, 3, 1> r_x;
        ChMatrixNM<double, 3, 1> r_xx;

        this->GetStateBlock(mD);
        Sdi.FillDiag(Nd(0));
        Sd.PasteMatrix(Sdi, 0, 0);
        Sdi.FillDiag(Nd(1));
        Sd.PasteMatrix(Sdi, 0, 3);
        Sdi.FillDiag(Nd(2));
        Sd.PasteMatrix(Sdi, 0, 6);
        Sdi.FillDiag(Nd(3));
        Sd.PasteMatrix(Sdi, 0, 9);
        Sdi.Reset();
        Sdi.FillDiag(Ndd(0));
        Sdd.PasteMatrix(Sdi, 0, 0);
        Sdi.FillDiag(Ndd(1));
        Sdd.PasteMatrix(Sdi, 0, 3);
        Sdi.FillDiag(Ndd(2));
        Sdd.PasteMatrix(Sdi, 0, 6);
        Sdi.FillDiag(Ndd(3));
        Sdd.PasteMatrix(Sdi, 0, 9);

        r_x.MatrMultiply(Sd, mD);  // r_x=d'*Nd';  (transposed)
        r_xx.MatrMultiply(Sdd, mD);

        ChVector<> vr_x(r_x(0), r_x(1), r_x(2));
        ChVector<> vr_xx(r_xx(0), r_xx(1), r_xx(2));
        ChVector<> vf1 = Vcross(vr_x, vr_xx);
        double f = vf1.Length();
        double g1 = vr_x.Length();
        double g = pow(g1, 3);

        StrainV.x() = (pow(r_x(0), 2) + pow(r_x(1), 2) + pow(r_x(2), 2) - 1.0);
        StrainV.y() = f / g;  // Bending strain measure (Gertmayer and Shabana, 2006)
    }

    /// Set structural damping.
    void SetAlphaDamp(double a) {
        m_alpha = a;
        if (std::abs(m_alpha) > 1e-10)
            m_use_damping = true;
    }

    //
    // Functions for interfacing to the solver
    //            (***not needed, thank to bookkeeping in parent class ChElementGeneric)

    //
    // Functions for ChLoadable interface
    //

    /// Gets the number of DOFs affected by this element (position part)
    virtual int LoadableGet_ndof_x() override { return 2 * 6; }

    /// Gets the number of DOFs affected by this element (speed part)
    virtual int LoadableGet_ndof_w() override { return 2 * 6; }

    /// Gets all the DOFs packed in a single vector (position part)
    virtual void LoadableGetStateBlock_x(int block_offset, ChVectorDynamic<>& mD) override {
        mD.PasteVector(this->nodes[0]->GetPos(), block_offset, 0);
        mD.PasteVector(this->nodes[0]->GetD(), block_offset + 3, 0);
        mD.PasteVector(this->nodes[1]->GetPos(), block_offset + 6, 0);
        mD.PasteVector(this->nodes[1]->GetD(), block_offset + 9, 0);
    }

    /// Gets all the DOFs packed in a single vector (speed part)
    virtual void LoadableGetStateBlock_w(int block_offset, ChVectorDynamic<>& mD) override {
        mD.PasteVector(this->nodes[0]->GetPos_dt(), block_offset, 0);
        mD.PasteVector(this->nodes[0]->GetD_dt(), block_offset + 3, 0);
        mD.PasteVector(this->nodes[1]->GetPos_dt(), block_offset + 6, 0);
        mD.PasteVector(this->nodes[1]->GetD_dt(), block_offset + 9, 0);
    }

    /// Number of coordinates in the interpolated field, ex=3 for a
    /// tetrahedron finite element or a cable, = 1 for a thermal problem, etc.
    virtual int Get_field_ncoords() override { return 6; }

    /// Tell the number of DOFs blocks (ex. =1 for a body, =4 for a tetrahedron, etc.)
    virtual int GetSubBlocks() override { return 2; }

    /// Get the offset of the i-th sub-block of DOFs in global vector
    virtual unsigned int GetSubBlockOffset(int nblock) override { return nodes[nblock]->NodeGetOffset_w(); }

    /// Get the size of the i-th sub-block of DOFs in global vector
    virtual unsigned int GetSubBlockSize(int nblock) override { return 6; }

    /// Get the pointers to the contained ChVariables, appending to the mvars vector.
    virtual void LoadableGetVariables(std::vector<ChVariables*>& mvars) override {
        mvars.push_back(&this->nodes[0]->Variables());
        mvars.push_back(&this->nodes[0]->Variables_D());
        mvars.push_back(&this->nodes[1]->Variables());
        mvars.push_back(&this->nodes[1]->Variables_D());
    };

    /// Evaluate N'*F , where N is some type of shape function
    /// evaluated at U,V coordinates of the surface, each ranging in -1..+1
    /// F is a load, N'*F is the resulting generalized load
    /// Returns also det[J] with J=[dx/du,..], that might be useful in gauss quadrature.
    virtual void ComputeNF(const double U,              ///< parametric coordinate in line
                           ChVectorDynamic<>& Qi,       ///< Return result of Q = N'*F  here
                           double& detJ,                ///< Return det[J] here
                           const ChVectorDynamic<>& F,  ///< Input F vector, size is =n. field coords.
                           ChVectorDynamic<>* state_x,  ///< if != 0, update state (pos. part) to this, then evaluate Q
                           ChVectorDynamic<>* state_w   ///< if != 0, update state (speed part) to this, then evaluate Q
                           ) override {
        ChMatrixNM<double, 1, 4> N;
        this->ShapeFunctions(
            N, (U + 1) * 0.5);  // evaluate shape functions (in compressed vector), btw. not dependant on state

        detJ = this->GetRestLength() / 2.0;

        ChVector<> tmp;
        ChVector<> Fv = F.ClipVector(0, 0);
        tmp = N(0) * Fv;
        Qi.PasteVector(tmp, 0, 0);
        tmp = N(1) * Fv;
        Qi.PasteVector(tmp, 3, 0);
        tmp = N(2) * Fv;
        Qi.PasteVector(tmp, 6, 0);
        tmp = N(3) * Fv;
        Qi.PasteVector(tmp, 9, 0);
    }

    /// Evaluate N'*F , where N is some type of shape function
    /// evaluated at U,V,W coordinates of the volume, each ranging in -1..+1
    /// F is a load, N'*F is the resulting generalized load
    /// Returns also det[J] with J=[dx/du,..], that might be useful in gauss quadrature.
    virtual void ComputeNF(const double U,              ///< parametric coordinate in volume
                           const double V,              ///< parametric coordinate in volume
                           const double W,              ///< parametric coordinate in volume
                           ChVectorDynamic<>& Qi,       ///< Return result of N'*F  here, maybe with offset block_offset
                           double& detJ,                ///< Return det[J] here
                           const ChVectorDynamic<>& F,  ///< Input F vector, size is = n.field coords.
                           ChVectorDynamic<>* state_x,  ///< if != 0, update state (pos. part) to this, then evaluate Q
                           ChVectorDynamic<>* state_w   ///< if != 0, update state (speed part) to this, then evaluate Q
                           ) override {
        this->ComputeNF(U, Qi, detJ, F, state_x, state_w);
        detJ /= 4.0;  // because volume
    }

    /// This is needed so that it can be accessed by ChLoaderVolumeGravity
    virtual double GetDensity() override { return this->section->Area * this->section->density; }
};

/// @} fea_elements

}  // end namespace fea
}  // end namespace chrono

#endif
