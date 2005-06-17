#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <zutil.h>
#include <math.h>
#include <string.h>
#include "ADRT/ADRT.h"
/***************************************************
 * Protocol Request Codes:
 * 000			 	- Request Image
 ***************************************************
 * Protocol Transmit Codes:
 * 000FORMAT.WIDTH.HEIGHT	- Send Image
 ***************************************************/

#define	AD_PORT		4096

#define	WIDTH		320
#define	HEIGHT		240


int main(int argc, char *args[]) {
  struct	sockaddr_in	server,client;
  unsigned	char		*imgbufin,*imgbufout;
  unsigned	long		complen;
  socklen_t			addrlen;
  int				buflen, create_socket, new_socket;
  short				code;
  float				cam[10];


  if ((create_socket= socket(AF_INET,SOCK_STREAM,0)) > 0)
    printf("The socket was created\n");

  server.sin_family= AF_INET;
  server.sin_addr.s_addr= INADDR_ANY;
  server.sin_port= htons(AD_PORT);

  addrlen= sizeof(struct sockaddr_in);
  if (bind(create_socket,(struct sockaddr*)&server,addrlen) == 0) {
    printf("Binding Socket\n");
  } else {
    printf("Socket already bound\n");
    exit(0);
  }

  listen(create_socket,3);
  new_socket= accept(create_socket,(struct sockaddr*)&client,&addrlen);
  if (new_socket > 0)
    printf("The client %s is connected...\n",inet_ntoa(client.sin_addr));


  ADRT_Init(WIDTH,HEIGHT);
  ADRT_RPP(1);
  ADRT_Threads(2);

/*
  RT -> Geometry("TRIANGLE -2.0 -2.0 0.0 2.0 -2.0 0.0 -2.0 2.0 0.0");
  RT -> Geometry("MATERIAL 0.8 0.0 0.0 0.0 0.0 0.0 0 ADRT_RI_GLASS");

  RT -> Geometry("TRIANGLE 2.0 -2.0 0.0 2.0 2.0 0.0 -2.0 2.0 0.0");
  RT -> Geometry("MATERIAL 0.0 0.8 0.0 0.0 0.0 0.0 0.0 ADRT_RI_GLASS");

  RT -> Geometry("TRIANGLE -2.0 2.0 0.0 2.0 2.0 0.0 2.0 2.0 4.0");
  RT -> Material(0.0,0.0,0.8,0.0,0.0,0.0,0,ADRT_RI_GLASS);

  RT -> Geometry("TRIANGLE -2.0 2.0 0.0 -2.0 2.0 4.0 2.0 2.0 4.0");
  RT -> Material(0.8,0.0,0.0,0.0,0.0,0.0,0,ADRT_RI_GLASS);

  RT -> Geometry("TRIANGLE -2.0 -2.0 0.0 2.0 -2.0 0.0 2.0 -2.0 4.0");
  RT -> Material(0.0,0.0,0.8,0.0,0.0,0.0,0,ADRT_RI_GLASS);

  RT -> Geometry("TRIANGLE -2.0 -2.0 0.0 -2.0 -2.0 4.0 2.0 -2.0 4.0");
  RT -> Material(0.8,0.0,0.0,0.0,0.0,0.0,0,ADRT_RI_GLASS);
*/



/*
  RT -> Geometry("SPHERE 1.0 0.0 0.0 2.0");
  RT -> Material(0.2,0.2,0.2,0.1,0.1,0.1,1.33,ADRT_RI_GLASS);

  // Floor
  RT -> Geometry("AABOX -5.0 -5.0 -0.1 5.0 5.0 0.0");
  RT -> Material(0.8,0.8,0.8,0.75,0.75,0.75,0,ADRT_RI_GLASS);

  RT -> Geometry("SPHERE 1.5 0.0 0.0 2.0");
  RT -> Material(0.2,0.2,0.2,0.1,0.1,0.1,1.33,ADRT_RI_GLASS);

  RT -> Geometry("AABOX -3.0 -3.0 0.0 -2.0 -2.0 2.0");
  RT -> Material(1.0,0.0,0.0,0.0,0.0,0.0,0,ADRT_RI_GLASS);

  RT -> Geometry("AABOX -3.0 -3.0 0.0 -2.0 -2.0 2.0");
  RT -> Material(1.0,0.0,0.0,0.0,0.0,0.0,0,ADRT_RI_GLASS);

  RT -> Geometry("AABOX 2.0 2.0 0.0 3.0 3.0 2.0");
  RT -> Material(0.0,1.0,0.0,0.0,0.0,0.0,0,ADRT_RI_GLASS);

  RT -> Geometry("AABOX -3.0 2.0 0.0 -2.0 3.0 2.0");
  RT -> Material(0.0,0.0,1.0,0.0,0.0,0.0,0,ADRT_RI_GLASS);

  RT -> Geometry("AABOX 2.0 -3.0 0.0 3.0 -2.0 2.0");
  RT -> Material(1.0,0.0,1.0,0.0,0.0,0.0,0,ADRT_RI_GLASS);
*/
  // Lighting
//  RT -> Geometry("SPHERE 0.5 4.0 -4.0 4.0");
//  RT -> Geometry("LIGHT 90.0 90.0 90.0");

//  RT -> Purge(ADRT_FAST);

  /* Setup Default Camera */
  cam[0]= 0;
  cam[1]= -10;
  cam[2]= 2;
  cam[3]= 0;
  cam[4]= 1;
  cam[5]= 0;
  cam[6]= 0;
  cam[7]= 0;
  cam[8]= 1;
  cam[9]= 90;

  imgbufin= (unsigned char*)malloc(sizeof(unsigned char)*(WIDTH*HEIGHT*3+64));
  imgbufout= (unsigned char*)malloc(sizeof(unsigned char)*(WIDTH*HEIGHT*3+64));

  do {
    buflen= recv(new_socket, &code, 2, 0);

    switch(code) {
      case 0:
/*        printf("image request\n");*/
        ADRT_Purge(ADRT_FAST);
        ADRT_Camera(cam[0], cam[1], cam[2]+cos(2*(cam[0]+cam[1]))/40.0, cam[0]+cam[3], cam[1]+cam[4], cam[2]+cam[5]+cos(2*(cam[0]+cam[1]))/40.0, cam[6], cam[7], cam[8], 35);
        ADRT_Render();

        memcpy(imgbufin,ADRT_FB_Get(),WIDTH*HEIGHT*3);
        complen= WIDTH*HEIGHT*3+64;
        compress(imgbufout, &complen, imgbufin, WIDTH*HEIGHT*3);
//        printf("size: %d\n",complen);


        {
          unsigned	char	ImageData[10];
          short			S1;
          int			I1;

          S1= 0;
          memcpy(&ImageData[0],&S1,2);
          S1= WIDTH;
          memcpy(&ImageData[2],&S1,2);
          S1= HEIGHT;
          memcpy(&ImageData[4],&S1,2);
          I1= complen;
          memcpy(&ImageData[6],&I1,4);
          send(new_socket, ImageData, 10, 0);
        }
        send(new_socket, imgbufout, complen, 0);
        break;

      case 1:
        {
          short	Key;
          buflen= recv(new_socket, &Key, 2, 0);
          switch(Key) {
            case SDLK_LEFT:
              cam[9]= cam[9] + 5 > 360 ? ((int)cam[9]+5)%360: cam[9] + 5;
              cam[3]= cos(cam[9]*ADM_PI/180.0);
              cam[4]= sin(cam[9]*ADM_PI/180.0);
              break;

            case SDLK_RIGHT:
              cam[9]= cam[9] - 5 < 0 ? ((int)cam[9]-5)%360: cam[9] - 5;
              cam[3]= cos(cam[9]*ADM_PI/180.0);
              cam[4]= sin(cam[9]*ADM_PI/180.0);
              break;

            case SDLK_UP:
              cam[0]+= cam[3]/4.0;
              cam[1]+= cam[4]/4.0;;
              break;

            case SDLK_DOWN:
              cam[0]-= cam[3]/4.0;
              cam[1]-= cam[4]/4.0;
              break;

            case SDLK_z:
              {
                cam[0]+=  -cam[4]/4.0;
                cam[1]+=  cam[3]/4.0;
              }
              break;

            case SDLK_x:
              {
                cam[0]+=  cam[4]/4.0;
                cam[1]+=  -cam[3]/4.0;
              }
              break;

            default:
              break;
          }
        }
        break;

      case 2:
        {
          int	x,y;
          buflen= recv(new_socket, &x, 4, 0);
          buflen= recv(new_socket, &y, 4, 0);

          cam[0]+= -y*cam[3]/64.0;
          cam[1]+= -y*cam[4]/64.0;;

          cam[9]-= 0.2*x;
          cam[3]= cos(cam[9]*ADM_PI/180.0);
          cam[4]= sin(cam[9]*ADM_PI/180.0);
        }
        break;

      default:
        printf("illegal code\n");
        break;
    }
  } while(1);

  free(imgbufin);
  free(imgbufout);
  close(new_socket);
  close(create_socket);

  return(0);
}
