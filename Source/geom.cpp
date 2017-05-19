/*
** SGI FREE SOFTWARE LICENSE B (Version 2.0, Sept. 18, 2008) 
** Copyright (C) [dates of first publication] Silicon Graphics, Inc.
** All Rights Reserved.
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
** of the Software, and to permit persons to whom the Software is furnished to do so,
** subject to the following conditions:
** 
** The above copyright notice including the dates of first publication and either this
** permission notice or a reference to http://oss.sgi.com/projects/FreeB/ shall be
** included in all copies or substantial portions of the Software. 
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
** INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
** PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL SILICON GRAPHICS, INC.
** BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
** TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
** OR OTHER DEALINGS IN THE SOFTWARE.
** 
** Except as contained in this notice, the name of Silicon Graphics, Inc. shall not
** be used in advertising or otherwise to promote the sale, use or other dealings in
** this Software without prior written authorization from Silicon Graphics, Inc.
*/
/*
** Author: Eric Veach, July 1994.
*/

//#include "tesos.h"
#include <assert.h>
#include "mesh.h"
#include "geom.h"
#include <math.h>
#include <stdlib.h>

namespace Tess
{
    float edgeEval( const TESSvertex *u, const TESSvertex *v, const TESSvertex *w )
    {
        /* Given three vertices u,v,w such that vertAreLessOrEqual(u,v) && vertAreLessOrEqual(v,w),
         * evaluates the t-coord of the edge uw at the s-coord of the vertex v.
         * Returns v->t - (uw)(v->s), ie. the signed distance from uw to v.
         * If uw is vertical (and thus passes thru v), the result is zero.
         *
         * The calculation is extremely accurate and stable, even when v
         * is very close to u or w.  In particular if we set v->t = 0 and
         * let r be the negated result (this evaluates (uw)(v->s)), then
         * r is guaranteed to satisfy MIN(u->t,w->t) <= r <= MAX(u->t,w->t).
         */
        float gapL, gapR;
        
        assert( vertAreLessOrEqual( u, v ) && vertAreLessOrEqual( v, w ));
        
        gapL = v->s - u->s;
        gapR = w->s - v->s;
        
        if( gapL + gapR > 0 ) {
            if( gapL < gapR ) {
                return (v->t - u->t) + (u->t - w->t) * (gapL / (gapL + gapR));
            } else {
                return (v->t - w->t) + (w->t - u->t) * (gapR / (gapL + gapR));
            }
        }
        /* vertical line */
        return 0;
    }
    
    float edgeSign( const TESSvertex *u, const TESSvertex *v, const TESSvertex *w )
    {
        /* Returns a number whose sign matches edgeEval(u,v,w) but which
         * is cheaper to evaluate.  Returns > 0, == 0 , or < 0
         * as v is above, on, or below the edge uw.
         */
        float gapL, gapR;
        
        assert( vertAreLessOrEqual( u, v ) && vertAreLessOrEqual( v, w ));
        
        gapL = v->s - u->s;
        gapR = w->s - v->s;
        
        if( gapL + gapR > 0 ) {
            return (v->t - w->t) * gapL + (v->t - u->t) * gapR;
        }
        /* vertical line */
        return 0;
    }
    
    
    /***********************************************************************
     * Define versions of edgeSign, edgeEval with s and t transposed.
     */
    
    float transEdgeEval( const TESSvertex *u, const TESSvertex *v, const TESSvertex *w )
    {
        /* Given three vertices u,v,w such that transVertAreLessOrEqual(u,v) && transVertAreLessOrEqual(v,w),
         * evaluates the t-coord of the edge uw at the s-coord of the vertex v.
         * Returns v->s - (uw)(v->t), ie. the signed distance from uw to v.
         * If uw is vertical (and thus passes thru v), the result is zero.
         *
         * The calculation is extremely accurate and stable, even when v
         * is very close to u or w.  In particular if we set v->s = 0 and
         * let r be the negated result (this evaluates (uw)(v->t)), then
         * r is guaranteed to satisfy MIN(u->s,w->s) <= r <= MAX(u->s,w->s).
         */
        float gapL, gapR;
        
        assert( transVertAreLessOrEqual( u, v ) && transVertAreLessOrEqual( v, w ));
        
        gapL = v->t - u->t;
        gapR = w->t - v->t;
        
        if( gapL + gapR > 0 ) {
            if( gapL < gapR ) {
                return (v->s - u->s) + (u->s - w->s) * (gapL / (gapL + gapR));
            } else {
                return (v->s - w->s) + (w->s - u->s) * (gapR / (gapL + gapR));
            }
        }
        /* vertical line */
        return 0;
    }
    
    float transEdgeSign( const TESSvertex *u, const TESSvertex *v, const TESSvertex *w )
    {
        /* Returns a number whose sign matches transEdgeEval(u,v,w) but which
         * is cheaper to evaluate.  Returns > 0, == 0 , or < 0
         * as v is above, on, or below the edge uw.
         */
        float gapL, gapR;
        
        assert( transVertAreLessOrEqual( u, v ) && transVertAreLessOrEqual( v, w ));
        
        gapL = v->t - u->t;
        gapR = w->t - v->t;
        
        if( gapL + gapR > 0 ) {
            return (v->s - w->s) * gapL + (v->s - u->s) * gapR;
        }
        /* vertical line */
        return 0;
    }

