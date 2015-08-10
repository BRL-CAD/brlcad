#define MAX_SPHERES 10


typedef struct
{
    float3 position;
    float radius;
    int is_light;
} Sphere;


__kernel void ray_trace(__global int16 *output, __constant Sphere *gspheres)
{
    __local Sphere spheres[MAX_SPHERES];

    spheres[get_global_id(0)] = gspheres[get_global_id(0)];
    barrier(CLK_LOCAL_MEM_FENCE);

    if (get_global_id(0) == 0) {
	for (int i = 0; i < MAX_SPHERES; ++i)
	printf("is_light: %d\n", spheres[i].is_light);
	printf("%d\n", get_local_size(0));
    }
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
