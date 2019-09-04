/*
 *  Copyright (C) 2006-2019 SplinterGU (Fenix/BennuGD)
 *
 *  This file is part of Bennu Game Development
 *
 *  This software is provided 'as-is', without any express or implied
 *  warranty. In no event will the authors be held liable for any damages
 *  arising from the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *     1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 *
 *     2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 *
 *     3. This notice may not be removed or altered from any source
 *     distribution.
 *
 */

/* --------------------------------------------------------------------------- */

#include <SDL.h>

#include <stdlib.h>

#include "bgdrtm.h"
#include "bgddl.h"

#include "resolution.h"

#include "m_resolxy.h"

#include "libbggfx.h"
#include "libmod_gfx.h"

#include "fmath.h"

#include "dlvaracc.h"

/* --------------------------------------------------------------------------- */

typedef struct {
    CBOX cbox;
    double  vertices[8];
    BGD_Box limits;
} __cbox_info;

typedef struct {
    int64_t x;
    int64_t y;
    int64_t width;
    int64_t height;
    double  center_x;
    double  center_y;
    int64_t angle;
    int64_t flags;
    double  scale_x;
    double  scale_y;
    int64_t ncboxes;
    __cbox_info * cboxes;
    int64_t ncboxes_box;
    int64_t ncboxes_circle;
} __obj_col_info;

/* --------------------------------------------------------------------------- */

#define in_radius(x,y,x2,y2,radius)             ((x-x2)*(x-x2)+(y-y2)*(y-y2))<((radius)*(radius))
#define in_radius2(x,y,x2,y2,radius,radius2)    ((x-x2)*(x-x2)+(y-y2)*(y-y2))<((radius+radius2)*(radius+radius2))

#define FREE(a) free(a);a=NULL

/* --------------------------------------------------------------------------- */
// scale must be with decimal points

static inline void __calculate_shape( __obj_col_info * oci )
{
    int i;
    double sx = 1, sy = -1;

    if ( oci->scale_x < 0.0 ) oci->scale_x = 0.0;
    if ( oci->scale_y < 0.0 ) oci->scale_y = 0.0;

    if ( oci->flags & B_HMIRROR ) sx = -1;
    if ( oci->flags & B_VMIRROR ) sy = 1;

    double cos_angle = cos_deg( oci->angle );
    double sin_angle = sin_deg( oci->angle );

    double  lef_x, top_y, rig_x, bot_y,
            delta_x, delta_y;

#define CALCULATE_SHAPE_BOX \
                    lef_x = oci->scale_x * ( oci->cboxes[i].cbox.x - oci->center_x ); \
                    rig_x = oci->scale_x * ( oci->cboxes[i].cbox.width - 1 ) + lef_x; \
                    top_y = oci->scale_y * ( oci->cboxes[i].cbox.y - oci->center_y ); \
                    bot_y = oci->scale_y * ( oci->cboxes[i].cbox.height - 1 ) + top_y; \
                    oci->cboxes[i].vertices[0] = ( lef_x * cos_angle + top_y * sin_angle ) * sx + oci->x; \
                    oci->cboxes[i].vertices[1] = ( lef_x * sin_angle - top_y * cos_angle ) * sy + oci->y; \
                    oci->cboxes[i].vertices[2] = ( rig_x * cos_angle + top_y * sin_angle ) * sx + oci->x; \
                    oci->cboxes[i].vertices[3] = ( rig_x * sin_angle - top_y * cos_angle ) * sy + oci->y; \
                    oci->cboxes[i].vertices[4] = ( rig_x * cos_angle + bot_y * sin_angle ) * sx + oci->x; \
                    oci->cboxes[i].vertices[5] = ( rig_x * sin_angle - bot_y * cos_angle ) * sy + oci->y; \
                    oci->cboxes[i].vertices[6] = ( lef_x * cos_angle + bot_y * sin_angle ) * sx + oci->x; \
                    oci->cboxes[i].vertices[7] = ( lef_x * sin_angle - bot_y * cos_angle ) * sy + oci->y;

#define CALCULATE_SHAPE_CIRCLE \
                    delta_x = oci->scale_x * ( oci->cboxes[i].cbox.x - oci->center_x ); \
                    delta_y = oci->scale_y * ( oci->cboxes[i].cbox.y - oci->center_y ); \
                    oci->cboxes[i].vertices[0] = ( delta_x * cos_angle + delta_y * sin_angle ) * sx + oci->x; \
                    oci->cboxes[i].vertices[1] = ( delta_x * sin_angle - delta_y * cos_angle ) * sy + oci->y;

    if ( oci->ncboxes_box && oci->ncboxes_circle ) {
        for ( i = 0; i < oci->ncboxes; i++ ) {
            switch ( oci->cboxes[i].cbox.shape ) {
                case BITMAP_CB_SHAPE_BOX:
                    CALCULATE_SHAPE_BOX
                    break;

                case BITMAP_CB_SHAPE_CIRCLE:
                    CALCULATE_SHAPE_CIRCLE
                    break;
            }
        }
    } else if ( oci->ncboxes_box ) {
        for ( i = 0; i < oci->ncboxes; i++ ) {
            CALCULATE_SHAPE_BOX
        }
    } else if ( oci->ncboxes_circle ) {
        for ( i = 0; i < oci->ncboxes; i++ ) {
            CALCULATE_SHAPE_CIRCLE
        }
    }
}

