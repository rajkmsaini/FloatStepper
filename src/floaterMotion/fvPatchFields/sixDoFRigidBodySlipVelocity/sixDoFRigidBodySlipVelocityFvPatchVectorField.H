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
    \\  /    A nd           | Copyright (C) 2011-2016 OpenFOAM Foundation
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

Class
    Foam::sixDoFRigidBodySlipVelocityFvPatchVectorField

Group
    grpWallBoundaryConditions grpGenericBoundaryConditions

Description
    This boundary condition provides a rigid body motion boundary condition
    with slip where the velocity field is set to rigid body motion with velocity,
    angular velocity and centre of rotation read from a sixDoFRigidBodyMotionSolver
    that is looked up from the object registry.

    This BC is intended for use for moving rigid body when body motion is
    solved using the floaterFoam solver.

    Note:
    I have experienced strange behavior where the cell layer closest to the
    rigid body has velocity between 0 and target velocity.
    This problem is not specific to floaterFoam with this BC.

    Copyright, Johan Roenby, STROMNING, 2022

See also
    Foam::fixedValueFvPatchField
    Foam::Function1Types

SourceFiles
    sixDoFRigidBodySlipVelocityFvPatchVectorField.C

\*---------------------------------------------------------------------------*/

#ifndef sixDoFRigidBodySlipVelocityFvPatchVectorField_H
#define sixDoFRigidBodySlipVelocityFvPatchVectorField_H

#include "fixedValueFvPatchFields.H"

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

namespace Foam
{

/*---------------------------------------------------------------------------*\
           Class sixDoFRigidBodySlipVelocityFvPatchVectorField Declaration
\*---------------------------------------------------------------------------*/

class sixDoFRigidBodySlipVelocityFvPatchVectorField
:
    public fixedValueFvPatchVectorField
{
    // Private data

public:

    //- Runtime type information
    TypeName("sixDoFRigidBodySlipVelocity");


    // Constructors

        //- Construct from patch and internal field
        sixDoFRigidBodySlipVelocityFvPatchVectorField
        (
            const fvPatch&,
            const DimensionedField<vector, volMesh>&
        );

        //- Construct from patch, internal field and dictionary
        sixDoFRigidBodySlipVelocityFvPatchVectorField
        (
            const fvPatch&,
            const DimensionedField<vector, volMesh>&,
            const dictionary&
        );

        //- Construct by mapping given sixDoFRigidBodySlipVelocityFvPatchVectorField
        //  onto a new patch
        sixDoFRigidBodySlipVelocityFvPatchVectorField
        (
            const sixDoFRigidBodySlipVelocityFvPatchVectorField&,
            const fvPatch&,
            const DimensionedField<vector, volMesh>&,
            const fvPatchFieldMapper&
        );

        //- Construct as copy
        sixDoFRigidBodySlipVelocityFvPatchVectorField
        (
            const sixDoFRigidBodySlipVelocityFvPatchVectorField&
        );

        //- Construct and return a clone
        virtual tmp<fvPatchVectorField> clone() const
        {
            return tmp<fvPatchVectorField>
            (
                new sixDoFRigidBodySlipVelocityFvPatchVectorField(*this)
            );
        }

        //- Construct as copy setting internal field reference
        sixDoFRigidBodySlipVelocityFvPatchVectorField
        (
            const sixDoFRigidBodySlipVelocityFvPatchVectorField&,
            const DimensionedField<vector, volMesh>&
        );

        //- Construct and return a clone setting internal field reference
        virtual tmp<fvPatchVectorField> clone
        (
             const DimensionedField<vector, volMesh>& iF
        ) const
        {
            return tmp<fvPatchVectorField>
            (
                new sixDoFRigidBodySlipVelocityFvPatchVectorField(*this, iF)
            );
        }



    // Member functions

        // Access functions

        //- Update the coefficients associated with the patch field
        virtual void updateCoeffs();

        //- Write
        virtual void write(Ostream&) const;
};


// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

} // End namespace Foam

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * //

#endif

// ************************************************************************* //
