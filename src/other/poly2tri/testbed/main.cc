/*
 * Poly2Tri Copyright (c) 2009-2010, Poly2Tri Contributors
 * http://code.google.com/p/poly2tri/
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of Poly2Tri nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without specific
 *   prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <cstdlib>
#include <GL/glfw.h>
#include <time.h>
#include <fstream>
#include <string>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <iostream>
using namespace std;

#include "../poly2tri/poly2tri.h"
using namespace p2t;

void Init();
void ShutDown(int return_code);
void MainLoop(const double zoom);
void Draw(const double zoom);
void DrawMap(const double zoom);
void ConstrainedColor(bool constrain);
double StringToDouble(const std::string& s);
double Random(double (*fun)(double), double xmin, double xmax);
double Fun(double x);

/// Dude hole examples
vector<Point*> CreateHeadHole();
vector<Point*> CreateChestHole();

float rotate_y = 0,
      rotate_z = 0;
const float rotations_per_tick = .2;

/// Screen center x
double cx = 0.0;
/// Screen center y
double cy = 0.0;

/// Constrained triangles
vector<Triangle*> triangles;
/// Triangle map
list<Triangle*> map;
/// Polylines
vector< vector<Point*> > polylines;

/// Draw the entire triangle map?
bool draw_map = false;
/// Create a random distribution of points?
bool random_distribution = false;

template <class C> void FreeClear( C & cntr ) {
    for ( typename C::iterator it = cntr.begin();
              it != cntr.end(); ++it ) {
        delete * it;
    }
    cntr.clear();
}

int main(int argc, char* argv[])
{

  int num_points = 0;
  double max, min;
  double zoom;

  if (argc != 5) {
    cout << "-== USAGE ==-" << endl;
    cout << "Load Data File: p2t filename center_x center_y zoom" << endl;
    cout << "Example: ./build/p2t testbed/data/dude.dat 500 500 1" << endl;
    return 1;
  }

  if(string(argv[1]) == "random") {
    num_points = atoi(argv[2]);
    random_distribution = true;
    char* pEnd;
    max = strtod(argv[3], &pEnd);
    min = -max;
    cx = cy = 0;
    zoom = atof(argv[4]);
  } else {
    zoom = atof(argv[4]);
    cx = atof(argv[2]);
    cy = atof(argv[3]);
  }

  vector<p2t::Point*> polyline;

  if(random_distribution) {
    // Create a simple bounding box
    polyline.push_back(new Point(min,min));
    polyline.push_back(new Point(min,max));
    polyline.push_back(new Point(max,max));
    polyline.push_back(new Point(max,min));
  } else {
    // Load pointset from file

    // Parse and tokenize data file
    string line;
    ifstream myfile(argv[1]);
    if (myfile.is_open()) {
      while (!myfile.eof()) {
        getline(myfile, line);
        if (line.size() == 0) {
          break;
        }
        istringstream iss(line);
        vector<string> tokens;
        copy(istream_iterator<string>(iss), istream_iterator<string>(),
             back_inserter<vector<string> >(tokens));
        double x = StringToDouble(tokens[0]);
        double y = StringToDouble(tokens[1]);
        polyline.push_back(new Point(x, y));
        num_points++;
      }
      myfile.close();
    } else {
      cout << "File not opened" << endl;
    }
  }

  cout << "Number of constrained edges = " << polyline.size() << endl;
  polylines.push_back(polyline);

  Init();

  /*
   * Perform triangulation!
   */

  double init_time = glfwGetTime();

  /*
   * STEP 1: Create CDT and add primary polyline
   * NOTE: polyline must be a simple polygon. The polyline's points
   * constitute constrained edges. No repeat points!!!
   */
  CDT* cdt = new CDT(polyline);

  /*
   * STEP 2: Add holes or Steiner points if necessary
   */

  string s(argv[1]);
  if(s.find("dude.dat", 0) != string::npos) {
    // Add head hole
    vector<Point*> head_hole = CreateHeadHole();
    num_points += head_hole.size();
    cdt->AddHole(head_hole);
    // Add chest hole
    vector<Point*> chest_hole = CreateChestHole();
    num_points += chest_hole.size();
    cdt->AddHole(chest_hole);
    polylines.push_back(head_hole);
    polylines.push_back(chest_hole);
  } else if (random_distribution) {
    max-=(1e-4);
    min+=(1e-4);
    for(int i = 0; i < num_points; i++) {
      double x = Random(Fun, min, max);
      double y = Random(Fun, min, max);
      cdt->AddPoint(new Point(x, y));
    }
  }

  /*
   * STEP 3: Triangulate!
   */
  cdt->Triangulate();

  double dt = glfwGetTime() - init_time;

  triangles = cdt->GetTriangles();
  map = cdt->GetMap();

  cout << "Number of points = " << num_points << endl;
  cout << "Number of triangles = " << triangles.size() << endl;
  cout << "Elapsed time (ms) = " << dt*1000.0 << endl;

  MainLoop(zoom);

  // Cleanup

  delete cdt;

  // Free points
  for(int i = 0; i < polylines.size(); i++) {
    vector<Point*> poly = polylines[i];
    FreeClear(poly);
  }

  ShutDown(0);
  return 0;
}