/* --------------------------------------------------------------------------- */

static inline void __normalize_circle( double * vertices, int64_t x, int64_t y, int64_t angle, int64_t flags, double *normalized_vertices ) {
    if ( flags & B_HMIRROR ) angle = -angle;
    if ( flags & B_VMIRROR ) angle = -angle;

    double cos_angle = cos_deg( angle );
    double sin_angle = sin_deg( angle );

    double x1 = vertices[0] - x;
    double y1 = vertices[1] - y;

    normalized_vertices[0] = ( x1 * cos_angle - y1 * sin_angle ) + x;
    normalized_vertices[1] = ( x1 * sin_angle + y1 * cos_angle ) + y;
}

/* --------------------------------------------------------------------------- */

static inline void __normalize_box( double * vertices, int64_t x, int64_t y, int64_t angle, int64_t flags, double *normalized_vertices ) {
    double cos_angle;
    double sin_angle;
    double x1, x2, x3, x4;
    double y1, y2, y3, y4;

    if ( flags & B_HMIRROR ) angle = -angle;
    if ( flags & B_VMIRROR ) angle = -angle;

    cos_angle = cos_deg( angle );
    sin_angle = sin_deg( angle );

    x1 = vertices[0] - x;
    y1 = vertices[1] - y;
    x2 = vertices[2] - x;
    y2 = vertices[3] - y;
    x3 = vertices[4] - x;
    y3 = vertices[5] - y;
    x4 = vertices[6] - x;
    y4 = vertices[7] - y;

    normalized_vertices[0] = ( x1 * cos_angle - y1 * sin_angle ) + x;
    normalized_vertices[1] = ( x1 * sin_angle + y1 * cos_angle ) + y;
    normalized_vertices[2] = ( x2 * cos_angle - y2 * sin_angle ) + x;
    normalized_vertices[3] = ( x2 * sin_angle + y2 * cos_angle ) + y;
    normalized_vertices[4] = ( x3 * cos_angle - y3 * sin_angle ) + x;
    normalized_vertices[5] = ( x3 * sin_angle + y3 * cos_angle ) + y;
    normalized_vertices[6] = ( x4 * cos_angle - y4 * sin_angle ) + x;
    normalized_vertices[7] = ( x4 * sin_angle + y4 * cos_angle ) + y;
}

/* --------------------------------------------------------------------------- */

static inline void __get_vertices_projection( double * normalized_vertices, BGD_Box * limits ) {
    limits->x = limits->x2 = normalized_vertices[0];
    limits->y = limits->y2 = normalized_vertices[1];

    if ( limits->x  > normalized_vertices[2] ) limits->x  = normalized_vertices[2];
    if ( limits->x2 < normalized_vertices[2] ) limits->x2 = normalized_vertices[2];
    if ( limits->y  > normalized_vertices[3] ) limits->y  = normalized_vertices[3];
    if ( limits->y2 < normalized_vertices[3] ) limits->y2 = normalized_vertices[3];
    if ( limits->x  > normalized_vertices[4] ) limits->x  = normalized_vertices[4];
    if ( limits->x2 < normalized_vertices[4] ) limits->x2 = normalized_vertices[4];
    if ( limits->y  > normalized_vertices[5] ) limits->y  = normalized_vertices[5];
    if ( limits->y2 < normalized_vertices[5] ) limits->y2 = normalized_vertices[5];
    if ( limits->x  > normalized_vertices[6] ) limits->x  = normalized_vertices[6];
    if ( limits->x2 < normalized_vertices[6] ) limits->x2 = normalized_vertices[6];
    if ( limits->y  > normalized_vertices[7] ) limits->y  = normalized_vertices[7];
    if ( limits->y2 < normalized_vertices[7] ) limits->y2 = normalized_vertices[7];
}

/* --------------------------------------------------------------------------- */

static inline void __calculate_box_limits( __obj_col_info * oci ) {
    int i;
    for ( i = 0; i < oci->ncboxes; i++ ) {
        if ( oci->cboxes[i].cbox.shape == BITMAP_CB_SHAPE_BOX ) {
            if ( oci->flags & B_HMIRROR ) {
                oci->cboxes[i].limits.x2 = oci->x + oci->scale_x * ( oci->center_x - oci->cboxes[i].cbox.x );
                oci->cboxes[i].limits.x = oci->cboxes[i].limits.x2 - oci->scale_x * ( oci->cboxes[i].cbox.width - 1 );
            } else {
                oci->cboxes[i].limits.x = oci->x + oci->scale_x * ( oci->cboxes[i].cbox.x - oci->center_x );
                oci->cboxes[i].limits.x2 = oci->cboxes[i].limits.x + oci->scale_x * ( oci->cboxes[i].cbox.width - 1 );
            }

            if ( oci->flags & B_VMIRROR ) {
                oci->cboxes[i].limits.y2 = oci->y + oci->scale_y * ( oci->center_y - oci->cboxes[i].cbox.y );
                oci->cboxes[i].limits.y = oci->cboxes[i].limits.y2 - oci->scale_y * ( oci->cboxes[i].cbox.height - 1 );
            } else {
                oci->cboxes[i].limits.y = oci->y + oci->scale_y * ( oci->cboxes[i].cbox.y - oci->center_y );
                oci->cboxes[i].limits.y2 = oci->cboxes[i].limits.y + oci->scale_y * ( oci->cboxes[i].cbox.height - 1 );
            }
        }
    }
}

