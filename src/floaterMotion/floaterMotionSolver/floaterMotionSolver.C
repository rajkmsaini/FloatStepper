/*---------------------------------------------------------------------------*\
|   Module Name:     FloatStepper                                             |
|   Description:     OpenFOAM extension module for fluid-rigid body coupling  |
|   License:         GNU General Public License (GPL) version 3               |
|   Copyright:       2025 Johan Roenby, STROMNING APS                         |
|---------------------------------------------------- ------------------------|
|-------Diversity-Equality-Inclusion----Slava-Ukraini----Free-Palestine-------|
\*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2013-2017 OpenFOAM Foundation
     \\/     M anipulation  |
-----------------------------------------------------------------------------
License
    This file is part of FloatStepper.

    FloatStepper is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    FloatStepper is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with FloatStepper.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "floaterMotionSolver.H"
#include "addToRunTimeSelectionTable.H"
#include "polyMesh.H"
#include "pointPatchDist.H"
#include "pointConstraints.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(floaterMotionSolver, 0);

    addToRunTimeSelectionTable
    (
        motionSolver,
        floaterMotionSolver,
        dictionary
    );
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::floaterMotionSolver::floaterMotionSolver
(
    const polyMesh& mesh,
    const IOdictionary& dict
)
:
    displacementMotionSolver(mesh, dict, typeName),
    motion_
    (
        coeffDict(), //Inherited from motionSolver - reads from dynamicMeshDict
        IOdictionary
        (
            IOobject
            (
                "floaters",
                mesh.time().timeName(),
                "uniform",
                mesh,
                IOobject::MUST_READ,
                IOobject::NO_WRITE,
                false
            )
        ).subDict(dict.dictName()),
        mesh.time()
    ),
    patches_(coeffDict().get<wordRes>("patches")),
    patchSet_(mesh.boundaryMesh().patchSet(patches_)),
    di_(coeffDict().get<scalar>("innerDistance")),
    do_(coeffDict().get<scalar>("outerDistance")),
    distStretch_(coeffDict().getOrDefault<tensor>("distStretch", tensor::I)),
    //rhoInf_(1.0),
    rhoName_(coeffDict().getOrDefault<word>("rho", "rho")),
    scale_
    (
        IOobject
        (
            "motionScale",
            mesh.time().timeName(),
            mesh,
            IOobject::NO_READ,
            IOobject::NO_WRITE,
            false
        ),
        pointMesh::New(mesh),
        dimensionedScalar(dimless, Zero)
    ),
    curTimeIndex_(-1),
    DoFs_(0)
{

    // Determining active degrees of freedom
    
    Vector<bool> linDirs(coeffDict().getOrDefault
    (
        "linDirs", Vector<bool>(1, 1, 1))
    );
    Vector<bool> rotDirs(coeffDict().getOrDefault
    (
        "rotDirs", Vector<bool>(1, 1, 1))
    );

    // Ensure no linear motion along and no rotational motion transverse to
    // empty directions
    const Vector<label> solDirs = (mesh.solutionD() + Vector<label>::one)/2;
    forAll(solDirs, i)
    {
        if (!solDirs[i])
        {
            linDirs[i] = 0;
            rotDirs[(i + 1) % 3] = 0;
            rotDirs[(i + 2) % 3] = 0;
        }
    }

    // Adding linear degrees of freedom to DoFs_
    forAll(linDirs, i)
    {
        if (linDirs[i])
        {
            DoFs_.append(i);
        }
    }

    // Adding rotational degrees of freedom to DoFs_
    forAll(rotDirs, i)
    {
        if (rotDirs[i])
        {
            DoFs_.append(i+3);
        }
    }
   Info << "Active degrees of freedom: " << DoFs_ << endl;

    // Calculate scaling factor everywhere
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    {
        const pointMesh& pMesh = pointMesh::New(mesh);

//        pointPatchDist pDist(pMesh, patchSet_, points0());
        pointPatchDist pDist(pMesh, patchSet_, (distStretch_ & points0()));

        // Scaling: 1 up to di then linear down to 0 at do away from patches
        scale_.primitiveFieldRef() =
            min
            (
                max
                (
                    (do_ - pDist.primitiveField())/(do_ - di_),
                    scalar(0)
                ),
                scalar(1)
            );

        // Convert the scale function to a cosine
        scale_.primitiveFieldRef() =
            min
            (
                max
                (
                    0.5
                  - 0.5
                   *cos(scale_.primitiveField()
                   *Foam::constant::mathematical::pi),
                    scalar(0)
                ),
                scalar(1)
            );

        pointConstraints::New(pMesh).constrain(scale_);
        scale_.write();
    }
}

// * * * * * * * * * * * * * * * Member Functions  * * * * * * * * * * * * * //

const Foam::floaterMotion&
Foam::floaterMotionSolver::motion() const
{
    return motion_;
}


Foam::floaterMotion&
Foam::floaterMotionSolver::motion()
{
    return motion_;
}


Foam::tmp<Foam::pointField>
Foam::floaterMotionSolver::curPoints() const
{
    return points0() + pointDisplacement_.primitiveField();
}


void Foam::floaterMotionSolver::solve()
{

    if (mesh().nPoints() != points0().size())
    {
        FatalErrorInFunction
            << "The number of points in the mesh seems to have changed." << endl
            << "In constant/polyMesh there are " << points0().size()
            << " points; in the current mesh there are " << mesh().nPoints()
            << " points." << exit(FatalError);
    }

    // Store the motion state at the beginning of the time-step
    if (curTimeIndex_ != this->db().time().timeIndex())
    {
        motion_.storeOldState();
        curTimeIndex_ = this->db().time().timeIndex();
    }

    // Body update was here but is now handled by floaterMotion

    // Update the displacements
    pointDisplacement_.primitiveFieldRef() =
        motion_.transform(points0(), scale_) - points0();

    // Displacement has changed. Update boundary conditions
    pointConstraints::New
    (
        pointDisplacement_.mesh()
    ).constrainDisplacement(pointDisplacement_);
}


bool Foam::floaterMotionSolver::writeObject
(
    IOstreamOption streamOpt,
    const bool valid
) const
{
    IOdictionary dict
    (
        IOobject
        (
            "floaters",
            mesh().time().timeName(),
            "uniform",
            mesh(),
            IOobject::READ_IF_PRESENT,
            IOobject::AUTO_WRITE,
            false
        )
    );

    // Create or get the sub-dictionary with the name from motion_.name()
    dictionary& floaterDict = dict.subDictOrAdd(motion_.name());
    motion_.write(floaterDict);

    return dict.regIOobject::write();
}


bool Foam::floaterMotionSolver::read()
{
    if (displacementMotionSolver::read())
    {
        motion_.read(coeffDict());

        return true;
    }
    else
    {
        return false;
    }
}


const Foam::labelList& Foam::floaterMotionSolver::DoFs()
{
    return DoFs_;
}

// ************************************************************************* //
