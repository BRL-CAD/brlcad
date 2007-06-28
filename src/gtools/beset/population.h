#define GEO_SPHERE 1

struct individual {
    point_t p;
    fastf_t r;
    char id[256];
    int type;
    fastf_t fitness;

    /* for raytracing */
    double gs[2]; //grid spacing
};

struct population {
    struct individual *individual;
    struct individual *offspring;
    int size;
};

void pop_init (struct population **p, int size);
void pop_spawn(struct population *p, struct rt_wdb *db_fp);
void pop_clean(struct population *p);
int  pop_wrand_ind(struct individual *i, int size, fastf_t total_fitness);
int  pop_wrand_gop(void);
fastf_t pop_rand(void);