/* --------------------------------------------------------------------------- */

static int __get_proc_info(
        INSTANCE * proc,
        __obj_col_info * oci // Box
) {
    GRAPH * graph;

    graph = instance_graph( proc );
    if ( !graph ) return 0;

    oci->scale_x = ( double ) LOCINT64( libmod_gfx, proc, GRAPHSIZEX );
    oci->scale_y = ( double ) LOCINT64( libmod_gfx, proc, GRAPHSIZEY );
    if ( oci->scale_x == 100.0 && oci->scale_y == 100.0 ) oci->scale_x = oci->scale_y = ( double ) LOCINT64( libmod_gfx, proc, GRAPHSIZE );

    oci->scale_x /= 100.0;
    oci->scale_y /= 100.0;

    oci->x = LOCINT64( libmod_gfx, proc, COORDX );
    oci->y = LOCINT64( libmod_gfx, proc, COORDY );

    RESOLXY( libmod_gfx, proc, oci->x, oci->y );

    oci->width  = LOCINT64( libmod_gfx, proc, CLIPW );
    oci->height = LOCINT64( libmod_gfx, proc, CLIPH );

    if ( !oci->width || !oci->height ) {
        oci->width  = graph->width;
        oci->height = graph->height;
    }

    /* Calculate the graphic center */

    oci->center_x = LOCINT64( libmod_gfx, proc, GRAPHCENTERX ) ;
    oci->center_y = LOCINT64( libmod_gfx, proc, GRAPHCENTERY ) ;
    if ( oci->center_x == POINT_UNDEFINED || oci->center_y == POINT_UNDEFINED ) {
        if ( graph->ncpoints && graph->cpoints[0].x != CPOINT_UNDEFINED ) {
            oci->center_x = graph->cpoints[0].x;
            oci->center_y = graph->cpoints[0].y;
        } else {
            oci->center_x = oci->width  / 2.0;
            oci->center_y = oci->height / 2.0;
        }
    }

    oci->angle = LOCINT64( libmod_gfx, proc, ANGLE );
    oci->flags = LOCQWORD( libmod_gfx, proc, FLAGS );

    int i;
    if ( graph->ncboxes ) {
        oci->cboxes = malloc( graph->ncboxes * sizeof( __cbox_info ) );
        if ( !oci->cboxes ) return 0;
        oci->ncboxes = graph->ncboxes;
        for ( i = 0; i < oci->ncboxes; i++ ) oci->cboxes[i].cbox = graph->cboxes[i];
    } else { // No graph cbox defined, use a virtual cbox
        oci->cboxes = malloc( sizeof( __cbox_info ) );
        if ( !oci->cboxes ) return 0;
        oci->ncboxes = 1;
        oci->cboxes->cbox.code   = -1;
        oci->cboxes->cbox.shape  = LOCINT64( libmod_gfx, proc, CSHAPE );
        oci->cboxes->cbox.x      = LOCINT64( libmod_gfx, proc, CBOX_X );
        oci->cboxes->cbox.y      = LOCINT64( libmod_gfx, proc, CBOX_Y );
        oci->cboxes->cbox.width  = LOCINT64( libmod_gfx, proc, CBOX_WIDTH );
        oci->cboxes->cbox.height = LOCINT64( libmod_gfx, proc, CBOX_HEIGHT );
    }

    oci->ncboxes_box = 0;
    oci->ncboxes_circle = 0;

    for ( i = 0; i < oci->ncboxes; i++ ) {
        switch ( oci->cboxes[i].cbox.shape ) {
            case BITMAP_CB_SHAPE_BOX:
            default:
                oci->cboxes[i].cbox.x = oci->cboxes[i].cbox.x == POINT_UNDEFINED ? 0 : oci->cboxes[i].cbox.x;
                oci->cboxes[i].cbox.y = oci->cboxes[i].cbox.y == POINT_UNDEFINED ? 0 : oci->cboxes[i].cbox.y;
                oci->cboxes[i].cbox.width = ( oci->cboxes[i].cbox.width < 1 ) ? oci->width : oci->cboxes[i].cbox.width;
                oci->cboxes[i].cbox.height = ( oci->cboxes[i].cbox.height < 1 ) ? oci->height : oci->cboxes[i].cbox.height;
                oci->ncboxes_box++;
                break;

            case BITMAP_CB_SHAPE_CIRCLE:
                oci->cboxes[i].cbox.x = oci->cboxes[i].cbox.x == POINT_UNDEFINED ? oci->center_x : oci->cboxes[i].cbox.x;
                oci->cboxes[i].cbox.y = oci->cboxes[i].cbox.y == POINT_UNDEFINED ? oci->center_y : oci->cboxes[i].cbox.y;

                // Width is radius
                if ( oci->cboxes[i].cbox.width < 1 ) {
                    switch ( oci->cboxes[i].cbox.width ) {
                        case BITMAP_CB_CIRCLE_GRAPH_SIZE:
                        case BITMAP_CB_CIRCLE_GRAPH_AVERAGE_SIZE:
                        default:
                            oci->cboxes[i].cbox.width = 0.5 + ( oci->width + oci->height ) / 4.0;
                            break;

                        case BITMAP_CB_CIRCLE_GRAPH_WIDTH:
                            oci->cboxes[i].cbox.width = 0.5 + oci->width / 2.0;
                            break;

                        case BITMAP_CB_CIRCLE_GRAPH_HEIGHT:
                            oci->cboxes[i].cbox.width = 0.5 + oci->height / 2.0;
                            break;

                        case BITMAP_CB_CIRCLE_GRAPH_MIN_SIZE:
                            oci->cboxes[i].cbox.width = 0.5 + MIN( oci->width, oci->height ) / 2.0;
                            break;

                        case BITMAP_CB_CIRCLE_GRAPH_MAX_SIZE:
                            oci->cboxes[i].cbox.width = 0.5 + MAX( oci->width, oci->height ) / 2.0;
                            break;
                    }
                }
                oci->ncboxes_circle++;
                break;
        }
    }

    return 1;
}