void Init()
{
  const int window_width = 800,
            window_height = 600;

  if (glfwInit() != GL_TRUE)
    ShutDown(1);
  // 800 x 600, 16 bit color, no depth, alpha or stencil buffers, windowed
  if (glfwOpenWindow(window_width, window_height, 5, 6, 5, 0, 0, 0, GLFW_WINDOW) != GL_TRUE)
    ShutDown(1);

  glfwSetWindowTitle("Poly2Tri - C++");
  glfwSwapInterval(1);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(0.0, 0.0, 0.0, 0.0);
  glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
}

void ShutDown(int return_code)
{
  glfwTerminate();
  exit(return_code);
}

void MainLoop(const double zoom)
{
  // the time of the previous frame
  double old_time = glfwGetTime();
  // this just loops as long as the program runs
  bool running = true;

  while (running) {
    // calculate time elapsed, and the amount by which stuff rotates
    double current_time = glfwGetTime(),
           delta_rotate = (current_time - old_time) * rotations_per_tick * 360;
    old_time = current_time;

    // escape to quit, arrow keys to rotate view
    // Check if ESC key was pressed or window was closed
    running = !glfwGetKey(GLFW_KEY_ESC) && glfwGetWindowParam(GLFW_OPENED);

    if (glfwGetKey(GLFW_KEY_LEFT) == GLFW_PRESS)
      rotate_y += delta_rotate;
    if (glfwGetKey(GLFW_KEY_RIGHT) == GLFW_PRESS)
      rotate_y -= delta_rotate;
    // z axis always rotates
    rotate_z += delta_rotate;

    // Draw the scene
    if (draw_map) {
      DrawMap(zoom);
    } else {
      Draw(zoom);
    }

    // swap back and front buffers
    glfwSwapBuffers();
  }
}

void ResetZoom(double zoom, double cx, double cy, double width, double height)
{
  double left = -width / zoom;
  double right = width / zoom;
  double bottom = -height / zoom;
  double top = height / zoom;

  // Reset viewport
  glLoadIdentity();
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();

  // Reset ortho view
  glOrtho(left, right, bottom, top, 1, -1);
  glTranslatef(-cx, -cy, 0);
  glMatrixMode(GL_MODELVIEW);
  glDisable(GL_DEPTH_TEST);
  glLoadIdentity();

  // Clear the screen
  glClear(GL_COLOR_BUFFER_BIT);
}

