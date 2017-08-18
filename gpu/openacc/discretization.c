/*
	File: discretization.c
	Role: implementation of discretized mathematical operations with OpenMP threading and OpenACC acceleration

	Questions/comments to trevor.keller@nist.gov
	Bugs/requests to https://github.com/usnistgov/phasefield-accelerator-benchmarks
*/

#include <math.h>
#include <omp.h>
#include <openacc.h>

#include "diffusion.h"

void set_threads(int n)
{
	omp_set_num_threads(n);
}

void five_point_Laplacian_stencil(fp_t dx, fp_t dy, fp_t** M)
{
	M[0][1] =  1. / (dy * dy); /* up */
	M[1][0] =  1. / (dx * dx); /* left */
	M[1][1] = -2. * (dx*dx + dy*dy) / (dx*dx * dy*dy); /* middle */
	M[1][2] =  1. / (dx * dx); /* right */
	M[2][1] =  1. / (dy * dy); /* down */
}

void nine_point_Laplacian_stencil(fp_t dx, fp_t dy, fp_t** M)
{
	M[0][0] =   1. / (6. * dx * dy);
	M[0][1] =   4. / (6. * dy * dy);
	M[0][2] =   1. / (6. * dx * dy);

	M[1][0] =   4. / (6. * dx * dx);
	M[1][1] = -10. * (dx*dx + dy*dy) / (6. * dx*dx * dy*dy);
	M[1][2] =   4. / (6. * dx * dx);

	M[2][0] =   1. / (6. * dx * dy);
	M[2][1] =   4. / (6. * dy * dy);
	M[2][2] =   1. / (6. * dx * dy);
}

void set_mask(fp_t dx, fp_t dy, int nm, fp_t** M)
{
	five_point_Laplacian_stencil(dx, dy, M);
}

void compute_convolution(fp_t** A, fp_t** C, fp_t** M, int nx, int ny, int nm, int bs)
{
	#pragma acc data copyin(A[0:ny][0:nx], M[0:nm][0:nm]) copyout(C[0:ny][0:nx])
	{
		#pragma acc parallel
		{
			int i, j, mi, mj;
			fp_t value;

			#pragma acc loop
			for (j = nm/2; j < ny-nm/2; j++) {
				#pragma acc loop
				for (i = nm/2; i < nx-nm/2; i++) {
					value = 0.;
					for (mj = -nm/2; mj < 1+nm/2; mj++) {
						for (mi = -nm/2; mi < 1+nm/2; mi++) {
							value += M[mj+nm/2][mi+nm/2] * A[j+mj][i+mi];
						}
					}
					C[j][i] = value;
				}
			}
		}
	}
}

void solve_diffusion_equation(fp_t** A, fp_t** B, fp_t** C,
                              int nx, int ny, int nm, int bs,
                              fp_t D, fp_t dt, fp_t* elapsed)
{
	#pragma acc data copyin(A[0:ny][0:nx], C[0:ny][0:nx]) copyout(B[0:ny][0:nx])
	{
		#pragma acc parallel
		{
			int i, j;

			#pragma acc loop
			for (j = nm/2; j < ny-nm/2; j++) {
				#pragma acc loop
				for (i = nm/2; i < nx-nm/2; i++) {
					B[j][i] = A[j][i] + dt * D * C[j][i];
				}
			}
		}
	}

	*elapsed += dt;
}

void check_solution(fp_t** A,
                    int nx, int ny, fp_t dx, fp_t dy, int nm, int bs,
                    fp_t elapsed, fp_t D, fp_t bc[2][2], fp_t* rss)
{
	/* OpenCL does not have a GPU-based erf() definition, using Maclaurin series approximation */
	fp_t sum=0.;
	#pragma acc data copyin(A[0:ny][0:nx], bc[0:2][0:2]) copy(sum)
	{
		#pragma acc parallel reduction(+:sum)
		{
			int i, j;
			fp_t ca, cal, car, cn, poly_erf, r, trss, z, z2;

			#pragma acc loop
			for (j = nm/2; j < ny-nm/2; j++) {
				#pragma acc loop
				for (i = nm/2; i < nx-nm/2; i++) {
					/* numerical solution */
					cn = A[j][i];

					/* shortest distance to left-wall source */
					r = (j < ny/2) ? dx * (i - nm/2) : sqrt(dx*dx * (i - nm/2) * (i - nm/2) + dy*dy * (j - ny/2) * (j - ny/2));
					z = r / sqrt(4. * D * elapsed);
					z2 = z * z;
					poly_erf = (z < 1.5) ? 2. * z * (1. + z2 * (-1./3 + z2 * (1./10 + z2 * (-1./42 + z2 / 216)))) / sqrt(M_PI) : 1.;
					cal = bc[1][0] * (1. - poly_erf);

					/* shortest distance to right-wall source */
					r = (j >= ny/2) ? dx * (nx-nm+1 - i) : sqrt(dx*dx * (nx-nm+1 - i)*(nx-nm+1 - i) + dy*dy * (ny/2 - j)*(ny/2 - j));
					z = r / sqrt(4. * D * elapsed);
					z2 = z * z;
					poly_erf = (z < 1.5) ? 2. * z * (1. + z2 * (-1./3 + z2 * (1./10 + z2 * (-1./42 + z2 / 216)))) / sqrt(M_PI) : 1.;
					car = bc[1][0] * (1. - poly_erf);

					/* superposition of analytical solutions */
					ca = cal + car;

					/* residual sum of squares (RSS) */
					trss = (ca - cn) * (ca - cn) / (fp_t)((nx-nm+1) * (ny-nm+1));
					sum += trss;
				}
			}
		}
	}

	*rss = sum;
}