/* --------------------------------------------------------------------------- */

static int __get_mouse_info( __obj_col_info * oci ) {

    oci->x = GLOINT64( libmod_gfx, MOUSEX );
    oci->y = GLOINT64( libmod_gfx, MOUSEY );

    if ( oci->x > scr_width || oci->y > scr_height || oci->x < 0 || oci->y < 0 ) return 0;

    oci->scale_x = 1.0;
    oci->scale_y = 1.0;

    oci->width  = 1;
    oci->height = 1;

    /* Calculate the graphic center */

    oci->center_x = 0;
    oci->center_y = 0;

    oci->angle = 0;
    oci->flags = 0;

    oci->cboxes = malloc( sizeof( __cbox_info ) );
    if ( !oci->cboxes ) return 0;
    oci->ncboxes = 1;
    oci->cboxes->cbox.code   = -1;
    oci->cboxes->cbox.shape  = BITMAP_CB_SHAPE_BOX;
    oci->cboxes->cbox.x      = 0;
    oci->cboxes->cbox.y      = 0;
    oci->cboxes->cbox.width  = 1;
    oci->cboxes->cbox.height = 1;

    oci->ncboxes_box = 1;
    oci->ncboxes_circle = 0;

    return 1;
}

/* --------------------------------------------------------------------------- */

static inline int __is_collision_box_circle( BGD_Box * limits, int64_t radius, double * normalized_vertices ) {
    double closest_x, closest_y;

    // Find the unrotated closest x point from center of unrotated circle
    if ( normalized_vertices[0] < limits->x )       closest_x = limits->x;
    else if ( normalized_vertices[0] > limits->x2 ) closest_x = limits->x2;
    else                                            closest_x = normalized_vertices[0];

    // Find the unrotated closest y point from center of unrotated circle
    if ( normalized_vertices[1] < limits->y )       closest_y = limits->y;
    else if ( normalized_vertices[1] > limits->y2 ) closest_y = limits->y2;
    else                                            closest_y = normalized_vertices[1];

    return in_radius( closest_x, closest_y, normalized_vertices[0], normalized_vertices[1], radius );

}

/* --------------------------------------------------------------------------- */

static inline int __is_collision_circle_circle( int64_t radiusA, double * verticesA, int64_t radiusB, double * verticesB ) {
    return in_radius2( verticesA[0], verticesA[1], verticesB[0], verticesB[1], radiusA, radiusB );
}

/* --------------------------------------------------------------------------- */
/*  ociA is precalculated, is collider */

