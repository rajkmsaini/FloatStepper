/*---------------------------------------------------------------------------*\
|   Module Name:     FloatStepper                                             |
|   Description:     OpenFOAM extension module for fluid-rigid body coupling  |
|   License:         GNU General Public License (GPL) version 3               |
|   Copyright:       2025 Johan Roenby, STROMNING APS                         |
|---------------------------------------------------- ------------------------|
|-------Diversity-Equality-Inclusion----Slava-Ukraini----Free-Palestine-------|
\*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*\
 =========                   |
 \\      /   F ield          | OpenFOAM: The Open Source CFD Toolbox
  \\    /    O peration      |
   \\  /     A nd            | www.openfoam.com
    \\/      M anipulation   |
-----------------------------------------------------------------------------
    Copyright (C) 2011-2016 OpenFOAM Foundation
    Copyright (C) 2015-2018 OpenCFD Ltd.
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

Application
    floaterInertia is based on surfaceInertia

Description
    Calculates the inertia tensor, principal axes and moments of a
    command line specified triSurface.

    Inertia can either be of the solid body or of a thin shell.

    Writes info to log needed by floatStepper, handles degenerate, symmetric
    cases where surfaceInertia fails and returns a symmetric inertia matrix.

\*---------------------------------------------------------------------------*/

#include "argList.H"
#include "ListOps.H"
#include "triSurface.H"
#include "OFstream.H"
#include "meshTools.H"
#include "Random.H"
#include "transform.H"
#include "IOmanip.H"
#include "Pair.H"
#include "momentOfInertia.H"
#include "mathematicalConstants.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

using namespace Foam;
using namespace Foam::constant::mathematical;

// Calculates eigenvalues of symmetric tensor
// Method included because OpenFOAM's version cannot cope with degenerate cases
// This method should be included in tensor.C
namespace Foam
{

vector symmTensorEigenValues(const symmTensor& T)
{
    // Reference:
    //    \verbatim
    //        Smith, Oliver K. (April 1961)
    //           "Eigenvalues of a symmetric 3 × 3 matrix."
    //           Communications of the ACM, 4 (4): 168
    //           doi:10.1145/355578.366316
    //      \endverbatim

    vector eig;

    scalar p1 = sqr(T.xy()) + sqr(T.xz()) + sqr(T.yz());
    if (p1 < SMALL)
    {
        // T is diagonal.
        eig[0] = T.xx();
        eig[1] = T.yy();
        eig[2] = T.zz();
    }
    else
    {
        scalar q = tr(T)/3;
        scalar p2 = sqr(T.xx() - q) + sqr(T.yy() - q) + sqr(T.zz() - q) + 2*p1;
        scalar p = sqrt(p2/6);
        symmTensor B = (1/p)*(T - q*symmTensor::I);
        scalar r = det(B)/2;

        // In exact arithmetic for a symmetric matrix  -1 <= r <= 1
        // but computation error can leave it slightly outside this range.
        scalar phi(0);
        if (r <= -1)
        {
           phi = pi/3;
        }
        else if (r >= 1)
        {
           phi = 0;
        }
        else
        {
           phi = acos(r)/3;
        }

        // the eigenvalues satisfy eig3 <= eig2 <= eig1
        eig[0] = q + 2*p*cos(phi);
        eig[2] = q + 2*p*cos(phi + (2*pi/3));
        eig[1] = 3*q - eig[0] - eig[2];
    }

    return eig;
}
}

