//   ______  _______   _______   ______  __
//  /      ||       \ |   ____| /      ||  |
// |  ,----'|  .--.  ||  |__   |  ,----'|  |
// |  |     |  |  |  ||   __|  |  |     |  |
// |  `----.|  '--'  ||  |     |  `----.|  |
//  \______||_______/ |__|      \______||__|
//
// Coordinate Descent Full Configuration Interaction (CDFCI) package in C++17
// https://github.com/quan-tum/CDFCI
//
// Copyright (c) 2019-2025, CDFCI Developers and Contributors
// All rights reserved.
//
// This source code is licensed under the BSD 3-Clause License found in the
// LICENSE file in the root directory of this source tree.
//
// Get a good initialization for Hubbard 4x4 half filled.

#include "../include/determinant.h"
#include "../include/hamiltonian.h"
#include "../include/wavefunction.h"

#include <Eigen/Dense>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

void save_txt(const Eigen::MatrixXd& A, const std::string& fname) {
    std::ofstream out(fname);
    out << std::setprecision(17);
    for (int i = 0; i < A.rows(); ++i) {
        for (int j = 0; j < A.cols(); ++j) {
            out << A(i,j);
            if (j + 1 < A.cols()) out << " ";
        }
        out << "\n";
    }
}



void save_vector_txt(const Eigen::VectorXd& v,
                     const std::string& fname)
{
    std::ofstream out(fname);
    out << std::setprecision(17);
    for (int i = 0; i < v.size(); ++i) {
        out << v(i) << "\n";
    }
}


int main(int argc, char *argv[])
{
    int norb = 72;
    const int N = 2;
    double U = 4.0;

    if (argc > 1) {
        U = std::atof(argv[1]);
    }

    HamiltonianHubbardK<N> hamiltonian;
    std::vector<int> lattice = {6, 6};
    hamiltonian.init_HamiltonianHubbardK(1.0, U, lattice, 36, 0);
    std::vector<double> orbital_energy = hamiltonian.orbital_energy;

    // Base determinant has 13 orbitals occupied.
    Determinant<N> base_determinant;
    std::vector<int> alpha_orbitals;
    std::vector<int> beta_orbitals;
    for (int i = 0; i < norb; ++i)
    {
        if (orbital_energy[i] < -1e-8)
            base_determinant.set_orbital(i);
        else if (fabs(orbital_energy[i]) < 1e-8)
        {
            if (i % 2 == 0)
                alpha_orbitals.push_back(i);
            else
                beta_orbitals.push_back(i);
        }
    }
    auto orbitals = base_determinant.get_occupied_orbitals();
    std::cout << "Occupied orbitals: ";
    for (auto orb : orbitals)
        std::cout << orb << " ";
    std::cout << std::endl;
    std::cout << "Alpha orbitals at zero energy: ";
    for (auto orb : alpha_orbitals)
        std::cout << orb << " ";
    std::cout << std::endl;
    std::cout << "Beta orbitals at zero energy: ";
    for (auto orb : beta_orbitals)
        std::cout << orb << " ";
    std::cout << std::endl;

    // Fill in the alpha and beta orbitals with 10 electrons, while keeping spin = 0.
    std::vector<Determinant<N>> determinants;
    for (int i_alpha = 0; i_alpha < alpha_orbitals.size(); i_alpha++)
    for (int i_beta = 0; i_beta < beta_orbitals.size(); i_beta++)
    for (int j_alpha = i_alpha + 1; j_alpha < alpha_orbitals.size(); j_alpha++)
    for (int j_beta = i_beta + 1; j_beta < beta_orbitals.size(); j_beta++)
    for (int k_alpha = j_alpha + 1; k_alpha < alpha_orbitals.size(); k_alpha++)
    for (int k_beta = j_beta + 1; k_beta < beta_orbitals.size(); k_beta++)
    for (int m_alpha = k_alpha + 1; m_alpha < alpha_orbitals.size(); m_alpha++)
    for (int m_beta = k_beta + 1; m_beta < beta_orbitals.size(); m_beta++)
    for (int n_alpha = m_alpha + 1; n_alpha < alpha_orbitals.size(); n_alpha++)
    for (int n_beta = m_beta + 1; n_beta < beta_orbitals.size(); n_beta++)
    {
        Determinant<N> det = base_determinant;
        det.set_orbital(alpha_orbitals[i_alpha]);
        det.set_orbital(beta_orbitals[i_beta]);
        det.set_orbital(alpha_orbitals[j_alpha]);
        det.set_orbital(beta_orbitals[j_beta]);
        det.set_orbital(alpha_orbitals[k_alpha]);
        det.set_orbital(beta_orbitals[k_beta]);
        det.set_orbital(alpha_orbitals[m_alpha]);
        det.set_orbital(beta_orbitals[m_beta]);
        det.set_orbital(alpha_orbitals[n_alpha]);
        det.set_orbital(beta_orbitals[n_beta]);
        determinants.push_back(det);
    }

    std::cout << "Total determinants generated: " << determinants.size() << std::endl;

    // Fill in the matrix A
    const int n = 252 * 252;
    // Eigen::MatrixXd A(n, n);
    // for (int i = 0; i < n; ++i)
    // {
    //     for (int j = 0; j < n; ++j)
    //     {
    //         Determinant<N> det_i = determinants[i];
    //         Determinant<N> det_j = determinants[j];
    //         // Compute Hamiltonian matrix element <det_i | H | det_j>
    //         A(i, j) = hamiltonian.get_entry(det_i, det_j);
    //     }
    // }

    // save_txt(A, "hubbard_4x4_half_filled_Hamiltonian_matrix.txt");

    // Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(A);
    // if (es.info() != Eigen::Success) {
    //     std::cerr << "eigensolver failed\n";
    //     return 1;
    // }

    // double lambda_min = es.eigenvalues()(0);
    // std::cout << "lambda_min = " << lambda_min << "\n";

    // Pushing everything to wavefunction now
    WaveFunction<ContainerRobinhood<Determinant<N>, std::array<NumericalType, 2>,
        DeterminantHash<N>, DeterminantEqual<N>>, 1> wavefunction;
    for (int i = 0; i < n; ++i)
    {
        Determinant<N> det = determinants[i];
        NumericalType coeff = 1.0/sqrt(n);
        // NumericalType coeff = es.eigenvectors().col(0)(i);
        wavefunction.update_x(det, coeff);
    }

    wavefunction.dump_wavefunction("hubbard_4x4_half_filled_noinit.dat");
    // Eigen::VectorXd v0 = es.eigenvectors().col(0);
    // save_vector_txt(v0, "eigvec_min.txt");
    return 0;
}

