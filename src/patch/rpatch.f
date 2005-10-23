c	Front end to patch.
c	Bill Mermagen Jr.
c     This pre-processor program alters the data file format
c     for use by the main conversion program.  An End-of-File
c     flag is used in the read statement to prevent loss of data
c     and provide a clean exit from this processing loop.

       program miniconv

       real x,y,z,hold,work
       character minus*1

       integer ity,ity1,ico, isq(8), m, n, k, cc, tmp

30    read(*,100,END=75) x,y,z,tmp,cc,(isq(k),k=1,8),m,n
       minus = '+'

c      Set volume mode / plate mode flag

       if (tmp.lt.0) then
          tmp = abs(tmp)
          minus = '-'
       end if

       hold = real(tmp/10000.0)

c     Isolate solid type code

       work = hold*10.0
       ity = int(work)
       hold = work-ity
c     New line added per Bob Strausser (Survice Engineering):
       if( ity.eq.4 ) ity = 8

c     Isolate thickness / radius value (1/100's inch)

       work = hold*100.0
       ico = int(work)
       hold = work-ico

c     Isolate space type code

       work = hold*10.0
       ity1 = int(work)
       hold = work-ity1

c     Write output file to new format for the conversion program

       write(*,101) x,y,z,minus,ity,ico,ity1,cc,(isq(k),k=1,8),m,n

c     Read next element code line

       go to 30

  75   continue
       stop

 100   format(f8.3,f8.3,f9.3,i6,i4,i11,7i4,2i3)
 101   format(f8.3,' ',f8.3,' ',f9.3,' ',a1,' ',i2,' ',i2,' ',i1,' ',
     1 i4,' ',i11,' ',7i5,' ',i3,' ',i3)
 200   format(f7.4,f7.4)
       end