void Draw(const double zoom)
{
  // reset zoom
  Point center = Point(cx, cy);

  ResetZoom(zoom, center.x, center.y, 800, 600);

  for (int i = 0; i < triangles.size(); i++) {
    Triangle& t = *triangles[i];
    Point& a = *t.GetPoint(0);
    Point& b = *t.GetPoint(1);
    Point& c = *t.GetPoint(2);

    // Red
    glColor3f(1, 0, 0);

    glBegin(GL_LINE_LOOP);
    glVertex2f(a.x, a.y);
    glVertex2f(b.x, b.y);
    glVertex2f(c.x, c.y);
    glEnd();
  }

  // green
  glColor3f(0, 1, 0);

  for(int i = 0; i < polylines.size(); i++) {
    vector<Point*> poly = polylines[i];
    glBegin(GL_LINE_LOOP);
      for(int j = 0; j < poly.size(); j++) {
        glVertex2f(poly[j]->x, poly[j]->y);
      }
    glEnd();
  }
}

void DrawMap(const double zoom)
{
  // reset zoom
  Point center = Point(cx, cy);

  ResetZoom(zoom, center.x, center.y, 800, 600);

  list<Triangle*>::iterator it;
  for (it = map.begin(); it != map.end(); it++) {
    Triangle& t = **it;
    Point& a = *t.GetPoint(0);
    Point& b = *t.GetPoint(1);
    Point& c = *t.GetPoint(2);

    ConstrainedColor(t.constrained_edge[2]);
    glBegin(GL_LINES);
    glVertex2f(a.x, a.y);
    glVertex2f(b.x, b.y);
    glEnd( );

    ConstrainedColor(t.constrained_edge[0]);
    glBegin(GL_LINES);
    glVertex2f(b.x, b.y);
    glVertex2f(c.x, c.y);
    glEnd( );

    ConstrainedColor(t.constrained_edge[1]);
    glBegin(GL_LINES);
    glVertex2f(c.x, c.y);
    glVertex2f(a.x, a.y);
    glEnd( );
  }
}

void ConstrainedColor(bool constrain)
{
  if (constrain) {
    // Green
    glColor3f(0, 1, 0);
  } else {
    // Red
    glColor3f(1, 0, 0);
  }
}

vector<Point*> CreateHeadHole() {

  vector<Point*> head_hole;
  head_hole.push_back(new Point(325, 437));
  head_hole.push_back(new Point(320, 423));
  head_hole.push_back(new Point(329, 413));
  head_hole.push_back(new Point(332, 423));

  return head_hole;
}

vector<Point*> CreateChestHole() {

  vector<Point*> chest_hole;
  chest_hole.push_back(new Point(320.72342,480));
  chest_hole.push_back(new Point(338.90617,465.96863));
  chest_hole.push_back(new Point(347.99754,480.61584));
  chest_hole.push_back(new Point(329.8148,510.41534));
  chest_hole.push_back(new Point(339.91632,480.11077));
  chest_hole.push_back(new Point(334.86556,478.09046));

  return chest_hole;

}



double StringToDouble(const std::string& s)
{
  std::istringstream i(s);
  double x;
  if (!(i >> x))
    return 0;
  return x;
}

double Fun(double x)
{
  return 2.5 + sin(10 * x) / x;
}

double Random(double (*fun)(double), double xmin = 0, double xmax = 1)
{
  static double (*Fun)(double) = NULL, YMin, YMax;
  static bool First = true;

  // Initialises random generator for first call
  if (First)
  {
    First = false;
    srand((unsigned) time(NULL));
  }

  // Evaluates maximum of function
  if (fun != Fun)
  {
    Fun = fun;
    YMin = 0, YMax = Fun(xmin);
    for (int iX = 1; iX < RAND_MAX; iX++)
    {
      double X = xmin + (xmax - xmin) * iX / RAND_MAX;
      double Y = Fun(X);
      YMax = Y > YMax ? Y : YMax;
    }
  }

  // Gets random values for X & Y
  double X = xmin + (xmax - xmin) * rand() / RAND_MAX;
  double Y = YMin + (YMax - YMin) * rand() / RAND_MAX;

  // Returns if valid and try again if not valid
  return Y < fun(X) ? X : Random(Fun, xmin, xmax);
}