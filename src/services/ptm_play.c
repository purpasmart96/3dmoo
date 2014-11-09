/*
 * Copyright (C) 2014 - plutoo
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "util.h"
#include "handles.h"
#include "mem.h"
#include "arm11.h"

#include "service_macros.h"

SERVICE_START(ptm_play);

SERVICE_CMD(0x08080000)   //GetPlayHistoryStart
{
    DEBUG("GetPlayHistoryStart\n");

    RESP(2, 0); // start?
    RESP(1, 0); // Result
    return 0;
}
SERVICE_CMD(0x08090000)   //GetPlayHistoryLength
{
    DEBUG("GetPlayHistoryLength\n");

    RESP(2, 4); // lenght?
    RESP(1, 0); // Result
    return 0;
}
SERVICE_CMD(0x000F0084)   //GetPlayHistoryLength
{
    DEBUG("GetPlayHistoryLength\n", CMD(0), CMD(1), CMD(2), CMD(3), CMD(4), CMD(5));

    RESP(1, 0); // Result
    return 0;
}
SERVICE_END();