/* ========================================================================= *
 *                                                                           *
 *                               OpenMesh                                    *
 *           Copyright (c) 2001-2022, RWTH-Aachen University                 *
 *           Department of Computer Graphics and Multimedia                  *
 *                          All rights reserved.                             *
 *                            www.openmesh.org                               *
 *                                                                           *
 *---------------------------------------------------------------------------*
 * This file is part of OpenMesh.                                            *
 *---------------------------------------------------------------------------*
 *                                                                           *
 * Redistribution and use in source and binary forms, with or without        *
 * modification, are permitted provided that the following conditions        *
 * are met:                                                                  *
 *                                                                           *
 * 1. Redistributions of source code must retain the above copyright notice, *
 *    this list of conditions and the following disclaimer.                  *
 *                                                                           *
 * 2. Redistributions in binary form must reproduce the above copyright      *
 *    notice, this list of conditions and the following disclaimer in the    *
 *    documentation and/or other materials provided with the distribution.   *
 *                                                                           *
 * 3. Neither the name of the copyright holder nor the names of its          *
 *    contributors may be used to endorse or promote products derived from   *
 *    this software without specific prior written permission.               *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED *
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A           *
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER *
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,  *
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,       *
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR        *
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    *
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      *
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        *
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              *
 *                                                                           *
 * ========================================================================= */



#ifdef _MSC_VER
#  pragma warning(disable: 4267 4311)
#endif

#include <iostream>
#include <fstream>

#include <QApplication>
#include <QMessageBox>
#include "SubdivideWidget.hh"
#if QT_VERSION_MAJOR > 5
#include <QOpenGLContext>
#endif

int main(int argc, char **argv)
{
  // OpenGL check
  QApplication app(argc,argv);

#if QT_VERSION_MAJOR < 6
  if ( !QGLFormat::hasOpenGL() ) {
#else
  if ( QOpenGLContext::openGLModuleType() != QOpenGLContext::LibGL ) {
#endif
    QString msg = "System has no OpenGL support!";
    QMessageBox::critical( nullptr, "OpenGL", msg + argv[1], QMessageBox::Ok );
    return -1;
  }

    
  // create widget
  SubdivideWidget* w = new SubdivideWidget(nullptr, "Subdivider");

  w->resize(400, 400);
  w->show();

  // load scene
  if (argc > 1)  
  {
     if ( ! w->open_mesh(argv[1]) )
     {
        QString msg = "Cannot read mesh from file ";
        QMessageBox::critical( nullptr, argv[1], msg + argv[1], QMessageBox::Ok );
        return -1;
     }
  }


  return app.exec();
}
