/*
 ******************************************************************************
 *  Copyright (c) 2015 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************
 */
#pragma once

#include "dct_impl.h"
#include "dct.h"


// current dct is at offset 10
// application data at offset 7548

#ifdef __cplusplus
extern "C" {
#endif

void dcd_migrate_data();

STATIC_ASSERT(offset_application_dct, (offsetof(complete_dct_t, application)==7548+1024) );
STATIC_ASSERT(size_complete_dct, (sizeof(complete_dct_t)<16384));


#ifdef __cplusplus
}
#endif