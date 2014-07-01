/*
#
#
# Copyright 2011-2014 Paul Morel, LIGM, Universite Paris-Est Marne La Vallee, France
#
# This file is part of MSPT- Motion Simulator for Proton Therapy.
#
#    MSPT- Motion Simulator for Proton Therapy is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    MSPT- Motion Simulator for Proton Therapy is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with MSPT- Motion Simulator for Proton Therapy.  If not, see <http://www.gnu.org/licenses/>.
#
#
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#include "calcRadiolDepth.h"


// List Coordinates
typedef struct
{
    int nbVoxels;
    int ** indList; // first coordinate: voxel number, second coordinate: 2: col, 1: row, 0:frame
    float * lenVoxels;
} INDICES;


// Prototypes
int singleRaytrace(VOLUME * grid,
             POINTXYZ point1,POINTXYZ point2 , INDICES * indRay);
