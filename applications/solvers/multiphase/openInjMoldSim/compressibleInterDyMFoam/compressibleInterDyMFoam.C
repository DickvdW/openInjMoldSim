/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2015 OpenFOAM Foundation
     \\/     M anipulation  |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

Application
    compressibleInterDyMFoam

Description
    Solver for 2 compressible, non-isothermal immiscible fluids using a VOF
    (volume of fluid) phase-fraction based interface capturing approach,
    with optional mesh motion and mesh topology changes including adaptive
    re-meshing.

    The momentum and other fluid properties are of the "mixture" and a single
    momentum equation is solved.

    Turbulence modelling is generic, i.e. laminar, RAS or LES may be selected.

\*---------------------------------------------------------------------------*/

#include "fvCFD.H"
#include "dynamicFvMesh.H"
#include "MULES.H"
#include "subCycle.H"
#include "interfaceProperties.H"
#include "twoPhaseMixture.H"
#include "twoPhaseMixtureThermo.H"
#include "mojTurbulentFluidThermoModel.H"
#include "pimpleControl.H"
#include "CorrectPhi.H"
#include "fixedFluxPressureFvPatchScalarField.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

int main(int argc, char *argv[])
{
    #include "setRootCase.H"
    #include "createTime.H"
    #include "createDynamicFvMesh.H"
    #include "initContinuityErrs.H"

    pimpleControl pimple(mesh);

    #include "createFields.H"
    #include "createUf.H"
    #include "createControls.H"
    #include "CourantNo.H"
    #include "setInitialDeltaT.H"

    // * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //
    Info<< "\nStarting time loop\n" << endl;

    while (runTime.run())
    {
        #include "readControls.H"

        {
            // Store divU from the previous mesh so that it can be mapped
            // and used in correctPhi to ensure the corrected phi has the
            // same divergence
            volScalarField divU("divU0", fvc::div(fvc::absolute(phi, U)));

            #include "CourantNo.H"
            #include "setDeltaT.H"

            runTime++;

            Info<< "Time = " << runTime.timeName() << nl << endl;

            scalar timeBeforeMeshUpdate = runTime.elapsedCpuTime();

            // Do any mesh changes
            mesh.update();

            if (mesh.changing())
            {
                Info<< "Execution time for mesh.update() = "
                    << runTime.elapsedCpuTime() - timeBeforeMeshUpdate
                    << " s" << endl;

                gh = (g & mesh.C()) - ghRef;
                ghf = (g & mesh.Cf()) - ghRef;
            }

            if (mesh.changing() && correctPhi)
            {
                // Calculate absolute flux from the mapped surface velocity
                phi = mesh.Sf() & Uf;

                #include "correctPhi.H"

                // Make the fluxes relative to the mesh motion
                fvc::makeRelative(phi, U);
            }
        }

        if (mesh.changing() && checkMeshCourantNo)
        {
            #include "meshCourantNo.H"
        }

        turbulence->correct();

        // --- Pressure-velocity PIMPLE corrector loop
        while (pimple.loop())
        {
            #include "alphaEqnsSubCycle.H"

            // correct interface on first PIMPLE corrector
            if (pimple.corr() == 1)
            {
                interface.correct();
            }

            solve(fvm::ddt(rho) + fvc::div(rhoPhi));

            #include "UEqn.H"
            #include "TEqn.H"

            // --- Pressure corrector loop
            while (pimple.correct())
            {
                #include "pEqn.H"
            }
        }

        rho = alpha1*rho1 + alpha2*rho2;

        runTime.write();

        Info<< "ExecutionTime = "
            << runTime.elapsedCpuTime()
            << " s\n\n" << endl;
    }

    Info<< "End\n" << endl;

    return 0;
}


// ************************************************************************* //
