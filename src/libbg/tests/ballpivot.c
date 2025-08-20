/*                     B A L L P I V O T . C
 * BRL-CAD
 *
 * Published in 2025 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file ballpivot.c
 *
 * Test for bg_3d_ballpivot: Generates a dense point cloud on a sphere,
 * runs the ball pivoting surface reconstruction, and compares the output
 * mesh surface area to the analytical sphere area.
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "bu.h"
#include "vmath.h"
#include "bg.h"

#define SPHERE_RADIUS 10.0
#define BALL_RADIUS   2.0
#define AREA_TOL      0.10 /* 10% area tolerance for mesh approximation */

/* Uniformly sample points on a sphere surface using the golden section spiral */
static void
generate_sphere_points(point_t *points, vect_t *normals, int n)
{
    double offset = 2.0 / n;
    double increment = M_PI * (3.0 - sqrt(5.0)); /* golden angle */
    for (int i = 0; i < n; ++i) {
	double y = ((i * offset) - 1.0) + (offset / 2.0);
	double r = sqrt(1.0 - y * y);
	double phi = i * increment;
	double x = cos(phi) * r;
	double z = sin(phi) * r;
	VSET(points[i], SPHERE_RADIUS * x, SPHERE_RADIUS * y, SPHERE_RADIUS * z);
	VSET(normals[i], x, y, z); // Outward normal
    }
}

/* Compute triangle area given 3 points */
static double
triangle_area(point_t *a, point_t *b, point_t *c)
{
    vect_t ab, ac, cross;
    VSUB2(ab, *b, *a);
    VSUB2(ac, *c, *a);
    VCROSS(cross, ab, ac);
    return 0.5 * MAGNITUDE(cross);
}

/* Compute total mesh surface area from vertices and face indices */
static double
mesh_surface_area(point_t *verts, const int *faces, int num_faces)
{
    double area = 0.0;
    for (int i = 0; i < num_faces; ++i) {
	point_t *a = &verts[faces[i*3+0]];
	point_t *b = &verts[faces[i*3+1]];
	point_t *c = &verts[faces[i*3+2]];
	area += triangle_area(a, b, c);
    }
    return area;
}

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);

    /* Choose density: spacing < BALL_RADIUS / sqrt(2) */
    double spacing = BALL_RADIUS / sqrt(2.0) * 0.9;
    double sphere_area = 4.0 * M_PI * SPHERE_RADIUS * SPHERE_RADIUS;
    int num_points = (int)(sphere_area / (spacing * spacing));

    /* Generate points and normals */
    point_t *points = (point_t *)bu_calloc(num_points, sizeof(point_t), "sphere points");
    vect_t *normals = (vect_t *)bu_calloc(num_points, sizeof(vect_t), "sphere normals");
    generate_sphere_points(points, normals, num_points);

    /* Run ball pivot */
    int *faces = NULL;
    int num_faces = 0;
    point_t *verts = NULL;
    int num_verts = 0;
    const double radii[] = {BALL_RADIUS};
    int result = bg_3d_ballpivot(&faces, &num_faces, &verts, &num_verts,
				 (const point_t *)points, (const vect_t *)normals, num_points, radii, 1);

    bu_log("Test #001:  Ball Pivot on Sphere:\n");
    bu_log("  Input points: %d\n", num_points);
    bu_log("  Output vertices: %d\n", num_verts);
    bu_log("  Output faces: %d\n", num_faces);

    if (result != 0 || num_faces == 0 || num_verts == 0) {
	bu_log("  Ball pivoting failed or returned empty mesh.\n");
	bu_free(points, "sphere points");
	bu_free(normals, "sphere normals");
	return -1;
    }

    /* Compute mesh area and compare */
    double mesh_area = mesh_surface_area(verts, faces, num_faces);
    bu_log("  Analytical sphere area: %.6f\n", sphere_area);
    bu_log("  Mesh surface area:      %.6f\n", mesh_area);
    double area_err = fabs(mesh_area - sphere_area) / sphere_area;
    bu_log("  Area error:             %.2f%%\n", area_err * 100.0);

    if (area_err > AREA_TOL) {
	bu_log("  Error: mesh surface area deviates more than %.2f%% from analytical value.\n", AREA_TOL * 100.0);
	bu_free(points, "sphere points");
	bu_free(normals, "sphere normals");
	bu_free(faces, "output faces");
	bu_free(verts, "output verts");
	return -2;
    }

    bu_log("Ball Pivot Sphere Test Passed!\n");

    bu_free(points, "sphere points");
    bu_free(normals, "sphere normals");
    bu_free(faces, "output faces");
    bu_free(verts, "output verts");
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
