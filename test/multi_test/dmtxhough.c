/*
libdmtx - Data Matrix Encoding/Decoding Library

Copyright (C) 2010 Mike Laughton

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

Contact: mblaughton@users.sourceforge.net
*/

/* $Id$ */

#include <string.h>
#include <assert.h>
#include <math.h>
#include "multi_test.h"

/**
 *
 *
 */
DmtxPassFail
HoughGridPopulate(DmtxDecode2 *dec)
{
   int row, col;
   DmtxHoughGrid *grid;

   dec->houghGrid = (DmtxHoughGrid *)calloc(1, sizeof(DmtxHoughGrid));
   if(dec->houghGrid == NULL)
      return DmtxFail;

   grid = dec->houghGrid;
   grid->rows = 1;
   grid->cols = 1;
   grid->count = grid->rows * grid->cols;
   grid->local = (DmtxHoughLocal *)malloc(grid->count * sizeof(DmtxHoughLocal));
   if(grid->local == NULL)
   {
      free(dec->houghGrid);
      dec->houghGrid = NULL;
      return DmtxFail;
   }

   for(row = 0; row < grid->rows; row++)
   {
      for(col = 0; col < grid->cols; col++)
      {
         HoughLocalAccumulate(dec, col, row);
      }
   }

   dec->fn.dmtxHoughLocalCallback(dec->houghGrid->local, 0);

   return DmtxPass;
}

/**
 *
 *
 */
DmtxPassFail
HoughGridDestroy(DmtxHoughGrid **grid)
{
   if(grid == NULL || *grid == NULL)
      return DmtxFail;

   if((*grid)->local != NULL)
      free((*grid)->local);

   free(*grid);
   *grid = NULL;

   return DmtxPass;
}

/**
 *
 *
 */
DmtxPassFail
HoughLocalAccumulate(DmtxDecode2 *dec, int gCol, int gRow)
{
   int rRow, rCol;
   int iRow, iCol;
   int phi;
   DmtxHoughLocal *hRegion;
   ZeroCrossing hhZXing, hsZXing, vsZXing, vvZXing, vbZXing, hbZXing;

   hRegion = &(dec->houghGrid->local[0]); /* [hCol * width + hRol]; */

   memset(hRegion, 0x00, sizeof(DmtxHoughLocal));

   hRegion->xOrigin = 100;
   hRegion->yOrigin = 100;

   /* calculate dOffset ? */

   for(rRow = 0; rRow < 64; rRow++)
   {
      iRow = hRegion->yOrigin + rRow;

      for(rCol = 0; rCol < 64; rCol++)
      {
         iCol = hRegion->xOrigin + rCol;

         hhZXing = GetZeroCrossing(dec->hhAccel, iCol, iRow);
         hsZXing = GetZeroCrossing(dec->hsAccel, iCol, iRow);
         vsZXing = GetZeroCrossing(dec->vsAccel, iCol, iRow);
         vvZXing = GetZeroCrossing(dec->vvAccel, iCol, iRow);
         vbZXing = GetZeroCrossing(dec->vbAccel, iCol, iRow);
         hbZXing = GetZeroCrossing(dec->hbAccel, iCol, iRow);
/*
   if(gState.displayEdge == DmtxUndefined || gState.displayEdge == sobelDir)
   {
      if(edgeDir == DmtxEdgeVertical && (sobelDir == DmtxEdgeVertical || sobelDir == DmtxEdgeBackslash))
*/
      if(vvZXing.mag > 50)
         dec->fn.zeroCrossingCallback(iCol, iRow, 255, 0);
/*
      else if(edgeDir == DmtxEdgeHorizontal && (sobelDir == SobelEdgeHorizontal || sobelDir == DmtxEdgeSlash))
         dec->fn.zeroCrossingCallback(iCol, iRow, edge.mag, 0);
   }
*/
         if(hhZXing.mag > 0)
         {
            for(phi = 0; phi < 16; phi++)
               HoughLocalAccumulateEdge(hRegion, phi, hhZXing);
            for(phi = 112; phi < 128; phi++)
               HoughLocalAccumulateEdge(hRegion, phi, hhZXing);
         }

         if(hsZXing.mag > 0)
            for(phi = 16; phi < 32; phi++)
               HoughLocalAccumulateEdge(hRegion, phi, hsZXing);

         if(vsZXing.mag > 0)
            for(phi = 32; phi < 48; phi++)
               HoughLocalAccumulateEdge(hRegion, phi, vsZXing);

         if(vvZXing.mag > 0)
            for(phi = 48; phi < 80; phi++)
               HoughLocalAccumulateEdge(hRegion, phi, vvZXing);

         if(vbZXing.mag > 0)
            for(phi = 80; phi < 96; phi++)
               HoughLocalAccumulateEdge(hRegion, phi, vbZXing);

         if(hbZXing.mag > 0)
            for(phi = 96; phi < 112; phi++)
               HoughLocalAccumulateEdge(hRegion, phi, hbZXing);
      }
   }

   return DmtxPass;
}

