      program brlcad

c     This is really a pointer to a BRL-CAD data structure.  With the advent
c     of FORTRAN-90 perhaps it should be delcared as such.
      integer*8 rtip

c     flag: did frtree fail to read the geometry?
      integer*8 fail

c     The number of hit locations we want returned to us
      integer*8 nloc

c     These next 4 are for the values we get when we shoot a ray
      REAL*8  indist(3)
      REAL*8  outdist(3)
      integer*8 region_ids(3)

c     we need six double-precision floating point values for context
c     for each possible result.  Since we are accepting 3 partitions
c     (the dimension of the arrays above) we need 3*6 = 18 values
c     for our context array.
c
c     Historically, this was done this way.  It can also be done as a
c     common block.  With the advent of FORTRAN-90 it could even be made
c     to resemble the C data structure that is actually being represented:
c
c     struct context {
c	double		co_vpriv[3];
c	struct soltab	*co_stp;
c	char		*co_priv;
c	int		co_inflip;
c     };

      REAL*8  context(18)

c     The ray origin and direction
      REAL*8  pt(3)
      REAL*8  dir(3)

c     Open a BRL-CAD geometry file called 'model.g' for reading
c     The "7" is the length of the string 'model.g'
      call frdir(rtip, 'model.g', 7)
      if (rtip .eq. 0) then
         stop
      end if


c     From the BRL-CAD geometry file, extract the geometry for the
c     object called 'all.g' (including everything that goes into making it)
c     The 5 is the length of the string 'all.g'
      call frtree(fail, rtip, 'all.g', 5)
      if (fail .ne. 0) then
         stop
      end if

c     Prepare the geometry for ray-tracing
c     This allows the library to pre-compute certain often-used values,
c     and prepare the geometry for raytracing
      call frprep(rtip)

c     Now we are going to shoot rays.
c     for this example we will shoot only 2 rays
c     First we set the point for the ray origin
c     and the direction for the ray.
      pt(1) = -.50
      pt(2) = .50
      pt(3) = .50
      dir(1) = 1.0
      dir(2) = 0.0
      dir(3) = 0.0

c     nloc indicates how many "partitions" we want to compute
c     and are prepared to handle.
c     A partition consists of both an inhit and an outhit
      nloc = 3;

c     shoot the ray
      call frshot(nloc, indist, outdist, region_ids, context, 
     1 rtip, pt, dir)

c     look to see how many things we hit and print them
      print *, 'number of partitions=',nloc
      if (nloc .gt. 0) then
         do i=1,nloc
            print *,indist(i),' --- ',outdist(i)
         end do
      end if

c     change directions and shoot a different ray from the same origin
      dir(1) = 0.0
      dir(2) = 0.0
      dir(3) = 1.0

      nloc = 3;

c     shoot the ray
      call frshot(nloc, indist, outdist, region_ids, context, 
     1 rtip, pt, dir)

c     look to see how many things we hit and print them
      print *, 'number of partitions=',nloc
      if (nloc .gt. 0) then
         do i=1,nloc
            print *,indist(i),' --- ',outdist(i)
         end do
      end if

      stop
      end