int main(int argc, char *argv[])
{
    argList::addNote
    (
        "Calculates the inertia tensor and principal axes and moments"
        "of the specified surface.\n"
        "Inertia can either be of the solid body or of a thin shell."
    );

    argList::noParallel();
    argList::addArgument("input", "The input surface file");

    argList::addBoolOption
    (
        "shellProperties",
        "Inertia of a thin shell"
    );

    argList::addOption
    (
        "density",
        "scalar",
        "Specify density, "
        "kg/m3 for solid properties, kg/m2 for shell properties"
    );

    argList::addOption
    (
        "referencePoint",
        "vector",
        "Inertia relative to this point, not the centre of mass"
    );

    argList args(argc, argv);

    const fileName surfFileName = args[1];
    const scalar density = args.get<scalar>("density", 1);

    vector refPt = Zero;
    bool calcAroundRefPt = args.readIfPresent("referencePoint", refPt);

    const triSurface surf(surfFileName);

    scalar m = 0.0;
    vector cM = Zero;
    tensor J = Zero;

    if (args.found("shellProperties"))
    {
        momentOfInertia::massPropertiesShell(surf, density, m, cM, J);
    }
    else
    {
        momentOfInertia::massPropertiesSolid(surf, density, m, cM, J);
    }

    if (m < 0)
    {
        WarningInFunction
            << "Negative mass detected, the surface may be inside-out." << endl;
    }

    // Finding eigenvalues and eigenvectors exploiting that J is symmetric
    symmTensor Jsym(J.xx(), J.xy(), J.xz(), J.yy(), J.yz(), J.zz());

    vector eVal = symmTensorEigenValues(Jsym);

    tensor eVec = eigenVectors(Jsym, eVal);

    J = Jsym;
//Removed by Johan
/*
    label pertI = 0;

    Random rand(57373);

    while ((magSqr(eVal) < VSMALL) && pertI < 10)
    {
        WarningInFunction
            << "No eigenValues found, shape may have symmetry, "
            << "perturbing inertia tensor diagonal" << endl;

        J.xx() *= 1.0 + SMALL*rand.sample01<scalar>();
        J.yy() *= 1.0 + SMALL*rand.sample01<scalar>();
        J.zz() *= 1.0 + SMALL*rand.sample01<scalar>();

        eVal = eigenValues(J);

        eVec = eigenVectors(J);

        pertI++;
    }
*/
    bool showTransform = true;

    if
    (
        (mag(eVec.x() ^ eVec.y()) > (1.0 - SMALL))
     && (mag(eVec.y() ^ eVec.z()) > (1.0 - SMALL))
     && (mag(eVec.z() ^ eVec.x()) > (1.0 - SMALL))
    )
    {
        // Make the eigenvectors a right handed orthogonal triplet
        eVec = tensor
        (
            eVec.x(),
            eVec.y(),
            eVec.z() * sign((eVec.x() ^ eVec.y()) & eVec.z())
        );

        // Finding the most natural transformation.  Using Lists
        // rather than tensors to allow indexed permutation.

        // Cartesian basis vectors - right handed orthogonal triplet
        List<vector> cartesian(3);

        cartesian[0] = vector(1, 0, 0);
        cartesian[1] = vector(0, 1, 0);
        cartesian[2] = vector(0, 0, 1);

        // Principal axis basis vectors - right handed orthogonal
        // triplet
        List<vector> principal(3);

        principal[0] = eVec.x();
        principal[1] = eVec.y();
        principal[2] = eVec.z();

        scalar maxMagDotProduct = -GREAT;

        // Matching axis indices, first: cartesian, second:principal

        Pair<label> match(-1, -1);

        forAll(cartesian, cI)
        {
            forAll(principal, pI)
            {
                scalar magDotProduct = mag(cartesian[cI] & principal[pI]);

                if (magDotProduct > maxMagDotProduct)
                {
                    maxMagDotProduct = magDotProduct;

                    match.first() = cI;

                    match.second() = pI;
                }
            }
        }

        scalar sense = sign
        (
            cartesian[match.first()] & principal[match.second()]
        );

        if (sense < 0)
        {
            // Invert the best match direction and swap the order of
            // the other two vectors

            List<vector> tPrincipal = principal;

            tPrincipal[match.second()] *= -1;

            tPrincipal[(match.second() + 1) % 3] =
                principal[(match.second() + 2) % 3];

            tPrincipal[(match.second() + 2) % 3] =
                principal[(match.second() + 1) % 3];

            principal = tPrincipal;

            vector tEVal = eVal;

            tEVal[(match.second() + 1) % 3] = eVal[(match.second() + 2) % 3];

            tEVal[(match.second() + 2) % 3] = eVal[(match.second() + 1) % 3];

            eVal = tEVal;
        }

        label permutationDelta = match.second() - match.first();

        if (permutationDelta != 0)
        {
            // Add 3 to the permutationDelta to avoid negative indices

            permutationDelta += 3;

            List<vector> tPrincipal = principal;

            vector tEVal = eVal;

            for (label i = 0; i < 3; i++)
            {
                tPrincipal[i] = principal[(i + permutationDelta) % 3];

                tEVal[i] = eVal[(i + permutationDelta) % 3];
            }

            principal = tPrincipal;

            eVal = tEVal;
        }

        label matchedAlready = match.first();

        match =Pair<label>(-1, -1);

        maxMagDotProduct = -GREAT;

        forAll(cartesian, cI)
        {
            if (cI == matchedAlready)
            {
                continue;
            }

            forAll(principal, pI)
            {
                if (pI == matchedAlready)
                {
                    continue;
                }

                scalar magDotProduct = mag(cartesian[cI] & principal[pI]);

                if (magDotProduct > maxMagDotProduct)
                {
                    maxMagDotProduct = magDotProduct;

                    match.first() = cI;

                    match.second() = pI;
                }
            }
        }

        sense = sign
        (
            cartesian[match.first()] & principal[match.second()]
        );

        if (sense < 0 || (match.second() - match.first()) != 0)
        {
            principal[match.second()] *= -1;

            List<vector> tPrincipal = principal;

            tPrincipal[(matchedAlready + 1) % 3] =
                principal[(matchedAlready + 2) % 3]*-sense;

            tPrincipal[(matchedAlready + 2) % 3] =
                principal[(matchedAlready + 1) % 3]*-sense;

            principal = tPrincipal;

            vector tEVal = eVal;

            tEVal[(matchedAlready + 1) % 3] = eVal[(matchedAlready + 2) % 3];

            tEVal[(matchedAlready + 2) % 3] = eVal[(matchedAlready + 1) % 3];

            eVal = tEVal;
        }

        eVec = tensor(principal[0], principal[1], principal[2]);

        // {
        //     tensor R = rotationTensor(vector(1, 0, 0), eVec.x());

        //     R = rotationTensor(R & vector(0, 1, 0), eVec.y()) & R;

        //     Info<< "R = " << nl << R << endl;

        //     Info<< "R - eVec.T() " << R - eVec.T() << endl;
        // }
    }
    else
    {
        WarningInFunction
            << "Non-unique eigenvectors, cannot compute transformation "
            << "from Cartesian axes" << endl;

//        showTransform = false;
    }

    // calculate the total surface area

    scalar surfaceArea = 0;

    forAll(surf, facei)
    {
        const labelledTri& f = surf[facei];

        if (f[0] == f[1] || f[0] == f[2] || f[1] == f[2])
        {
            WarningInFunction
               << "Illegal triangle " << facei << " vertices " << f
               << " coords " << f.points(surf.points()) << endl;
        }
        else
        {
            surfaceArea += triPointRef
            (
                surf.points()[f[0]],
                surf.points()[f[1]],
                surf.points()[f[2]]
            ).mag();
        }
    }

    Info<< nl << setprecision(12)
        << "Density: " << density << nl
        << "Mass: " << m << nl
        << "Centre of mass: " << cM << nl
        << "Surface area: " << surfaceArea << nl
        << "Inertia tensor around centre of mass: " << nl << J << nl
        << "eigenValues (principal moments): " << eVal << nl
        << "eigenVectors (principal axes): " << nl
        << eVec.x() << nl << eVec.y() << nl << eVec.z() << endl;

    if (showTransform)
    {
        Info<< "Transform tensor from reference state (orientation):" << nl
            << eVec.T() << nl
            << "Rotation tensor required to transform "
               "from the body reference frame to the global "
               "reference frame, i.e.:" << nl
            << "globalVector = orientation & bodyLocalVector"
            << endl;

        Info<< nl
            << "Entries for sixDoFRigidBodyDisplacement boundary condition:"
            << nl
            << "        mass            " << m << token::END_STATEMENT << nl
            << "        centreOfMass    " << cM << token::END_STATEMENT << nl
            << "        momentOfInertia " << eVal << token::END_STATEMENT << nl
            << "        orientation     " << eVec.T() << token::END_STATEMENT
            << endl;

        Jsym = symmTensor(J.xx(), J.xy(), J.xz(), J.yy(), J.yz(), J.zz());
        Info<< nl
            << "Entries for floatStepper:"
            << nl
            << "        mass            " << m << token::END_STATEMENT << nl
            << "        centreOfMass    " << cM << token::END_STATEMENT << nl
            << "        momentOfInertia " << Jsym << token::END_STATEMENT << nl
            << "        principleAxes   " << eVec.T() << token::END_STATEMENT
            << endl;
    }

    if (calcAroundRefPt)
    {
        tensor JRefP =
            momentOfInertia::applyParallelAxisTheorem(m, cM, J, refPt);
        symmTensor JRefPoint = symmTensor
        (
            JRefP.xx(), JRefP.xy(), JRefP.xz(),
            JRefP.yy(), JRefP.yz(), JRefP.zz()
        );
        Info<< "        momentOfInertia " << JRefPoint << token::END_STATEMENT
            << nl 
            << "        refPoint        " << refPt << token::END_STATEMENT
            << endl;
    }

    OFstream str("axes.obj");

    Info<< nl << "Writing scaled principal axes at centre of mass of "
        << surfFileName <<  " to " << str.name() << endl;

    scalar scale = mag(cM - surf.points()[0])/eVal.component(findMin(eVal));

    meshTools::writeOBJ(str, cM);
    meshTools::writeOBJ(str, cM + scale*eVal.x()*eVec.x());
    meshTools::writeOBJ(str, cM + scale*eVal.y()*eVec.y());
    meshTools::writeOBJ(str, cM + scale*eVal.z()*eVec.z());

    for (label i = 1; i < 4; i++)
    {
        str << "l " << 1 << ' ' << i + 1 << endl;
    }

    Info<< "\nEnd\n" << endl;

    return 0;
}




// ************************************************************************* //
