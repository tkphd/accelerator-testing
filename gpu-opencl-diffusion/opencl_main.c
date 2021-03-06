/**********************************************************************************
 HiPerC: High Performance Computing Strategies for Boundary Value Problems
 Written by Trevor Keller and available from https://github.com/usnistgov/hiperc
 **********************************************************************************/

/**
 \file	opencl_main.c
 \brief OpenCL implementation of semi-infinite diffusion equation
*/

/* system includes */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* common includes */
#include "boundaries.h"
#include "mesh.h"
#include "numerics.h"
#include "output.h"
#include "timer.h"

/* specific includes */
#include "opencl_data.h"

/**
 \brief Run simulation using input parameters specified on the command line
*/
int main(int argc, char* argv[])
{
	FILE* output;
	struct OpenCLData dev;

	/* declare default mesh size and resolution */
	fp_t** conc_old, **conc_new, **conc_lap, **mask_lap;
	int bx=32, by=32, nx=512, ny=512, nm=3, code=53;
	fp_t dx=0.5, dy=0.5, h;

	/* declare default materials and numerical parameters */
	fp_t D=0.00625, linStab=0.1, dt=1., elapsed=0., rss=0.;
	int step=0, steps=100000, checks=10000;
	double start_time=0.;
	struct Stopwatch watch = {0., 0., 0., 0.};

	StartTimer();

	param_parser(argc, argv, &bx, &by, &checks, &code, &D, &dx, &dy, &linStab, &nm, &nx, &ny, &steps);

	h = (dx > dy) ? dy : dx;
	dt = (linStab * h * h) / (4.0 * D);

	/* initialize memory */
	make_arrays(&conc_old, &conc_new, &conc_lap, &mask_lap, nx, ny, nm);
	set_mask(dx, dy, code, mask_lap, nm);

	print_progress(step, steps);

	start_time = GetTimer();
	apply_initial_conditions(conc_old, nx, ny, nm);
	watch.step = GetTimer() - start_time;

	/* initialize GPU */
	init_opencl(conc_old, mask_lap, nx, ny, nm, &dev);

	/* write initial condition data */
	start_time = GetTimer();
	write_png(conc_old, nx, ny, 0);

	/* prepare to log comparison to analytical solution */
	output = fopen("runlog.csv", "w");
	if (output == NULL) {
		printf("Error: unable to open %s for output. Check permissions.\n", "runlog.csv");
		exit(-1);
	}
	watch.file = GetTimer() - start_time;

	fprintf(output, "iter,sim_time,wrss,conv_time,step_time,IO_time,soln_time,run_time\n");
	fprintf(output, "%i,%f,%f,%f,%f,%f,%f,%f\n", step, elapsed, rss, watch.conv, watch.step, watch.file, watch.soln, GetTimer());
	fflush(output);

	/* Note: block is equivalent to a typical
	   for (int step=1; step < steps+1; step++),
	   1-indexed so as not to overwrite the initial condition image,
	   but the loop-internals are farmed out to a coprocessor.
	   So we use a while loop instead.
	 */

	/* do the work */
	for (step = 1; step < steps+1; step++) {
		print_progress(step, steps);
		const int flip = (step % 2 == 0)? 0 : 1;

		/* === Start Architecture-Specific Kernel === */
		device_boundaries(&dev, flip, nx, ny, nm, bx, by);

		start_time = GetTimer();
		device_convolution(&dev, flip, nx, ny, nm, bx, by);
		watch.conv += GetTimer() - start_time;

		start_time = GetTimer();
		device_diffusion(&dev, flip, nx, ny, nm, bx, by, D, dt);
		watch.conv += GetTimer() - start_time;
		/* === Finish Architecture-Specific Kernel === */

		elapsed += dt;

		if (step % checks == 0) {
			/* transfer result to host (conc_new) from device (dev.conc_old) */
			start_time = GetTimer();
			read_out_result(&dev, flip, conc_new, nx, ny);
			watch.file += GetTimer() - start_time;

			start_time = GetTimer();
			write_png(conc_new, nx, ny, step);
			watch.file += GetTimer() - start_time;

			start_time = GetTimer();
			check_solution(conc_new, conc_lap, nx, ny, dx, dy, nm, elapsed, D, &rss);
			watch.soln += GetTimer() - start_time;

			fprintf(output, "%i,%f,%f,%f,%f,%f,%f,%f\n", step, elapsed, rss,
			        watch.conv, watch.step, watch.file, watch.soln, GetTimer());
			fflush(output);
		}
	}

	write_csv(conc_new, nx, ny, dx, dy, steps);

	/* clean up */
	fclose(output);
	free_arrays(conc_old, conc_new, conc_lap, mask_lap);
	free_opencl(&dev);

	return 0;
}
