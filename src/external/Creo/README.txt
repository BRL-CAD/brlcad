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
   (which invokes Creo's protk_unlock.bat utility on the plugin binary).
   For iterative local development without unlocking after every build,
   configure CREOPMA_FEATURE_NAME in parametric.psf so Creo acquires a
   Pro/Toolkit license at startup.
