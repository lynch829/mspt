/*
########################################################################
#
#  fillDensityArray_PythonExt.cpp
# 
# Created by Paul Morel, LIGM, Universite Paris-Est Marne La Vallee, France
# paul.morel@univ-mlv.fr
# September 2012
# 
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
########################################################################
*/

#include "fillDensityArray_PythonExtDouble.h"


char errstr[200];  // error string that all routines have access to
/* ==== Set up the methods table ====================== */
static PyMethodDef _CPP_extensionMethods[] = {
    {"fillDensityGrid", fillDensityGrid, METH_VARARGS},

    {NULL, NULL}     /* Sentinel - marks the end of this structure */
};


/* ==== Initialize the CPP functions ====================== */
// Module name must be _fillDensityArray_PythonExtDouble in compile and linked
extern "C" { 
    void init_fillDensityArray_PythonExtDouble()  {
        (void) Py_InitModule("_fillDensityArray_PythonExtDouble", _CPP_extensionMethods);
        import_array();  // Must be present for NumPy.  Called first after above line.
    }
}


static PyObject * fillDensityGrid(PyObject *self, PyObject *args){
/*
 Function to fill the density grid based using the CT grid and a conversion table
 Input:
    ctGrid : 3D numpy array representing the CT
    conversionTable :2D Array: 2 rows, n columns: row 0 : CT values, row 1 :density values 
    
 
 Output: 
    3D numpy array representing a new CT grid.
 
 */ 
    
    //Initialize pointers that will receive input data
    PyArrayObject * ctGrid = NULL; // We assume that ct Value have been previously correctly scaled.
    PyArrayObject * conversionTable = NULL; 
 
    //Parse input args:
    if (!PyArg_ParseTuple(args, "O!O!", 
        &PyArray_Type,&ctGrid, 
        &PyArray_Type,&conversionTable))  return NULL;

    if(!ctGrid) return NULL;
    if(!conversionTable) return NULL;

    
    //Cast values to double values: numpy creates array with double values
    if (ctGrid->descr->type_num != NPY_DOUBLE) ctGrid = (PyArrayObject*)PyArray_Cast(ctGrid,PyArray_DOUBLE);
    if (conversionTable->descr->type_num != NPY_DOUBLE) conversionTable = (PyArrayObject*)PyArray_Cast(conversionTable,PyArray_DOUBLE);
    
    double  * dataDensityGrid, * dataCTGrid, * dataConversion;
    
    dataCTGrid = (double*) ctGrid -> data;  
    dataConversion = (double*) conversionTable -> data; 
   
    int nbColsConvTab = conversionTable->dimensions[1];
  
    int nbFramesCT = ctGrid->dimensions[0];
    int nbRowsCT = ctGrid->dimensions[1];
    int nbColsCT = ctGrid-> dimensions[2];
    int dimGrid[] ={nbFramesCT,nbRowsCT,nbColsCT} ;
    
    PyArrayObject * densGrid = NULL;
       // create output grid
    if(  ( densGrid = (PyArrayObject *) PyArray_FromDims(ctGrid->nd, dimGrid, PyArray_DOUBLE) ) == NULL ){
        PyErr_SetString(PyExc_ValueError,"Error - malloc in fillDensityGrid - densGrid allocation failed\n");
    }
    dataDensityGrid = (double*) densGrid -> data;
    
    double * slope, * intercept;
    slope = (double *) malloc(sizeof(double) * (nbColsConvTab-1) );
    if(!slope) {
        PyErr_SetString(PyExc_ValueError,"Error allocation slope tab in fillDensity\n");
    }
    intercept = (double *) malloc(sizeof(double) * (nbColsConvTab-1) );
    if(!intercept) {
        PyErr_SetString(PyExc_ValueError,"Error allocation intercept tab in fillDensity\n");
    }
    for(int i = 0; i < nbColsConvTab -1; i++){
        slope[i] = DBL_MAX;
        intercept[i] = DBL_MAX;
    }
    
    
    double ctValue;
    double convertedValue;
    int idx;
    for(int f = 0 ; f < nbFramesCT ; f++){ // End - iterate through frames
        for(int r = 0 ; r < nbRowsCT ; r++){ // End - iterate through rows
            for(int c = 0 ; c < nbColsCT ; c++){ // End - iterate through columns
                idx = f * nbColsCT * nbRowsCT + r * nbColsCT + c;
                ctValue = dataCTGrid[ idx ];
                convertedValue = 0;
                if( ctValue <= dataConversion[0] &&  ctValue >= (dataConversion[0] - abs(dataConversion[0]*0.1) )) {
                    convertedValue = dataConversion[1 * nbColsConvTab];
                }
                else{
                    if (  ctValue >= dataConversion[nbColsConvTab - 1] &&  ctValue <= (dataConversion[nbColsConvTab - 1] + abs(dataConversion[nbColsConvTab - 1]*0.1)) ){
                        convertedValue = dataConversion[1 * nbColsConvTab + nbColsConvTab - 1];
                    }
                    else{
                        if( ctValue > dataConversion[0] && ctValue < dataConversion[nbColsConvTab - 1])
                        {
                            for( int i = 0 ; i < nbColsConvTab-1 ; i ++){
                                if(slope[i] == DBL_MAX){
                                        slope[i] = (dataConversion[nbColsConvTab + i]- dataConversion[nbColsConvTab + i + 1])/ ( dataConversion[i]- dataConversion[i+1]);
                                }
                                if(intercept[i] == DBL_MAX){
                                        intercept[i] = dataConversion[nbColsConvTab + i] - dataConversion[i] * slope[i];
                                }
                                if(ctValue >= dataConversion[i] && ctValue < dataConversion[i+1]){
                                    convertedValue = ctValue * slope[i] + intercept[i];
                                    break;
                                }
                                
                            }
                        }
                        else{
                            printf("Error- fill density array - ct value out of range [%f , %f]: ctValue = %f\n",dataConversion[0],dataConversion[nbColsConvTab - 1],ctValue);
                            PyErr_SetString(PyExc_ValueError,"Error ct value in fill density array...\n");                     
                            }
                    }
                }
                
                
                dataDensityGrid[ idx ] = convertedValue;

        
            } // End - iterate through columns
        } // End - iterate through rows
    } // End - iterate through frames
    if (slope) free(slope);
    if (intercept) free(intercept);
    return PyArray_Return(densGrid);
}

double absolute(double val){
    if (val < 0) return (-val);
    return val;

}