    static inline float interpolate(float& a, float x, float& b, float y)
    /* Given parameters a,x,b,y returns the value (b*x+a*y)/(a+b),
     * or (x+y)/2 if a==b==0.  It requires that a,b >= 0, and enforces
     * this in the rare case that one argument is slightly negative.
     * The implementation is extremely stable numerically.
     * In particular it guarantees that the result r satisfies
     * MIN(x,y) <= r <= MAX(x,y), and the results are very accurate
     * even when a and b differ greatly in magnitude.
     */
    {
        if (a < 0.f) a = 0.f;
        if (b < 0.f) b = 0.f;
        if (a > b)
        {
            return (y + (x-y) * (b/(a+b)));
        }
        if (b != 0)
        {
            return (x + (y-x) * (a/(a+b)));
        }
        
        return ((x+y) / 2);
    }

    void edgeIntersect( const TESSvertex *o1, const TESSvertex *d1,
                          const TESSvertex *o2, const TESSvertex *d2,
                          TESSvertex *v )
    /* Given edges (o1,d1) and (o2,d2), compute their point of intersection.
     * The computed point is guaranteed to lie in the intersection of the
     * bounding rectangles defined by each edge.
     */
    {
        float z1, z2;
        
        /* This is certainly not the most efficient way to find the intersection
         * of two line segments, but it is very numerically stable.
         *
         * Strategy: find the two middle vertices in the vertAreLessOrEqual ordering,
         * and interpolate the intersection s-value from these.  Then repeat
         * using the transVertAreLessOrEqual ordering to find the intersection t-value.
         */
        
        if( ! vertAreLessOrEqual( o1, d1 )) { std::swap( o1, d1 ); }
        if( ! vertAreLessOrEqual( o2, d2 )) { std::swap( o2, d2 ); }
        if( ! vertAreLessOrEqual( o1, o2 )) { std::swap( o1, o2 ); std::swap( d1, d2 ); }
        
        if( ! vertAreLessOrEqual( o2, d1 )) {
            /* Technically, no intersection -- do our best */
            v->s = (o2->s + d1->s) / 2;
        } else if( vertAreLessOrEqual( d1, d2 )) {
            /* Interpolate between o2 and d1 */
            z1 = edgeEval( o1, o2, d1 );
            z2 = edgeEval( o2, d1, d2 );
            if( z1+z2 < 0 ) { z1 = -z1; z2 = -z2; }
            v->s = interpolate( z1, o2->s, z2, d1->s );
        } else {
            /* Interpolate between o2 and d2 */
            z1 = edgeSign( o1, o2, d1 );
            z2 = -edgeSign( o1, d2, d1 );
            if( z1+z2 < 0 ) { z1 = -z1; z2 = -z2; }
            v->s = interpolate( z1, o2->s, z2, d2->s );
        }
        
        /* Now repeat the process for t */
        
        if( ! transVertAreLessOrEqual( o1, d1 )) { std::swap( o1, d1 ); }
        if( ! transVertAreLessOrEqual( o2, d2 )) { std::swap( o2, d2 ); }
        if( ! transVertAreLessOrEqual( o1, o2 )) { std::swap( o1, o2 ); std::swap( d1, d2 ); }
        
        if( ! transVertAreLessOrEqual( o2, d1 )) {
            /* Technically, no intersection -- do our best */
            v->t = (o2->t + d1->t) / 2;
        } else if( transVertAreLessOrEqual( d1, d2 )) {
            /* Interpolate between o2 and d1 */
            z1 = transEdgeEval( o1, o2, d1 );
            z2 = transEdgeEval( o2, d1, d2 );
            if( z1+z2 < 0 ) { z1 = -z1; z2 = -z2; }
            v->t = interpolate( z1, o2->t, z2, d1->t );
        } else {
            /* Interpolate between o2 and d2 */
            z1 = transEdgeSign( o1, o2, d1 );
            z2 = -transEdgeSign( o1, d2, d1 );
            if( z1+z2 < 0 ) { z1 = -z1; z2 = -z2; }
            v->t = interpolate( z1, o2->t, z2, d2->t );
        }
    }
    
    /*
     Calculate the angle between v1-v2 and v1-v0
     */
    static inline float calcAngle( const TESSvertex *v0, const TESSvertex *v1, const TESSvertex *v2 )
    {
        float num;
        float den;
        float a[2];
        float b[2];
        a[0] = v2->s - v1->s;
        a[1] = v2->t - v1->t;
        b[0] = v0->s - v1->s;
        b[1] = v0->t - v1->t;
        num = a[0] * b[0] + a[1] * b[1];
        den = sqrt( a[0] * a[0] + a[1] * a[1] ) * sqrt( b[0] * b[0] + b[1] * b[1] );
        if ( den > 0.0 ) num /= den;
        if ( num < -1.0 ) num = -1.0;
        if ( num >  1.0 ) num =  1.0;
        return acos( num );
    }
    
    bool edgeIsLocallyDelaunay( const TESShalfEdge *e )
    {
        return (calcAngle(e->Lnext()->Org(), e->Lnext()->Lnext()->Org(), e->Org()) +
                calcAngle(e->Sym()->Lnext()->Org(), e->Sym()->Lnext()->Lnext()->Org(), e->Sym()->Org())) < (M_PI + 0.01);
    }
}