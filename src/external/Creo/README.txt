This tool exports Creo Parametric parts and assemblies into BRL-CAD .g
databases.  Building it requires a Creo installation with the Pro/Toolkit
SDK and license available, together with a compatible 64-bit BRL-CAD
package.

Notes:

1. Pro/Toolkit is an optional component in the Creo installer.  Enable the
   API Toolkits option during installation (or add it afterwards through
   the Creo installer) so the Pro/Toolkit headers, libraries, and
   protk_unlock.bat utility are present under Common Files/protoolkit.

2. The CMake build produces a self-contained, relocatable bundle under
   <build>/Creo/<config>/ containing:
     bin/        creo-brl.dll, creo-brl-core.dll, and BRL-CAD runtime DLLs
     text/       Creo UI resource and message files, plus ptc_materials.mtl
     config/     Default Creo_to_BRL.g profile
     creo-brl.dat

   No BRL-CAD DLLs or resource files need to be copied into the Creo
   installation directory.  Point Creo at creo-brl.dat via config.pro
   (protkdat / creotkdat / toolkit_registry_file) or Tools -> Auxiliary
   Applications.

3. Before distributing a built plugin bundle, run the CMake UNLOCK target
   (which invokes Creo's protk_unlock.bat utility on creo-brl.dll).

   !! Avoid running UNLOCK multiple times during active development: each
   CLI invocation checks out a Pro/Toolkit seat and immediately exits, which
   typically triggers a 15-minute FlexLM linger freeze on shared license
   servers. For iterative local development without repeating UNLOCK or
   restarting Creo:

   - Core-only changes:
     Creo only checks the Pro/Toolkit unlock signature on creo-brl.dll,
     which loads creo-brl-core.dll dynamically via LoadLibraryExW and
     unloads it on stop.  Once creo-brl.dll is unlocked, changes to
     core.cpp or conversion/* only require rebuilding the creo-brl-core
        > cmake --build <build> --target creo-brl-core

   - On-demand license toggling for frontend/* or loader.c changes:
     When rebuilding creo-brl.dll without unlocking, check out the
     Pro/Toolkit license inside your running Creo session via
     File -> Options -> Licensing
     Creo will load unlocked creo-brl.dll without stacking locks of the
     license with the caveat of a license being held the entire time the 
     option is enabled. Uncheck the license in File -> Options -> Licensing
     when done to return it to the shared pool.
