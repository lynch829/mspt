/*
########################################################################
#
# calcRadiolDepth_PythonExt.cpp
# 
# Created by Paul Morel, LIGM, Universite Paris-Est Marne La Vallee, France
# paul.morel@univ-mlv.fr
# September 2012
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
##
########################################################################


Computes the radiological depth for an entire volume. 
It computes all the ray tracings needed to fill the matrix.

*/
#include "calcRadiolDepth_PythonExt.h"

char errstr[200];  // error string that all routines have access to

/* ==== Set up the methods table ====================== */
static PyMethodDef _CPP_extensionMethods[] = {
    {"calcRadDeff", calcRadDeff, METH_VARARGS},

    {NULL, NULL}     /* Sentinel - marks the end of this structure */
};


/* ==== Initialize the CPP functions ====================== */
// Module name must be _calcRadiolDepth_PythonExt in compile and linked
extern "C" { 
    void init_calcRadiolDepth_PythonExt()  {
        (void) Py_InitModule("_calcRadiolDepth_PythonExt", _CPP_extensionMethods);
        import_array();  // Must be present for NumPy.  Called first after above line.
    }
}


static PyObject * calcRadDeff(PyObject *self, PyObject *args){
    /*
    This function is the python extension. It calculates the radiological depth of all the voxels in a volume.
    Inputs:
        densityGrid : 3D numpy array representing the volume: density or relative stopping power (depending if this is photons or protons)
        startVec : numpy array (1,3) : Coordinates of the first voxel (0,0,0)
        incVec : numpy array (1,3): Increment vector (spacing) when incrementing (+1,+1,+1) the voxels indices.
        sourceVec : numpy array (1,3): Coordinates of the source of the ray
    
    Output:
        Numpy array (3D) in which voxels are represented by their radiological depth
    */

    //Initialize pointers that will receive input data
    PyArrayObject * densityGrid = NULL;
    PyArrayObject * startVec = NULL;
    PyArrayObject * incVec = NULL;
    PyArrayObject * sourceVec = NULL;
    
    //Parse input args:
    if (!PyArg_ParseTuple(args, "O!O!O!O!", 
        &PyArray_Type,&densityGrid, 
        &PyArray_Type,&startVec, 
        &PyArray_Type,&incVec, 
        &PyArray_Type,&sourceVec))  return NULL;

    if(!densityGrid) return NULL;
    if(!startVec) return NULL;
    if(!incVec) return NULL;
    if(!sourceVec) return NULL;
    
    //Cast values to float values: numpy creates array with float values
    if (densityGrid->descr->type_num != NPY_FLOAT) densityGrid = (PyArrayObject*)PyArray_Cast(densityGrid,PyArray_FLOAT);
    if (startVec->descr->type_num != NPY_FLOAT) startVec = (PyArrayObject*)PyArray_Cast(startVec,PyArray_FLOAT);
    if (incVec->descr->type_num != NPY_FLOAT) incVec = (PyArrayObject*)PyArray_Cast(incVec,PyArray_FLOAT);
    if (sourceVec->descr->type_num != NPY_FLOAT) sourceVec = (PyArrayObject*)PyArray_Cast(sourceVec,PyArray_FLOAT);


    // scalars
    int i,j,k,M,N,Q;

    // pointers to vectors that do not need to be freed
    int *densDim, *startDim, *incDim ,*sourceVecDim, * countPtr;
    float *densPtr,*startPtr,*incPtr,*deffPtr, *sourceVecPtr;

    // pointers to vectors that need to be freed
    float *x,*y,*z;

    // other
    int numDimGrid = -1;//Number of dimensions of the density grid - 3 is expected
    
    // point structures that will be used for raytrace operation
    POINTXYZ p1;
    POINTXYZ p2;

    // grid structures for raytracing operation
    VOLUME deff;
    VOLUME dens;
    
    //Output data 
    PyArrayObject * deffGrid = NULL;

    // get dimensions of the input arrays   
    densDim = fillDimensionTable(densityGrid);
    startDim = fillDimensionTable(startVec);
    incDim = fillDimensionTable(incVec);
    sourceVecDim = fillDimensionTable(sourceVec);
    
    //Check validity of input arguments
    if( densityGrid -> nd != 3) PyErr_SetString(PyExc_ValueError,"Error in calcDeffParallel - size density grid is not 3.\n");
    numDimGrid = densityGrid -> nd;
    if( startVec -> nd != 1) PyErr_SetString(PyExc_ValueError,"Error in calcDeffParallel - size start vector is not 1.\n");
    if( startDim[0] != 3){ 
        sprintf(errstr,"Error in calcDeffParallel - start vector must have 3 elements (%d given).\n",startDim[0]);
        PyErr_SetString(PyExc_ValueError,errstr);
    }
    if( incVec -> nd != 1) PyErr_SetString(PyExc_ValueError,"Error in calcDeffParallel - size increment vector is not 1.\n");
    if( incDim[0] != 3){
        sprintf(errstr,"Error in calcDeffParallel - increment vector must have 3 elements (%d given).\n",incDim[0]);
        PyErr_SetString(PyExc_ValueError,errstr);
    }
    if( sourceVec -> nd != 1) PyErr_SetString(PyExc_ValueError,"Error in calcDeffParallel - size source vector is not 1.");
    if( sourceVecDim[0] != 3){ 
        sprintf(errstr, "Error in calcDeffParallel - source vector must have 3 elements(%d given).\n",sourceVecDim[0]);
        PyErr_SetString(PyExc_ValueError,errstr);
    }// End validity check

    // assign data pointers
    densPtr = (float *) densityGrid -> data;
    startPtr = (float *)startVec -> data;
    incPtr = (float *) incVec -> data;
    sourceVecPtr = (float *)sourceVec -> data;
    
    M = densDim[2];// Number of columns
    N = densDim[1];// Number of rows
    Q = densDim[0];// Number of frames
    
    countPtr = (int *) malloc(sizeof(int)*M*N*Q);
    for ( int i = 0; i < M*N*Q; i++) countPtr[i] = 0;
  
    // create spatial grid location vectors
    x = (float *)malloc(sizeof(float)*M);
    if (!x)  PyErr_SetString(PyExc_ValueError,"Error - malloc in calcDeffParallel - x allocation failed\n");
    y = (float *)malloc(sizeof(float)*N);
    if (!y)  PyErr_SetString(PyExc_ValueError,"Error - malloc in calcDeffParallel - y allocation failed\n");
    z = (float *)malloc(sizeof(float)*Q);
    if (!z)  PyErr_SetString(PyExc_ValueError,"Error - malloc in calcDeffParallel - z allocation failed\n");

    // calculate spatial grid location vectors
    for (i=0;i<M;i++) x[i] = startPtr[0] + ((float)i)*incPtr[0];
    for (j=0;j<N;j++) y[j] = startPtr[1] + ((float)j)*incPtr[1];
    for (k=0;k<Q;k++) z[k] = startPtr[2] + ((float)k)*incPtr[2];
        
        
        
    // create output grid
    if(  ( deffGrid = (PyArrayObject *) PyArray_FromDims(densityGrid -> nd, densDim, PyArray_FLOAT) ) == NULL ){
        PyErr_SetString(PyExc_ValueError,"Error - malloc in calcDeffParallel - deffGrid allocation failed\n");
    }
    deffPtr = (float *)deffGrid -> data;
    
    //initialize deffGrid, with -1 signifying voxels of interest in the raytrace
    for (i=0;i<M*N*Q;i++) deffPtr[i] = -1.0;

    // Assign dens and deff parameters to structures for input to raytrace
    dens.matrix = densPtr;
    dens.start.x = x[0];
    dens.start.y = y[0];
    dens.start.z = z[0];
    dens.inc.x = incPtr[0];
    dens.inc.y = incPtr[1];
    dens.inc.z = incPtr[2];
    dens.nbCols = M;
    dens.nbRows = N;
    dens.nbFrames = Q;

    deff.matrix = deffPtr;
    deff.start.x = x[0];
    deff.start.y = y[0];
    deff.start.z = z[0];
    deff.inc.x = incPtr[0];
    deff.inc.y = incPtr[1];
    deff.inc.z = incPtr[2];
    deff.nbCols = M;
    deff.nbRows = N;
    deff.nbFrames = Q;

    
    p1.x = sourceVecPtr[0];
    p1.y = sourceVecPtr[1];
    p1.z = sourceVecPtr[2];
    
    

    
    // calculate radiological depths
    int index = 0;
    for(k=0;k<Q;k++){ 
        for(j=0;j<N;j++){ 
            for (i=0;i<M;i++){
                if (deff.matrix[index] == -1.0)
                {
                    // location of voxel center 
                    p2.x = x[i];
                    p2.y = y[j];
                    p2.z = z[k];
                
                    // extend the ray by a factor of 1000 to include more voxels in the raytrace
                    p2.x = p1.x + (p2.x-p1.x)*(float)100.0;
                    p2.y = p1.y + (p2.y-p1.y)*(float)100.0;
                    p2.z = p1.z + (p2.z-p1.z)*(float)100.0;
                                
                    // do the raytrace, filling in voxels that are passed on the way
                     if ( calcRadiolDepth(&dens,&deff,p1,p2,countPtr) == FAILURE)
                         PyErr_SetString(PyExc_ValueError,errstr);

                    if (deff.matrix[index] == -1.0){
                        printf("(f,r,c)(%i,%i,%i) -- \n",k,j,i);
                        PyErr_SetString(PyExc_ValueError,"Error - Raytrace Failed\n");
                    }           
                }
                index++;
            }
        }
    }
    
    
    for (i=0;i<M*N*Q;i++){
        double count = countPtr[i];
        deffPtr[i] = deffPtr[i] / count;
    }
    
    free(densDim);
    free(startDim);
    free(incDim);
    free(sourceVecDim);
    free(x);
    free(y);
    free(z);
    free(countPtr);

    return PyArray_Return(deffGrid);
}

int * fillDimensionTable( PyArrayObject * array){
// fillDimensionTable is used to return a table of dimensions for a given array. For instance, let's assume array shape is (4,7,6),
// array -> nd will return 3, therefore tabDim[0] = 4, tabDim[1] = 7,tabDim[2] = 6. tabDim will be returned.
    int i;
    if ( !array) return NULL;
    if( array -> nd <= 0) return NULL;// check that the number of dimension composants is at least 1.
    int * tabDim = (int *) malloc(sizeof(int) * (array -> nd));
    if(!tabDim) PyErr_SetString(PyExc_ValueError,"Error - malloc in fillDimanesionTable (calcDeffParallel) - tabDim allocation failed\n");
    for (i = 0 ; i < array -> nd; i++){
        tabDim[i] =     array -> dimensions[i];
        if (tabDim[i] <= 0) PyErr_SetString(PyExc_ValueError,"Error in fillDimensionTable - a dimension is < or = 0.\n"); 
    }

    return tabDim;
}


