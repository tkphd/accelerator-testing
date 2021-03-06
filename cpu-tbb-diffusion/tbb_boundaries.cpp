/**********************************************************************************
 HiPerC: High Performance Computing Strategies for Boundary Value Problems
 Written by Trevor Keller and available from https://github.com/usnistgov/hiperc
 **********************************************************************************/

/**
 \file  tbb_boundaries.cpp
 \brief Implementation of boundary condition functions with TBB threading
*/

#include <math.h>
#include <tbb/tbb.h>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>
#include <tbb/blocked_range2d.h>
#include "boundaries.h"

void apply_initial_conditions(fp_t** conc, const int nx, const int ny, const int nm)
{
	/* Lambda function executed on each thread, applying flat field values */
	tbb::parallel_for(tbb::blocked_range2d<int>(0, nx, 0, ny),
		[=](const tbb::blocked_range2d<int>& r) {
			for (int j = r.cols().begin(); j != r.cols().end(); j++) {
				for (int i = r.rows().begin(); i != r.rows().end(); i++) {
					conc[j][i] = 0.;
				}
			}
		}
	);

	/* Lambda function executed on each thread, applying left boundary values */
	tbb::parallel_for(tbb::blocked_range2d<int>(0, 1+nm/2, 0, ny/2),
		[=](const tbb::blocked_range2d<int>& r) {
			for (int j = r.cols().begin(); j != r.cols().end(); j++) {
				for (int i = r.rows().begin(); i != r.rows().end(); i++) {
					conc[j][i] = 1.;
				}
			}
		}
	);

	/* Lambda function executed on each thread, applying right boundary values */
	tbb::parallel_for(tbb::blocked_range2d<int>(nx-1-nm/2, nx, ny/2, ny),
		[=](const tbb::blocked_range2d<int>& r) {
			for (int j = r.cols().begin(); j != r.cols().end(); j++) {
				for (int i = r.rows().begin(); i != r.rows().end(); i++) {
					conc[j][i] = 1.;
				}
			}
		}
	);
}

void apply_boundary_conditions(fp_t** conc, const int nx, const int ny, const int nm)
{
	/* apply fixed boundary values: sequence does not matter */

	/* Lambda function executed on each thread, applying left boundary values */
	tbb::parallel_for(tbb::blocked_range2d<int>(0, 1+nm/2, 0, ny/2),
		[=](const tbb::blocked_range2d<int>& r) {
			for (int j = r.cols().begin(); j != r.cols().end(); j++) {
				for (int i = r.rows().begin(); i != r.rows().end(); i++) {
					conc[j][i] = 1.;
				}
			}
		}
	);

	/* Lambda function executed on each thread, applying right boundary values */
	tbb::parallel_for(tbb::blocked_range2d<int>(nx-1-nm/2, nx, ny/2, ny),
		[=](const tbb::blocked_range2d<int>& r) {
			for (int j = r.cols().begin(); j != r.cols().end(); j++) {
				for (int i = r.rows().begin(); i != r.rows().end(); i++) {
					conc[j][i] = 1.;
				}
			}
		}
	);

	/* apply no-flux boundary conditions: inside to out, sequence matters */

	for (int offset = 0; offset < nm/2; offset++) {
		const int ilo = nm/2 - offset;
		const int ihi = nx - 1 - nm/2 + offset;
		/* Lambda function executed on each thread, applying x-axis boundary condition */
		tbb::parallel_for(tbb::blocked_range<int>(0, ny),
			[=](const tbb::blocked_range<int>& r) {
				for (int j = r.begin(); j != r.end(); j++) {
					conc[j][ilo-1] = conc[j][ilo]; /* left */
					conc[j][ihi+1] = conc[j][ihi]; /* right */
				}
			}
		);
	}

	for (int offset = 0; offset < nm/2; offset++) {
		const int jlo = nm/2 - offset;
		const int jhi = ny - 1 - nm/2 + offset;
		/* Lambda function executed on each thread, applying y-axis boundary condition */
		tbb::parallel_for(tbb::blocked_range<int>(0, nx),
			[=](const tbb::blocked_range<int>& r) {
				for (int i = r.begin(); i != r.end(); i++) {
					conc[jlo-1][i] = conc[jlo][i]; /* bottom */
					conc[jhi+1][i] = conc[jhi][i]; /* top */
				}
			}
		);
	}
}
