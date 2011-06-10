#include "osl-renderer.h"

using namespace OpenImageIO;

static std::string outputfile = "image.png";
static int w = 1024, h = 768;
static int samples = 4;
static unsigned short Xi[3];

inline double clamp(double x) { return x<0 ? 0 : x>1 ? 1 : x; }

struct Ray {
    Vec3 o, d;
	
    Ray(Vec3 o_, Vec3 d_)
	: o(o_), d(d_) {}
};

typedef Imath::Vec3<double> Vec3d;

struct Sphere {
    double radius;
    Vec3d p;
    ShadingAttribStateRef shaderstate;
    const char *shadername;
    bool light;

    Sphere(double radius_, Vec3d p_, const char *shadername_, bool light_)
	: radius(radius_), p(p_), shadername(shadername_), light(light_) {}

    // returns distance, 0 if no hit
    double intersect(const Ray &r) const
    {
        // Solve t^2*d.d + 2*t*(o-p).d +(o-p).(o-p)-R^2 = 0
        Vec3d op = p - Vec3d(r.o.x, r.o.y, r.o.z);
        double t, eps = 1e-4;
        double b = op.dot(Vec3(r.d.x, r.d.y, r.d.z));
        double det = b*b - op.dot(op) + radius*radius;

        if(det<0) return 0;
        else {
            det=sqrt(det);
            return(t=b-det) > eps ? t :((t=b+det)>eps ? t : 0);
        }
    }

    Vec3 normal(const Vec3& P)
    {
        return Vec3(P[0] - p[0], P[1] - p[1], P[2] - p[2]).normalized();
    }
};

// Scene(modified cornell box): radius, position, shader name
Sphere spheres[] = {
    Sphere(1e5,  Vec3d(1e5+1, 40.8, 81.6),   "cornell_wall", false), // left
    Sphere(1e5,  Vec3d(-1e5+99, 40.8, 81.6), "cornell_wall", false), // right
    Sphere(1e5,  Vec3d(50, 40.8, 1e5),       "cornell_wall", false), // back
    Sphere(1e5,  Vec3d(50, 1e5, 81.6),       "cornell_wall", false), // front
    Sphere(1e5,  Vec3d(50, -1e5+81.6, 81.6), "cornell_wall", false), // bottom
    Sphere(1e5,  Vec3d(50, 40.8, -1e5+170),  "cornell_wall", false), // top
    Sphere(16.5, Vec3d(27, 16.5, 47),        "mirror",       false), // left ball
    Sphere(16.5, Vec3d(73, 16.5, 78),        "glass",        false), // right ball
    Sphere(600,  Vec3d(50, 681.6-1.27, 81.6),"emitter",      true)   // light
};

int numSpheres = sizeof(spheres)/sizeof(Sphere);

inline bool intersect(const Ray &r, double &t, int &id)
{
    double d, inf=t=1e20;

    for(int i=sizeof(spheres)/sizeof(Sphere); i--;)
        if((d=spheres[i].intersect(r))&&d<t) {
            t=d;
            id=i;
        }

    return t<inf;
}

void write_image(float *buffer, int w, int h)
{
    // write image using OIIO
    ImageOutput *out = ImageOutput::create(outputfile);
    ImageSpec spec(w, h, 3, TypeDesc::UINT8);

    out->open(outputfile, spec);
    out->write_image(TypeDesc::FLOAT, buffer);

    out->close();
    delete out;
}


int main (){

    OSLRenderer oslr;

    // camera parameters
    Vec3 cam_o = Vec3(50, 52, 295.6); // positions
    Vec3 cam_d = Vec3(0, -0.042612, -1).normalized(); // direction
    Vec3 cam_x = Vec3(w*.5135/h, 0.0, 0.0); // ???
    Vec3 cam_y = (cam_x.cross(cam_d)).normalized()*.5135; // ???

    // pixels
    Color3 *buffer = new Color3[w*h]; 
    for(int i=0; i<w*h; i++)
        buffer[i] = Color3(0.0f);

    float scale = 10.0f;
    int samps = 1; // number of samples

    for(int y=0; y<h; y++) {
        
        fprintf(stderr, "\rRendering (%d spp) %5.2f%%", samps*4, 100.*y/(h-1));

        Xi[0] = 0;
        Xi[1] = 0;
        Xi[2] = y*y*y;

        for(int x=0; x<w; x++) {
            
            int i = (h - y - 1)*w + x;

            // 2x2 subixel with tent filter
            for(int sy = 0; sy < 2; sy++) {
                for(int sx = 0; sx < 2; sx++) {
                    Vec3 r(0.0f);
                    
                    for(int s=0; s<samps; s++) {
                        double r1 = 2*erand48(Xi);
                        double r2 = 2*erand48(Xi);
						
                        double dx = (r1 < 1)? sqrt(r1)-1: 1-sqrt(2-r1);
                        double dy = (r2 < 1)? sqrt(r2)-1: 1-sqrt(2-r2);

                        Vec3 d = cam_x*(((sx+.5 + dx)/2 + x)/w - .5) +
                            cam_y*(((sy+.5 + dy)/2 + y)/h - .5) + cam_d;

                        Ray ray(cam_o + d*130, d.normalized());
                        
                        // -----------------------------------------
                        // -           Fill in RenderInfo          -
                        // -----------------------------------------
                        RenderInfo info;

                        info.screen_x = x;
                        info.screen_y = y;

                        // find the sphere that was hit
                        double t; // distance to intersection
                        int id = 0; // id of the sphere
                        if(!intersect(ray, t, id)) continue;
                        if(id != 6 && id != 7) continue;

                        Sphere &obj = spheres[id];

                        Vec3 P = ray.o + ray.d*t;
                        info.P[0] = P[0];
                        info.P[1] = P[1];
                        info.P[2] = P[2];
                        
                        Vec3 N = obj.normal(P);
                        info.N[0] = N[0];
                        info.N[1] = N[1];
                        info.N[2] = N[2];

                        float phi = atan2(P.y, P.x);
                        if(phi < 0.) phi += 2*M_PI;
                        info.u = phi/(2*M_PI);
                        info.v = acos(P.z/obj.radius)/M_PI;

                        // tangents
                        float invzr = 1./sqrt(P.x*P.x + P.y*P.y);
                        float cosphi = P.x*invzr;
                        float sinphi = P.y*invzr;
                        
                        Vec3 dPdu = Vec3(-2*M_PI*P.y, -2*M_PI*P.x, 0.);
                        Vec3 dPdv = Vec3(P.z*cosphi, P.z*sinphi,
                                         -obj.radius*sin(info.v*M_PI))*M_PI;

                        info.dPdu[0] = dPdu[0];
                        info.dPdu[1] = dPdu[1];
                        info.dPdu[2] = dPdu[2];

                        info.dPdv[0] = dPdv[0];
                        info.dPdv[1] = dPdv[1];
                        info.dPdv[2] = dPdv[2];


                        info.depth = 0;
                        info.isbackfacing = 0;
                        info.surfacearea = 1.0;
                        
                        r = r + oslr.QueryColor(&info)*(1.0f/samps);
                    }

                    // normalize
                    r = Vec3(clamp(r.x),
                             clamp(r.y),
                             clamp(r.z));
                    buffer[i] += r*scale*0.25f;
                }
            }
            // gamma correction
            buffer[i] = Vec3(pow(buffer[i].x, 1.0/2.2),
                             pow(buffer[i].y, 1.0/2.2), 
                             pow(buffer[i].z, 1.0/2.2));
        }
    }

    write_image((float*)buffer, w, h);
    delete buffer;
    
    return 0;
}