static inline int __check_collision( __obj_col_info * ociA, __obj_col_info * ociB, int64_t * idxA, int64_t * idxB, int64_t * cbox_result_code ) {
    int collisionA, collisionB;
    int64_t shapeA, shapeB;
    BGD_Box projectionA, projectionB;
    double normalized_verticesA[8], normalized_verticesB[8], normalized_verticesC[8];

    // Get real vertices
    __calculate_shape( ociB );
    if ( ociB->ncboxes_box ) __calculate_box_limits( ociB );

#define __NORMALIZE_BOX(A,B,idxA) \
                __normalize_box( oci##A->cboxes[idxA].vertices, oci##B->x, oci##B->y, oci##B->angle, oci##B->flags, normalized_vertices##A ); /* Normalize B Box axis */ \
                __get_vertices_projection( normalized_vertices##A, &projection##A ); /* Get limits */

#define __NORMALIZE_CIRCLE(A,B,idxA) \
                __normalize_circle( oci##A->cboxes[idxA].vertices, oci##B->x, oci##B->y, oci##B->angle, oci##B->flags, normalized_vertices##A ); /* Normalize B Box axis */

#define __CHECK_LIMITS(A,B,idxA,idxB) \
                collision##A = !( projection##A.x > oci##B->cboxes[idxB].limits.x2 || projection##A.x2 < oci##B->cboxes[idxB].limits.x || projection##A.y > oci##B->cboxes[idxB].limits.y2 || projection##A.y2 < oci##B->cboxes[idxB].limits.y );

#define __CHECK_COLLISION_BOX_BOX(A,B,idxA,idxB) \
                __CHECK_LIMITS(A,B,idxA,idxB) \
                /* If not collisionA then don't need check for second box */ \
                if ( collisionA ) { \
                    __NORMALIZE_BOX(B,A,idxB) \
                    __CHECK_LIMITS(B,A,idxB,idxA) \
                }

#define __CHECK_COLLISION_CIRCLE_BOX(A,B,idxA,idxB) \
                collisionB = __is_collision_box_circle( &(oci##A->cboxes[idxA].limits), oci##B->cboxes[idxB].cbox.width * oci##B->scale_x, normalized_vertices##B );

#define __CHECK_COLLISION_BOX_CIRCLE(A,B,idxA,idxB) \
                __NORMALIZE_CIRCLE(B,A,idxB) \
                __CHECK_COLLISION_CIRCLE_BOX(A,B,idxA,idxB)

#define __CHECK_COLLISION_CIRCLE_CIRCLE(A,B,idxA,idxB) \
                collision##B = __is_collision_circle_circle( oci##A->cboxes[idxA].cbox.width * oci##A->scale_x, oci##A->cboxes[idxA].vertices, oci##B->cboxes[idxB].cbox.width * oci##B->scale_x, oci##B->cboxes[idxB].vertices );

#define IS_COLLISION(A,B,idxA,idxB) \
                if ( collisionB ) { \
                    cbox_result_code[0] = oci##A->cboxes[idxA].cbox.code; \
                    cbox_result_code[1] = oci##B->cboxes[idxB].cbox.code; \
                    return 1; \
                }

    /**** BOX A + CIRCLE A ****/
    if ( ociA->ncboxes_box && ociA->ncboxes_circle ) {
        if ( ociB->ncboxes_box && ociB->ncboxes_circle ) { /* BOX A + CIRCLE A + BOX B + CIRCLE B */
            for ( ; *idxA < ociA->ncboxes; (*idxA)++ ) {
                switch ( ociA->cboxes[*idxA].cbox.shape ) {
                    case BITMAP_CB_SHAPE_BOX:
                        __NORMALIZE_BOX(A,B,*idxA)
                        for ( ; *idxB < ociB->ncboxes; (*idxB)++ ) {
                            collisionB = 0;
                            switch ( ociB->cboxes[*idxB].cbox.shape ) {
                                case BITMAP_CB_SHAPE_BOX:
                                    __CHECK_COLLISION_BOX_BOX(A,B,*idxA,*idxB)
                                    break;

                                case BITMAP_CB_SHAPE_CIRCLE:
                                    __CHECK_COLLISION_BOX_CIRCLE(A,B,*idxA,*idxB)
                                    break;
                            }
                            IS_COLLISION(A,B,*idxA,*idxB)
                        }
                        break;

                    case BITMAP_CB_SHAPE_CIRCLE:
                        __NORMALIZE_CIRCLE(A,B,*idxA)
                        for ( ; *idxB < ociB->ncboxes; (*idxB)++ ) {
                            collisionB = 0;
                            switch ( ociB->cboxes[*idxB].cbox.shape ) {
                                case BITMAP_CB_SHAPE_BOX:
                                    __CHECK_COLLISION_CIRCLE_BOX(B,A,*idxB,*idxA)
                                    break;

                                case BITMAP_CB_SHAPE_CIRCLE:
                                    __CHECK_COLLISION_CIRCLE_CIRCLE(A,B,*idxA,*idxB)
                                    break;
                            }
                            IS_COLLISION(A,B,*idxA,*idxB)
                        }
                        break;
                }
                *idxB = 0;
            }
        } else if ( ociB->ncboxes_box ) { /* BOX A + CIRCLE A + BOX B */
            for ( ; *idxA < ociA->ncboxes; (*idxA)++ ) {
                switch ( ociA->cboxes[*idxA].cbox.shape ) {
                    case BITMAP_CB_SHAPE_BOX:
                        __NORMALIZE_BOX(A,B,*idxA)
                        for ( ; *idxB < ociB->ncboxes; (*idxB)++ ) {
                            collisionB = 0;
                            __CHECK_COLLISION_BOX_BOX(A,B,*idxA,*idxB)
                            IS_COLLISION(A,B,*idxA,*idxB)
                        }
                        break;

                    case BITMAP_CB_SHAPE_CIRCLE:
                        __NORMALIZE_CIRCLE(A,B,*idxA)
                        for ( ; *idxB < ociB->ncboxes; (*idxB)++ ) {
                            __CHECK_COLLISION_CIRCLE_BOX(B,A,*idxB,*idxA)
                            IS_COLLISION(A,B,*idxA,*idxB)
                        }
                        break;
                }
                *idxB = 0;
            }
        } else if ( ociB->ncboxes_circle ) { /* BOX A + CIRCLE A + CIRCLE B */
            for ( ; *idxA < ociA->ncboxes; (*idxA)++ ) {
                switch ( ociA->cboxes[*idxA].cbox.shape ) {
                    case BITMAP_CB_SHAPE_BOX:
                        __NORMALIZE_BOX(A,B,*idxA)
                        for ( ; *idxB < ociB->ncboxes; (*idxB)++ ) {
                            __CHECK_COLLISION_BOX_CIRCLE(A,B,*idxA,*idxB)
                            IS_COLLISION(A,B,*idxA,*idxB)
                        }
                        break;

                    case BITMAP_CB_SHAPE_CIRCLE:
                        __NORMALIZE_CIRCLE(A,B,*idxA)
                        for ( ; *idxB < ociB->ncboxes; (*idxB)++ ) {
                            __CHECK_COLLISION_CIRCLE_CIRCLE(A,B,*idxA,*idxB)
                            IS_COLLISION(A,B,*idxA,*idxB)
                        }
                        break;
                }
                *idxB = 0;
            }
        }
    /**** BOX A ****/
    } else if ( ociA->ncboxes_box ) {
        if ( ociB->ncboxes_box && ociB->ncboxes_circle ) { /* BOX A + BOX B + CIRCLE B */
            for ( ; *idxA < ociA->ncboxes; (*idxA)++ ) {
                __NORMALIZE_BOX(A,B,*idxA)
                for ( ; *idxB < ociB->ncboxes; (*idxB)++ ) {
                    collisionB = 0;
                    switch ( ociB->cboxes[*idxB].cbox.shape ) {
                        case BITMAP_CB_SHAPE_BOX:
                            __CHECK_COLLISION_BOX_BOX(A,B,*idxA,*idxB)
                            break;

                        case BITMAP_CB_SHAPE_CIRCLE:
                            __CHECK_COLLISION_BOX_CIRCLE(A,B,*idxA,*idxB)
                            break;
                    }
                    IS_COLLISION(A,B,*idxA,*idxB)
                }
                *idxB = 0;
            }
        } else if ( ociB->ncboxes_box ) { /* BOX A + BOX B */
            for ( ; *idxA < ociA->ncboxes; (*idxA)++ ) {
                __NORMALIZE_BOX(A,B,*idxA)
                for ( ; *idxB < ociB->ncboxes; (*idxB)++ ) {
                    collisionB = 0;
                    __CHECK_COLLISION_BOX_BOX(A,B,*idxA,*idxB)
                    IS_COLLISION(A,B,*idxA,*idxB)
                }
                *idxB = 0;
            }
        } else if ( ociB->ncboxes_circle ) { /* BOX A + CIRCLE B */
            for ( ; *idxA < ociA->ncboxes; (*idxA)++ ) {
                __NORMALIZE_BOX(A,B,*idxA)
                for ( ; *idxB < ociB->ncboxes; (*idxB)++ ) {
                    __CHECK_COLLISION_BOX_CIRCLE(A,B,*idxA,*idxB)
                    IS_COLLISION(A,B,*idxA,*idxB)
                }
                *idxB = 0;
            }
        }
    /**** CIRCLE ****/
    } else if ( ociA->ncboxes_circle ) {
        if ( ociB->ncboxes_box && ociB->ncboxes_circle ) { /* CIRCLE A + BOX B + CIRCLE B */
            for ( ; *idxA < ociA->ncboxes; (*idxA)++ ) {
                __NORMALIZE_CIRCLE(A,B,*idxA)
                for ( ; *idxB < ociB->ncboxes; (*idxB)++ ) {
                    collisionB = 0;
                    switch ( ociB->cboxes[*idxB].cbox.shape ) {
                        case BITMAP_CB_SHAPE_BOX:
                            __CHECK_COLLISION_CIRCLE_BOX(B,A,*idxB,*idxA)
                            break;

                        case BITMAP_CB_SHAPE_CIRCLE:
                            __CHECK_COLLISION_CIRCLE_CIRCLE(A,B,*idxA,*idxB)
                            break;
                    }
                    IS_COLLISION(A,B,*idxA,*idxB)
                }
                *idxB = 0;

            }
        } else if ( ociB->ncboxes_box ) { /* CIRCLE A + BOX B */
            for ( ; *idxA < ociA->ncboxes; (*idxA)++ ) {
                __NORMALIZE_CIRCLE(A,B,*idxA)
                for ( ; *idxB < ociB->ncboxes; (*idxB)++ ) {
                    __CHECK_COLLISION_CIRCLE_BOX(B,A,*idxB,*idxA)
                    IS_COLLISION(A,B,*idxA,*idxB)
                }
                *idxB = 0;
            }
        } else if ( ociB->ncboxes_circle ) { /* CIRCLE A + CIRCLE B */
            for ( ; *idxA < ociA->ncboxes; (*idxA)++ ) {
                __NORMALIZE_CIRCLE(A,B,*idxA)
                for ( ; *idxB < ociB->ncboxes; (*idxB)++ ) {
                    __CHECK_COLLISION_CIRCLE_CIRCLE(A,B,*idxA,*idxB)
                    IS_COLLISION(A,B,*idxA,*idxB)
                }
                *idxB = 0;
            }
        }
    }
    *idxA = 0;

    return 0;
}

/* --------------------------------------------------------------------------- */

static __obj_col_info __ociA = { 0 }, __ociB = { 0 };

static int64_t __collision( INSTANCE * my, int64_t id ) {
    int             collision = 0;
    INSTANCE        * ptr,
                    ** ctx;
    __obj_col_info  * ociA = &__ociA,
                    * ociB = &__ociB;
    uint64_t        id_scroll = LOCQWORD( libmod_gfx, my, COLLISION_RESERVED_ID_SCROLL );
    int64_t         cbox_result_code[2],
                    * idxA = ( int64_t * ) LOCADDR( libmod_gfx, my, COLLISION_RESERVED_IDX_CBOXA ),
                    * idxB = ( int64_t * ) LOCADDR( libmod_gfx, my, COLLISION_RESERVED_IDX_CBOXB ),
                    mode = LOCINT64( libmod_gfx, my, COLLISION_RESERVED_MODE ); /* Mode:
                                                                                    0 - mouse
                                                                                    1 - single instance
                                                                                    2 - all instances
                                                                                    3 - process type
                                                                                */

    if ( !__get_proc_info( my, ociA ) ) return 0;

    // Get real vertices
    __calculate_shape( ociA );
    if ( ociA->ncboxes_box ) __calculate_box_limits( ociA );

    /* Checks collision with mouse */
    if ( id == -1 ) {
        collision = 0;
        if ( __get_mouse_info( ociB ) ) {
            if ( mode != 0 || id_scroll >= MAX_SCROLLS ) {
                LOCINT64( libmod_gfx, my, COLLISION_RESERVED_MODE ) = 0;
                LOCQWORD( libmod_gfx, my, COLLISION_RESERVED_ID_SCAN ) = 0;
                LOCQWORD( libmod_gfx, my, COLLISION_RESERVED_TYPE_SCAN ) = 0;
                id_scroll = 0;
                * idxA = 0;
                * idxB = 0;
            }

            if ( LOCQWORD( libmod_gfx, my, CTYPE ) == C_SCROLL ) {
                int i;

                int64_t cnumber = LOCQWORD( libmod_gfx, my, CNUMBER );
                if ( !cnumber ) cnumber = 0xffffffffffffffff;

                for ( ; id_scroll < MAX_SCROLLS && !collision; id_scroll++ ) {
                    if ( scrolls[id_scroll].active && ( cnumber & ( 1 << (id_scroll) ) ) ) {
                        REGION * r = scrolls[id_scroll].region;

                        if ( ociB->x > r->x2 || ociB->y > r->y2 || ociB->x < r->x || ociB->y < r->y ) continue;

                        ociB->x += scrolls[id_scroll].posx0 - r->x;
                        ociB->y += scrolls[id_scroll].posy0 - r->y;

                        collision = __check_collision( ociA, ociB, idxA, idxB, cbox_result_code ) ;

                        ociB->x -= scrolls[id_scroll].posx0 - r->x;
                        ociB->y -= scrolls[id_scroll].posy0 - r->y;
                    }
                }
                FREE( ociB->cboxes );
                FREE( ociA->cboxes );
                if ( collision ) {
                    (*idxB)++;
                    LOCQWORD( libmod_gfx, my, COLLISION_RESERVED_ID_SCROLL ) = id_scroll;
                    LOCINT64( libmod_gfx, my, COLLIDER_CBOX ) = -1;
                    LOCINT64( libmod_gfx, my, COLLIDED_ID   ) = -1;
                    LOCINT64( libmod_gfx, my, COLLIDED_CBOX ) = cbox_result_code[1];
                    return 1;
                }
                return 0;
            }
            LOCQWORD( libmod_gfx, my, COLLISION_RESERVED_ID_SCROLL ) = 0;
            collision = __check_collision( ociA, ociB, idxA, idxB, cbox_result_code ) ;
            FREE( ociB->cboxes );
        }
        FREE( ociA->cboxes );
        if ( collision ) {
            (*idxB)++;
            LOCINT64( libmod_gfx, my, COLLIDER_CBOX ) = -1;
            LOCINT64( libmod_gfx, my, COLLIDED_ID   ) = -1;
            LOCINT64( libmod_gfx, my, COLLIDED_CBOX ) = cbox_result_code[1];
            return 1;
        }
        LOCINT64( libmod_gfx, my, COLLISION_RESERVED_MODE ) = -1;
        return 0;
    }

    int64_t ctype = LOCQWORD( libmod_gfx, my, CTYPE );
    int64_t id_scan = LOCQWORD( libmod_gfx, my, COLLISION_RESERVED_ID_SCAN );

    /* SINGLE INSTANCE */

    /* Checks only for a single instance */
    if ( id >= FIRST_INSTANCE_ID ) {
        if ( mode != 1 || id_scan != id ) {
            LOCINT64( libmod_gfx, my, COLLISION_RESERVED_MODE ) = 1;
            LOCQWORD( libmod_gfx, my, COLLISION_RESERVED_ID_SCAN ) = 0;
            * idxA = 0;
            * idxB = 0;
        }

        ptr = instance_get( id );
        if ( ptr && ctype == LOCQWORD( libmod_gfx, ptr, CTYPE ) && LOCQWORD( libmod_gfx, ptr, STATUS ) & ( STATUS_RUNNING | STATUS_FROZEN ) && ptr != my ) {
            if ( __get_proc_info( ptr, ociB ) ) {
                collision = __check_collision( ociA, ociB, idxA, idxB, cbox_result_code ) ;
                FREE( ociB->cboxes );
                if ( collision ) {
                    (*idxB)++;
                    FREE( ociA->cboxes );

                    LOCQWORD( libmod_gfx, my, COLLISION_RESERVED_ID_SCAN ) = LOCQWORD( libmod_gfx, ptr, PROCESS_ID );
                    LOCINT64( libmod_gfx, my, COLLIDER_CBOX ) = cbox_result_code[0];
                    LOCINT64( libmod_gfx, my, COLLIDED_ID   ) = LOCQWORD( libmod_gfx, ptr, PROCESS_ID );
                    LOCINT64( libmod_gfx, my, COLLIDED_CBOX ) = cbox_result_code[1];
                    return LOCQWORD( libmod_gfx, ptr, PROCESS_ID );;
                }
            }
        }
        FREE( ociA->cboxes );
        LOCINT64( libmod_gfx, my, COLLISION_RESERVED_MODE ) = -1;
        return 0;
    }

    /* ALL INSTANCES */

    /* we must use full list of instances or get types from it */
    if ( !id ) {
        ptr = first_instance;

        if ( mode != 2 || !id_scan ) {
            LOCINT64( libmod_gfx, my, COLLISION_RESERVED_MODE ) = 2;
            LOCQWORD( libmod_gfx, my, COLLISION_RESERVED_ID_SCAN ) = 0;
            * idxA = 0;
            * idxB = 0;
        }

        if ( id_scan ) ptr = instance_get( id_scan );

        while ( ptr ) {
            if ( ctype == LOCQWORD( libmod_gfx, ptr, CTYPE ) && LOCQWORD( libmod_gfx, ptr, STATUS ) & ( STATUS_RUNNING | STATUS_FROZEN ) && ptr != my ) {
                if ( __get_proc_info( ptr, ociB ) ) {
                    collision = __check_collision( ociA, ociB, idxA, idxB, cbox_result_code ) ;
                    FREE( ociB->cboxes );
                    if ( collision ) {
                        (*idxB)++;
                        FREE( ociA->cboxes );
                        LOCQWORD( libmod_gfx, my, COLLISION_RESERVED_ID_SCAN ) = LOCQWORD( libmod_gfx, ptr, PROCESS_ID );
                        LOCINT64( libmod_gfx, my, COLLIDER_CBOX ) = cbox_result_code[0];
                        LOCINT64( libmod_gfx, my, COLLIDED_ID   ) = LOCQWORD( libmod_gfx, ptr, PROCESS_ID );
                        LOCINT64( libmod_gfx, my, COLLIDED_CBOX ) = cbox_result_code[1];
                        return LOCQWORD( libmod_gfx, ptr, PROCESS_ID );;
                    }
                }
            }
            ptr = ptr->next;
        }
        FREE( ociA->cboxes );
        LOCINT64( libmod_gfx, my, COLLISION_RESERVED_MODE ) = -1;
        return 0;
    }

    /* TYPE */

    /* Check if already in scan by type and we reach limit */

    ctx = ( INSTANCE ** ) LOCADDR( libmod_gfx, my, COLLISION_RESERVED_CONTEXT );

    if ( mode != 3 || LOCQWORD( libmod_gfx, my, COLLISION_RESERVED_TYPE_SCAN ) != id ) { /* Check if type change from last call */
        LOCINT64( libmod_gfx, my, COLLISION_RESERVED_MODE ) = 3;
        LOCQWORD( libmod_gfx, my, COLLISION_RESERVED_ID_SCAN ) = 0;
        LOCQWORD( libmod_gfx, my, COLLISION_RESERVED_TYPE_SCAN ) = id;
        * ctx = NULL;
        * idxA = 0;
        * idxB = 0;
    }

    ptr = NULL;

    if ( ( id_scan = LOCQWORD( libmod_gfx, my, COLLISION_RESERVED_ID_SCAN ) ) ) ptr = instance_get( id_scan );
    if ( !ptr ) ptr = instance_get_by_type( id, ctx );

    while ( ptr ) {
        if ( ctype == LOCQWORD( libmod_gfx, ptr, CTYPE ) && LOCQWORD( libmod_gfx, ptr, STATUS ) & ( STATUS_RUNNING | STATUS_FROZEN ) && ptr != my ) {
            if ( __get_proc_info( ptr, ociB ) ) {
                collision = __check_collision( ociA, ociB, idxA, idxB, cbox_result_code ) ;
                FREE( ociB->cboxes );
                if ( collision ) {
                    (*idxB)++;
                    FREE( ociA->cboxes );
                    LOCQWORD( libmod_gfx, my, COLLISION_RESERVED_ID_SCAN ) = LOCQWORD( libmod_gfx, ptr, PROCESS_ID );
                    LOCINT64( libmod_gfx, my, COLLIDER_CBOX ) = cbox_result_code[0];
                    LOCINT64( libmod_gfx, my, COLLIDED_ID   ) = LOCQWORD( libmod_gfx, ptr, PROCESS_ID );
                    LOCINT64( libmod_gfx, my, COLLIDED_CBOX ) = cbox_result_code[1];
                    return LOCQWORD( libmod_gfx, ptr, PROCESS_ID );;
                }
            }
        }
        ptr = instance_get_by_type( id, ctx );
    }
    FREE( ociA->cboxes );
    LOCINT64( libmod_gfx, my, COLLISION_RESERVED_MODE ) = -1;
    return 0;
}

/* --------------------------------------------------------------------------- */

int64_t libmod_gfx_collision( INSTANCE * my, int64_t * params ) {
    return __collision( my, params[ 0 ] );
}

/* --------------------------------------------------------------------------- */

int64_t libmod_gfx_collision2( INSTANCE * my, int64_t * params ) {
    my = instance_get( params[ 0 ] );
    if ( !my ) return 0;
    return __collision( my, params[ 1 ] );
}

/* --------------------------------------------------------------------------- */
