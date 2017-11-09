/**********************************************************************************
 HiPerC: High Performance Computing Strategies for Boundary Value Problems
 written by Trevor Keller and available from https://github.com/usnistgov/hiperc

 This software was developed at the National Institute of Standards and Technology
 by employees of the Federal Government in the course of their official duties.
 Pursuant to title 17 section 105 of the United States Code this software is not
 subject to copyright protection and is in the public domain. NIST assumes no
 responsibility whatsoever for the use of this software by other parties, and makes
 no guarantees, expressed or implied, about its quality, reliability, or any other
 characteristic. We would appreciate acknowledgement if the software is used.

 This software can be redistributed and/or modified freely provided that any
 derivative works bear some notice that they are derived from it, and any modified
 versions bear some notice that they have been modified.

 Questions/comments to Trevor Keller (trevor.keller@nist.gov)
 **********************************************************************************/

/**
 \file  cuda_boundaries.cu
 \brief Implementation of boundary condition functions with OpenMP threading
*/

#include <math.h>
#include <omp.h>

extern "C" {
#include "boundaries.h"
}

#include "cuda_kernels.cuh"

__constant__ fp_t d_bc[2][2];

void set_boundaries(fp_t bc[2][2])
{
	/* Change these values to your liking: */
	fp_t clo = 0.0, chi = 1.0;

	bc[0][0] = clo; /* bottom boundary */
	bc[0][1] = clo; /* top boundary */
	bc[1][0] = chi; /* left boundary */
	bc[1][1] = chi; /* right boundary */
}

void apply_initial_conditions(fp_t** conc, const int nx, const int ny, const int nm, fp_t bc[2][2])
{
	#pragma omp parallel
	{
		#pragma omp for collapse(2)
		for (int j = 0; j < ny; j++)
			for (int i = 0; i < nx; i++)
				conc[j][i] = bc[0][0];

		#pragma omp for collapse(2)
		for (int j = 0; j < ny/2; j++)
			for (int i = 0; i < 1+nm/2; i++)
				conc[j][i] = bc[1][0]; /* left half-wall */

		#pragma omp for collapse(2)
		for (int j = ny/2; j < ny; j++)
			for (int i = nx-1-nm/2; i < nx; i++)
				conc[j][i] = bc[1][1]; /* right half-wall */
	}
}

__global__ void boundary_kernel(fp_t* d_conc,
                                const int nx,
                                const int ny,
                                const int nm)
{
	/* determine indices on which to operate */
	const int tx = threadIdx.x;
	const int ty = threadIdx.y;

	const int row = blockDim.y * blockIdx.y + ty;
	const int col = blockDim.x * blockIdx.x + tx;

	/* apply fixed boundary values: sequence does not matter */

	if (row < ny/2 && col < 1+nm/2) {
		d_conc[row * nx + col] = d_bc[1][0]; /* left value */
	}

	if (row >= ny/2 && row < ny && col >= nx-1-nm/2 && col < nx) {
		d_conc[row * nx + col] = d_bc[1][1]; /* right value */
	}

	/* wait for all threads to finish writing */
	__syncthreads();

	/* apply no-flux boundary conditions: inside to out, sequence matters */

	for (int offset = 0; offset < nm/2; offset++) {
		const int ilo = nm/2 - offset;
		const int ihi = nx - 1 - nm/2 + offset;
		const int jlo = nm/2 - offset;
		const int jhi = ny - 1 - nm/2 + offset;

		if (ilo-1 == col && row < ny) {
			d_conc[row * nx + ilo-1] = d_conc[row * nx + ilo]; /* left condition */
		}
		if (ihi+1 == col && row < ny) {
			d_conc[row * nx + ihi+1] = d_conc[row * nx + ihi]; /* right condition */
		}
		if (jlo-1 == row && col < nx) {
			d_conc[(jlo-1) * nx + col] = d_conc[jlo * nx + col]; /* bottom condition */
		}
		if (jhi+1 == row && col < nx) {
			d_conc[(jhi+1) * nx + col] = d_conc[jhi * nx + col]; /* top condition */
		}

		__syncthreads();
	}
}