/**
 *
 *
 */
ZeroCrossing
GetZeroCrossing(DmtxValueGrid *accel, int iCol, int iRow)
{
   int aInc, aIdx, aIdxNext;
   int aRow, aCol;
   int aWidth, aHeight;
   int aHere, aNext, aPrev;
   double smidge;
   ZeroCrossing edge = { 0, 0.0, 0.0 };

   aWidth = dmtxValueGridGetWidth(accel);
   aHeight = dmtxValueGridGetHeight(accel);
   aRow = iRow - 1;
   aCol = iCol - 1;
   aIdx = aRow * aWidth + aCol;
   aInc = (accel->type == DmtxEdgeVertical) ? 1 : aWidth;
   aIdxNext = aIdx + aInc;

   aHere = accel->value[aIdx];
   aNext = accel->value[aIdxNext];

   if(OPPOSITE_SIGNS(aHere, aNext))
   {
      /* Zero crossing: Neighbors with opposite signs [-10,+10] */
      smidge = abs(aHere/(aHere - aNext));
      edge = SetZeroCrossingFromIndex(accel, aCol, aRow, smidge);
   }
   else if(aHere == 0 && aNext != 0)
   {
      /* No previous value for comparison (beginning of row/col) */
      if(aIdx < aInc)
         return edge;

      aPrev = accel->value[aIdx-aInc];
      if(OPPOSITE_SIGNS(aPrev, aNext))
      {
         /* Zero crossing: Opposite signs separated by zero [-10,0,+10] */
         edge = SetZeroCrossingFromIndex(accel, aCol, aRow, 0.0);
      }
   }

   return edge;
}

/**
 * 0 < smidge < 1
 *
 */
ZeroCrossing
SetZeroCrossingFromIndex(DmtxValueGrid *accel, int aCol, int aRow, double smidge)
{
   int sIdx;
   ZeroCrossing edge;
   DmtxValueGrid *sobel = accel->ref;

   if(accel->type == DmtxEdgeVertical)
   {
      edge.x = (double)aCol + 2.0 + smidge;
      edge.y = (double)aRow + 1.5;
   }
   else
   {
      edge.x = (double)aCol + 1.5;
      edge.y = (double)aRow + 2.0 + smidge;
   }

   sIdx = SobelGridGetIndexFromZXing(sobel, accel->type, aCol, aRow);
   edge.mag = abs(sobel->value[sIdx]);

   return edge;
}

/**
 *
 *
 */
DmtxPassFail
HoughLocalAccumulateEdge(DmtxHoughLocal *local, int phi, ZeroCrossing edge)
{
   double d;
   int dInt;

   d = HoughGetLocalOffset(edge.x, edge.y, phi);
   dInt = (int)d;

if(dInt < 0 || dInt > 63)
   return DmtxFail;
if(phi < 0 || phi > 127)
   return DmtxFail;

   local->bucket[dInt][phi] += edge.mag;

   return DmtxPass;
}

/**
 *
 *
 */
double
HoughGetLocalOffset(double xLoc, double yLoc, int phi)
{
   double phiRad, sinPhi, cosPhi;
   double scale, d;

   phiRad = (phi * M_PI)/128.0;
   sinPhi = sin(phiRad);
   cosPhi = cos(phiRad);

   if(phi <= 64)
   {
      scale = 64.0 / (sinPhi + cosPhi);
      d = (xLoc * cosPhi + yLoc * sinPhi) * scale;
   }
   else
   {
      scale = 64.0 / (sinPhi - cosPhi);
      d = ((xLoc * cosPhi + yLoc * sinPhi) - cosPhi) * scale;
   }

   return d;
}
