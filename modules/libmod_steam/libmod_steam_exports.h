/*
 *  Copyright (C) 2016 Pablo Antonio Navarro Reyes <panreyes@panreyes.com>
 *  Copyright (C) 2016 Joseba García Etxebarria <joseba.gar@gmail.com>
 *
 *  This file is part of PixTudio
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

#ifndef __LIBMOD_STEAM_EXPORTS
#define __LIBMOD_STEAM_EXPORTS

#ifdef __cplusplus
extern "C" {
#endif

#include <bgddl.h>
#include "libmod_steam.h"

#if defined(__BGDC__) || !defined(__STATIC__)
DLCONSTANT  __bgdexport( libmod_steam, constants_def )[] = {
    { "AVATAR_SMALL"   , TYPE_INT, AVATAR_SMALL  },
    { "AVATAR_MEDIUM"  , TYPE_INT, AVATAR_MEDIUM },
    { "AVATAR_LARGE"   , TYPE_INT, AVATAR_LARGE  },
    { NULL             , 0       , 0 }
} ;

char * __bgdexport( libmod_steam, globals_def ) =
    "INT steam_appid = 0;\n"
    "STRING steam_username = \"\";\n";

#endif

DLSYSFUNCS  __bgdexport( libmod_steam, functions_exports )[] = {
    FUNC("STEAM_ACHIEVEMENT_UNLOCK"  , "S" , TYPE_INT   , libmodsteam_achievement_unlock  ),
    FUNC("STEAM_LANG_GET"            , ""  , TYPE_STRING, libmodsteam_lang_get            ),
    FUNC("STEAM_IS_SUBSCRIBED_APP"   , "I"  , TYPE_INT  , libmodsteam_is_subscribed_app   ),
    FUNC("STEAM_INIT"                , "I" , TYPE_INT   , libmodsteam_init                ),
    FUNC(0, 0, 0, 0)
};

char * __bgdexport( libmod_steam, module_dependency )[] = {
    "libgrbase",
    NULL
};

#endif

#ifdef __cplusplus
}
#endif
